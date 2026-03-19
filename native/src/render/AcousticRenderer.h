/**
 * @file AcousticRenderer.h
 * @brief Main acoustic rendering pipeline orchestrator.
 *
 * Drives per-frame computation of direct paths, reflections, diffraction,
 * and late-field estimation for every active source-listener pair.
 */

#ifndef MAGNAUNDASONI_RENDER_ACOUSTIC_RENDERER_H
#define MAGNAUNDASONI_RENDER_ACOUSTIC_RENDERER_H

#include "../core/BVH.h"
#include "../core/EdgeExtractor.h"
#include "../core/Scene.h"
#include "../core/Types.h"
#include "Magnaundasoni.h"

#include "BandProcessor.h"
#include "DiffractionSolver.h"
#include "DirectPathSolver.h"
#include "OutputMixer.h"
#include "ReflectionSolver.h"
#include "ReverbEstimator.h"
#include "TemporalSmoother.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace magnaundasoni {

class AcousticRenderer {
public:
    struct Config {
        QualityLevel quality              = QualityLevel::High;
        uint32_t     maxReflectionOrder   = 3;
        uint32_t     maxDiffractionDepth  = 2;
        uint32_t     raysPerSource        = 512;
        uint32_t     maxReflectionTaps    = 8;
        uint32_t     maxDiffractionTaps   = 4;
        uint32_t     effectiveBandCount   = 8;
        float        maxPropagationDistance = 500.0f;
        float        speedOfSound         = 343.0f;
        float        humidity             = 50.0f;
        float        temperature          = 20.0f;
    };

    AcousticRenderer();
    ~AcousticRenderer();

    /// Reconfigure the renderer and all sub-systems.
    void configure(const Config& config);

    /// Main per-frame update. Computes acoustics for all active pairs.
    void update(Scene& scene, const BVH& bvh,
                const EdgeExtractor& edgeExtractor, float deltaTime);

    /// Retrieve the result for a specific source-listener pair.
    /// Returns nullptr if no result is available.
    const MagAcousticResult* getResult(uint32_t sourceID, uint32_t listenerID) const;

    /// Performance statistics.
    float    getLastUpdateTimeMs() const { return lastUpdateTimeMs_; }
    uint32_t getActiveRayCount()   const { return activeRayCount_; }
    uint32_t getActiveEdgeCount()  const { return activeEdgeCount_; }

    /// Access the output mixer (for audio callback integration).
    OutputMixer& getOutputMixer() { return outputMixer_; }

private:
    // Per-pair computation stages
    void computeDirectPaths(Scene& scene, const BVH& bvh);
    void computeReflections(Scene& scene, const BVH& bvh);
    void computeDiffraction(Scene& scene, const BVH& bvh,
                            const EdgeExtractor& edgeExtractor);
    void computeLateField(Scene& scene);
    void accumulateResults();
    void applyTemporalSmoothing(float deltaTime);

    // Sub-systems
    DirectPathSolver   directSolver_;
    ReflectionSolver   reflectionSolver_;
    DiffractionSolver  diffractionSolver_;
    ReverbEstimator    reverbEstimator_;
    TemporalSmoother   temporalSmoother_;
    OutputMixer        outputMixer_;

    Config config_;

    // Active source/listener pairs for this frame
    struct PairContext {
        uint32_t sourceID   = 0;
        uint32_t listenerID = 0;
        uint64_t pairKey    = 0;

        Vec3 sourcePos;
        Vec3 listenerPos;

        DirectPathResult                     directResult;
        std::vector<ReflectionTapInternal>   reflectionTaps;
        std::vector<DiffractionTapInternal>  diffractionTaps;
        LateFieldResult                      lateFieldResult;
        ReflectionStats                      reflStats;
    };

    std::vector<PairContext> pairs_;

    // Cached output results: pairKey → MagAcousticResult
    mutable std::mutex resultMutex_;
    std::unordered_map<uint64_t, MagAcousticResult> results_;

    // Owned dynamic memory for result arrays
    struct ResultArrays {
        std::vector<MagReflectionTap>  reflections;
        std::vector<MagDiffractionTap> diffractions;
    };
    std::unordered_map<uint64_t, ResultArrays> resultArrays_;

    // Stats
    float    lastUpdateTimeMs_ = 0.0f;
    uint32_t activeRayCount_   = 0;
    uint32_t activeEdgeCount_  = 0;

    // Cached edges per frame (reused across pairs)
    std::vector<DiffractionEdge> cachedEdges_;

    static uint64_t makePairKey(uint32_t a, uint32_t b) {
        return (static_cast<uint64_t>(a) << 32) | b;
    }
};

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_RENDER_ACOUSTIC_RENDERER_H
