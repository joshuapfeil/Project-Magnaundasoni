#include "pch.h"
#include "CppUnitTest.h"
#include "../native/include/Magnaundasoni.h"
#include "../native/src/backends/ComputeBackend.h"
#include "../native/src/core/BVH.h"
#include <chrono>
#include <memory>
#include <sstream>
#include <vector>
#include <thread>
#include <string>
#include <cstdlib>
#include <cstring>
#include <Windows.h>
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace magnaundasoni;

namespace MagnaundasoniPerfTests
{
    namespace
    {
        Triangle MakeTriangle(const Vec3& a, const Vec3& b, const Vec3& c, uint32_t geometryId)
        {
            Triangle tri{};
            tri.v0 = a;
            tri.v1 = b;
            tri.v2 = c;
            tri.normal = (b - a).cross(c - a).normalized();
            tri.materialID = 1;
            tri.geometryID = geometryId;
            tri.updateDerivedData();
            return tri;
        }

        std::vector<Triangle> MakeSceneTriangles()
        {
            constexpr int gridSize = 64;
            constexpr float cellSize = 1.0f;

            std::vector<Triangle> triangles;
            triangles.reserve(gridSize * gridSize * 4);

            for (int z = 0; z < gridSize; ++z)
            {
                for (int x = 0; x < gridSize; ++x)
                {
                    const float x0 = static_cast<float>(x) * cellSize;
                    const float x1 = static_cast<float>(x + 1) * cellSize;
                    const float z0 = static_cast<float>(z) * cellSize;
                    const float z1 = static_cast<float>(z + 1) * cellSize;

                    triangles.push_back(MakeTriangle(Vec3{x0, 0.0f, z0}, Vec3{x1, 0.0f, z0}, Vec3{x1, 0.0f, z1}, 1));
                    triangles.push_back(MakeTriangle(Vec3{x0, 0.0f, z0}, Vec3{x1, 0.0f, z1}, Vec3{x0, 0.0f, z1}, 1));
                    triangles.push_back(MakeTriangle(Vec3{x0, 4.0f, z0}, Vec3{x1, 4.0f, z1}, Vec3{x1, 4.0f, z0}, 2));
                    triangles.push_back(MakeTriangle(Vec3{x0, 4.0f, z0}, Vec3{x0, 4.0f, z1}, Vec3{x1, 4.0f, z1}, 2));
                }
            }

            return triangles;
        }

        BVH MakeSceneBvh()
        {
            BVH bvh;
            bvh.build(MakeSceneTriangles());
            return bvh;
        }

        std::vector<Ray> MakeRays()
        {
            constexpr int width = 64;
            constexpr int height = 64;
            std::vector<Ray> rays;
            rays.reserve(width * height);

            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    const float fx = static_cast<float>(x) * 0.9f + 0.5f;
                    const float fy = 1.0f + static_cast<float>(y % 8) * 0.1f;
                    const float fz = static_cast<float>(y) * 0.9f + 0.5f;

                    Ray ray{};
                    ray.origin = Vec3{fx, fy, fz};
                    ray.direction = Vec3{0.0f, -1.0f, 0.0f};
                    ray.tMin = 0.001f;
                    ray.tMax = 100.0f;
                    rays.push_back(ray);
                }
            }

            return rays;
        }
    }

    TEST_CLASS(Dx12TracePerfTests)
    {
    public:
        TEST_METHOD(ClosestTraceBatchBaseline)
        {
#if !defined(_WIN32)
            Logger::WriteMessage(L"DX12 performance test requires Windows.");
            return;
#else
            // Optional: wait for attach when MAG_WAIT_FOR_ATTACH=1 is set in the environment.
            // Create a marker file (in %TEMP%) named mag_attach_<pid>.ready to continue.
            char* waitEnv = nullptr;
            size_t envLen = 0;
            if (_dupenv_s(&waitEnv, &envLen, "MAG_WAIT_FOR_ATTACH") == 0 && waitEnv) {
                if (std::strcmp(waitEnv, "1") == 0) {
                    DWORD pid = GetCurrentProcessId();
                    char tempPath[MAX_PATH] = {0};
                    DWORD len = GetTempPathA(MAX_PATH, tempPath);
                    std::string flagPath = std::string(tempPath ? tempPath : "C:\\temp\\") + "mag_attach_" + std::to_string(pid) + ".ready";
                    std::wstring msg = L"Waiting for attach. Create file: ";
                    // convert flagPath to wstring for logging
                    std::wstring wflag(flagPath.begin(), flagPath.end());
                    msg += wflag;
                    Logger::WriteMessage(msg.c_str());

                    const int kMaxSeconds = 300; // 5 minutes
                    int waitedMs = 0;
                    while (waitedMs < kMaxSeconds * 1000) {
                        DWORD attr = GetFileAttributesA(flagPath.c_str());
                        if (attr != INVALID_FILE_ATTRIBUTES) break;
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        waitedMs += 500;
                    }
                }
                free(waitEnv);
            }
            auto backend = createComputeBackend(MAG_BACKEND_DX12);
            Assert::IsTrue(static_cast<bool>(backend), L"DX12 backend factory returned null.");

            if (!backend || !backend->available())
            {
                Logger::WriteMessage(L"DX12 compute backend is unavailable on this machine.");
                return;
            }

            const BVH bvh = MakeSceneBvh();
            Assert::IsFalse(bvh.empty(), L"Scene BVH should not be empty.");
            Assert::IsTrue(backend->syncScene(bvh), L"Failed to upload BVH to the DX12 backend.");

            const std::vector<Ray> rays = MakeRays();
            std::vector<HitResult> hits;
            constexpr int iterations = 32;

            const auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < iterations; ++i)
            {
                Assert::IsTrue(backend->traceClosestBatch(rays, hits), L"DX12 closest-trace batch failed.");
                Assert::AreEqual(rays.size(), hits.size(), L"Each ray should produce one hit result.");
            }
            const auto end = std::chrono::steady_clock::now();

            const double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
            std::wstringstream message;
            message << L"DX12 closest trace baseline: rays=" << rays.size()
                    << L", iterations=" << iterations
                    << L", totalMs=" << totalMs
                    << L", avgBatchMs=" << (totalMs / iterations);
            Logger::WriteMessage(message.str().c_str());
#endif
        }
    };
}


