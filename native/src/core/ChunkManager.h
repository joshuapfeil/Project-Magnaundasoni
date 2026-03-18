/**
 * @file ChunkManager.h
 * @brief World-chunk streaming manager with adaptive fidelity zones.
 */

#ifndef MAGNAUNDASONI_CORE_CHUNK_MANAGER_H
#define MAGNAUNDASONI_CORE_CHUNK_MANAGER_H

#include "BVH.h"
#include "Types.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace magnaundasoni {

/* ------------------------------------------------------------------ */
/* ChunkID                                                            */
/* ------------------------------------------------------------------ */
struct ChunkID {
    int32_t x = 0, y = 0, z = 0;
    bool operator==(const ChunkID& o) const { return x == o.x && y == o.y && z == o.z; }
};

struct ChunkIDHash {
    size_t operator()(const ChunkID& c) const {
        size_t h = 2166136261u;
        h ^= static_cast<size_t>(c.x); h *= 16777619u;
        h ^= static_cast<size_t>(c.y); h *= 16777619u;
        h ^= static_cast<size_t>(c.z); h *= 16777619u;
        return h;
    }
};

/* ------------------------------------------------------------------ */
/* ChunkFidelity                                                      */
/* ------------------------------------------------------------------ */
enum class ChunkFidelity : uint32_t {
    Detailed    = 0,
    Reduced     = 1,
    Summarized  = 2,
    Inactive    = 3
};

/* ------------------------------------------------------------------ */
/* Chunk                                                              */
/* ------------------------------------------------------------------ */
struct Chunk {
    BVH                   bvh;
    ChunkFidelity         fidelity = ChunkFidelity::Inactive;
    AABB                  bounds;
    std::vector<uint32_t> geometryIDs;
    double                lastAccessTime = 0.0;
};

/* ------------------------------------------------------------------ */
/* ChunkManager                                                       */
/* ------------------------------------------------------------------ */
class ChunkManager {
public:
    explicit ChunkManager(float chunkSize = 32.0f);

    void setFidelityRadii(float detailed, float reduced, float summarized);

    void activateChunk(const ChunkID& id, ChunkFidelity fidelity);
    void deactivateChunk(const ChunkID& id);

    /** Update fidelity zones around the listener. */
    void updateFidelityZones(const Vec3& listenerPos);

    /** Ordered list of chunks traversed by a ray. */
    std::vector<ChunkID> getChunksForRay(const Ray& ray, float maxDist = 500.0f) const;

    /** Assign triangles to the chunk and build its BVH. */
    void buildChunkBVH(const ChunkID& id, const std::vector<Triangle>& triangles);

    /** Get a chunk (or nullptr). */
    const Chunk* getChunk(const ChunkID& id) const;
    Chunk*       getChunkMut(const ChunkID& id);

    /** Convert world position to ChunkID. */
    ChunkID chunkOf(const Vec3& pos) const;

    uint32_t getActiveChunkCount() const;
    size_t   getMemoryUsage() const;

private:
    float chunkSize_;
    float invChunkSize_;

    float detailedRadius_    = 48.0f;
    float reducedRadius_     = 96.0f;
    float summarizedRadius_  = 192.0f;

    mutable std::mutex mutex_;
    std::unordered_map<ChunkID, Chunk, ChunkIDHash> chunks_;
};

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_CORE_CHUNK_MANAGER_H
