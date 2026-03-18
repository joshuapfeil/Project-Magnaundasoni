/**
 * @file ChunkManager.cpp
 * @brief World-chunk streaming and fidelity-zone management.
 */

#include "core/ChunkManager.h"

#include <algorithm>
#include <cmath>

namespace magnaundasoni {

ChunkManager::ChunkManager(float chunkSize)
    : chunkSize_(chunkSize), invChunkSize_(1.0f / chunkSize) {}

void ChunkManager::setFidelityRadii(float detailed, float reduced, float summarized) {
    detailedRadius_   = detailed;
    reducedRadius_    = reduced;
    summarizedRadius_ = summarized;
}

/* ------------------------------------------------------------------ */
/* Chunk coordinate                                                   */
/* ------------------------------------------------------------------ */
ChunkID ChunkManager::chunkOf(const Vec3& pos) const {
    return {static_cast<int32_t>(std::floor(pos.x * invChunkSize_)),
            static_cast<int32_t>(std::floor(pos.y * invChunkSize_)),
            static_cast<int32_t>(std::floor(pos.z * invChunkSize_))};
}

/* ------------------------------------------------------------------ */
/* Activate / Deactivate                                              */
/* ------------------------------------------------------------------ */
void ChunkManager::activateChunk(const ChunkID& id, ChunkFidelity fidelity) {
    std::lock_guard lock(mutex_);
    auto& chunk = chunks_[id];
    chunk.fidelity = fidelity;
    chunk.bounds   = AABB(
        Vec3(id.x * chunkSize_, id.y * chunkSize_, id.z * chunkSize_),
        Vec3((id.x + 1) * chunkSize_, (id.y + 1) * chunkSize_, (id.z + 1) * chunkSize_));
}

void ChunkManager::deactivateChunk(const ChunkID& id) {
    std::lock_guard lock(mutex_);
    chunks_.erase(id);
}

/* ------------------------------------------------------------------ */
/* Fidelity zones                                                     */
/* ------------------------------------------------------------------ */
void ChunkManager::updateFidelityZones(const Vec3& listenerPos) {
    std::lock_guard lock(mutex_);

    float detSq   = detailedRadius_   * detailedRadius_;
    float redSq   = reducedRadius_    * reducedRadius_;
    float sumSq   = summarizedRadius_ * summarizedRadius_;

    for (auto& [id, chunk] : chunks_) {
        Vec3 center = chunk.bounds.center();
        float distSq = (center - listenerPos).lengthSq();

        if (distSq <= detSq)
            chunk.fidelity = ChunkFidelity::Detailed;
        else if (distSq <= redSq)
            chunk.fidelity = ChunkFidelity::Reduced;
        else if (distSq <= sumSq)
            chunk.fidelity = ChunkFidelity::Summarized;
        else
            chunk.fidelity = ChunkFidelity::Inactive;
    }
}

/* ------------------------------------------------------------------ */
/* Ray → chunk list (3-D DDA)                                         */
/* ------------------------------------------------------------------ */
std::vector<ChunkID> ChunkManager::getChunksForRay(const Ray& ray, float maxDist) const {
    std::vector<ChunkID> result;

    Vec3 invDir;
    for (int i = 0; i < 3; ++i)
        invDir[i] = 1.0f / (std::abs(ray.direction[i]) > 1e-12f ? ray.direction[i] : 1e-12f);

    ChunkID cell = chunkOf(ray.origin);

    int32_t step[3];
    float tDelta[3], tMax_[3];

    for (int i = 0; i < 3; ++i) {
        float d = ray.direction[i];
        if (d > 0.0f) {
            step[i] = 1;
            float edge = ((&cell.x)[i] + 1) * chunkSize_;
            tMax_[i] = (edge - ray.origin[i]) * invDir[i];
        } else if (d < 0.0f) {
            step[i] = -1;
            float edge = (&cell.x)[i] * chunkSize_;
            tMax_[i] = (edge - ray.origin[i]) * invDir[i];
        } else {
            step[i]  = 0;
            tMax_[i] = std::numeric_limits<float>::max();
        }
        tDelta[i] = std::abs(chunkSize_ * invDir[i]);
    }

    float t = 0.0f;
    int maxSteps = static_cast<int>(maxDist * invChunkSize_) + 3;
    for (int s = 0; s < maxSteps && t <= maxDist; ++s) {
        result.push_back(cell);

        if (tMax_[0] < tMax_[1] && tMax_[0] < tMax_[2]) {
            t = tMax_[0]; cell.x += step[0]; tMax_[0] += tDelta[0];
        } else if (tMax_[1] < tMax_[2]) {
            t = tMax_[1]; cell.y += step[1]; tMax_[1] += tDelta[1];
        } else {
            t = tMax_[2]; cell.z += step[2]; tMax_[2] += tDelta[2];
        }
    }

    return result;
}

/* ------------------------------------------------------------------ */
/* BVH for a chunk                                                    */
/* ------------------------------------------------------------------ */
void ChunkManager::buildChunkBVH(const ChunkID& id,
                                  const std::vector<Triangle>& triangles) {
    std::lock_guard lock(mutex_);
    auto it = chunks_.find(id);
    if (it == chunks_.end()) return;
    it->second.bvh.build(triangles);
}

const Chunk* ChunkManager::getChunk(const ChunkID& id) const {
    std::lock_guard lock(mutex_);
    auto it = chunks_.find(id);
    return (it != chunks_.end()) ? &it->second : nullptr;
}

Chunk* ChunkManager::getChunkMut(const ChunkID& id) {
    std::lock_guard lock(mutex_);
    auto it = chunks_.find(id);
    return (it != chunks_.end()) ? &it->second : nullptr;
}

/* ------------------------------------------------------------------ */
/* Stats                                                              */
/* ------------------------------------------------------------------ */
uint32_t ChunkManager::getActiveChunkCount() const {
    std::lock_guard lock(mutex_);
    uint32_t count = 0;
    for (const auto& [id, c] : chunks_) {
        if (c.fidelity != ChunkFidelity::Inactive) ++count;
    }
    return count;
}

size_t ChunkManager::getMemoryUsage() const {
    std::lock_guard lock(mutex_);
    size_t bytes = sizeof(*this);
    for (const auto& [id, c] : chunks_) {
        bytes += sizeof(c);
        bytes += c.geometryIDs.capacity() * sizeof(uint32_t);
        bytes += c.bvh.triangleCount() * sizeof(Triangle);
    }
    return bytes;
}

} // namespace magnaundasoni
