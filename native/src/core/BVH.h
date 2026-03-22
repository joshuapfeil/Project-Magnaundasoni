/**
 * @file BVH.h
 * @brief SAH-based Bounding Volume Hierarchy for ray–triangle intersection.
 */

#ifndef MAGNAUNDASONI_CORE_BVH_H
#define MAGNAUNDASONI_CORE_BVH_H

#include "Types.h"
#include <vector>

namespace magnaundasoni {

/* ------------------------------------------------------------------ */
/* Triangle                                                           */
/* ------------------------------------------------------------------ */
struct Triangle {
    Vec3     v0, v1, v2;
    Vec3     normal;
    Vec3     e1;
    Vec3     e2;
    Vec3     centroidValue;
    AABB     bounds;
    uint32_t materialID  = 0;
    uint32_t geometryID  = 0;

    void updateDerivedData() {
        e1 = v1 - v0;
        e2 = v2 - v0;

        bounds = AABB{};
        bounds.expand(v0);
        bounds.expand(v1);
        bounds.expand(v2);

        centroidValue = (v0 + v1 + v2) * (1.0f / 3.0f);
    }

    AABB computeAABB() const { return bounds; }

    Vec3 centroid() const { return centroidValue; }
};

/* ------------------------------------------------------------------ */
/* BVHNode – flat layout for cache-friendly traversal                 */
/* ------------------------------------------------------------------ */
struct BVHNode {
    AABB     bounds;
    uint32_t leftChild    = 0;   // index into nodes_ (0 = invalid for non-root)
    uint32_t rightChild   = 0;
    uint32_t primStart    = 0;   // index into triangles_ (leaf only)
    uint32_t primCount    = 0;   // 0 means internal node
    bool     isLeaf       = false;
};

/* ------------------------------------------------------------------ */
/* BVH                                                                */
/* ------------------------------------------------------------------ */
class BVH {
public:
    BVH() = default;

    /** Build from a set of triangles.  Existing data is replaced. */
    void build(const std::vector<Triangle>& triangles);

    /** Incremental rebuild (currently full rebuild). */
    void rebuild();

    /** Find the closest intersection.  Returns HitResult with hit==true on success. */
    HitResult intersect(const Ray& ray) const;

    /** Shadow / occlusion test – returns true if *any* hit exists. */
    bool intersectAny(const Ray& ray) const;

    /** Number of triangles in the BVH. */
    uint32_t triangleCount() const { return static_cast<uint32_t>(triangles_.size()); }

    /** Access the internal triangle array (read-only). */
    const std::vector<Triangle>& triangles() const { return triangles_; }

    /** Access the internal BVH node array (read-only). */
    const std::vector<BVHNode>& nodes() const { return nodes_; }

    bool empty() const { return nodes_.empty(); }

private:
    static constexpr uint32_t kMaxLeafPrims = 4;
    static constexpr uint32_t kSAHBins      = 12;
    static constexpr float    kTraversalCost = 1.0f;
    static constexpr float    kIntersectCost = 1.0f;

    void buildRecursive(uint32_t nodeIdx, uint32_t start, uint32_t end);

    /** Möller–Trumbore ray–triangle intersection. */
    static bool rayTriangle(const Ray& ray, const Triangle& tri,
                            float& outT, float& outU, float& outV);

    /** Slab-test for AABB. */
    static bool rayAABB(const Ray& ray, const AABB& box,
                        const Vec3& invDirection,
                        const std::array<uint8_t, 3>& directionSign,
                        float tMin, float tMax,
                        float* outEntryT = nullptr);

    std::vector<Triangle> triangles_;
    std::vector<BVHNode>  nodes_;
};

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_CORE_BVH_H
