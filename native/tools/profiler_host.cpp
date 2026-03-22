/**
 * @file profiler_host.cpp
 * @brief Small executable for profiling the native acoustics DLL.
 */

#include "Magnaundasoni.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Options {
    uint32_t raysPerSource = 256;
    uint32_t frames = 600;
    uint32_t threadCount = 1;
    uint32_t sources = 1;
    float deltaTime = 1.0f / 60.0f;
    MagBackendType backend = MAG_BACKEND_AUTO;
    bool compareBackends = false;
};

const char* backendName(MagBackendType backend) {
    switch (backend) {
        case MAG_BACKEND_AUTO: return "auto";
        case MAG_BACKEND_SOFTWARE_BVH: return "software";
        case MAG_BACKEND_DXR: return "dxr";
        case MAG_BACKEND_VULKAN_RT: return "vulkan";
        case MAG_BACKEND_COMPUTE: return "compute";
    }
    return "unknown";
}

bool parseBackend(const char* text, MagBackendType& backend) {
    if (!text) return false;
    std::string value = text;
    if (value == "auto") backend = MAG_BACKEND_AUTO;
    else if (value == "software" || value == "softwarebvh") backend = MAG_BACKEND_SOFTWARE_BVH;
    else if (value == "compute") backend = MAG_BACKEND_COMPUTE;
    else if (value == "dxr") backend = MAG_BACKEND_DXR;
    else if (value == "vulkan" || value == "vulkanrt") backend = MAG_BACKEND_VULKAN_RT;
    else return false;
    return true;
}

bool parseUInt(const char* text, uint32_t& value) {
    if (!text || *text == '\0') return false;
    char* end = nullptr;
    unsigned long parsed = std::strtoul(text, &end, 10);
    if (!end || *end != '\0') return false;
    value = static_cast<uint32_t>(parsed);
    return true;
}

bool parseFloat(const char* text, float& value) {
    if (!text || *text == '\0') return false;
    char* end = nullptr;
    value = std::strtof(text, &end);
    return end && *end == '\0';
}

Options parseArgs(int argc, char** argv) {
    Options opts;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--rays" || arg == "-r") && i + 1 < argc) {
            parseUInt(argv[++i], opts.raysPerSource);
        } else if ((arg == "--frames" || arg == "-f") && i + 1 < argc) {
            parseUInt(argv[++i], opts.frames);
        } else if ((arg == "--threads" || arg == "-t") && i + 1 < argc) {
            parseUInt(argv[++i], opts.threadCount);
        } else if ((arg == "--sources" || arg == "-s") && i + 1 < argc) {
            parseUInt(argv[++i], opts.sources);
        } else if ((arg == "--dt") && i + 1 < argc) {
            parseFloat(argv[++i], opts.deltaTime);
        } else if ((arg == "--backend" || arg == "-b") && i + 1 < argc) {
            parseBackend(argv[++i], opts.backend);
        } else if (arg == "--compare-backends") {
            opts.compareBackends = true;
        }
    }

    if (opts.frames == 0) opts.frames = 1;
    if (opts.sources == 0) opts.sources = 1;
    return opts;
}

bool ok(MagStatus status, const char* step) {
    if (status == MAG_OK) return true;
    std::cerr << "Step failed: " << step << " (status=" << status << ")\n";
    return false;
}

bool registerRoom(MagEngine engine, MagMaterialID materialID) {
    const std::vector<float> vertices = {
        -10.0f, 0.0f, -10.0f,
         10.0f, 0.0f, -10.0f,
         10.0f, 0.0f,  10.0f,
        -10.0f, 0.0f,  10.0f,
        -10.0f, 8.0f, -10.0f,
         10.0f, 8.0f, -10.0f,
         10.0f, 8.0f,  10.0f,
        -10.0f, 8.0f,  10.0f
    };

    const std::vector<uint32_t> indices = {
        0, 1, 2, 0, 2, 3,
        4, 6, 5, 4, 7, 6,
        0, 4, 5, 0, 5, 1,
        1, 5, 6, 1, 6, 2,
        2, 6, 7, 2, 7, 3,
        3, 7, 4, 3, 4, 0
    };

    MagGeometryDesc room{};
    room.vertices = vertices.data();
    room.vertexCount = static_cast<uint32_t>(vertices.size() / 3);
    room.indices = indices.data();
    room.indexCount = static_cast<uint32_t>(indices.size());
    room.materialID = materialID;
    room.dynamicFlag = 0;

    MagGeometryID geometryID = 0;
    return ok(mag_geometry_register(engine, &room, &geometryID), "mag_geometry_register(room)");
}

int runScenario(const Options& options, MagBackendType backend) {
    MagEngineConfig config{};
    if (!ok(mag_engine_config_defaults(&config), "mag_engine_config_defaults")) {
        return 1;
    }

    config.preferredBackend = backend;
    config.raysPerSource = options.raysPerSource;
    config.threadCount = options.threadCount;
    config.maxSources = options.sources;

    MagEngine engine = nullptr;
    if (!ok(mag_engine_create(&config, &engine), "mag_engine_create") || !engine) {
        return 1;
    }

    MagBackendDiagnostics diagnostics{};
    if (!ok(mag_get_backend_diagnostics(engine, &diagnostics), "mag_get_backend_diagnostics")) {
        mag_engine_destroy(engine);
        return 1;
    }

    MagMaterialDesc material{};
    if (!ok(mag_material_get_preset("concrete", &material), "mag_material_get_preset")) {
        mag_engine_destroy(engine);
        return 1;
    }

    MagMaterialID materialID = 0;
    if (!ok(mag_material_register(engine, &material, &materialID), "mag_material_register")) {
        mag_engine_destroy(engine);
        return 1;
    }

    if (!registerRoom(engine, materialID)) {
        mag_engine_destroy(engine);
        return 1;
    }

    MagListenerDesc listener{};
    listener.position[0] = 0.0f;
    listener.position[1] = 1.7f;
    listener.position[2] = 0.0f;
    listener.forward[2] = 1.0f;
    listener.up[1] = 1.0f;

    MagListenerID listenerID = 0;
    if (!ok(mag_listener_register(engine, &listener, &listenerID), "mag_listener_register")) {
        mag_engine_destroy(engine);
        return 1;
    }

    std::vector<MagSourceID> sourceIDs;
    sourceIDs.reserve(options.sources);
    for (uint32_t i = 0; i < options.sources; ++i) {
        MagSourceDesc source{};
        const float angle = (2.0f * 3.14159265f * static_cast<float>(i)) / static_cast<float>(options.sources);
        source.position[0] = std::cos(angle) * 3.0f;
        source.position[1] = 1.5f;
        source.position[2] = std::sin(angle) * 3.0f;
        source.direction[2] = 1.0f;
        source.importance = 1;

        MagSourceID sourceID = 0;
        if (!ok(mag_source_register(engine, &source, &sourceID), "mag_source_register")) {
            mag_engine_destroy(engine);
            return 1;
        }
        sourceIDs.push_back(sourceID);
    }

    std::cout << "magnaundasoni_profiler\n";
    std::cout << "  requestedBackend=" << backendName(backend)
              << " activeBackend=" << backendName(diagnostics.activeBackend)
              << " computeAvailable=" << diagnostics.computeAvailable
              << " computeEnabled=" << diagnostics.computeEnabled
              << " externalD3D11=" << diagnostics.usingExternalD3D11Device
              << " raysPerSource=" << options.raysPerSource
              << " frames=" << options.frames
              << " threadCount=" << options.threadCount
              << " sources=" << options.sources
              << " deltaTime=" << options.deltaTime << "\n";

    auto wallStart = std::chrono::high_resolution_clock::now();
    double totalCpuMs = 0.0;
    float maxCpuMs = 0.0f;
    uint64_t totalRays = 0;

    for (uint32_t frame = 0; frame < options.frames; ++frame) {
        const float t = static_cast<float>(frame) * options.deltaTime;
        for (uint32_t i = 0; i < sourceIDs.size(); ++i) {
            MagSourceDesc source{};
            const float angle = t * (0.4f + 0.1f * static_cast<float>(i));
            source.position[0] = std::cos(angle + static_cast<float>(i)) * (2.5f + 0.2f * static_cast<float>(i));
            source.position[1] = 1.5f + 0.25f * std::sin(t * 0.5f + static_cast<float>(i));
            source.position[2] = std::sin(angle + static_cast<float>(i)) * (2.5f + 0.2f * static_cast<float>(i));
            source.direction[2] = 1.0f;
            source.importance = 1;

            if (!ok(mag_source_update(engine, sourceIDs[i], &source), "mag_source_update")) {
                mag_engine_destroy(engine);
                return 1;
            }
        }

        if (!ok(mag_update(engine, options.deltaTime), "mag_update")) {
            mag_engine_destroy(engine);
            return 1;
        }

        MagGlobalState state{};
        if (!ok(mag_get_global_state(engine, &state), "mag_get_global_state")) {
            mag_engine_destroy(engine);
            return 1;
        }

        uint32_t rays = 0;
        if (!ok(mag_debug_get_ray_count(engine, &rays), "mag_debug_get_ray_count")) {
            mag_engine_destroy(engine);
            return 1;
        }

        totalCpuMs += state.cpuTimeMs;
        if (state.cpuTimeMs > maxCpuMs) maxCpuMs = state.cpuTimeMs;
        totalRays += rays;

        if ((frame + 1) % 120 == 0 || frame + 1 == options.frames) {
            std::cout << "frame=" << (frame + 1)
                      << " cpuMs=" << std::fixed << std::setprecision(3) << state.cpuTimeMs
                      << " rays=" << rays << "\n";
        }
    }

    auto wallEnd = std::chrono::high_resolution_clock::now();
    const double wallMs = std::chrono::duration<double, std::milli>(wallEnd - wallStart).count();
    const double avgCpuMs = totalCpuMs / static_cast<double>(options.frames);
    const double avgRays = static_cast<double>(totalRays) / static_cast<double>(options.frames);
    const double updatesPerSecond = wallMs > 0.0 ? (static_cast<double>(options.frames) * 1000.0) / wallMs : 0.0;

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "summary backend=" << backendName(diagnostics.activeBackend)
              << " avgCpuMs=" << avgCpuMs
              << " maxCpuMs=" << maxCpuMs
              << " avgRays=" << avgRays
              << " updatesPerSecond=" << updatesPerSecond << "\n";

    mag_engine_destroy(engine);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    const Options options = parseArgs(argc, argv);

    if (options.compareBackends) {
        if (runScenario(options, MAG_BACKEND_SOFTWARE_BVH) != 0) return 1;
        std::cout << "---\n";
        return runScenario(options, MAG_BACKEND_COMPUTE);
    }

    return runScenario(options, options.backend);
}
