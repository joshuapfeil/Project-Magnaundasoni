#include "backends/ComputeBackend.h"

#if defined(_WIN32) && defined(MAGNAUNDASONI_COMPUTE)

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <vector>

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

HRESULT compileShader(ID3D12Device* device, const char* source, const char* entryPoint,
                      ComPtr<ID3DBlob>& blob) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG;
#endif

    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3DCompile(source, std::strlen(source), "MagnaundasoniDX12Backend",
                            nullptr, nullptr, entryPoint, "cs_5_0", flags, 0,
                            blob.GetAddressOf(), errorBlob.GetAddressOf());
    return hr;
}

D3D12_RESOURCE_DESC bufferDesc(UINT64 size, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = flags;
    return desc;
}

D3D12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES props{};
    props.Type = type;
    props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    props.CreationNodeMask = 1;
    props.VisibleNodeMask = 1;
    return props;
}

template <typename T>
bool uploadBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* list,
                  const T* data, UINT64 count,
                  ComPtr<ID3D12Resource>& defaultBuffer,
                  ComPtr<ID3D12Resource>& uploadBuffer,
                  D3D12_RESOURCE_STATES finalState,
                  D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COPY_DEST) {
    const UINT64 byteSize = std::max<UINT64>(1, count) * sizeof(T);
    auto defaultHeap = heapProps(D3D12_HEAP_TYPE_DEFAULT);
    auto uploadHeap = heapProps(D3D12_HEAP_TYPE_UPLOAD);
    auto copyDesc = bufferDesc(byteSize);

    if (!defaultBuffer || defaultBuffer->GetDesc().Width < byteSize) {
        if (FAILED(device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE,
                                                   &copyDesc, initialState, nullptr,
                                                   IID_PPV_ARGS(defaultBuffer.ReleaseAndGetAddressOf())))) {
            return false;
        }
    }

    if (!uploadBuffer || uploadBuffer->GetDesc().Width < byteSize) {
        if (FAILED(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE,
                                                   &copyDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                   nullptr, IID_PPV_ARGS(uploadBuffer.ReleaseAndGetAddressOf())))) {
            return false;
        }
    }

    void* mapped = nullptr;
    if (FAILED(uploadBuffer->Map(0, nullptr, &mapped))) return false;
    std::memcpy(mapped, data, static_cast<size_t>(byteSize));
    uploadBuffer->Unmap(0, nullptr);

    D3D12_RESOURCE_BARRIER toCopy = {};
    toCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toCopy.Transition.pResource = defaultBuffer.Get();
    toCopy.Transition.StateBefore = finalState;
    toCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    toCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    list->ResourceBarrier(1, &toCopy);

    list->CopyBufferRegion(defaultBuffer.Get(), 0, uploadBuffer.Get(), 0, byteSize);

    D3D12_RESOURCE_BARRIER toFinal = {};
    toFinal.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toFinal.Transition.pResource = defaultBuffer.Get();
    toFinal.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    toFinal.Transition.StateAfter = finalState;
    toFinal.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    list->ResourceBarrier(1, &toFinal);
    return true;
}

class D3D12ComputeBackend final : public ComputeBackend {
public:
    D3D12ComputeBackend() {
        initializeInternalDevice();
    }

    bool available() const override { return available_; }
    bool attachExternalD3D11Device(void*, void*) override { return false; }
    bool usingExternalD3D11Device() const override { return false; }
    bool attachExternalD3D12Device(void* device) override {
        std::lock_guard<std::mutex> lock(mutex_);
        reset();
        device_ = static_cast<ID3D12Device*>(device);
        usingExternalDevice_ = (device_ != nullptr);
        if (!device_) {
            initializeInternalDevice();
            return available_;
        }
        return initializeWithDevice(device_.Get());
    }
    bool usingExternalD3D12Device() const override { return usingExternalDevice_; }

    bool syncScene(const BVH& bvh) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!available_ || !device_ || !commandList_ || !commandAllocator_) return false;

        const auto& nodes = bvh.nodes();
        const auto& triangles = bvh.triangles();
        nodeCount_ = static_cast<uint32_t>(nodes.size());
        triangleCount_ = static_cast<uint32_t>(triangles.size());
        if (nodes.empty() || triangles.empty()) {
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

        commandAllocator_->Reset();
        commandList_->Reset(commandAllocator_.Get(), nullptr);

        if (!uploadBuffer(device_.Get(), commandList_.Get(), gpuNodes.data(), static_cast<UINT64>(gpuNodes.size()),
                          nodeBuffer_, nodeUploadBuffer_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)) {
            return false;
        }
        if (!uploadBuffer(device_.Get(), commandList_.Get(), gpuTriangles.data(), static_cast<UINT64>(gpuTriangles.size()),
                          triangleBuffer_, triangleUploadBuffer_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)) {
            return false;
        }

        commandList_->Close();
        executeAndWait();
        sceneUploaded_ = true;
        return true;
    }

    bool traceClosestBatch(const std::vector<Ray>& rays,
                           std::vector<HitResult>& outHits) override {
        return traceBatch(rays, outHits, true);
    }

    bool traceAnyBatch(const std::vector<Ray>& rays,
                       std::vector<uint8_t>& outHits) override {
        std::vector<HitResult> dummy(rays.size());
        bool ok = traceBatch(rays, dummy, false);
        outHits.resize(dummy.size());
        for (size_t i = 0; i < dummy.size(); ++i) outHits[i] = dummy[i].hit ? 1u : 0u;
        return ok;
    }

private:
    void reset() {
        available_ = false;
        sceneUploaded_ = false;
        usingExternalDevice_ = false;
        nodeCount_ = 0;
        triangleCount_ = 0;
        rayCapacity_ = 0;
        outputCapacity_ = 0;
        nodeBuffer_.Reset();
        nodeUploadBuffer_.Reset();
        triangleBuffer_.Reset();
        triangleUploadBuffer_.Reset();
        rayBuffer_.Reset();
        rayUploadBuffer_.Reset();
        outputBuffer_.Reset();
        outputReadbackBuffer_.Reset();
        closestPso_.Reset();
        anyPso_.Reset();
        rootSignature_.Reset();
        queue_.Reset();
        commandAllocator_.Reset();
        commandList_.Reset();
        fence_.Reset();
        fenceValue_ = 1;
    }

    bool initializeInternalDevice() {
        ComPtr<IDXGIFactory4> factory;
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf())))) return false;
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; factory->EnumAdapters1(i, adapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device_.GetAddressOf())))) {
                usingExternalDevice_ = false;
                return initializeWithDevice(device_.Get());
            }
        }
        if (SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device_.GetAddressOf())))) {
            usingExternalDevice_ = false;
            return initializeWithDevice(device_.Get());
        }
        return false;
    }

    bool initializeWithDevice(ID3D12Device* device) {
        if (!device) return false;
        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(queue_.GetAddressOf())))) return false;
        if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(commandAllocator_.GetAddressOf())))) return false;

        if (!createRootSignatureAndPso(device)) return false;
        if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, commandAllocator_.Get(), nullptr,
                                             IID_PPV_ARGS(commandList_.GetAddressOf())))) return false;
        if (FAILED(commandList_->Close())) return false;
        if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.GetAddressOf())))) return false;
        fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!fenceEvent_) return false;
        available_ = true;
        return true;
    }

    bool createRootSignatureAndPso(ID3D12Device* device) {
        D3D12_ROOT_PARAMETER params[5] = {};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[0].Constants.ShaderRegister = 0;
        params[0].Constants.RegisterSpace = 0;
        params[0].Constants.Num32BitValues = 4;
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        params[1].Descriptor.ShaderRegister = 0;
        params[1].Descriptor.RegisterSpace = 0;
        params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        params[2].Descriptor.ShaderRegister = 1;
        params[2].Descriptor.RegisterSpace = 0;
        params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        params[3].Descriptor.ShaderRegister = 2;
        params[3].Descriptor.RegisterSpace = 0;
        params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        params[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        params[4].Descriptor.ShaderRegister = 0;
        params[4].Descriptor.RegisterSpace = 0;
        params[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC rsDesc{};
        rsDesc.NumParameters = 5;
        rsDesc.pParameters = params;
        rsDesc.NumStaticSamplers = 0;
        rsDesc.pStaticSamplers = nullptr;
        rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
                       D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS |
                       D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
                       D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                       D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;

        ComPtr<ID3DBlob> sigBlob;
        ComPtr<ID3DBlob> errBlob;
        if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, sigBlob.GetAddressOf(), errBlob.GetAddressOf()))) {
            return false;
        }
        if (FAILED(device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature_.GetAddressOf())))) {
            return false;
        }

        ComPtr<ID3DBlob> closestBlob;
        ComPtr<ID3DBlob> anyBlob;
        if (FAILED(compileShader(device, kClosestShaderSource, "ClosestMain", closestBlob))) return false;
        if (FAILED(compileShader(device, kAnyShaderSource, "AnyMain", anyBlob))) return false;

        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = rootSignature_.Get();
        psoDesc.CS = { closestBlob->GetBufferPointer(), closestBlob->GetBufferSize() };
        if (FAILED(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(closestPso_.GetAddressOf())))) return false;
        psoDesc.CS = { anyBlob->GetBufferPointer(), anyBlob->GetBufferSize() };
        if (FAILED(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(anyPso_.GetAddressOf())))) return false;
        return true;
    }

    template <typename T>
    bool ensureRayBuffer(UINT64 count, ComPtr<ID3D12Resource>& defaultBuffer, ComPtr<ID3D12Resource>& uploadBuffer,
                         D3D12_RESOURCE_STATES defaultState) {
        UINT64 byteSize = std::max<UINT64>(1, count) * sizeof(T);
        auto defaultHeap = heapProps(D3D12_HEAP_TYPE_DEFAULT);
        auto uploadHeap = heapProps(D3D12_HEAP_TYPE_UPLOAD);
        auto desc = bufferDesc(byteSize);
        if (!defaultBuffer || defaultBuffer->GetDesc().Width < byteSize) {
            if (FAILED(device_->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE,
                                                       &desc, defaultState, nullptr,
                                                       IID_PPV_ARGS(defaultBuffer.ReleaseAndGetAddressOf())))) return false;
        }
        if (!uploadBuffer || uploadBuffer->GetDesc().Width < byteSize) {
            if (FAILED(device_->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE,
                                                       &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                       IID_PPV_ARGS(uploadBuffer.ReleaseAndGetAddressOf())))) return false;
        }
        return true;
    }

    bool ensureOutputBuffer(UINT64 count, UINT stride,
                            ComPtr<ID3D12Resource>& defaultBuffer,
                            ComPtr<ID3D12Resource>& readbackBuffer,
                            D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) {
        UINT64 byteSize = std::max<UINT64>(1, count) * stride;
        if (!defaultBuffer || defaultBuffer->GetDesc().Width < byteSize) {
            auto defaultHeap = heapProps(D3D12_HEAP_TYPE_DEFAULT);
            auto defaultDesc = bufferDesc(byteSize, flags);
            if (FAILED(device_->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE,
                                                       &defaultDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                                       nullptr, IID_PPV_ARGS(defaultBuffer.ReleaseAndGetAddressOf())))) return false;
        }
        if (!readbackBuffer || readbackBuffer->GetDesc().Width < byteSize) {
            auto readbackHeap = heapProps(D3D12_HEAP_TYPE_READBACK);
            auto readbackDesc = bufferDesc(byteSize);
            if (FAILED(device_->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE,
                                                       &readbackDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                       IID_PPV_ARGS(readbackBuffer.ReleaseAndGetAddressOf())))) return false;
        }
        return true;
    }

    bool traceBatch(const std::vector<Ray>& rays, std::vector<HitResult>& outHits, bool closest) {
        std::lock_guard<std::mutex> lock(mutex_);
        outHits.assign(rays.size(), HitResult{});
        if (!available_ || !sceneUploaded_ || rays.empty() || nodeCount_ == 0 || triangleCount_ == 0) return true;

        if (!ensureRayBuffer<GPURay>(rays.size(), rayBuffer_, rayUploadBuffer_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)) return false;
        if (closest) {
            if (!ensureOutputBuffer(rays.size(), sizeof(GPUClosestHit), outputBuffer_, outputReadbackBuffer_)) return false;
        } else {
            if (!ensureOutputBuffer(rays.size(), sizeof(GPUAnyHit), outputBuffer_, outputReadbackBuffer_)) return false;
        }

        std::vector<GPURay> gpuRays(rays.size());
        for (size_t i = 0; i < rays.size(); ++i) {
            const Ray& ray = rays[i];
            gpuRays[i].origin[0] = ray.origin.x;
            gpuRays[i].origin[1] = ray.origin.y;
            gpuRays[i].origin[2] = ray.origin.z;
            gpuRays[i].tMin = ray.tMin;
            gpuRays[i].direction[0] = ray.direction.x;
            gpuRays[i].direction[1] = ray.direction.y;
            gpuRays[i].direction[2] = ray.direction.z;
            gpuRays[i].tMax = ray.tMax;
        }

        commandAllocator_->Reset();
        commandList_->Reset(commandAllocator_.Get(), closest ? closestPso_.Get() : anyPso_.Get());

        if (outputState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
            D3D12_RESOURCE_BARRIER toUav = {};
            toUav.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            toUav.Transition.pResource = outputBuffer_.Get();
            toUav.Transition.StateBefore = outputState_;
            toUav.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            toUav.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            commandList_->ResourceBarrier(1, &toUav);
            outputState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }

        if (!uploadBuffer(device_.Get(), commandList_.Get(), gpuRays.data(), static_cast<UINT64>(gpuRays.size()),
                          rayBuffer_, rayUploadBuffer_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)) {
            return false;
        }

        DispatchConstants constants{};
        constants.rayCount = static_cast<uint32_t>(rays.size());
        constants.nodeCount = nodeCount_;
        constants.triangleCount = triangleCount_;

        commandList_->SetComputeRootSignature(rootSignature_.Get());
        commandList_->SetComputeRoot32BitConstants(0, 4, &constants, 0);
        commandList_->SetComputeRootShaderResourceView(1, nodeBuffer_->GetGPUVirtualAddress());
        commandList_->SetComputeRootShaderResourceView(2, triangleBuffer_->GetGPUVirtualAddress());
        commandList_->SetComputeRootShaderResourceView(3, rayBuffer_->GetGPUVirtualAddress());
        commandList_->SetComputeRootUnorderedAccessView(4, outputBuffer_->GetGPUVirtualAddress());

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.pResource = outputBuffer_.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        commandList_->ResourceBarrier(1, &barrier);
        outputState_ = D3D12_RESOURCE_STATE_COPY_SOURCE;

        commandList_->Dispatch((static_cast<UINT>(rays.size()) + 63u) / 64u, 1, 1);
        commandList_->CopyResource(outputReadbackBuffer_.Get(), outputBuffer_.Get());
        commandList_->Close();
        executeAndWait();

        void* mapped = nullptr;
        if (FAILED(outputReadbackBuffer_->Map(0, nullptr, &mapped))) return false;
        if (closest) {
            const auto* results = static_cast<const GPUClosestHit*>(mapped);
            for (size_t i = 0; i < rays.size(); ++i) {
                outHits[i].hit = results[i].hit != 0;
                outHits[i].distance = results[i].distance;
                outHits[i].materialID = results[i].materialID;
                outHits[i].geometryID = results[i].geometryID;
                outHits[i].hitPoint = Vec3{results[i].hitPoint[0], results[i].hitPoint[1], results[i].hitPoint[2]};
                outHits[i].normal = Vec3{results[i].normal[0], results[i].normal[1], results[i].normal[2]};
            }
        } else {
            const auto* results = static_cast<const GPUAnyHit*>(mapped);
            for (size_t i = 0; i < rays.size(); ++i) {
                outHits[i].hit = results[i].hit != 0;
            }
        }
        outputReadbackBuffer_->Unmap(0, nullptr);
        return true;
    }

    void executeAndWait() {
        ID3D12CommandList* lists[] = { commandList_.Get() };
        queue_->ExecuteCommandLists(1, lists);
        const UINT64 fenceToWaitFor = fenceValue_++;
        queue_->Signal(fence_.Get(), fenceToWaitFor);
        if (fence_->GetCompletedValue() < fenceToWaitFor) {
            fence_->SetEventOnCompletion(fenceToWaitFor, fenceEvent_);
            WaitForSingleObject(fenceEvent_, INFINITE);
        }
    }

    bool available_ = false;
    bool sceneUploaded_ = false;
    bool usingExternalDevice_ = false;
    uint32_t nodeCount_ = 0;
    uint32_t triangleCount_ = 0;
    UINT64 rayCapacity_ = 0;
    UINT64 outputCapacity_ = 0;
    UINT64 fenceValue_ = 1;
    D3D12_RESOURCE_STATES outputState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    std::mutex mutex_;

    D3D12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE type) { return magnaundasoni::heapProps(type); }
    D3D12_RESOURCE_DESC bufferDesc(UINT64 size, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) { return magnaundasoni::bufferDesc(size, flags); }

    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<ID3D12CommandAllocator> commandAllocator_;
    ComPtr<ID3D12GraphicsCommandList> commandList_;
    ComPtr<ID3D12Fence> fence_;
    HANDLE fenceEvent_ = nullptr;

    ComPtr<ID3D12RootSignature> rootSignature_;
    ComPtr<ID3D12PipelineState> closestPso_;
    ComPtr<ID3D12PipelineState> anyPso_;

    ComPtr<ID3D12Resource> nodeBuffer_;
    ComPtr<ID3D12Resource> nodeUploadBuffer_;
    ComPtr<ID3D12Resource> triangleBuffer_;
    ComPtr<ID3D12Resource> triangleUploadBuffer_;
    ComPtr<ID3D12Resource> rayBuffer_;
    ComPtr<ID3D12Resource> rayUploadBuffer_;
    ComPtr<ID3D12Resource> outputBuffer_;
    ComPtr<ID3D12Resource> outputReadbackBuffer_;
};

class NullComputeBackend final : public ComputeBackend {
public:
    bool available() const override { return false; }
    bool attachExternalD3D11Device(void*, void*) override { return false; }
    bool usingExternalD3D11Device() const override { return false; }
    bool attachExternalD3D12Device(void*) override { return false; }
    bool usingExternalD3D12Device() const override { return false; }
    bool syncScene(const BVH&) override { return false; }
    bool traceClosestBatch(const std::vector<Ray>&, std::vector<HitResult>&) override { return false; }
    bool traceAnyBatch(const std::vector<Ray>&, std::vector<uint8_t>&) override { return false; }
};

} // namespace

std::unique_ptr<ComputeBackend> createD3D12ComputeBackend() {
    auto backend = std::make_unique<D3D12ComputeBackend>();
    if (backend->available()) return backend;
    return std::make_unique<NullComputeBackend>();
}

} // namespace magnaundasoni

#else

namespace magnaundasoni {
std::unique_ptr<ComputeBackend> createD3D12ComputeBackend() {
    struct NullComputeBackend final : public ComputeBackend {
        bool available() const override { return false; }
        bool attachExternalD3D11Device(void*, void*) override { return false; }
        bool usingExternalD3D11Device() const override { return false; }
        bool attachExternalD3D12Device(void*) override { return false; }
        bool usingExternalD3D12Device() const override { return false; }
        bool syncScene(const BVH&) override { return false; }
        bool traceClosestBatch(const std::vector<Ray>&, std::vector<HitResult>&) override { return false; }
        bool traceAnyBatch(const std::vector<Ray>&, std::vector<uint8_t>&) override { return false; }
    };
    return std::make_unique<NullComputeBackend>();
}
} // namespace magnaundasoni

#endif
