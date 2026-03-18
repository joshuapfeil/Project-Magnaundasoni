/**
 * @file SpatialGrid.cpp
 * @brief Sparse spatial hash grid implementation.
 */

#include "core/SpatialGrid.h"

#include <algorithm>
#include <cmath>

namespace magnaundasoni {

SpatialGrid::SpatialGrid(float cellSize)
    : cellSize_(cellSize), invCellSize_(1.0f / cellSize) {}

void SpatialGrid::setCellSize(float size) {
    cellSize_    = size;
    invCellSize_ = 1.0f / size;
    // NOTE: existing objects are NOT re-inserted; caller should rebuild.
}

/* ------------------------------------------------------------------ */
/* Cell coordinate helpers                                            */
/* ------------------------------------------------------------------ */
SpatialGrid::CellCoord SpatialGrid::cellOf(const Vec3& p) const {
    return {static_cast<int32_t>(std::floor(p.x * invCellSize_)),
            static_cast<int32_t>(std::floor(p.y * invCellSize_)),
            static_cast<int32_t>(std::floor(p.z * invCellSize_))};
}

void SpatialGrid::cellRange(const AABB& bounds, CellCoord& lo, CellCoord& hi) const {
    lo = cellOf(bounds.min);
    hi = cellOf(bounds.max);
}

/* ------------------------------------------------------------------ */
/* Insert / Remove / Update                                           */
/* ------------------------------------------------------------------ */
void SpatialGrid::insert(uint32_t id, const AABB& bounds) {
    CellCoord lo, hi;
    cellRange(bounds, lo, hi);

    auto& objCells = objectCells_[id];
    for (int32_t z = lo.z; z <= hi.z; ++z)
        for (int32_t y = lo.y; y <= hi.y; ++y)
            for (int32_t x = lo.x; x <= hi.x; ++x) {
                CellCoord c{x, y, z};
                cells_[c].insert(id);
                objCells.push_back(c);
            }
}

void SpatialGrid::remove(uint32_t id) {
    auto it = objectCells_.find(id);
    if (it == objectCells_.end()) return;

    for (const auto& c : it->second) {
        auto cellIt = cells_.find(c);
        if (cellIt != cells_.end()) {
            cellIt->second.erase(id);
            if (cellIt->second.empty())
                cells_.erase(cellIt);
        }
    }
    objectCells_.erase(it);
}

void SpatialGrid::update(uint32_t id, const AABB& newBounds) {
    remove(id);
    insert(id, newBounds);
}

/* ------------------------------------------------------------------ */
/* Queries                                                            */
/* ------------------------------------------------------------------ */
std::vector<uint32_t> SpatialGrid::query(const AABB& region) const {
    CellCoord lo, hi;
    cellRange(region, lo, hi);

    std::unordered_set<uint32_t> found;
    for (int32_t z = lo.z; z <= hi.z; ++z)
        for (int32_t y = lo.y; y <= hi.y; ++y)
            for (int32_t x = lo.x; x <= hi.x; ++x) {
                auto it = cells_.find({x, y, z});
                if (it != cells_.end())
                    found.insert(it->second.begin(), it->second.end());
            }

    return {found.begin(), found.end()};
}

std::vector<uint32_t> SpatialGrid::queryRay(const Ray& ray, float maxDist) const {
    // 3-D DDA (Amanatides & Woo) through the grid
    std::unordered_set<uint32_t> found;

    Vec3 invDir{1.0f / (std::abs(ray.direction.x) > 1e-12f ? ray.direction.x : 1e-12f),
                1.0f / (std::abs(ray.direction.y) > 1e-12f ? ray.direction.y : 1e-12f),
                1.0f / (std::abs(ray.direction.z) > 1e-12f ? ray.direction.z : 1e-12f)};

    CellCoord cell = cellOf(ray.origin);

    int32_t step[3];
    float tDelta[3], tMax_[3];

    for (int i = 0; i < 3; ++i) {
        float dir_i = ray.direction[i];
        if (dir_i > 0.0f) {
            step[i]   = 1;
            float edge = ((&cell.x)[i] + 1) * cellSize_;
            tMax_[i]   = (edge - ray.origin[i]) * invDir[i];
        } else if (dir_i < 0.0f) {
            step[i]   = -1;
            float edge = (&cell.x)[i] * cellSize_;
            tMax_[i]   = (edge - ray.origin[i]) * invDir[i];
        } else {
            step[i]  = 0;
            tMax_[i] = std::numeric_limits<float>::max();
        }
        tDelta[i] = std::abs(cellSize_ * invDir[i]);
    }

    float t = 0.0f;
    int maxSteps = static_cast<int>(maxDist * invCellSize_) + 3;
    for (int s = 0; s < maxSteps && t <= maxDist; ++s) {
        auto it = cells_.find(cell);
        if (it != cells_.end())
            found.insert(it->second.begin(), it->second.end());

        // Advance along the axis with the smallest tMax
        if (tMax_[0] < tMax_[1] && tMax_[0] < tMax_[2]) {
            t = tMax_[0]; cell.x += step[0]; tMax_[0] += tDelta[0];
        } else if (tMax_[1] < tMax_[2]) {
            t = tMax_[1]; cell.y += step[1]; tMax_[1] += tDelta[1];
        } else {
            t = tMax_[2]; cell.z += step[2]; tMax_[2] += tDelta[2];
        }
    }

    return {found.begin(), found.end()};
}

void SpatialGrid::clear() {
    cells_.clear();
    objectCells_.clear();
}

} // namespace magnaundasoni
