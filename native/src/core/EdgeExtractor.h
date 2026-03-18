/**
 * @file EdgeExtractor.h
 * @brief Diffraction-edge extraction and ranking for the UTD solver.
 */

#ifndef MAGNAUNDASONI_CORE_EDGE_EXTRACTOR_H
#define MAGNAUNDASONI_CORE_EDGE_EXTRACTOR_H

#include "BVH.h"
#include "Types.h"

#include <cstdint>
#include <vector>

namespace magnaundasoni {

/* ------------------------------------------------------------------ */
/* DiffractionEdge                                                    */
/* ------------------------------------------------------------------ */
struct DiffractionEdge {
    Vec3     endpoint0;
    Vec3     endpoint1;
    Vec3     centerPoint;
    float    wedgeAngle  = 0.0f;       // radians
    uint32_t adjacentMaterialIDs[2]{};
    uint32_t thicknessClass = 1;
    float    sharpness      = 1.0f;    // 0=rounded, 1=sharp
    uint32_t dynamicFlag    = 0;       // 0=static
};

/* ------------------------------------------------------------------ */
/* EdgeExtractor                                                      */
/* ------------------------------------------------------------------ */
class EdgeExtractor {
public:
    /** Minimum wedge angle (radians) for an edge to be considered diffracting. */
    float minWedgeAngle = 0.2618f; // ~15 degrees

    /** Extract diffraction edges from a triangle mesh. */
    std::vector<DiffractionEdge> extractEdges(
        const std::vector<Triangle>& triangles) const;

    /** Find silhouette edges with respect to a viewpoint. */
    std::vector<DiffractionEdge> findSilhouetteEdges(
        const std::vector<Triangle>& triangles,
        const Vec3& viewpoint) const;

    /** Classify whether an edge is diffracting (wedge angle > threshold). */
    bool classifyEdge(const DiffractionEdge& edge) const;

    /**
     * Prune and rank edges, returning the top-K most relevant to the
     * source-listener pair.
     */
    std::vector<DiffractionEdge> pruneEdges(
        const std::vector<DiffractionEdge>& edges,
        const Vec3& sourcePos,
        const Vec3& listenerPos,
        uint32_t maxCount) const;

private:
    /** Score an edge by its relevance to a source-listener pair. */
    float scoreEdge(const DiffractionEdge& edge,
                    const Vec3& sourcePos,
                    const Vec3& listenerPos) const;

    /** Closest point on a line segment to a point. */
    static Vec3 closestPointOnSegment(const Vec3& a, const Vec3& b, const Vec3& p);
};

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_CORE_EDGE_EXTRACTOR_H
