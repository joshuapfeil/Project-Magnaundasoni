/**
 * @file AcousticRenderer.cpp
 * @brief Main acoustic rendering pipeline implementation.
 */

#include "AcousticRenderer.h"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace magnaundasoni {

AcousticRenderer::AcousticRenderer() {
    configure(Config{});
}

AcousticRenderer::~AcousticRenderer() {
    // Nothing dynamic to free beyond what the maps own
}

void AcousticRenderer::configure(const Config& config) {
    config_ = config;

    // Configure sub-systems
    DirectPathSolver::Config dpCfg;
    dpCfg.speedOfSound    = config.speedOfSound;
    dpCfg.maxDistance      = config.maxPropagationDistance;
    dpCfg.humidity         = config.humidity;
    dpCfg.temperature      = config.temperature;
    dpCfg.effectiveBands   = config.effectiveBandCount;
    directSolver_.configure(dpCfg);

    ReflectionSolver::Config rCfg;
    rCfg.raysPerSource       = config.raysPerSource;
    rCfg.maxReflectionOrder  = config.maxReflectionOrder;
    rCfg.maxTaps             = config.maxReflectionTaps;
    rCfg.speedOfSound        = config.speedOfSound;
    rCfg.maxPropagationDist  = config.maxPropagationDistance;
    rCfg.effectiveBands      = config.effectiveBandCount;
    rCfg.humidity             = config.humidity;
    rCfg.temperature          = config.temperature;
    reflectionSolver_.configure(rCfg);

    DiffractionSolver::Config dCfg;
    dCfg.quality               = config.quality;
    dCfg.maxDiffractionDepth   = config.maxDiffractionDepth;
    dCfg.maxTaps               = config.maxDiffractionTaps;
    dCfg.speedOfSound          = config.speedOfSound;
    dCfg.maxDistance            = config.maxPropagationDistance;
    dCfg.effectiveBands        = config.effectiveBandCount;
    diffractionSolver_.configure(dCfg);

    ReverbEstimator::Config rvCfg;
    rvCfg.effectiveBands = config.effectiveBandCount;
    reverbEstimator_.configure(rvCfg);
}

void AcousticRenderer::update(Scene& scene, const BVH& bvh,
                               const EdgeExtractor& edgeExtractor,
                               float deltaTime) {
    auto t0 = std::chrono::high_resolution_clock::now();

    // Gather active source-listener pairs
    auto sourceIDs   = scene.getActiveSourceIDs();
    auto listenerIDs = scene.getActiveListenerIDs();

    pairs_.clear();
    for (uint32_t sid : sourceIDs) {
        const SourceEntry* src = scene.getSource(sid);
        if (!src) continue;

        for (uint32_t lid : listenerIDs) {
            const ListenerEntry* lis = scene.getListener(lid);
            if (!lis) continue;

            PairContext ctx;
            ctx.sourceID   = sid;
            ctx.listenerID = lid;
            ctx.pairKey    = makePairKey(sid, lid);
            ctx.sourcePos  = src->position;
            ctx.listenerPos = lis->position;
            pairs_.push_back(ctx);
        }
    }

    activeRayCount_  = 0;
    activeEdgeCount_ = 0;

    // Extract edges once per frame (reuse across pairs)
    if (!bvh.empty()) {
        // Build triangle list for edge extraction
        // We use the BVH's triangle set; but EdgeExtractor needs the triangles.
        // Since BVH owns them internally and EdgeExtractor wants a vector,
        // we reconstruct from the scene geometry.
        std::vector<Triangle> tris;
        auto geoIDs = scene.getAllGeometryIDs();
        for (uint32_t gid : geoIDs) {
            const GeometryEntry* geo = scene.getGeometry(gid);
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
                    {geo->vertices[i0*3], geo->vertices[i0*3+1], geo->vertices[i0*3+2]});
                tri.v1 = geo->transform.transformPoint(
                    {geo->vertices[i1*3], geo->vertices[i1*3+1], geo->vertices[i1*3+2]});
                tri.v2 = geo->transform.transformPoint(
                    {geo->vertices[i2*3], geo->vertices[i2*3+1], geo->vertices[i2*3+2]});
                tri.normal = (tri.v1 - tri.v0).cross(tri.v2 - tri.v0).normalized();
                tri.materialID = geo->materialID;
                tri.geometryID = gid;
                tris.push_back(tri);
            }
        }

        cachedEdges_ = edgeExtractor.extractEdges(tris);
    } else {
        cachedEdges_.clear();
    }

    // Run pipeline stages
    computeDirectPaths(scene, bvh);
    computeReflections(scene, bvh);
    computeDiffraction(scene, bvh, edgeExtractor);
    computeLateField(scene);
    accumulateResults();
    applyTemporalSmoothing(deltaTime);

    // Stage results into output mixer
    {
        std::lock_guard<std::mutex> lock(resultMutex_);
        for (const auto& [key, res] : results_) {
            uint32_t sid = static_cast<uint32_t>(key >> 32);
            uint32_t lid = static_cast<uint32_t>(key & 0xFFFFFFFF);
            outputMixer_.stageResult(sid, lid, res);
        }
    }
    outputMixer_.commitStaged();

    auto t1 = std::chrono::high_resolution_clock::now();
    lastUpdateTimeMs_ = std::chrono::duration<float, std::milli>(t1 - t0).count();
}

void AcousticRenderer::computeDirectPaths(Scene& scene, const BVH& bvh) {
    for (auto& pair : pairs_) {
        pair.directResult = directSolver_.solve(
            pair.sourcePos, pair.listenerPos, bvh, scene);
        activeRayCount_++; // one ray per direct path
    }
}

void AcousticRenderer::computeReflections(Scene& scene, const BVH& bvh) {
    for (auto& pair : pairs_) {
        pair.reflectionTaps = reflectionSolver_.solve(
            pair.sourcePos, pair.listenerPos, bvh, scene);
        pair.reflStats = reflectionSolver_.getLastStats();
        activeRayCount_ += pair.reflStats.totalRays;
    }
}

void AcousticRenderer::computeDiffraction(Scene& scene, const BVH& bvh,
                                           const EdgeExtractor& edgeExtractor) {
    for (auto& pair : pairs_) {
        // Prune edges relevant to this pair
        auto relevantEdges = edgeExtractor.pruneEdges(
            cachedEdges_, pair.sourcePos, pair.listenerPos,
            config_.maxDiffractionTaps * 4);

        pair.diffractionTaps = diffractionSolver_.solve(
            pair.sourcePos, pair.listenerPos, relevantEdges, bvh);

        activeEdgeCount_ += static_cast<uint32_t>(relevantEdges.size());
    }
}

void AcousticRenderer::computeLateField(Scene& scene) {
    for (auto& pair : pairs_) {
        pair.lateFieldResult = reverbEstimator_.estimate(pair.reflStats, scene);
    }
}

void AcousticRenderer::accumulateResults() {
    std::lock_guard<std::mutex> lock(resultMutex_);

    results_.clear();
    resultArrays_.clear();

    for (const auto& pair : pairs_) {
        MagAcousticResult res{};
        auto& arrays = resultArrays_[pair.pairKey];

        // Direct
        res.direct.delay          = pair.directResult.delay;
        res.direct.direction[0]   = pair.directResult.direction.x;
        res.direct.direction[1]   = pair.directResult.direction.y;
        res.direct.direction[2]   = pair.directResult.direction.z;
        std::memcpy(res.direct.perBandGain, pair.directResult.perBandGain.data(),
                     sizeof(float) * 8);
        res.direct.occlusionLPF   = pair.directResult.occlusionLPF;
        res.direct.confidence     = pair.directResult.confidence;

        // Reflections
        arrays.reflections.resize(pair.reflectionTaps.size());
        for (size_t i = 0; i < pair.reflectionTaps.size(); ++i) {
            const auto& src = pair.reflectionTaps[i];
            auto& dst = arrays.reflections[i];
            dst.tapID        = src.tapID;
            dst.delay        = src.delay;
            dst.direction[0] = src.direction.x;
            dst.direction[1] = src.direction.y;
            dst.direction[2] = src.direction.z;
            std::memcpy(dst.perBandEnergy, src.perBandEnergy.data(), sizeof(float) * 8);
            dst.order     = src.order;
            dst.stability = src.stability;
        }
        res.reflections     = arrays.reflections.empty() ? nullptr : arrays.reflections.data();
        res.reflectionCount = static_cast<uint32_t>(arrays.reflections.size());

        // Diffraction
        arrays.diffractions.resize(pair.diffractionTaps.size());
        for (size_t i = 0; i < pair.diffractionTaps.size(); ++i) {
            const auto& src = pair.diffractionTaps[i];
            auto& dst = arrays.diffractions[i];
            dst.edgeID       = src.edgeID;
            dst.delay        = src.delay;
            dst.direction[0] = src.direction.x;
            dst.direction[1] = src.direction.y;
            dst.direction[2] = src.direction.z;
            std::memcpy(dst.perBandAttenuation, src.perBandAttenuation.data(),
                         sizeof(float) * 8);
        }
        res.diffractions     = arrays.diffractions.empty() ? nullptr : arrays.diffractions.data();
        res.diffractionCount = static_cast<uint32_t>(arrays.diffractions.size());

        // Late field
        std::memcpy(res.lateField.perBandDecay, pair.lateFieldResult.perBandDecay.data(),
                     sizeof(float) * 8);
        std::memcpy(res.lateField.rt60, pair.lateFieldResult.rt60.data(), sizeof(float) * 8);
        res.lateField.roomSizeEstimate      = pair.lateFieldResult.roomSizeEstimate;
        res.lateField.diffuseDirectionality = pair.lateFieldResult.diffuseDirectionality;

        results_[pair.pairKey] = res;
    }
}

void AcousticRenderer::applyTemporalSmoothing(float deltaTime) {
    std::lock_guard<std::mutex> lock(resultMutex_);

    for (auto& pair : pairs_) {
        auto it = results_.find(pair.pairKey);
        if (it == results_.end()) continue;

        temporalSmoother_.smooth(pair.pairKey, it->second, pair.sourcePos, deltaTime);
    }
}

const MagAcousticResult* AcousticRenderer::getResult(uint32_t sourceID,
                                                      uint32_t listenerID) const {
    std::lock_guard<std::mutex> lock(resultMutex_);
    auto it = results_.find(makePairKey(sourceID, listenerID));
    if (it == results_.end()) return nullptr;
    return &it->second;
}

} // namespace magnaundasoni
