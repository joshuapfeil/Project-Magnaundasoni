/**
 * @file DiffractionSolver.h
 * @brief UTD-inspired diffraction computation around edges.
 */

#ifndef MAGNAUNDASONI_RENDER_DIFFRACTION_SOLVER_H
#define MAGNAUNDASONI_RENDER_DIFFRACTION_SOLVER_H

#include "../core/BVH.h"
#include "../core/EdgeExtractor.h"
#include "../core/Scene.h"
#include "../core/Types.h"
#include "Magnaundasoni.h"

#include <cstdint>
#include <vector>

namespace magnaundasoni {

enum class QualityLevel : uint32_t {
    Low    = 0,
    Medium = 1,
    High   = 2,
    Ultra  = 3
};

/// Internal diffraction tap.
struct DiffractionTapInternal {
    uint32_t  edgeID  = 0;
    float     delay   = 0.0f;     // seconds
    Vec3      direction;          // arrival direction at listener
    BandArray perBandAttenuation = {};
    float     pathLength = 0.0f;
    uint32_t  depth      = 1;     // diffraction depth (1 = single edge)
};

class DiffractionSolver {
public:
    struct Config {
        QualityLevel quality           = QualityLevel::High;
        uint32_t     maxDiffractionDepth = 2;
        uint32_t     maxTaps           = 4;
        float        speedOfSound      = 343.0f;
        float        maxDistance        = 500.0f;
        uint32_t     effectiveBands    = 8;
    };

    void configure(const Config& cfg) { config_ = cfg; }

    /// Compute diffraction taps for a source-listener pair.
    std::vector<DiffractionTapInternal> solve(
        const Vec3& sourcePos,
        const Vec3& listenerPos,
        const std::vector<DiffractionEdge>& edges,
        const BVH& bvh) const;

private:
    /// Compute per-band UTD diffraction coefficient for a single edge.
    BandArray computeUTDCoefficients(
        const DiffractionEdge& edge,
        const Vec3& sourcePos,
        const Vec3& listenerPos) const;

    /// Simplified Fresnel approximation (low quality).
    BandArray computeFresnelApprox(
        const DiffractionEdge& edge,
        const Vec3& sourcePos,
        const Vec3& listenerPos) const;

    /// Test visibility from a point to an edge center.
    bool isVisible(const Vec3& from, const Vec3& to,
                   const BVH& bvh) const;

    /// Cascaded diffraction through two edges.
    BandArray computeCascadedUTD(
        const DiffractionEdge& edge1,
        const DiffractionEdge& edge2,
        const Vec3& sourcePos,
        const Vec3& listenerPos) const;

    Config config_;
};

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_RENDER_DIFFRACTION_SOLVER_H
