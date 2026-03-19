/**
 * @file SpatialGrid.h
 * @brief Sparse spatial hash grid for broadphase queries on dynamic objects.
 */

#ifndef MAGNAUNDASONI_CORE_SPATIAL_GRID_H
#define MAGNAUNDASONI_CORE_SPATIAL_GRID_H

#include "Types.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace magnaundasoni {

/* ------------------------------------------------------------------ */
/* SpatialGrid                                                        */
/* ------------------------------------------------------------------ */
class SpatialGrid {
public:
    explicit SpatialGrid(float cellSize = 8.0f);

    void  setCellSize(float size);
    float getCellSize() const { return cellSize_; }

    void insert(uint32_t id, const AABB& bounds);
    void remove(uint32_t id);
    void update(uint32_t id, const AABB& newBounds);

    /** Return IDs of objects whose cells overlap the query box. */
    std::vector<uint32_t> query(const AABB& region) const;

    /** Return IDs of objects whose cells are traversed by the ray. */
    std::vector<uint32_t> queryRay(const Ray& ray, float maxDist = 1000.0f) const;

    void     clear();
    uint32_t objectCount() const { return static_cast<uint32_t>(objectCells_.size()); }

private:
    struct CellCoord {
        int32_t x, y, z;
        bool operator==(const CellCoord& o) const { return x == o.x && y == o.y && z == o.z; }
    };

    struct CellHash {
        size_t operator()(const CellCoord& c) const {
            // FNV-1a inspired combine
            size_t h = 2166136261u;
            h ^= static_cast<size_t>(c.x); h *= 16777619u;
            h ^= static_cast<size_t>(c.y); h *= 16777619u;
            h ^= static_cast<size_t>(c.z); h *= 16777619u;
            return h;
        }
    };

    CellCoord cellOf(const Vec3& p) const;
    void      cellRange(const AABB& bounds, CellCoord& lo, CellCoord& hi) const;

    float cellSize_;
    float invCellSize_;

    // cell -> set of object IDs
    std::unordered_map<CellCoord, std::unordered_set<uint32_t>, CellHash> cells_;

    // object -> list of cells it occupies (for fast removal)
    std::unordered_map<uint32_t, std::vector<CellCoord>> objectCells_;
};

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_CORE_SPATIAL_GRID_H
