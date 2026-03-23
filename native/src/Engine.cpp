/**
 * @file Engine.cpp
 * @brief C ABI entry-point that bridges the public API to the internal C++ engine.
 */

#include "Magnaundasoni.h"

#include "core/BVH.h"
#include "backends/ComputeBackend.h"
#include "core/ChunkManager.h"
#include "core/EdgeExtractor.h"
#include "core/MaterialPresets.h"
#include "core/Scene.h"
#include "core/SpatialGrid.h"
#include "core/ThreadPool.h"
#include "core/Types.h"
#include "render/AcousticRenderer.h"
#include "render/OutputMixer.h"
#include "spatial/HRTFDatabase.h"
#include "spatial/Quaternion.h"
#include "spatial/SpatialConfig.h"

#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <deque>
#include <unordered_map>
#include <vector>

using namespace magnaundasoni;

namespace {
constexpr int kUnityGfxDeviceEventInitialize = 0;
constexpr int kUnityGfxDeviceEventShutdown = 1;
constexpr int kUnityGfxDeviceEventBeforeReset = 2;
constexpr int kUnityGfxDeviceEventAfterReset = 3;
constexpr int kUnityGfxRendererD3D11 = 2;
constexpr int kUnityGfxRendererD3D12 = 18;

std::mutex g_unityGraphicsMutex;
void* g_unityGraphicsDevice = nullptr;
int g_unityGraphicsRenderer = -1;
} // namespace

/* ------------------------------------------------------------------ */
/* Internal engine state                                              */
/* ------------------------------------------------------------------ */
struct MagEngine_T {
    MagEngineConfig  config{};
    MagQualityLevel  activeQuality = MAG_QUALITY_MEDIUM;
    MagBackendType   backendUsed   = MAG_BACKEND_SOFTWARE_BVH;
    MagBackendType   requestedBackend = MAG_BACKEND_AUTO;
    MagBackendType   computeBackendFlavor = MAG_BACKEND_COMPUTE;

    Scene            scene;
    BVH              bvh;
    SpatialGrid      spatialGrid{8.0f};
    std::unique_ptr<ChunkManager> chunkManager;
    EdgeExtractor    edgeExtractor;
    std::unique_ptr<ThreadPool>   threadPool;
    std::unique_ptr<ComputeBackend> computeBackend;
    AcousticRenderer acousticRenderer;

    // Per-frame stats
    std::atomic<uint32_t> lastRayCount{0};
    std::atomic<uint32_t> lastEdgeCount{0};
    uint64_t              lastBuiltGeometryRevision = 0;
    bool                  lastSceneSyncSucceeded = false;
    double                timestamp = 0.0;
    float                 lastCpuTimeMs = 0.0f;

    void*                 externalD3D11Device = nullptr;
    void*                 externalD3D11DeviceContext = nullptr;
    void*                 externalD3D12Device = nullptr;

    std::mutex spatialMutex;
    MagSpatialConfig spatialConfig = defaultSpatialConfig();
    MagSpeakerLayout speakerLayout = defaultSpeakerLayout(MAG_SPEAKERS_STEREO);
    HRTFDatabase hrtfDatabase;
    std::unordered_map<uint32_t, std::array<float, 4>> listenerHeadPoses;

    std::mutex audioMutex;
    OutputMixer outputMixer;
    OutputMixer::Config outputMixerConfig{};
    std::unordered_map<uint32_t, std::deque<float>> pendingSourceAudio;
};

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */
static Vec3 toVec3(const float* p) { return {p[0], p[1], p[2]}; }

/** Build the global BVH from all registered geometry. */
static void rebuildBVH(MagEngine_T* e) {
    std::vector<Triangle> allTris;

    auto geoIDs = e->scene.getAllGeometryIDs();
    for (uint32_t gid : geoIDs) {
        const GeometryEntry* geo = e->scene.getGeometry(gid);
        if (!geo || geo->vertices.empty() || geo->indices.empty()) continue;

        for (uint32_t i = 0; i + 2 < static_cast<uint32_t>(geo->indices.size()); i += 3) {
            uint32_t i0 = geo->indices[i + 0];
            uint32_t i1 = geo->indices[i + 1];
            uint32_t i2 = geo->indices[i + 2];
            if (i0 >= geo->vertices.size() / 3 ||
                i1 >= geo->vertices.size() / 3 ||
                i2 >= geo->vertices.size() / 3) continue;

            Triangle tri;
            tri.v0 = geo->transform.transformPoint(
                {geo->vertices[i0 * 3], geo->vertices[i0 * 3 + 1], geo->vertices[i0 * 3 + 2]});
            tri.v1 = geo->transform.transformPoint(
                {geo->vertices[i1 * 3], geo->vertices[i1 * 3 + 1], geo->vertices[i1 * 3 + 2]});
            tri.v2 = geo->transform.transformPoint(
                {geo->vertices[i2 * 3], geo->vertices[i2 * 3 + 1], geo->vertices[i2 * 3 + 2]});
            tri.normal = (tri.v1 - tri.v0).cross(tri.v2 - tri.v0).normalized();
            tri.materialID = geo->materialID;
            tri.geometryID = gid;
            allTris.push_back(tri);
        }
    }

    e->bvh.build(allTris);
    e->lastBuiltGeometryRevision = e->scene.geometryRevision();
    if (e->computeBackend && e->computeBackend->available()) {
        e->lastSceneSyncSucceeded = e->computeBackend->syncScene(e->bvh);
    } else {
        e->lastSceneSyncSucceeded = false;
    }
}

static QualityLevel toRendererQuality(MagQualityLevel level) {
    switch (level) {
        case MAG_QUALITY_LOW:    return QualityLevel::Low;
        case MAG_QUALITY_MEDIUM: return QualityLevel::Medium;
        case MAG_QUALITY_HIGH:   return QualityLevel::High;
        case MAG_QUALITY_ULTRA:  return QualityLevel::Ultra;
    }

    return QualityLevel::High;
}

static AcousticRenderer::Config makeRendererConfig(const MagEngine_T* engine) {
    AcousticRenderer::Config config;
    config.quality = toRendererQuality(engine->activeQuality);
    config.maxReflectionOrder = std::max(1u, engine->config.maxReflectionOrder);
    config.maxDiffractionDepth = std::max(1u, engine->config.maxDiffractionDepth);
    config.raysPerSource = (engine->config.raysPerSource > 0) ? engine->config.raysPerSource : 64;
    config.effectiveBandCount = engine->config.effectiveBandCount;
    return config;
}

static OutputMixer::SpatializationMode toMixerSpatialMode(MagSpatialMode mode) {
    switch (mode) {
        case MAG_SPATIAL_BINAURAL:
        case MAG_SPATIAL_WINDOWS_SONIC:
        case MAG_SPATIAL_DOLBY_ATMOS:
        case MAG_SPATIAL_STEAM_AUDIO:
        case MAG_SPATIAL_META_XR:
        case MAG_SPATIAL_OPENXR:
        case MAG_SPATIAL_CORE_AUDIO:
            return OutputMixer::SpatializationMode::Binaural;
        case MAG_SPATIAL_SURROUND_STEREO:
        case MAG_SPATIAL_SURROUND_QUAD:
        case MAG_SPATIAL_SURROUND_51:
        case MAG_SPATIAL_SURROUND_71:
        case MAG_SPATIAL_SURROUND_714:
            return OutputMixer::SpatializationMode::Surround;
        case MAG_SPATIAL_AUTO:
        case MAG_SPATIAL_PASSTHROUGH:
        default:
            return OutputMixer::SpatializationMode::Passthrough;
    }
}

static void configureOutputMixer(MagEngine_T* engine,
                                 uint32_t sampleRate,
                                 uint32_t channelCount) {
    OutputMixer::Config config = engine->outputMixerConfig;
    config.sampleRate = std::max(1u, sampleRate);
    config.channels = std::max(1u, channelCount);
    config.maxBlockSize = std::max(config.maxBlockSize, 4096u);

    {
        std::lock_guard lock(engine->spatialMutex);
        config.spatializationMode = toMixerSpatialMode(engine->spatialConfig.mode);
        config.maxBinauralSources = std::max(1u, engine->spatialConfig.maxBinauralSources);
        config.speakerLayout = engine->speakerLayout;
        if (config.speakerLayout.channelCount == 0) {
            config.speakerLayout = defaultSpeakerLayout(MAG_SPEAKERS_STEREO);
        }
    }

    if (config.sampleRate != engine->outputMixerConfig.sampleRate
        || config.channels != engine->outputMixerConfig.channels
        || config.spatializationMode != engine->outputMixerConfig.spatializationMode
        || config.maxBinauralSources != engine->outputMixerConfig.maxBinauralSources
        || config.speakerLayout.channelCount != engine->outputMixerConfig.speakerLayout.channelCount) {
        engine->outputMixer.configure(config);
        engine->outputMixerConfig = config;
    }
}

static bool ensureComputeBackend(MagEngine_T* engine, MagBackendType backendFlavor) {
    if (!engine->computeBackend || engine->computeBackendFlavor != backendFlavor) {
        engine->computeBackend = createComputeBackend(backendFlavor);
        engine->computeBackendFlavor = backendFlavor;
    }

    if (!engine->computeBackend || !engine->computeBackend->available()) return false;

    if (backendFlavor == MAG_BACKEND_DX12) {
        if (engine->externalD3D12Device) {
            if (!engine->computeBackend->attachExternalD3D12Device(engine->externalD3D12Device)) {
                return false;
            }
        }
    } else {
        if (engine->externalD3D11Device || engine->externalD3D11DeviceContext) {
            if (!engine->computeBackend->attachExternalD3D11Device(
                    engine->externalD3D11Device,
                    engine->externalD3D11DeviceContext)) {
                return false;
            }
        }
    }

    engine->acousticRenderer.setComputeBackend(engine->computeBackend.get());
    return true;
}

static void selectBackend(MagEngine_T* engine) {
    engine->requestedBackend = engine->config.preferredBackend;
    engine->backendUsed = MAG_BACKEND_SOFTWARE_BVH;
    engine->acousticRenderer.setComputeBackend(nullptr);

    const bool unityWantsDx12 = (g_unityGraphicsRenderer == kUnityGfxRendererD3D12);
    const bool hasDx12Device = engine->externalD3D12Device != nullptr;
    MagBackendType desiredCompute = (unityWantsDx12 || hasDx12Device || engine->computeBackendFlavor == MAG_BACKEND_DX12)
        ? MAG_BACKEND_DX12
        : MAG_BACKEND_COMPUTE;

    auto tryCompute = [engine, desiredCompute]() {
        return ensureComputeBackend(engine, desiredCompute);
    };

    switch (engine->requestedBackend) {
        case MAG_BACKEND_COMPUTE:
        case MAG_BACKEND_DX12:
            if (tryCompute()) engine->backendUsed = desiredCompute;
            break;
        case MAG_BACKEND_DXR:
        case MAG_BACKEND_VULKAN_RT:
            if (tryCompute()) engine->backendUsed = desiredCompute;
            break;
        case MAG_BACKEND_AUTO:
            if (tryCompute()) engine->backendUsed = desiredCompute;
            break;
        case MAG_BACKEND_SOFTWARE_BVH:
        default:
            break;
    }

    if ((engine->backendUsed == MAG_BACKEND_COMPUTE || engine->backendUsed == MAG_BACKEND_DX12) && !engine->bvh.empty()) {
        engine->lastSceneSyncSucceeded = engine->computeBackend->syncScene(engine->bvh);
    }
}

/* ------------------------------------------------------------------ */
/* Default configuration                                              */
/* ------------------------------------------------------------------ */
MagStatus mag_engine_config_defaults(MagEngineConfig* outConfig) {
    if (!outConfig) return MAG_INVALID_PARAM;

    outConfig->quality             = MAG_QUALITY_MEDIUM;
    outConfig->preferredBackend    = MAG_BACKEND_AUTO;
    outConfig->maxSources          = 64;
    outConfig->maxReflectionOrder  = 2;
    outConfig->maxDiffractionDepth = 2;
    outConfig->raysPerSource       = 64;
    outConfig->threadCount         = 0;  // auto-detect
    outConfig->worldChunkSize      = 50.0f;
    outConfig->effectiveBandCount  = 8;

    return MAG_OK;
}

/* ------------------------------------------------------------------ */
/* Engine lifecycle                                                    */
/* ------------------------------------------------------------------ */
MagStatus mag_engine_create(const MagEngineConfig* config, MagEngine* outEngine) {
    if (!config || !outEngine) return MAG_INVALID_PARAM;

    auto* engine = new (std::nothrow) MagEngine_T;
    if (!engine) return MAG_OUT_OF_MEMORY;

    engine->config        = *config;
    engine->activeQuality = config->quality;
    engine->requestedBackend = config->preferredBackend;

    // Configure chunk manager
    float chunkSize = (config->worldChunkSize > 0.0f) ? config->worldChunkSize : 32.0f;
    engine->chunkManager = std::make_unique<ChunkManager>(chunkSize);
    engine->spatialGrid.setCellSize(chunkSize * 0.25f);

    // Thread pool (0 = auto)
    engine->threadPool = std::make_unique<ThreadPool>(config->threadCount);
    engine->acousticRenderer.setThreadPool(engine->threadPool.get());
    engine->acousticRenderer.configure(makeRendererConfig(engine));
    selectBackend(engine);

    *outEngine = engine;
    return MAG_OK;
}

MagStatus mag_get_backend_diagnostics(MagEngine engine,
                                      MagBackendDiagnostics* outDiagnostics) {
    if (!engine || !outDiagnostics) return MAG_INVALID_PARAM;

    outDiagnostics->requestedBackend = engine->requestedBackend;
    outDiagnostics->activeBackend = engine->backendUsed;
    outDiagnostics->computeAvailable =
        (engine->computeBackend && engine->computeBackend->available()) ? 1u : 0u;
    outDiagnostics->computeEnabled =
        (engine->backendUsed == MAG_BACKEND_COMPUTE || engine->backendUsed == MAG_BACKEND_DX12) ? 1u : 0u;
    outDiagnostics->usingExternalD3D11Device =
        (engine->computeBackend && engine->computeBackend->usingExternalD3D11Device()) ? 1u : 0u;
    outDiagnostics->usingExternalD3D12Device =
        (engine->computeBackend && engine->computeBackend->usingExternalD3D12Device()) ? 1u : 0u;
    outDiagnostics->lastSceneSyncSucceeded = engine->lastSceneSyncSucceeded ? 1u : 0u;
    return MAG_OK;
}

MagStatus mag_engine_destroy(MagEngine engine) {
    if (!engine) return MAG_INVALID_PARAM;

    delete engine;
    return MAG_OK;
}

/* ------------------------------------------------------------------ */
/* Material management                                                */
/* ------------------------------------------------------------------ */
MagStatus mag_material_register(MagEngine engine,
                                const MagMaterialDesc* desc,
                                MagMaterialID* outID) {
    if (!engine || !desc || !outID) return MAG_INVALID_PARAM;

    MaterialEntry mat;
    std::memcpy(mat.absorption.data(),  desc->absorption,  sizeof(desc->absorption));
    std::memcpy(mat.transmission.data(), desc->transmission, sizeof(desc->transmission));
    std::memcpy(mat.scattering.data(),  desc->scattering,  sizeof(desc->scattering));
    mat.roughness      = desc->roughness;
    mat.thicknessClass = desc->thicknessClass;
    mat.leakageHint    = desc->leakageHint;
    if (desc->categoryTag) mat.categoryTag = desc->categoryTag;

    *outID = engine->scene.registerMaterial(mat);
    return MAG_OK;
}

MagStatus mag_material_get_preset(const char* presetName, MagMaterialDesc* outDesc) {
    if (!presetName || !outDesc) return MAG_INVALID_PARAM;
    return getMaterialPreset(presetName, *outDesc) ? MAG_OK : MAG_ERROR;
}

/* ------------------------------------------------------------------ */
/* Geometry management                                                */
/* ------------------------------------------------------------------ */
MagStatus mag_geometry_register(MagEngine engine,
                                const MagGeometryDesc* desc,
                                MagGeometryID* outID) {
    if (!engine || !desc || !outID) return MAG_INVALID_PARAM;
    if (!desc->vertices || desc->vertexCount == 0) return MAG_INVALID_PARAM;
    if (!desc->indices  || desc->indexCount  == 0) return MAG_INVALID_PARAM;

    GeometryEntry geo;
    geo.vertices.assign(desc->vertices, desc->vertices + desc->vertexCount * 3);
    geo.indices.assign(desc->indices, desc->indices + desc->indexCount);
    geo.materialID = desc->materialID;
    geo.importance = static_cast<Importance>(desc->dynamicFlag);

    *outID = engine->scene.registerGeometry(geo);

    // Insert into spatial grid
    const GeometryEntry* entry = engine->scene.getGeometry(*outID);
    if (entry) {
        engine->spatialGrid.insert(*outID, entry->bounds);
    }

    return MAG_OK;
}

MagStatus mag_geometry_unregister(MagEngine engine, MagGeometryID id) {
    if (!engine) return MAG_INVALID_PARAM;
    engine->spatialGrid.remove(id);
    return engine->scene.unregisterGeometry(id) ? MAG_OK : MAG_ERROR;
}

MagStatus mag_geometry_update_transform(MagEngine engine,
                                        MagGeometryID id,
                                        const float* transform4x4) {
    if (!engine || !transform4x4) return MAG_INVALID_PARAM;

    Mat4x4 xform = Mat4x4::fromColumnMajor(transform4x4);
    if (!engine->scene.updateTransform(id, xform)) return MAG_ERROR;

    const GeometryEntry* entry = engine->scene.getGeometry(id);
    if (entry) {
        engine->spatialGrid.update(id, entry->bounds);
    }
    return MAG_OK;
}

/* ------------------------------------------------------------------ */
/* Source management                                                  */
/* ------------------------------------------------------------------ */
MagStatus mag_source_register(MagEngine engine,
                              const MagSourceDesc* desc,
                              MagSourceID* outID) {
    if (!engine || !desc || !outID) return MAG_INVALID_PARAM;

    SourceEntry src;
    src.position  = toVec3(desc->position);
    src.direction = toVec3(desc->direction);
    src.radius    = desc->radius;
    src.importance = desc->importance;

    *outID = engine->scene.registerSource(src);
    return MAG_OK;
}

MagStatus mag_source_unregister(MagEngine engine, MagSourceID id) {
    if (!engine) return MAG_INVALID_PARAM;
    {
        std::lock_guard lock(engine->audioMutex);
        engine->pendingSourceAudio.erase(id);
    }
    return engine->scene.unregisterSource(id) ? MAG_OK : MAG_ERROR;
}

MagStatus mag_source_update(MagEngine engine, MagSourceID id,
                            const MagSourceDesc* desc) {
    if (!engine || !desc) return MAG_INVALID_PARAM;

    SourceEntry src;
    src.position   = toVec3(desc->position);
    src.direction  = toVec3(desc->direction);
    src.radius     = desc->radius;
    src.importance = desc->importance;

    return engine->scene.updateSource(id, src) ? MAG_OK : MAG_ERROR;
}

/* ------------------------------------------------------------------ */
/* Listener management                                                */
/* ------------------------------------------------------------------ */
MagStatus mag_listener_register(MagEngine engine,
                                const MagListenerDesc* desc,
                                MagListenerID* outID) {
    if (!engine || !desc || !outID) return MAG_INVALID_PARAM;

    ListenerEntry lis;
    lis.position = toVec3(desc->position);
    lis.forward  = toVec3(desc->forward);
    lis.up       = toVec3(desc->up);

    *outID = engine->scene.registerListener(lis);
    return MAG_OK;
}

MagStatus mag_listener_unregister(MagEngine engine, MagListenerID id) {
    if (!engine) return MAG_INVALID_PARAM;
    return engine->scene.unregisterListener(id) ? MAG_OK : MAG_ERROR;
}

MagStatus mag_listener_update(MagEngine engine, MagListenerID id,
                               const MagListenerDesc* desc) {
    if (!engine || !desc) return MAG_INVALID_PARAM;

    ListenerEntry lis;
    lis.position = toVec3(desc->position);
    lis.forward  = toVec3(desc->forward);
    lis.up       = toVec3(desc->up);

    return engine->scene.updateListener(id, lis) ? MAG_OK : MAG_ERROR;
}

/* ------------------------------------------------------------------ */
/* Spatialisation                                                     */
/* ------------------------------------------------------------------ */
MagStatus mag_set_spatial_config(MagEngine engine,
                                 const MagSpatialConfig* config) {
    if (!engine || !config) return MAG_INVALID_PARAM;
    if (!isValidSpatialMode(config->mode) ||
        !isValidHRTFPreset(config->hrtfPreset) ||
        !isValidSpeakerLayoutPreset(config->speakerLayout)) {
        return MAG_INVALID_PARAM;
    }

    std::lock_guard lock(engine->spatialMutex);
    MagSpeakerLayoutPreset layoutPreset = speakerLayoutForMode(config->mode,
                                                               config->speakerLayout);
    MagSpatialConfig sanitizedConfig = *config;
    sanitizedConfig.speakerLayout = layoutPreset;
    if (sanitizedConfig.maxBinauralSources == 0) {
        sanitizedConfig.maxBinauralSources = 16;
    }
    engine->spatialConfig = sanitizedConfig;
    if (layoutPreset != MAG_SPEAKERS_CUSTOM) {
        engine->speakerLayout = defaultSpeakerLayout(layoutPreset);
    } else if (engine->speakerLayout.channelCount == 0) {
        engine->speakerLayout = defaultSpeakerLayout(MAG_SPEAKERS_STEREO);
    }
    configureOutputMixer(engine,
                         engine->outputMixerConfig.sampleRate,
                         engine->outputMixerConfig.channels);
    return MAG_OK;
}

MagStatus mag_get_spatial_config(MagEngine engine,
                                 MagSpatialConfig* outConfig) {
    if (!engine || !outConfig) return MAG_INVALID_PARAM;
    std::lock_guard lock(engine->spatialMutex);
    *outConfig = engine->spatialConfig;
    return MAG_OK;
}

MagStatus mag_set_hrtf_dataset(MagEngine engine,
                               const void* sofaData,
                               size_t sofaSize) {
    if (!engine || !sofaData || sofaSize == 0) return MAG_INVALID_PARAM;
    std::lock_guard lock(engine->spatialMutex);
    return engine->hrtfDatabase.setCustomDataset(sofaData, sofaSize)
               ? MAG_OK
               : MAG_ERROR;
}

MagStatus mag_set_hrtf_preset(MagEngine engine, MagHRTFPreset preset) {
    if (!engine || !isValidHRTFPreset(preset)) return MAG_INVALID_PARAM;
    std::lock_guard lock(engine->spatialMutex);
    engine->spatialConfig.hrtfPreset = preset;
    engine->hrtfDatabase.setPreset(preset);
    return MAG_OK;
}

MagStatus mag_set_listener_head_pose(MagEngine engine,
                                     uint32_t listenerID,
                                     const float quaternion[4]) {
    if (!engine || !quaternion) return MAG_INVALID_PARAM;

    float normalised[4];
    if (!normalizeQuaternion(quaternion, normalised)) return MAG_INVALID_PARAM;

    if (!engine->scene.getListener(listenerID)) return MAG_ERROR;

    std::lock_guard lock(engine->spatialMutex);
    // Head pose is tracked separately from the listener basis configured via
    // mag_listener_register/mag_listener_update so head-tracking stays a
    // relative spatialization input instead of clobbering world orientation.
    engine->listenerHeadPoses[listenerID] = {
        normalised[0], normalised[1], normalised[2], normalised[3]
    };
    return MAG_OK;
}

MagStatus mag_set_speaker_layout(MagEngine engine,
                                 const MagSpeakerLayout* layout) {
    if (!engine || !isValidSpeakerLayout(layout)) return MAG_INVALID_PARAM;
    std::lock_guard lock(engine->spatialMutex);
    engine->speakerLayout = *layout;
    engine->spatialConfig.speakerLayout = layout->preset;
    configureOutputMixer(engine,
                         engine->outputMixerConfig.sampleRate,
                         engine->outputMixerConfig.channels);
    return MAG_OK;
}

MagStatus mag_get_spatial_backend_info(MagEngine engine,
                                       MagSpatialBackendInfo* outInfo) {
    if (!engine || !outInfo) return MAG_INVALID_PARAM;
    std::lock_guard lock(engine->spatialMutex);
    *outInfo = resolveSpatialBackend(engine->spatialConfig,
                                     engine->speakerLayout,
                                     engine->hrtfDatabase.hasCustomDataset());
    return MAG_OK;
}

MagStatus mag_submit_source_audio(MagEngine engine,
                                  MagSourceID sourceID,
                                  const float* interleavedSamples,
                                  uint32_t frameCount,
                                  uint32_t channelCount) {
    if (!engine || !interleavedSamples || frameCount == 0 || channelCount == 0) {
        return MAG_INVALID_PARAM;
    }

    if (!engine->scene.getSource(sourceID)) {
        return MAG_ERROR;
    }

    std::lock_guard lock(engine->audioMutex);
    auto& pending = engine->pendingSourceAudio[sourceID];
    pending.resize(pending.size() + frameCount);

    const size_t writeOffset = pending.size() - frameCount;
    for (uint32_t frameIndex = 0; frameIndex < frameCount; ++frameIndex) {
        float sum = 0.0f;
        for (uint32_t channelIndex = 0; channelIndex < channelCount; ++channelIndex) {
            sum += interleavedSamples[frameIndex * channelCount + channelIndex];
        }
        pending[writeOffset + frameIndex] = sum / static_cast<float>(channelCount);
    }

    return MAG_OK;
}

MagStatus mag_render_audio(MagEngine engine,
                           MagListenerID listenerID,
                           float* outputBuffer,
                           uint32_t frameCount,
                           uint32_t channelCount,
                           uint32_t sampleRate) {
    if (!engine || !outputBuffer || frameCount == 0 || channelCount == 0 || sampleRate == 0) {
        return MAG_INVALID_PARAM;
    }

    if (!engine->scene.getListener(listenerID)) {
        return MAG_ERROR;
    }

    configureOutputMixer(engine, sampleRate, channelCount);

    {
        std::lock_guard lock(engine->spatialMutex);
        auto poseIt = engine->listenerHeadPoses.find(listenerID);
        if (poseIt != engine->listenerHeadPoses.end()) {
            engine->outputMixer.setListenerHeadPose(poseIt->second.data());
        }
    }

    const auto sourceIDs = engine->scene.getActiveSourceIDs();
    for (MagSourceID sourceID : sourceIDs) {
        MagAcousticResult result{};
        if (engine->acousticRenderer.copyResult(sourceID, listenerID, result)) {
            std::vector<float> sourceFrames(frameCount, 0.0f);
            {
                std::lock_guard lock(engine->audioMutex);
                auto pendingIt = engine->pendingSourceAudio.find(sourceID);
                if (pendingIt != engine->pendingSourceAudio.end()) {
                    auto& pending = pendingIt->second;
                    const uint32_t framesToCopy = std::min<uint32_t>(frameCount, static_cast<uint32_t>(pending.size()));
                    for (uint32_t frameIndex = 0; frameIndex < framesToCopy; ++frameIndex) {
                        sourceFrames[frameIndex] = pending.front();
                        pending.pop_front();
                    }
                }
            }

            engine->outputMixer.stageResult(sourceID, listenerID, result,
                                            sourceFrames.data(), frameCount);
        }
    }

    engine->outputMixer.commitStaged();
    engine->outputMixer.mix(outputBuffer, frameCount);
    return MAG_OK;
}

/* ------------------------------------------------------------------ */
/* Simulation tick                                                    */
/* ------------------------------------------------------------------ */
MagStatus mag_update(MagEngine engine, float deltaTime) {
    if (!engine) return MAG_INVALID_PARAM;

    auto t0 = std::chrono::high_resolution_clock::now();

    if (engine->lastBuiltGeometryRevision != engine->scene.geometryRevision()) {
        rebuildBVH(engine);
    }

    // Update chunk fidelity around the first active listener
    auto listenerIDs = engine->scene.getActiveListenerIDs();
    if (!listenerIDs.empty()) {
        const ListenerEntry* lis = engine->scene.getListener(listenerIDs[0]);
        if (lis) {
            engine->chunkManager->updateFidelityZones(lis->position);
        }
    }

    engine->acousticRenderer.update(engine->scene, engine->bvh, engine->edgeExtractor, deltaTime);

    engine->lastRayCount.store(engine->acousticRenderer.getActiveRayCount(), std::memory_order_relaxed);
    engine->lastEdgeCount.store(engine->acousticRenderer.getActiveEdgeCount(), std::memory_order_relaxed);
    engine->timestamp += static_cast<double>(deltaTime);

    auto t1 = std::chrono::high_resolution_clock::now();
    engine->lastCpuTimeMs = std::chrono::duration<float, std::milli>(t1 - t0).count();

    return MAG_OK;
}

/* ------------------------------------------------------------------ */
/* Query results                                                      */
/* ------------------------------------------------------------------ */
MagStatus mag_get_acoustic_result(MagEngine engine,
                                  MagSourceID sourceID,
                                  MagListenerID listenerID,
                                  MagAcousticResult* outResult) {
    if (!engine || !outResult) return MAG_INVALID_PARAM;

    return engine->acousticRenderer.copyResult(sourceID, listenerID, *outResult)
        ? MAG_OK
        : MAG_ERROR;
}

MagStatus mag_get_global_state(MagEngine engine, MagGlobalState* outState) {
    if (!engine || !outState) return MAG_INVALID_PARAM;

    outState->activeQuality   = engine->activeQuality;
    outState->backendUsed     = engine->backendUsed;
    outState->timestamp       = engine->timestamp;
    outState->activeSourceCount = engine->scene.getActiveSourceCount();
    outState->cpuTimeMs       = engine->lastCpuTimeMs;

    return MAG_OK;
}

/* ------------------------------------------------------------------ */
/* Quality control                                                    */
/* ------------------------------------------------------------------ */
MagStatus mag_set_quality(MagEngine engine, MagQualityLevel level) {
    if (!engine) return MAG_INVALID_PARAM;
    engine->activeQuality = level;

    // Adjust rays per source based on quality
    switch (level) {
        case MAG_QUALITY_LOW:    engine->config.raysPerSource = 32;  break;
        case MAG_QUALITY_MEDIUM: engine->config.raysPerSource = 64;  break;
        case MAG_QUALITY_HIGH:   engine->config.raysPerSource = 128; break;
        case MAG_QUALITY_ULTRA:  engine->config.raysPerSource = 256; break;
    }

    engine->acousticRenderer.configure(makeRendererConfig(engine));
    selectBackend(engine);

    return MAG_OK;
}

MagStatus mag_set_d3d12_device(MagEngine engine,
                               void* d3d12Device) {
    if (!engine) return MAG_INVALID_PARAM;

    engine->externalD3D12Device = d3d12Device;
    if (d3d12Device) {
        engine->computeBackendFlavor = MAG_BACKEND_DX12;
    }
    selectBackend(engine);
    return MAG_OK;
}

MagStatus mag_set_unity_graphics_renderer(int deviceType) {
    std::lock_guard lock(g_unityGraphicsMutex);
    g_unityGraphicsRenderer = deviceType;
    return MAG_OK;
}

MagStatus mag_set_d3d11_device(MagEngine engine,
                               void* d3d11Device,
                               void* d3d11DeviceContext) {
    if (!engine) return MAG_INVALID_PARAM;

    engine->externalD3D11Device = d3d11Device;
    engine->externalD3D11DeviceContext = d3d11DeviceContext;
    selectBackend(engine);
    return MAG_OK;
}

MagStatus mag_bind_unity_graphics_device(MagEngine engine) {
    if (!engine) return MAG_INVALID_PARAM;

    std::lock_guard lock(g_unityGraphicsMutex);
    if (g_unityGraphicsRenderer == kUnityGfxRendererD3D11) {
        return mag_set_d3d11_device(engine, g_unityGraphicsDevice, nullptr);
    }

    if (g_unityGraphicsRenderer == kUnityGfxRendererD3D12) {
        return mag_set_d3d12_device(engine, g_unityGraphicsDevice);
    }

    return MAG_OK;
}

extern "C" MAG_API void UnitySetGraphicsDevice(void* device, int deviceType, int eventType) {
    std::lock_guard lock(g_unityGraphicsMutex);

    if (eventType == kUnityGfxDeviceEventInitialize ||
        eventType == kUnityGfxDeviceEventAfterReset) {
        g_unityGraphicsDevice = device;
        g_unityGraphicsRenderer = deviceType;
    } else if (eventType == kUnityGfxDeviceEventShutdown ||
               eventType == kUnityGfxDeviceEventBeforeReset) {
        g_unityGraphicsDevice = nullptr;
        g_unityGraphicsRenderer = deviceType;
    }
}

/* ------------------------------------------------------------------ */
/* Debug                                                              */
/* ------------------------------------------------------------------ */
MagStatus mag_debug_get_ray_count(MagEngine engine, uint32_t* outCount) {
    if (!engine || !outCount) return MAG_INVALID_PARAM;
    *outCount = engine->lastRayCount.load(std::memory_order_relaxed);
    return MAG_OK;
}

MagStatus mag_debug_get_active_edges(MagEngine engine, uint32_t* outCount) {
    if (!engine || !outCount) return MAG_INVALID_PARAM;
    *outCount = engine->lastEdgeCount.load(std::memory_order_relaxed);
    return MAG_OK;
}
