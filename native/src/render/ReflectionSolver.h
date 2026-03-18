/**
 * @file ReflectionSolver.h
 * @brief Monte Carlo reflection solver with multi-order bounces and importance sampling.
 */

#ifndef MAGNAUNDASONI_RENDER_REFLECTION_SOLVER_H
#define MAGNAUNDASONI_RENDER_REFLECTION_SOLVER_H

#include "../core/BVH.h"
#include "../core/Scene.h"
#include "../core/Types.h"
#include "Magnaundasoni.h"

#include <cstdint>
#include <vector>

namespace magnaundasoni {

/// Internal representation of a reflection tap.
struct ReflectionTapInternal {
    uint32_t  tapID        = 0;
    float     delay        = 0.0f;   // seconds
    Vec3      direction;             // arrival direction at listener
    BandArray perBandEnergy = {};
    uint32_t  order        = 0;      // bounce count
    float     stability    = 0.0f;   // tap temporal stability [0,1]
    float     pathLength   = 0.0f;
    Vec3      lastHitPoint;          // for clustering
};

/// Statistics gathered during reflection tracing (used by reverb estimator).
struct ReflectionStats {
    float    totalPathLength     = 0.0f;
    uint32_t totalBounces        = 0;
    uint32_t totalHits           = 0;
    uint32_t totalRays           = 0;
    BandArray accumulatedAbsorption = {};
    float    meanFreePathEstimate = 0.0f;
};

class ReflectionSolver {
public:
    struct Config {
        uint32_t raysPerSource       = 512;
        uint32_t maxReflectionOrder  = 3;
        uint32_t maxTaps             = 8;
        float    speedOfSound        = 343.0f;
        float    maxPropagationDist  = 500.0f;
        float    listenerRadius      = 1.5f;  // cone acceptance radius at listener
        float    scatteringBlend     = 0.5f;  // [0=specular, 1=diffuse]
        float    importanceBias      = 0.3f;  // fraction of rays biased toward listener
        uint32_t effectiveBands      = 8;
        float    humidity            = 50.0f;
        float    temperature         = 20.0f;
    };

    void configure(const Config& cfg) { config_ = cfg; }

    /// Trace reflections from source, returning sorted top-K taps.
    std::vector<ReflectionTapInternal> solve(
        const Vec3& sourcePos,
        const Vec3& listenerPos,
        const BVH& bvh,
        const Scene& scene);

    /// Access the statistics from the last solve() call.
    const ReflectionStats& getLastStats() const { return lastStats_; }

private:
    /// Generate ray direction using Fibonacci sphere distribution.
    static Vec3 fibonacciSphereDir(uint32_t index, uint32_t total);

    /// Generate importance-biased ray direction (blend toward listener).
    Vec3 biasedRayDir(uint32_t index, uint32_t total,
                      const Vec3& toListener) const;

    /// Compute specular reflection direction.
    static Vec3 reflect(const Vec3& incident, const Vec3& normal);

    /// Random-ish scattered direction around normal (deterministic from seed).
    static Vec3 scatterDir(const Vec3& normal, uint32_t seed);

    /// Cluster nearby taps and merge.
    void clusterTaps(std::vector<ReflectionTapInternal>& taps) const;

    Config          config_;
    ReflectionStats lastStats_;
};

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_RENDER_REFLECTION_SOLVER_H
