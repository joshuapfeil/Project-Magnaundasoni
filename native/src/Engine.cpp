/**
 * @file Engine.cpp
 * @brief C ABI entry-point that bridges the public API to the internal C++ engine.
 */

#include "Magnaundasoni.h"

#include "core/BVH.h"
#include "core/ChunkManager.h"
#include "core/EdgeExtractor.h"
#include "core/MaterialPresets.h"
#include "core/Scene.h"
#include "core/SpatialGrid.h"
#include "core/ThreadPool.h"
#include "core/Types.h"
#include "spatial/HRTFDatabase.h"
#include "spatial/Quaternion.h"
#include "spatial/SpatialConfig.h"

#include <atomic>
#include <array>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

using namespace magnaundasoni;

/* ------------------------------------------------------------------ */
/* Internal engine state                                              */
/* ------------------------------------------------------------------ */
struct MagEngine_T {
    MagEngineConfig  config{};
    MagQualityLevel  activeQuality = MAG_QUALITY_MEDIUM;
    MagBackendType   backendUsed   = MAG_BACKEND_SOFTWARE_BVH;

    Scene            scene;
    BVH              bvh;
    SpatialGrid      spatialGrid{8.0f};
    std::unique_ptr<ChunkManager> chunkManager;
    EdgeExtractor    edgeExtractor;
    std::unique_ptr<ThreadPool>   threadPool;

    // Per-frame stats
    std::atomic<uint32_t> lastRayCount{0};
    std::atomic<uint32_t> lastEdgeCount{0};
    double                timestamp = 0.0;
    float                 lastCpuTimeMs = 0.0f;

    // Cached results (source,listener) -> result
    std::mutex resultMutex;
    std::unordered_map<uint64_t, MagAcousticResult> cachedResults;

    std::mutex spatialMutex;
    MagSpatialConfig spatialConfig = defaultSpatialConfig();
    MagSpeakerLayout speakerLayout = defaultSpeakerLayout(MAG_SPEAKERS_STEREO);
    HRTFDatabase hrtfDatabase;
    std::unordered_map<uint32_t, std::array<float, 4>> listenerHeadPoses;
};

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */
static inline uint64_t pairKey(uint32_t a, uint32_t b) {
    return (static_cast<uint64_t>(a) << 32) | b;
}

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

    // Select backend
    if (config->preferredBackend == MAG_BACKEND_AUTO)
        engine->backendUsed = MAG_BACKEND_SOFTWARE_BVH;
    else
        engine->backendUsed = config->preferredBackend;

    // Configure chunk manager
    float chunkSize = (config->worldChunkSize > 0.0f) ? config->worldChunkSize : 32.0f;
    engine->chunkManager = std::make_unique<ChunkManager>(chunkSize);
    engine->spatialGrid.setCellSize(chunkSize * 0.25f);

    // Thread pool (0 = auto)
    engine->threadPool = std::make_unique<ThreadPool>(config->threadCount);

    *outEngine = engine;
    return MAG_OK;
}

MagStatus mag_engine_destroy(MagEngine engine) {
    if (!engine) return MAG_INVALID_PARAM;

    // Free any cached reflection/diffraction arrays
    {
        std::lock_guard lock(engine->resultMutex);
        for (auto& [key, res] : engine->cachedResults) {
            delete[] res.reflections;
            delete[] res.diffractions;
            res.reflections = nullptr;
            res.diffractions = nullptr;
        }
        engine->cachedResults.clear();
    }

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
        !isValidHRTFPreset(config->hrtfPreset)) {
        return MAG_INVALID_PARAM;
    }

    std::lock_guard lock(engine->spatialMutex);
    engine->spatialConfig = *config;
    if (engine->spatialConfig.maxBinauralSources == 0) {
        engine->spatialConfig.maxBinauralSources = 16;
    }
    MagSpeakerLayoutPreset layoutPreset = speakerLayoutForMode(config->mode,
                                                               config->speakerLayout);
    if (layoutPreset != MAG_SPEAKERS_CUSTOM) {
        engine->speakerLayout = defaultSpeakerLayout(layoutPreset);
    } else if (engine->speakerLayout.channelCount == 0) {
        engine->speakerLayout = defaultSpeakerLayout(MAG_SPEAKERS_STEREO);
    }
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
    if (!normaliseQuaternion(quaternion, normalised)) return MAG_INVALID_PARAM;

    const ListenerEntry* existing = engine->scene.getListener(listenerID);
    if (!existing) return MAG_ERROR;

    ListenerEntry updated = *existing;
    updated.forward = rotateByQuaternion({0.0f, 0.0f, 1.0f}, normalised).normalized();
    updated.up = rotateByQuaternion({0.0f, 1.0f, 0.0f}, normalised).normalized();
    if (!engine->scene.updateListener(listenerID, updated)) return MAG_ERROR;

    std::lock_guard lock(engine->spatialMutex);
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

/* ------------------------------------------------------------------ */
/* Simulation tick                                                    */
/* ------------------------------------------------------------------ */
MagStatus mag_update(MagEngine engine, float deltaTime) {
    if (!engine) return MAG_INVALID_PARAM;

    auto t0 = std::chrono::high_resolution_clock::now();

    // Rebuild BVH (in production this would be incremental)
    rebuildBVH(engine);

    // Update chunk fidelity around the first active listener
    auto listenerIDs = engine->scene.getActiveListenerIDs();
    if (!listenerIDs.empty()) {
        const ListenerEntry* lis = engine->scene.getListener(listenerIDs[0]);
        if (lis) {
            engine->chunkManager->updateFidelityZones(lis->position);
        }
    }

    auto sourceIDs = engine->scene.getActiveSourceIDs();
    uint32_t totalRays = 0;
    uint32_t totalEdges = 0;

    // Clear old cached results
    {
        std::lock_guard lock(engine->resultMutex);
        for (auto& [key, res] : engine->cachedResults) {
            delete[] res.reflections;
            delete[] res.diffractions;
            res.reflections = nullptr;
            res.diffractions = nullptr;
        }
        engine->cachedResults.clear();
    }

    // For each source × listener pair, trace rays and compute acoustic result
    for (uint32_t sid : sourceIDs) {
        const SourceEntry* src = engine->scene.getSource(sid);
        if (!src) continue;

        for (uint32_t lid : listenerIDs) {
            const ListenerEntry* lis = engine->scene.getListener(lid);
            if (!lis) continue;

            MagAcousticResult result{};

            // --- Direct path ---
            Vec3 toListener = lis->position - src->position;
            float dist = toListener.length();
            Vec3 dir = (dist > 1e-6f) ? toListener / dist : Vec3{0, 0, 1};

            result.direct.delay = dist / 343.0f; // speed of sound
            result.direct.direction[0] = dir.x;
            result.direct.direction[1] = dir.y;
            result.direct.direction[2] = dir.z;
            result.direct.confidence = 1.0f;

            // Occlusion test
            Ray occRay;
            occRay.origin    = src->position;
            occRay.direction = dir;
            occRay.tMin      = 0.001f;
            occRay.tMax      = dist - 0.001f;

            bool occluded = engine->bvh.intersectAny(occRay);
            float occlusionFactor = occluded ? 0.2f : 1.0f;
            float lpf = occluded ? 2000.0f : 0.0f;

            result.direct.occlusionLPF = lpf;

            // Distance attenuation per band (1/r² with air absorption at HF)
            float invDistSq = 1.0f / (dist * dist + 1.0f);
            static const float airAbsorption[MAG_MAX_BANDS] =
                {0.001f, 0.002f, 0.005f, 0.01f, 0.02f, 0.05f, 0.10f, 0.15f};
            for (int b = 0; b < MAG_MAX_BANDS; ++b) {
                float air = std::exp(-airAbsorption[b] * dist);
                result.direct.perBandGain[b] = invDistSq * air * occlusionFactor;
            }
            totalRays++;

            // --- Reflections (simple first-order) ---
            uint32_t raysPerSource = engine->config.raysPerSource;
            if (raysPerSource == 0) raysPerSource = 64;

            std::vector<MagReflectionTap> reflections;

            for (uint32_t r = 0; r < raysPerSource; ++r) {
                // Fibonacci sphere distribution
                float golden = 2.399963f; // pi*(1+sqrt(5))
                float theta = golden * r;
                float phi = std::acos(1.0f - 2.0f * (r + 0.5f) / static_cast<float>(raysPerSource));

                Vec3 rayDir{std::sin(phi) * std::cos(theta),
                            std::sin(phi) * std::sin(theta),
                            std::cos(phi)};

                Ray ray;
                ray.origin    = src->position;
                ray.direction = rayDir;
                ray.tMin      = 0.001f;
                ray.tMax      = 500.0f;

                HitResult hit = engine->bvh.intersect(ray);
                if (!hit.hit) { totalRays++; continue; }

                // Reflect towards listener
                Vec3 reflPoint = hit.hitPoint;
                Vec3 toL = lis->position - reflPoint;
                float reflDist = toL.length();
                Vec3 reflDir = (reflDist > 1e-6f) ? toL / reflDist : Vec3{0, 1, 0};

                // Check if reflected path is unoccluded
                Ray reflRay;
                reflRay.origin    = reflPoint + hit.normal * 0.01f;
                reflRay.direction = reflDir;
                reflRay.tMin      = 0.001f;
                reflRay.tMax      = reflDist - 0.001f;

                if (!engine->bvh.intersectAny(reflRay)) {
                    float totalDist = hit.distance + reflDist;
                    float totalInvSq = 1.0f / (totalDist * totalDist + 1.0f);

                    MagReflectionTap tap{};
                    tap.tapID = r;
                    tap.delay = totalDist / 343.0f;
                    tap.direction[0] = reflDir.x;
                    tap.direction[1] = reflDir.y;
                    tap.direction[2] = reflDir.z;
                    tap.order = 1;
                    tap.stability = 0.8f;

                    // Per-band energy accounting for material absorption
                    const MaterialEntry* mat = engine->scene.getMaterial(hit.materialID);
                    for (int b = 0; b < MAG_MAX_BANDS; ++b) {
                        float refl = 1.0f;
                        if (mat) refl = 1.0f - mat->absorption[b];
                        float air = std::exp(-airAbsorption[b] * totalDist);
                        tap.perBandEnergy[b] = totalInvSq * refl * air;
                    }

                    reflections.push_back(tap);
                }
                totalRays++;
            }

            // Copy reflections into heap-allocated array
            result.reflectionCount = static_cast<uint32_t>(reflections.size());
            if (!reflections.empty()) {
                result.reflections = new (std::nothrow) MagReflectionTap[reflections.size()];
                if (result.reflections) {
                    std::memcpy(result.reflections, reflections.data(),
                                reflections.size() * sizeof(MagReflectionTap));
                }
            }

            // --- Late-field estimate ---
            // Rough RT60 based on average absorption
            for (int b = 0; b < MAG_MAX_BANDS; ++b) {
                float bandAvg = 0.0f;
                int matCount = 0;
                // Average over all registered materials
                auto geoIDs = engine->scene.getAllGeometryIDs();
                for (uint32_t gid : geoIDs) {
                    const GeometryEntry* geo = engine->scene.getGeometry(gid);
                    if (!geo) continue;
                    const MaterialEntry* m = engine->scene.getMaterial(geo->materialID);
                    if (m) { bandAvg += m->absorption[b]; matCount++; }
                }
                if (matCount > 0) bandAvg /= static_cast<float>(matCount);

                float alpha = std::max(bandAvg, 0.01f);
                // Sabine equation: RT60 = 0.161 * V / (S * alpha)
                // We estimate room size from the BVH bounds
                float roomEst = 1000.0f; // cubic metres fallback
                float surfEst = 600.0f;  // square metres fallback
                result.lateField.rt60[b] = 0.161f * roomEst / (surfEst * alpha);
                result.lateField.perBandDecay[b] = alpha;
            }
            result.lateField.roomSizeEstimate = 1000.0f;
            result.lateField.diffuseDirectionality = 0.3f;

            // --- Diffraction (stub: no edges for now, count is 0) ---
            result.diffractionCount = 0;
            result.diffractions = nullptr;

            // Cache
            {
                std::lock_guard lock(engine->resultMutex);
                engine->cachedResults[pairKey(sid, lid)] = result;
            }

            totalEdges += 0; // placeholder for real edge count
        }
    }

    engine->lastRayCount.store(totalRays, std::memory_order_relaxed);
    engine->lastEdgeCount.store(totalEdges, std::memory_order_relaxed);
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

    std::lock_guard lock(engine->resultMutex);
    auto it = engine->cachedResults.find(pairKey(sourceID, listenerID));
    if (it == engine->cachedResults.end()) return MAG_ERROR;

    *outResult = it->second;
    return MAG_OK;
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

    return MAG_OK;
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
