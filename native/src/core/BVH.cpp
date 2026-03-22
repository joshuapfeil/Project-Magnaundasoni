/**
 * @file BVH.cpp
 * @brief SAH-binned BVH construction and stack-based iterative traversal.
 */

#include "core/BVH.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <numeric>

namespace magnaundasoni {

/* ------------------------------------------------------------------ */
/* Build                                                              */
/* ------------------------------------------------------------------ */
void BVH::build(const std::vector<Triangle>& triangles) {
    triangles_ = triangles;
    nodes_.clear();

    if (triangles_.empty()) return;

    for (Triangle& tri : triangles_)
        tri.updateDerivedData();

    // Reserve a generous amount (2N-1 is the theoretical max for a binary tree)
    nodes_.reserve(2 * triangles_.size());

    // Root node
    nodes_.push_back(BVHNode{});
    nodes_[0].primStart = 0;
    nodes_[0].primCount = static_cast<uint32_t>(triangles_.size());

    buildRecursive(0, 0, static_cast<uint32_t>(triangles_.size()));
}

void BVH::rebuild() {
    std::vector<Triangle> copy = triangles_;
    build(copy);
}

/* ------------------------------------------------------------------ */
/* Recursive SAH-binned build                                         */
/* ------------------------------------------------------------------ */
void BVH::buildRecursive(uint32_t nodeIdx, uint32_t start, uint32_t end) {
    BVHNode& node = nodes_[nodeIdx];
    uint32_t count = end - start;

    // Compute bounds over the range
    AABB bounds;
    for (uint32_t i = start; i < end; ++i)
        bounds.expand(triangles_[i].computeAABB());
    node.bounds = bounds;

    // Leaf condition
    if (count <= kMaxLeafPrims) {
        node.primStart = start;
        node.primCount = count;
        node.isLeaf    = true;
        return;
    }

    // --- SAH binned split ---
    AABB centroidBounds;
    for (uint32_t i = start; i < end; ++i)
        centroidBounds.expand(triangles_[i].centroid());

    Vec3 ext = centroidBounds.extent();
    int bestAxis = 0;
    if (ext.y > ext.x) bestAxis = 1;
    if (ext.z > ext[bestAxis]) bestAxis = 2;

    float axisExtent = ext[bestAxis];

    // Degenerate – all centroids at the same position; make a leaf
    if (axisExtent < 1e-7f) {
        node.primStart = start;
        node.primCount = count;
        node.isLeaf    = true;
        return;
    }

    // Bin primitives
    struct Bin {
        AABB     bounds;
        uint32_t count = 0;
    };
    std::array<Bin, kSAHBins> bins{};

    float minC = centroidBounds.min[bestAxis];
    float scale = static_cast<float>(kSAHBins) / axisExtent;

    for (uint32_t i = start; i < end; ++i) {
        float c = triangles_[i].centroid()[bestAxis];
        int b = static_cast<int>((c - minC) * scale);
        if (b >= static_cast<int>(kSAHBins)) b = kSAHBins - 1;
        bins[b].bounds.expand(triangles_[i].computeAABB());
        bins[b].count++;
    }

    // Sweep from left to find cost at each split
    float bestCost = std::numeric_limits<float>::max();
    uint32_t bestSplit = 0;

    // Prefix: left sweep
    std::array<float, kSAHBins - 1> costLeft{};
    {
        AABB running;
        uint32_t runCount = 0;
        for (uint32_t i = 0; i < kSAHBins - 1; ++i) {
            running.expand(bins[i].bounds);
            runCount += bins[i].count;
            float sa = running.valid() ? running.surfaceArea() : 0.0f;
            costLeft[i] = sa * static_cast<float>(runCount);
        }
    }

    // Suffix: right sweep
    {
        AABB running;
        uint32_t runCount = 0;
        for (int i = static_cast<int>(kSAHBins) - 1; i >= 1; --i) {
            running.expand(bins[i].bounds);
            runCount += bins[i].count;
            float sa = running.valid() ? running.surfaceArea() : 0.0f;
            float cost = kTraversalCost +
                         kIntersectCost * (costLeft[i - 1] + sa * static_cast<float>(runCount));
            if (cost < bestCost) {
                bestCost  = cost;
                bestSplit = static_cast<uint32_t>(i);
            }
        }
    }

    // Compare with leaf cost
    float parentSA = bounds.valid() ? bounds.surfaceArea() : 1.0f;
    float leafCost = kIntersectCost * static_cast<float>(count) * parentSA;
    if (bestCost >= leafCost && count <= kMaxLeafPrims * 4) {
        node.primStart = start;
        node.primCount = count;
        node.isLeaf    = true;
        return;
    }

    // Partition
    float splitPos = minC + static_cast<float>(bestSplit) / scale;
    auto mid = std::partition(
        triangles_.begin() + start,
        triangles_.begin() + end,
        [bestAxis, splitPos](const Triangle& t) {
            return t.centroid()[bestAxis] < splitPos;
        });

    uint32_t midIdx = static_cast<uint32_t>(std::distance(triangles_.begin(), mid));

    // Ensure we don't degenerate into an empty partition
    if (midIdx == start || midIdx == end) {
        midIdx = start + count / 2;
    }

    // Create child nodes
    uint32_t leftIdx  = static_cast<uint32_t>(nodes_.size());
    nodes_.push_back(BVHNode{});
    uint32_t rightIdx = static_cast<uint32_t>(nodes_.size());
    nodes_.push_back(BVHNode{});

    // Re-fetch the node reference as the vector may have reallocated
    nodes_[nodeIdx].leftChild  = leftIdx;
    nodes_[nodeIdx].rightChild = rightIdx;
    nodes_[nodeIdx].primCount  = 0;
    nodes_[nodeIdx].isLeaf     = false;

    buildRecursive(leftIdx,  start,  midIdx);
    buildRecursive(rightIdx, midIdx, end);
}

/* ------------------------------------------------------------------ */
/* Möller–Trumbore ray–triangle                                       */
/* ------------------------------------------------------------------ */
bool BVH::rayTriangle(const Ray& ray, const Triangle& tri,
                      float& outT, float& outU, float& outV) {
    const float EPSILON = 1e-8f;

    Vec3 h  = ray.direction.cross(tri.e2);
    float a = tri.e1.dot(h);

    if (std::abs(a) < EPSILON) return false;

    float f = 1.0f / a;
    Vec3  s = ray.origin - tri.v0;
    float u = f * s.dot(h);
    if (u < 0.0f || u > 1.0f) return false;

    Vec3  q = s.cross(tri.e1);
    float v = f * ray.direction.dot(q);
    if (v < 0.0f || u + v > 1.0f) return false;

    float t = f * tri.e2.dot(q);
    if (t < ray.tMin || t > ray.tMax) return false;

    outT = t;
    outU = u;
    outV = v;
    return true;
}

/* ------------------------------------------------------------------ */
/* Ray–AABB slab test                                                 */
/* ------------------------------------------------------------------ */
bool BVH::rayAABB(const Ray& ray, const AABB& box,
                  const Vec3& invDirection,
                  const std::array<uint8_t, 3>& directionSign,
                  float tMin, float tMax,
                  float* outEntryT) {
    for (int i = 0; i < 3; ++i) {
        float t0 = (box.min[i] - ray.origin[i]) * invDirection[i];
        float t1 = (box.max[i] - ray.origin[i]) * invDirection[i];
        if (directionSign[i]) std::swap(t0, t1);
        tMin = t0 > tMin ? t0 : tMin;
        tMax = t1 < tMax ? t1 : tMax;
        if (tMax < tMin) return false;
    }
    if (outEntryT) *outEntryT = tMin;
    return true;
}

/* ------------------------------------------------------------------ */
/* Closest-hit traversal (stack-based, iterative)                     */
/* ------------------------------------------------------------------ */
HitResult BVH::intersect(const Ray& ray) const {
    HitResult result;
    if (nodes_.empty()) return result;

    auto safeReciprocal = [](float v) {
        constexpr float kMinDir = 1e-12f;
        if (std::abs(v) < kMinDir) {
            v = std::signbit(v) ? -kMinDir : kMinDir;
        }
        return 1.0f / v;
    };

    const Vec3 invDirection{
        safeReciprocal(ray.direction.x),
        safeReciprocal(ray.direction.y),
        safeReciprocal(ray.direction.z)
    };
    const std::array<uint8_t, 3> directionSign{
        static_cast<uint8_t>(invDirection.x < 0.0f),
        static_cast<uint8_t>(invDirection.y < 0.0f),
        static_cast<uint8_t>(invDirection.z < 0.0f)
    };

    // Manual stack to avoid recursion
    uint32_t stack[64];
    int stackPtr = 0;
    stack[stackPtr++] = 0; // root

    float closestT = ray.tMax;

    while (stackPtr > 0) {
        uint32_t idx = stack[--stackPtr];
        const BVHNode& node = nodes_[idx];

        if (!rayAABB(ray, node.bounds, invDirection, directionSign, ray.tMin, closestT))
            continue;

        if (node.isLeaf) {
            for (uint32_t i = node.primStart; i < node.primStart + node.primCount; ++i) {
                float t, u, v;
                if (rayTriangle(ray, triangles_[i], t, u, v) && t < closestT) {
                    closestT          = t;
                    result.distance   = t;
                    result.hitPoint   = ray.origin + ray.direction * t;
                    result.normal     = triangles_[i].normal;
                    result.materialID = triangles_[i].materialID;
                    result.geometryID = triangles_[i].geometryID;
                    result.hit        = true;
                }
            }
        } else {
            uint32_t first  = node.leftChild;
            uint32_t second = node.rightChild;
            float firstEntryT = 0.0f;
            float secondEntryT = 0.0f;
            bool firstHit = rayAABB(ray, nodes_[first].bounds, invDirection,
                                     directionSign, ray.tMin, closestT, &firstEntryT);
            bool secondHit = rayAABB(ray, nodes_[second].bounds, invDirection,
                                      directionSign, ray.tMin, closestT, &secondEntryT);

            if (firstHit && secondHit) {
                if (firstEntryT > secondEntryT) std::swap(first, second);

                if (stackPtr < 63) stack[stackPtr++] = second;
                if (stackPtr < 63) stack[stackPtr++] = first;
            } else if (firstHit) {
                if (stackPtr < 63) stack[stackPtr++] = first;
            } else if (secondHit) {
                if (stackPtr < 63) stack[stackPtr++] = second;
            }
        }
    }

    return result;
}

/* ------------------------------------------------------------------ */
/* Any-hit traversal (early-exit)                                     */
/* ------------------------------------------------------------------ */
bool BVH::intersectAny(const Ray& ray) const {
    if (nodes_.empty()) return false;

    auto safeReciprocal = [](float v) {
        constexpr float kMinDir = 1e-12f;
        if (std::abs(v) < kMinDir) {
            v = std::signbit(v) ? -kMinDir : kMinDir;
        }
        return 1.0f / v;
    };

    const Vec3 invDirection{
        safeReciprocal(ray.direction.x),
        safeReciprocal(ray.direction.y),
        safeReciprocal(ray.direction.z)
    };
    const std::array<uint8_t, 3> directionSign{
        static_cast<uint8_t>(invDirection.x < 0.0f),
        static_cast<uint8_t>(invDirection.y < 0.0f),
        static_cast<uint8_t>(invDirection.z < 0.0f)
    };

    uint32_t stack[64];
    int stackPtr = 0;
    stack[stackPtr++] = 0;

    while (stackPtr > 0) {
        uint32_t idx = stack[--stackPtr];
        const BVHNode& node = nodes_[idx];

        if (!rayAABB(ray, node.bounds, invDirection, directionSign, ray.tMin, ray.tMax))
            continue;

        if (node.isLeaf) {
            for (uint32_t i = node.primStart; i < node.primStart + node.primCount; ++i) {
                float t, u, v;
                if (rayTriangle(ray, triangles_[i], t, u, v))
                    return true;
            }
        } else {
            float leftEntryT = 0.0f;
            float rightEntryT = 0.0f;
            bool leftHit = rayAABB(ray, nodes_[node.leftChild].bounds, invDirection,
                                   directionSign, ray.tMin, ray.tMax, &leftEntryT);
            bool rightHit = rayAABB(ray, nodes_[node.rightChild].bounds, invDirection,
                                    directionSign, ray.tMin, ray.tMax, &rightEntryT);

            if (leftHit && rightHit) {
                uint32_t first = node.leftChild;
                uint32_t second = node.rightChild;
                if (leftEntryT > rightEntryT) std::swap(first, second);

                if (stackPtr < 63) stack[stackPtr++] = second;
                if (stackPtr < 63) stack[stackPtr++] = first;
            } else if (leftHit) {
                if (stackPtr < 63) stack[stackPtr++] = node.leftChild;
            } else if (rightHit) {
                if (stackPtr < 63) stack[stackPtr++] = node.rightChild;
            }
        }
    }

    return false;
}

} // namespace magnaundasoni
