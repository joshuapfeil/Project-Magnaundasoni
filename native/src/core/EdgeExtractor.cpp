/**
 * @file EdgeExtractor.cpp
 * @brief Diffraction-edge extraction, classification, and ranking.
 */

#include "core/EdgeExtractor.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

namespace magnaundasoni {

namespace {

// Canonical edge key: sorted pair of vertex positions (quantised to avoid FP noise)
struct EdgeKey {
    int64_t ax, ay, az, bx, by, bz;
    bool operator<(const EdgeKey& o) const {
        return std::tie(ax, ay, az, bx, by, bz) <
               std::tie(o.ax, o.ay, o.az, o.bx, o.by, o.bz);
    }
};

constexpr float kQuantize = 10000.0f; // 0.1 mm precision

int64_t q(float v) { return static_cast<int64_t>(std::round(v * kQuantize)); }

EdgeKey makeKey(const Vec3& a, const Vec3& b) {
    EdgeKey ka{q(a.x), q(a.y), q(a.z), q(b.x), q(b.y), q(b.z)};
    EdgeKey kb{q(b.x), q(b.y), q(b.z), q(a.x), q(a.y), q(a.z)};
    return (ka < kb) ? ka : kb;
}

struct HalfEdge {
    Vec3     v0, v1;
    Vec3     faceNormal;
    uint32_t materialID;
};

} // anonymous namespace

/* ------------------------------------------------------------------ */
/* extractEdges                                                       */
/* ------------------------------------------------------------------ */
std::vector<DiffractionEdge> EdgeExtractor::extractEdges(
        const std::vector<Triangle>& triangles) const {

    // Collect half-edges keyed by quantised vertex pair
    std::map<EdgeKey, std::vector<HalfEdge>> edgeMap;

    for (const auto& tri : triangles) {
        Vec3 verts[3] = {tri.v0, tri.v1, tri.v2};
        Vec3 n = (tri.v1 - tri.v0).cross(tri.v2 - tri.v0).normalized();

        for (int i = 0; i < 3; ++i) {
            const Vec3& a = verts[i];
            const Vec3& b = verts[(i + 1) % 3];
            EdgeKey key = makeKey(a, b);
            edgeMap[key].push_back({a, b, n, tri.materialID});
        }
    }

    std::vector<DiffractionEdge> result;
    result.reserve(edgeMap.size() / 4);

    for (const auto& [key, halfEdges] : edgeMap) {
        // We only consider edges shared by exactly 2 faces (manifold)
        if (halfEdges.size() != 2) continue;

        const Vec3& n0 = halfEdges[0].faceNormal;
        const Vec3& n1 = halfEdges[1].faceNormal;

        float cosAngle = n0.dot(n1);
        cosAngle = std::max(-1.0f, std::min(1.0f, cosAngle));
        float wedgeAngle = std::acos(cosAngle);

        if (wedgeAngle < minWedgeAngle) continue;

        DiffractionEdge edge;
        edge.endpoint0   = halfEdges[0].v0;
        edge.endpoint1   = halfEdges[0].v1;
        edge.centerPoint = (edge.endpoint0 + edge.endpoint1) * 0.5f;
        edge.wedgeAngle  = wedgeAngle;
        edge.adjacentMaterialIDs[0] = halfEdges[0].materialID;
        edge.adjacentMaterialIDs[1] = halfEdges[1].materialID;
        edge.sharpness   = std::min(wedgeAngle / 3.14159f, 1.0f);

        result.push_back(edge);
    }

    return result;
}

/* ------------------------------------------------------------------ */
/* findSilhouetteEdges                                                */
/* ------------------------------------------------------------------ */
std::vector<DiffractionEdge> EdgeExtractor::findSilhouetteEdges(
        const std::vector<Triangle>& triangles,
        const Vec3& viewpoint) const {

    std::map<EdgeKey, std::vector<HalfEdge>> edgeMap;

    for (const auto& tri : triangles) {
        Vec3 verts[3] = {tri.v0, tri.v1, tri.v2};
        Vec3 n = (tri.v1 - tri.v0).cross(tri.v2 - tri.v0).normalized();

        for (int i = 0; i < 3; ++i) {
            const Vec3& a = verts[i];
            const Vec3& b = verts[(i + 1) % 3];
            EdgeKey key = makeKey(a, b);
            edgeMap[key].push_back({a, b, n, tri.materialID});
        }
    }

    std::vector<DiffractionEdge> result;

    for (const auto& [key, halfEdges] : edgeMap) {
        if (halfEdges.size() != 2) continue;

        Vec3 mid = (halfEdges[0].v0 + halfEdges[0].v1) * 0.5f;
        Vec3 toView = (viewpoint - mid).normalized();

        float d0 = halfEdges[0].faceNormal.dot(toView);
        float d1 = halfEdges[1].faceNormal.dot(toView);

        // Silhouette: one face faces towards the viewpoint, the other faces away
        if ((d0 > 0.0f) != (d1 > 0.0f)) {
            float cosAngle = halfEdges[0].faceNormal.dot(halfEdges[1].faceNormal);
            cosAngle = std::max(-1.0f, std::min(1.0f, cosAngle));

            DiffractionEdge edge;
            edge.endpoint0   = halfEdges[0].v0;
            edge.endpoint1   = halfEdges[0].v1;
            edge.centerPoint = mid;
            edge.wedgeAngle  = std::acos(cosAngle);
            edge.adjacentMaterialIDs[0] = halfEdges[0].materialID;
            edge.adjacentMaterialIDs[1] = halfEdges[1].materialID;
            edge.sharpness   = std::min(edge.wedgeAngle / 3.14159f, 1.0f);

            result.push_back(edge);
        }
    }

    return result;
}

/* ------------------------------------------------------------------ */
/* classifyEdge                                                       */
/* ------------------------------------------------------------------ */
bool EdgeExtractor::classifyEdge(const DiffractionEdge& edge) const {
    return edge.wedgeAngle >= minWedgeAngle;
}

/* ------------------------------------------------------------------ */
/* Scoring and pruning                                                */
/* ------------------------------------------------------------------ */
Vec3 EdgeExtractor::closestPointOnSegment(const Vec3& a, const Vec3& b, const Vec3& p) {
    Vec3 ab = b - a;
    float t = (p - a).dot(ab) / (ab.dot(ab) + 1e-12f);
    t = std::max(0.0f, std::min(1.0f, t));
    return a + ab * t;
}

float EdgeExtractor::scoreEdge(const DiffractionEdge& edge,
                                const Vec3& sourcePos,
                                const Vec3& listenerPos) const {
    // Midpoint of the source-listener segment
    Vec3 midSL = (sourcePos + listenerPos) * 0.5f;

    // Distance from the edge to the source-listener midpoint
    Vec3 closest = closestPointOnSegment(edge.endpoint0, edge.endpoint1, midSL);
    float dist = (closest - midSL).length() + 1e-3f;

    // Score: higher is better  →  large wedge angle, close to the SL midpoint
    float wedgeFactor = edge.wedgeAngle / 3.14159f;
    return wedgeFactor / dist;
}

std::vector<DiffractionEdge> EdgeExtractor::pruneEdges(
        const std::vector<DiffractionEdge>& edges,
        const Vec3& sourcePos,
        const Vec3& listenerPos,
        uint32_t maxCount) const {

    if (edges.size() <= maxCount)
        return edges;

    // Build (score, index) pairs
    struct Scored { float score; size_t idx; };
    std::vector<Scored> scored;
    scored.reserve(edges.size());

    for (size_t i = 0; i < edges.size(); ++i)
        scored.push_back({scoreEdge(edges[i], sourcePos, listenerPos), i});

    // Partial sort (top-K)
    std::partial_sort(scored.begin(),
                      scored.begin() + maxCount,
                      scored.end(),
                      [](const Scored& a, const Scored& b) { return a.score > b.score; });

    std::vector<DiffractionEdge> result;
    result.reserve(maxCount);
    for (uint32_t i = 0; i < maxCount; ++i)
        result.push_back(edges[scored[i].idx]);

    return result;
}

} // namespace magnaundasoni
