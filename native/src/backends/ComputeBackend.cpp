#include "backends/ComputeBackend.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <mutex>
#include <vector>

#if defined(_WIN32) && defined(MAGNAUNDASONI_COMPUTE)

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

namespace magnaundasoni {

using Microsoft::WRL::ComPtr;

namespace {

struct GPUNode {
    float    boundsMin[3];
    uint32_t leftChild = 0;
    float    boundsMax[3];
    uint32_t rightChild = 0;
    uint32_t primStart = 0;
    uint32_t primCount = 0;
    uint32_t isLeaf = 0;
    uint32_t pad = 0;
};

struct GPUTriangle {
    float    v0[3];
    uint32_t materialID = 0;
    float    e1[3];
    uint32_t geometryID = 0;
    float    e2[3];
    float    pad0 = 0.0f;
    float    normal[3];
    float    pad1 = 0.0f;
};

struct GPURay {
    float origin[3];
    float tMin = 0.0f;
    float direction[3];
    float tMax = 0.0f;
};

struct GPUClosestHit {
    uint32_t hit = 0;
    float    distance = 0.0f;
    uint32_t materialID = 0;
    uint32_t geometryID = 0;
    float    hitPoint[3];
    float    pad0 = 0.0f;
    float    normal[3];
    float    pad1 = 0.0f;
};

struct GPUAnyHit {
    uint32_t hit = 0;
    uint32_t pad0 = 0;
    uint32_t pad1 = 0;
    uint32_t pad2 = 0;
};

struct DispatchConstants {
    uint32_t rayCount = 0;
    uint32_t nodeCount = 0;
    uint32_t triangleCount = 0;
    uint32_t pad = 0;
};

static_assert(sizeof(GPUNode) == 48, "Unexpected GPUNode size");
static_assert(sizeof(GPUTriangle) == 64, "Unexpected GPUTriangle size");
static_assert(sizeof(GPURay) == 32, "Unexpected GPURay size");
static_assert(sizeof(GPUClosestHit) == 48, "Unexpected GPUClosestHit size");
static_assert(sizeof(GPUAnyHit) == 16, "Unexpected GPUAnyHit size");
static_assert(sizeof(DispatchConstants) == 16, "Unexpected DispatchConstants size");

constexpr const char* kClosestShaderSource = R"HLSL(
struct Node {
    float3 boundsMin;
    uint leftChild;
    float3 boundsMax;
    uint rightChild;
    uint primStart;
    uint primCount;
    uint isLeaf;
    uint pad;
};

struct Triangle {
    float3 v0;
    uint materialID;
    float3 e1;
    uint geometryID;
    float3 e2;
    float pad0;
    float3 normal;
    float pad1;
};

struct RayIn {
    float3 origin;
    float tMin;
    float3 direction;
    float tMax;
};

struct ClosestHitOut {
    uint hit;
    float distance;
    uint materialID;
    uint geometryID;
    float3 hitPoint;
    float pad0;
    float3 normal;
    float pad1;
};

cbuffer DispatchConstants : register(b0)
{
    uint rayCount;
    uint nodeCount;
    uint triangleCount;
    uint pad;
};

StructuredBuffer<Node> gNodes : register(t0);
StructuredBuffer<Triangle> gTriangles : register(t1);
StructuredBuffer<RayIn> gRays : register(t2);
RWStructuredBuffer<ClosestHitOut> gOut : register(u0);

float SafeReciprocal(float value)
{
    float absValue = abs(value);
    if (absValue < 1.0e-12f)
        value = (value < 0.0f) ? -1.0e-12f : 1.0e-12f;
    return 1.0f / value;
}

bool RayBox(RayIn ray, float3 boundsMin, float3 boundsMax, float tMaxLimit, out float entryT)
{
    float3 invDir = float3(SafeReciprocal(ray.direction.x),
                           SafeReciprocal(ray.direction.y),
                           SafeReciprocal(ray.direction.z));

    float tMin = ray.tMin;
    float tMax = tMaxLimit;

    [unroll]
    for (int i = 0; i < 3; ++i) {
        float t0 = (boundsMin[i] - ray.origin[i]) * invDir[i];
        float t1 = (boundsMax[i] - ray.origin[i]) * invDir[i];
        if (invDir[i] < 0.0f) {
            float tmp = t0;
            t0 = t1;
            t1 = tmp;
        }
        tMin = max(tMin, t0);
        tMax = min(tMax, t1);
        if (tMax < tMin) {
            entryT = 0.0f;
            return false;
        }
    }

    entryT = tMin;
    return true;
}

bool RayTriangle(RayIn ray, Triangle tri, out float outT)
{
    const float EPSILON = 1.0e-8f;
    float3 h = cross(ray.direction, tri.e2);
    float a = dot(tri.e1, h);
    if (abs(a) < EPSILON) {
        outT = 0.0f;
        return false;
    }

    float f = 1.0f / a;
    float3 s = ray.origin - tri.v0;
    float u = f * dot(s, h);
    if (u < 0.0f || u > 1.0f) {
        outT = 0.0f;
        return false;
    }

    float3 q = cross(s, tri.e1);
    float v = f * dot(ray.direction, q);
    if (v < 0.0f || u + v > 1.0f) {
        outT = 0.0f;
        return false;
    }

    float t = f * dot(tri.e2, q);
    if (t < ray.tMin || t > ray.tMax) {
        outT = 0.0f;
        return false;
    }

    outT = t;
    return true;
}

[numthreads(64, 1, 1)]
void ClosestMain(uint3 dtid : SV_DispatchThreadID)
{
    uint rayIndex = dtid.x;
    if (rayIndex >= rayCount) return;

    RayIn ray = gRays[rayIndex];
    ClosestHitOut output;
    output.hit = 0;
    output.distance = ray.tMax;
    output.materialID = 0;
    output.geometryID = 0;
    output.hitPoint = float3(0.0f, 0.0f, 0.0f);
    output.normal = float3(0.0f, 0.0f, 0.0f);
    output.pad0 = 0.0f;
    output.pad1 = 0.0f;

    if (nodeCount == 0 || triangleCount == 0) {
        gOut[rayIndex] = output;
        return;
    }

    uint stack[64];
    uint stackPtr = 0;
    stack[stackPtr++] = 0;
    float closestT = ray.tMax;

    while (stackPtr > 0) {
        uint nodeIndex = stack[--stackPtr];
        Node node = gNodes[nodeIndex];
        float nodeEntryT = 0.0f;
        if (!RayBox(ray, node.boundsMin, node.boundsMax, closestT, nodeEntryT))
            continue;

        if (node.isLeaf != 0) {
            [loop]
            for (uint i = 0; i < node.primCount; ++i) {
                Triangle tri = gTriangles[node.primStart + i];
                float t = 0.0f;
                if (RayTriangle(ray, tri, t) && t < closestT) {
                    closestT = t;
                    output.hit = 1;
                    output.distance = t;
                    output.materialID = tri.materialID;
                    output.geometryID = tri.geometryID;
                    output.hitPoint = ray.origin + ray.direction * t;
                    output.normal = tri.normal;
                }
            }
        } else {
            float leftEntryT = 0.0f;
            float rightEntryT = 0.0f;
            bool leftHit = RayBox(ray, gNodes[node.leftChild].boundsMin, gNodes[node.leftChild].boundsMax,
                                  closestT, leftEntryT);
            bool rightHit = RayBox(ray, gNodes[node.rightChild].boundsMin, gNodes[node.rightChild].boundsMax,
                                   closestT, rightEntryT);

            if (leftHit && rightHit) {
                uint first = node.leftChild;
                uint second = node.rightChild;
                if (leftEntryT > rightEntryT) {
                    first = node.rightChild;
                    second = node.leftChild;
                }

                if (stackPtr < 63) stack[stackPtr++] = second;
                if (stackPtr < 63) stack[stackPtr++] = first;
            } else if (leftHit) {
                if (stackPtr < 64) stack[stackPtr++] = node.leftChild;
            } else if (rightHit) {
                if (stackPtr < 64) stack[stackPtr++] = node.rightChild;
            }
        }
    }

    gOut[rayIndex] = output;
}
)HLSL";

constexpr const char* kAnyShaderSource = R"HLSL(
struct Node {
    float3 boundsMin;
    uint leftChild;
    float3 boundsMax;
    uint rightChild;
    uint primStart;
    uint primCount;
    uint isLeaf;
    uint pad;
};

struct Triangle {
    float3 v0;
    uint materialID;
    float3 e1;
    uint geometryID;
    float3 e2;
    float pad0;
    float3 normal;
    float pad1;
};

struct RayIn {
    float3 origin;
    float tMin;
    float3 direction;
    float tMax;
};

struct AnyHitOut {
    uint hit;
    uint pad0;
    uint pad1;
    uint pad2;
};

cbuffer DispatchConstants : register(b0)
{
    uint rayCount;
    uint nodeCount;
    uint triangleCount;
    uint pad;
};

StructuredBuffer<Node> gNodes : register(t0);
StructuredBuffer<Triangle> gTriangles : register(t1);
StructuredBuffer<RayIn> gRays : register(t2);
RWStructuredBuffer<AnyHitOut> gOut : register(u0);

float SafeReciprocal(float value)
{
    float absValue = abs(value);
    if (absValue < 1.0e-12f)
        value = (value < 0.0f) ? -1.0e-12f : 1.0e-12f;
    return 1.0f / value;
}

bool RayBox(RayIn ray, float3 boundsMin, float3 boundsMax, float tMaxLimit, out float entryT)
{
    float3 invDir = float3(SafeReciprocal(ray.direction.x),
                           SafeReciprocal(ray.direction.y),
                           SafeReciprocal(ray.direction.z));

    float tMin = ray.tMin;
    float tMax = tMaxLimit;

    [unroll]
    for (int i = 0; i < 3; ++i) {
        float t0 = (boundsMin[i] - ray.origin[i]) * invDir[i];
        float t1 = (boundsMax[i] - ray.origin[i]) * invDir[i];
        if (invDir[i] < 0.0f) {
            float tmp = t0;
            t0 = t1;
            t1 = tmp;
        }
        tMin = max(tMin, t0);
        tMax = min(tMax, t1);
        if (tMax < tMin) {
            entryT = 0.0f;
            return false;
        }
    }

    entryT = tMin;
    return true;
}

bool RayTriangle(RayIn ray, Triangle tri)
{
    const float EPSILON = 1.0e-8f;
    float3 h = cross(ray.direction, tri.e2);
    float a = dot(tri.e1, h);
    if (abs(a) < EPSILON) return false;

    float f = 1.0f / a;
    float3 s = ray.origin - tri.v0;
    float u = f * dot(s, h);
    if (u < 0.0f || u > 1.0f) return false;

    float3 q = cross(s, tri.e1);
    float v = f * dot(ray.direction, q);
    if (v < 0.0f || u + v > 1.0f) return false;

    float t = f * dot(tri.e2, q);
    return (t >= ray.tMin && t <= ray.tMax);
}

[numthreads(64, 1, 1)]
void AnyMain(uint3 dtid : SV_DispatchThreadID)
{
    uint rayIndex = dtid.x;
    if (rayIndex >= rayCount) return;

    AnyHitOut output;
    output.hit = 0;
    output.pad0 = 0;
    output.pad1 = 0;
    output.pad2 = 0;

    if (nodeCount == 0 || triangleCount == 0) {
        gOut[rayIndex] = output;
        return;
    }

    RayIn ray = gRays[rayIndex];
    uint stack[64];
    uint stackPtr = 0;
    stack[stackPtr++] = 0;

    while (stackPtr > 0) {
        uint nodeIndex = stack[--stackPtr];
        Node node = gNodes[nodeIndex];
        float nodeEntryT = 0.0f;
        if (!RayBox(ray, node.boundsMin, node.boundsMax, ray.tMax, nodeEntryT))
            continue;

        if (node.isLeaf != 0) {
            [loop]
            for (uint i = 0; i < node.primCount; ++i) {
                if (RayTriangle(ray, gTriangles[node.primStart + i])) {
                    output.hit = 1;
                    gOut[rayIndex] = output;
                    return;
                }
            }
        } else {
            float leftEntryT = 0.0f;
            float rightEntryT = 0.0f;
            bool leftHit = RayBox(ray, gNodes[node.leftChild].boundsMin, gNodes[node.leftChild].boundsMax,
                                  ray.tMax, leftEntryT);
            bool rightHit = RayBox(ray, gNodes[node.rightChild].boundsMin, gNodes[node.rightChild].boundsMax,
                                   ray.tMax, rightEntryT);

            if (leftHit && rightHit) {
                uint first = node.leftChild;
                uint second = node.rightChild;
                if (leftEntryT > rightEntryT) {
                    first = node.rightChild;
                    second = node.leftChild;
                }

                if (stackPtr < 63) stack[stackPtr++] = second;
                if (stackPtr < 63) stack[stackPtr++] = first;
            } else if (leftHit) {
                if (stackPtr < 64) stack[stackPtr++] = node.leftChild;
            } else if (rightHit) {
                if (stackPtr < 64) stack[stackPtr++] = node.rightChild;
            }
        }
    }

    gOut[rayIndex] = output;
}
)HLSL";

HRESULT compileComputeShader(ID3D11Device* device,
                             const char* source,
                             const char* entryPoint,
                             ID3D11ComputeShader** shader) {
    *shader = nullptr;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG;
#endif

    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;
    const char* profile = (device->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0) ? "cs_5_0" : "cs_4_0";
    HRESULT hr = D3DCompile(source, std::strlen(source), "MagnaundasoniComputeBackend",
                            nullptr, nullptr, entryPoint, profile, flags, 0,
                            shaderBlob.GetAddressOf(), errorBlob.GetAddressOf());
    if (FAILED(hr)) {
        return hr;
    }

    return device->CreateComputeShader(shaderBlob->GetBufferPointer(),
                                       shaderBlob->GetBufferSize(),
                                       nullptr, shader);
}

template <typename T>
HRESULT createStructuredBuffer(ID3D11Device* device,
                               UINT count,
                               UINT bindFlags,
                               D3D11_USAGE usage,
                               UINT cpuAccessFlags,
                               const T* initialData,
                               ID3D11Buffer** buffer) {
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = (std::max)(1u, count) * static_cast<UINT>(sizeof(T));
    desc.Usage = usage;
    desc.BindFlags = bindFlags;
    desc.CPUAccessFlags = cpuAccessFlags;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = static_cast<UINT>(sizeof(T));

    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem = initialData;
    return device->CreateBuffer(&desc, initialData ? &data : nullptr, buffer);
}

template <typename T>
HRESULT createBufferSRV(ID3D11Device* device, ID3D11Buffer* buffer, UINT count,
                        ID3D11ShaderResourceView** srv) {
    D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
    desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    desc.Buffer.FirstElement = 0;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.Buffer.NumElements = count;
    return device->CreateShaderResourceView(buffer, &desc, srv);
}

template <typename T>
HRESULT createBufferUAV(ID3D11Device* device, ID3D11Buffer* buffer, UINT count,
                        ID3D11UnorderedAccessView** uav) {
    D3D11_UNORDERED_ACCESS_VIEW_DESC desc{};
    desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    desc.Buffer.FirstElement = 0;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.Buffer.NumElements = count;
    return device->CreateUnorderedAccessView(buffer, &desc, uav);
}

class D3D11ComputeBackend final : public ComputeBackend {
public:
    D3D11ComputeBackend() {
        initializeInternalDevice();
    }

    bool available() const override {
        return available_;
    }

    bool attachExternalD3D11Device(void* device, void* deviceContext) override {
        std::lock_guard<std::mutex> lock(mutex_);

        resetResources();
        device_.Reset();
        context_.Reset();

        if (!device) {
            usingExternalDevice_ = false;
            initializeInternalDevice();
            return available_;
        }

        auto* d3dDevice = static_cast<ID3D11Device*>(device);
        auto* d3dContext = static_cast<ID3D11DeviceContext*>(deviceContext);
        device_ = d3dDevice;
        if (d3dContext) {
            context_ = d3dContext;
        } else {
            device_->GetImmediateContext(context_.GetAddressOf());
        }

        usingExternalDevice_ = true;
        available_ = initializeWithCurrentDevice();
        return available_;
    }

    bool usingExternalD3D11Device() const override {
        return usingExternalDevice_;
    }

    bool syncScene(const BVH& bvh) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!available_) return false;

        const auto& nodes = bvh.nodes();
        const auto& triangles = bvh.triangles();
        nodeCount_ = static_cast<uint32_t>(nodes.size());
        triangleCount_ = static_cast<uint32_t>(triangles.size());

        if (nodes.empty() || triangles.empty()) {
            nodeBuffer_.Reset();
            nodeSRV_.Reset();
            triangleBuffer_.Reset();
            triangleSRV_.Reset();
            sceneUploaded_ = true;
            return true;
        }

        std::vector<GPUNode> gpuNodes(nodes.size());
        for (size_t i = 0; i < nodes.size(); ++i) {
            const BVHNode& node = nodes[i];
            GPUNode& gpuNode = gpuNodes[i];
            gpuNode.boundsMin[0] = node.bounds.min.x;
            gpuNode.boundsMin[1] = node.bounds.min.y;
            gpuNode.boundsMin[2] = node.bounds.min.z;
            gpuNode.leftChild = node.leftChild;
            gpuNode.boundsMax[0] = node.bounds.max.x;
            gpuNode.boundsMax[1] = node.bounds.max.y;
            gpuNode.boundsMax[2] = node.bounds.max.z;
            gpuNode.rightChild = node.rightChild;
            gpuNode.primStart = node.primStart;
            gpuNode.primCount = node.primCount;
            gpuNode.isLeaf = node.isLeaf ? 1u : 0u;
        }

        std::vector<GPUTriangle> gpuTriangles(triangles.size());
        for (size_t i = 0; i < triangles.size(); ++i) {
            const Triangle& tri = triangles[i];
            GPUTriangle& gpuTri = gpuTriangles[i];
            gpuTri.v0[0] = tri.v0.x;
            gpuTri.v0[1] = tri.v0.y;
            gpuTri.v0[2] = tri.v0.z;
            gpuTri.materialID = tri.materialID;
            gpuTri.e1[0] = tri.e1.x;
            gpuTri.e1[1] = tri.e1.y;
            gpuTri.e1[2] = tri.e1.z;
            gpuTri.geometryID = tri.geometryID;
            gpuTri.e2[0] = tri.e2.x;
            gpuTri.e2[1] = tri.e2.y;
            gpuTri.e2[2] = tri.e2.z;
            gpuTri.normal[0] = tri.normal.x;
            gpuTri.normal[1] = tri.normal.y;
            gpuTri.normal[2] = tri.normal.z;
        }

        if (FAILED(createStructuredBuffer(device_.Get(), nodeCount_, D3D11_BIND_SHADER_RESOURCE,
                                          D3D11_USAGE_DEFAULT, 0, gpuNodes.data(),
                                          nodeBuffer_.ReleaseAndGetAddressOf()))) {
            return false;
        }
        if (FAILED(createBufferSRV<GPUNode>(device_.Get(), nodeBuffer_.Get(), nodeCount_,
                                            nodeSRV_.ReleaseAndGetAddressOf()))) {
            return false;
        }

        if (FAILED(createStructuredBuffer(device_.Get(), triangleCount_, D3D11_BIND_SHADER_RESOURCE,
                                          D3D11_USAGE_DEFAULT, 0, gpuTriangles.data(),
                                          triangleBuffer_.ReleaseAndGetAddressOf()))) {
            return false;
        }
        if (FAILED(createBufferSRV<GPUTriangle>(device_.Get(), triangleBuffer_.Get(), triangleCount_,
                                                triangleSRV_.ReleaseAndGetAddressOf()))) {
            return false;
        }

        sceneUploaded_ = true;
        return true;
    }

    bool traceClosestBatch(const std::vector<Ray>& rays,
                           std::vector<HitResult>& outHits) override {
        std::lock_guard<std::mutex> lock(mutex_);
        outHits.assign(rays.size(), HitResult{});
        if (!available_ || !sceneUploaded_) return false;
        if (rays.empty()) return true;
        if (nodeCount_ == 0 || triangleCount_ == 0) return true;

        if (!ensureRayInputCapacity(static_cast<uint32_t>(rays.size())) ||
            !ensureClosestOutputCapacity(static_cast<uint32_t>(rays.size()))) {
            return false;
        }

        std::vector<GPURay> gpuRays(rays.size());
        for (size_t i = 0; i < rays.size(); ++i) {
            const Ray& ray = rays[i];
            GPURay& gpuRay = gpuRays[i];
            gpuRay.origin[0] = ray.origin.x;
            gpuRay.origin[1] = ray.origin.y;
            gpuRay.origin[2] = ray.origin.z;
            gpuRay.tMin = ray.tMin;
            gpuRay.direction[0] = ray.direction.x;
            gpuRay.direction[1] = ray.direction.y;
            gpuRay.direction[2] = ray.direction.z;
            gpuRay.tMax = ray.tMax;
        }

        if (!uploadRayInputs(gpuRays.data(), static_cast<uint32_t>(gpuRays.size()))) {
            return false;
        }

        if (!runClosestShader(static_cast<uint32_t>(gpuRays.size()))) {
            return false;
        }

        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = context_->Map(closestReadbackBuffer_.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(hr)) return false;

        const auto* results = static_cast<const GPUClosestHit*>(mapped.pData);
        for (size_t i = 0; i < rays.size(); ++i) {
            const GPUClosestHit& src = results[i];
            HitResult& dst = outHits[i];
            dst.distance = src.distance;
            dst.materialID = src.materialID;
            dst.geometryID = src.geometryID;
            dst.hitPoint = Vec3{src.hitPoint[0], src.hitPoint[1], src.hitPoint[2]};
            dst.normal = Vec3{src.normal[0], src.normal[1], src.normal[2]};
            dst.hit = src.hit != 0;
        }

        context_->Unmap(closestReadbackBuffer_.Get(), 0);
        return true;
    }

    bool traceAnyBatch(const std::vector<Ray>& rays,
                       std::vector<uint8_t>& outHits) override {
        std::lock_guard<std::mutex> lock(mutex_);
        outHits.assign(rays.size(), 0);
        if (!available_ || !sceneUploaded_) return false;
        if (rays.empty()) return true;
        if (nodeCount_ == 0 || triangleCount_ == 0) return true;

        if (!ensureRayInputCapacity(static_cast<uint32_t>(rays.size())) ||
            !ensureAnyOutputCapacity(static_cast<uint32_t>(rays.size()))) {
            return false;
        }

        std::vector<GPURay> gpuRays(rays.size());
        for (size_t i = 0; i < rays.size(); ++i) {
            const Ray& ray = rays[i];
            GPURay& gpuRay = gpuRays[i];
            gpuRay.origin[0] = ray.origin.x;
            gpuRay.origin[1] = ray.origin.y;
            gpuRay.origin[2] = ray.origin.z;
            gpuRay.tMin = ray.tMin;
            gpuRay.direction[0] = ray.direction.x;
            gpuRay.direction[1] = ray.direction.y;
            gpuRay.direction[2] = ray.direction.z;
            gpuRay.tMax = ray.tMax;
        }

        if (!uploadRayInputs(gpuRays.data(), static_cast<uint32_t>(gpuRays.size()))) {
            return false;
        }

        if (!runAnyShader(static_cast<uint32_t>(gpuRays.size()))) {
            return false;
        }

        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = context_->Map(anyReadbackBuffer_.Get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(hr)) return false;

        const auto* results = static_cast<const GPUAnyHit*>(mapped.pData);
        for (size_t i = 0; i < rays.size(); ++i)
            outHits[i] = results[i].hit ? 1u : 0u;

        context_->Unmap(anyReadbackBuffer_.Get(), 0);
        return true;
    }

private:
    void resetResources() {
        available_ = false;
        sceneUploaded_ = false;
        nodeCount_ = 0;
        triangleCount_ = 0;
        rayCapacity_ = 0;
        closestOutputCapacity_ = 0;
        anyOutputCapacity_ = 0;

        closestShader_.Reset();
        anyShader_.Reset();
        dispatchConstants_.Reset();
        nodeBuffer_.Reset();
        nodeSRV_.Reset();
        triangleBuffer_.Reset();
        triangleSRV_.Reset();
        rayBuffer_.Reset();
        raySRV_.Reset();
        closestOutputBuffer_.Reset();
        closestOutputUAV_.Reset();
        closestReadbackBuffer_.Reset();
        anyOutputBuffer_.Reset();
        anyOutputUAV_.Reset();
        anyReadbackBuffer_.Reset();
    }

    bool initializeWithCurrentDevice() {
        if (!device_ || !context_) return false;

        D3D_FEATURE_LEVEL featureLevel = device_->GetFeatureLevel();
        if (featureLevel < D3D_FEATURE_LEVEL_11_0) {
            D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS options{};
            HRESULT hr = device_->CheckFeatureSupport(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS,
                                                      &options, sizeof(options));
            if (FAILED(hr) || !options.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x)
                return false;
        }

        if (FAILED(compileComputeShader(device_.Get(), kClosestShaderSource,
                                        "ClosestMain", closestShader_.ReleaseAndGetAddressOf()))) {
            return false;
        }

        if (FAILED(compileComputeShader(device_.Get(), kAnyShaderSource,
                                        "AnyMain", anyShader_.ReleaseAndGetAddressOf()))) {
            return false;
        }

        D3D11_BUFFER_DESC constantDesc{};
        constantDesc.ByteWidth = sizeof(DispatchConstants);
        constantDesc.Usage = D3D11_USAGE_DYNAMIC;
        constantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        constantDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(device_->CreateBuffer(&constantDesc, nullptr,
                                         dispatchConstants_.ReleaseAndGetAddressOf()))) {
            return false;
        }

        available_ = true;
        return true;
    }

    void initializeInternalDevice() {
        resetResources();

        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        const D3D_FEATURE_LEVEL levels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };

        D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                       levels, static_cast<UINT>(sizeof(levels) / sizeof(levels[0])),
                                       D3D11_SDK_VERSION,
                                       device_.GetAddressOf(), &featureLevel,
                                       context_.GetAddressOf());
        if (hr == E_INVALIDARG) {
            hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                   &levels[1], static_cast<UINT>((sizeof(levels) / sizeof(levels[0])) - 1),
                                   D3D11_SDK_VERSION,
                                   device_.GetAddressOf(), &featureLevel,
                                   context_.GetAddressOf());
        }
        if (FAILED(hr)) return;
        usingExternalDevice_ = false;
        available_ = initializeWithCurrentDevice();
    }

    bool ensureRayInputCapacity(uint32_t rayCount) {
        if (rayCount <= rayCapacity_ && rayBuffer_ && raySRV_)
            return true;

        rayCapacity_ = (std::max)(1u, rayCount);
        if (FAILED(createStructuredBuffer<GPURay>(device_.Get(), rayCapacity_, D3D11_BIND_SHADER_RESOURCE,
                                                  D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE,
                                                  nullptr, rayBuffer_.ReleaseAndGetAddressOf()))) {
            return false;
        }
        if (FAILED(createBufferSRV<GPURay>(device_.Get(), rayBuffer_.Get(), rayCapacity_,
                                           raySRV_.ReleaseAndGetAddressOf()))) {
            return false;
        }
        return true;
    }

    bool ensureClosestOutputCapacity(uint32_t rayCount) {
        if (rayCount <= closestOutputCapacity_ && closestOutputBuffer_ && closestOutputUAV_ && closestReadbackBuffer_)
            return true;

        closestOutputCapacity_ = (std::max)(1u, rayCount);
        if (FAILED(createStructuredBuffer<GPUClosestHit>(device_.Get(), closestOutputCapacity_, D3D11_BIND_UNORDERED_ACCESS,
                                                         D3D11_USAGE_DEFAULT, 0, nullptr,
                                                         closestOutputBuffer_.ReleaseAndGetAddressOf()))) {
            return false;
        }
        if (FAILED(createBufferUAV<GPUClosestHit>(device_.Get(), closestOutputBuffer_.Get(), closestOutputCapacity_,
                                                  closestOutputUAV_.ReleaseAndGetAddressOf()))) {
            return false;
        }
        if (FAILED(createStructuredBuffer<GPUClosestHit>(device_.Get(), closestOutputCapacity_, 0,
                                                         D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ, nullptr,
                                                         closestReadbackBuffer_.ReleaseAndGetAddressOf()))) {
            return false;
        }
        return true;
    }

    bool ensureAnyOutputCapacity(uint32_t rayCount) {
        if (rayCount <= anyOutputCapacity_ && anyOutputBuffer_ && anyOutputUAV_ && anyReadbackBuffer_)
            return true;

        anyOutputCapacity_ = (std::max)(1u, rayCount);
        if (FAILED(createStructuredBuffer<GPUAnyHit>(device_.Get(), anyOutputCapacity_, D3D11_BIND_UNORDERED_ACCESS,
                                                     D3D11_USAGE_DEFAULT, 0, nullptr,
                                                     anyOutputBuffer_.ReleaseAndGetAddressOf()))) {
            return false;
        }
        if (FAILED(createBufferUAV<GPUAnyHit>(device_.Get(), anyOutputBuffer_.Get(), anyOutputCapacity_,
                                              anyOutputUAV_.ReleaseAndGetAddressOf()))) {
            return false;
        }
        if (FAILED(createStructuredBuffer<GPUAnyHit>(device_.Get(), anyOutputCapacity_, 0,
                                                     D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ, nullptr,
                                                     anyReadbackBuffer_.ReleaseAndGetAddressOf()))) {
            return false;
        }
        return true;
    }

    bool uploadRayInputs(const GPURay* rays, uint32_t rayCount) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = context_->Map(rayBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (FAILED(hr)) return false;
        std::memcpy(mapped.pData, rays, sizeof(GPURay) * rayCount);
        context_->Unmap(rayBuffer_.Get(), 0);
        return true;
    }

    bool updateDispatchConstants(uint32_t rayCount) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = context_->Map(dispatchConstants_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (FAILED(hr)) return false;

        auto* constants = static_cast<DispatchConstants*>(mapped.pData);
        constants->rayCount = rayCount;
        constants->nodeCount = nodeCount_;
        constants->triangleCount = triangleCount_;
        constants->pad = 0;
        context_->Unmap(dispatchConstants_.Get(), 0);
        return true;
    }

    bool runClosestShader(uint32_t rayCount) {
        if (!updateDispatchConstants(rayCount)) return false;

        ID3D11ShaderResourceView* srvs[] = { nodeSRV_.Get(), triangleSRV_.Get(), raySRV_.Get() };
        ID3D11UnorderedAccessView* uavs[] = { closestOutputUAV_.Get() };
        ID3D11Buffer* constantBuffers[] = { dispatchConstants_.Get() };

        context_->CSSetShader(closestShader_.Get(), nullptr, 0);
        context_->CSSetShaderResources(0, static_cast<UINT>(std::size(srvs)), srvs);
        context_->CSSetUnorderedAccessViews(0, static_cast<UINT>(std::size(uavs)), uavs, nullptr);
        context_->CSSetConstantBuffers(0, 1, constantBuffers);
        context_->Dispatch((rayCount + 63u) / 64u, 1, 1);

        ID3D11ShaderResourceView* nullSRVs[3] = {};
        ID3D11UnorderedAccessView* nullUAVs[1] = {};
        ID3D11Buffer* nullCB[1] = {};
        context_->CSSetShader(nullptr, nullptr, 0);
        context_->CSSetShaderResources(0, 3, nullSRVs);
        context_->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullptr);
        context_->CSSetConstantBuffers(0, 1, nullCB);
        context_->CopyResource(closestReadbackBuffer_.Get(), closestOutputBuffer_.Get());
        return true;
    }

    bool runAnyShader(uint32_t rayCount) {
        if (!updateDispatchConstants(rayCount)) return false;

        ID3D11ShaderResourceView* srvs[] = { nodeSRV_.Get(), triangleSRV_.Get(), raySRV_.Get() };
        ID3D11UnorderedAccessView* uavs[] = { anyOutputUAV_.Get() };
        ID3D11Buffer* constantBuffers[] = { dispatchConstants_.Get() };

        context_->CSSetShader(anyShader_.Get(), nullptr, 0);
        context_->CSSetShaderResources(0, static_cast<UINT>(std::size(srvs)), srvs);
        context_->CSSetUnorderedAccessViews(0, static_cast<UINT>(std::size(uavs)), uavs, nullptr);
        context_->CSSetConstantBuffers(0, 1, constantBuffers);
        context_->Dispatch((rayCount + 63u) / 64u, 1, 1);

        ID3D11ShaderResourceView* nullSRVs[3] = {};
        ID3D11UnorderedAccessView* nullUAVs[1] = {};
        ID3D11Buffer* nullCB[1] = {};
        context_->CSSetShader(nullptr, nullptr, 0);
        context_->CSSetShaderResources(0, 3, nullSRVs);
        context_->CSSetUnorderedAccessViews(0, 1, nullUAVs, nullptr);
        context_->CSSetConstantBuffers(0, 1, nullCB);
        context_->CopyResource(anyReadbackBuffer_.Get(), anyOutputBuffer_.Get());
        return true;
    }

    bool available_ = false;
    bool sceneUploaded_ = false;
    bool usingExternalDevice_ = false;
    uint32_t nodeCount_ = 0;
    uint32_t triangleCount_ = 0;
    uint32_t rayCapacity_ = 0;
    uint32_t closestOutputCapacity_ = 0;
    uint32_t anyOutputCapacity_ = 0;

    std::mutex mutex_;
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<ID3D11ComputeShader> closestShader_;
    ComPtr<ID3D11ComputeShader> anyShader_;
    ComPtr<ID3D11Buffer> dispatchConstants_;

    ComPtr<ID3D11Buffer> nodeBuffer_;
    ComPtr<ID3D11ShaderResourceView> nodeSRV_;
    ComPtr<ID3D11Buffer> triangleBuffer_;
    ComPtr<ID3D11ShaderResourceView> triangleSRV_;

    ComPtr<ID3D11Buffer> rayBuffer_;
    ComPtr<ID3D11ShaderResourceView> raySRV_;

    ComPtr<ID3D11Buffer> closestOutputBuffer_;
    ComPtr<ID3D11UnorderedAccessView> closestOutputUAV_;
    ComPtr<ID3D11Buffer> closestReadbackBuffer_;

    ComPtr<ID3D11Buffer> anyOutputBuffer_;
    ComPtr<ID3D11UnorderedAccessView> anyOutputUAV_;
    ComPtr<ID3D11Buffer> anyReadbackBuffer_;
};

class NullComputeBackend final : public ComputeBackend {
public:
    bool available() const override { return false; }
    bool attachExternalD3D11Device(void*, void*) override { return false; }
    bool usingExternalD3D11Device() const override { return false; }
    bool syncScene(const BVH&) override { return false; }
    bool traceClosestBatch(const std::vector<Ray>&, std::vector<HitResult>&) override { return false; }
    bool traceAnyBatch(const std::vector<Ray>&, std::vector<uint8_t>&) override { return false; }
};

} // namespace

std::unique_ptr<ComputeBackend> createComputeBackend() {
    auto backend = std::make_unique<D3D11ComputeBackend>();
    if (backend->available()) return backend;
    return std::make_unique<NullComputeBackend>();
}

} // namespace magnaundasoni

#else

namespace magnaundasoni {

namespace {

class NullComputeBackend final : public ComputeBackend {
public:
    bool available() const override { return false; }
    bool attachExternalD3D11Device(void*, void*) override { return false; }
    bool usingExternalD3D11Device() const override { return false; }
    bool syncScene(const BVH&) override { return false; }
    bool traceClosestBatch(const std::vector<Ray>&, std::vector<HitResult>&) override { return false; }
    bool traceAnyBatch(const std::vector<Ray>&, std::vector<uint8_t>&) override { return false; }
};

} // namespace

std::unique_ptr<ComputeBackend> createComputeBackend() {
    return std::make_unique<NullComputeBackend>();
}

} // namespace magnaundasoni

#endif
