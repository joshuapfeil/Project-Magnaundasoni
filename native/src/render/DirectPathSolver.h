/**
 * @file DirectPathSolver.h
 * @brief Dedicated direct-path computation: occlusion, transmission, air absorption.
 */

#ifndef MAGNAUNDASONI_RENDER_DIRECT_PATH_SOLVER_H
#define MAGNAUNDASONI_RENDER_DIRECT_PATH_SOLVER_H

#include "../core/BVH.h"
#include "../core/Scene.h"
#include "../core/Types.h"
#include "Magnaundasoni.h"

#include <cstdint>
#include <vector>

namespace magnaundasoni {

/// Internal result produced by the direct-path solver.
struct DirectPathResult {
    float     delay         = 0.0f;     // seconds
    Vec3      direction;                // normalised: listener → source
    BandArray perBandGain   = {};
    float     occlusionLPF  = 0.0f;     // Hz, 0 = unoccluded
    float     confidence    = 0.0f;
    float     distance      = 0.0f;
    bool      occluded      = false;
    uint32_t  occluderCount = 0;
};

class DirectPathSolver {
public:
    struct Config {
        float speedOfSound      = 343.0f;
        float nearFieldRadius   = 0.1f;
        float maxDistance        = 500.0f;
        float humidity          = 50.0f;
        float temperature       = 20.0f;
        uint32_t effectiveBands = 8;
    };

    void configure(const Config& cfg) { config_ = cfg; }

    /// Compute the direct path from source to listener.
    DirectPathResult solve(const Vec3& sourcePos,
                           const Vec3& listenerPos,
                           const BVH& bvh,
                           const Scene& scene) const;

private:
    /// Trace through occluding geometry and accumulate per-band transmission.
    BandArray accumulateTransmission(const Vec3& origin,
                                     const Vec3& direction,
                                     float maxDist,
                                     const BVH& bvh,
                                     const Scene& scene,
                                     uint32_t& occluderCount) const;

    Config config_;
};

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_RENDER_DIRECT_PATH_SOLVER_H
