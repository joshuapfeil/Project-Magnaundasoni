/**
 * @file Scene.cpp
 * @brief Scene implementation – thread-safe world model.
 */

#include "core/Scene.h"
#include <algorithm>

namespace magnaundasoni {

Scene::Scene() = default;

/* ------------------------------------------------------------------ */
/* Materials                                                          */
/* ------------------------------------------------------------------ */
uint32_t Scene::registerMaterial(const MaterialEntry& mat) {
    uint32_t id = nextMaterialID_.fetch_add(1, std::memory_order_relaxed);
    std::unique_lock lock(mutex_);
    materials_[id] = mat;
    return id;
}

const MaterialEntry* Scene::getMaterial(uint32_t id) const {
    std::shared_lock lock(mutex_);
    auto it = materials_.find(id);
    return (it != materials_.end()) ? &it->second : nullptr;
}

/* ------------------------------------------------------------------ */
/* Geometry                                                           */
/* ------------------------------------------------------------------ */
uint32_t Scene::registerGeometry(const GeometryEntry& geo) {
    uint32_t id = nextGeometryID_.fetch_add(1, std::memory_order_relaxed);
    std::unique_lock lock(mutex_);
    GeometryEntry& entry = geometries_[id];
    entry = geo;

    // Compute AABB from vertices
    entry.bounds = AABB{};
    for (uint32_t i = 0; i < static_cast<uint32_t>(entry.vertices.size()) / 3; ++i) {
        Vec3 v{entry.vertices[i * 3 + 0],
                entry.vertices[i * 3 + 1],
                entry.vertices[i * 3 + 2]};
        entry.bounds.expand(v);
    }
    return id;
}

bool Scene::unregisterGeometry(uint32_t id) {
    std::unique_lock lock(mutex_);
    return geometries_.erase(id) > 0;
}

bool Scene::updateTransform(uint32_t id, const Mat4x4& xform) {
    std::unique_lock lock(mutex_);
    auto it = geometries_.find(id);
    if (it == geometries_.end()) return false;

    it->second.transform = xform;

    // Recompute world-space AABB by transforming all vertices
    AABB newBounds;
    const auto& verts = it->second.vertices;
    for (uint32_t i = 0; i < static_cast<uint32_t>(verts.size()) / 3; ++i) {
        Vec3 local{verts[i * 3 + 0], verts[i * 3 + 1], verts[i * 3 + 2]};
        Vec3 world = xform.transformPoint(local);
        newBounds.expand(world);
    }
    it->second.bounds = newBounds;
    return true;
}

const GeometryEntry* Scene::getGeometry(uint32_t id) const {
    std::shared_lock lock(mutex_);
    auto it = geometries_.find(id);
    return (it != geometries_.end()) ? &it->second : nullptr;
}

/* ------------------------------------------------------------------ */
/* Sources                                                            */
/* ------------------------------------------------------------------ */
uint32_t Scene::registerSource(const SourceEntry& src) {
    uint32_t id = nextSourceID_.fetch_add(1, std::memory_order_relaxed);
    std::unique_lock lock(mutex_);
    sources_[id] = src;
    return id;
}

bool Scene::unregisterSource(uint32_t id) {
    std::unique_lock lock(mutex_);
    return sources_.erase(id) > 0;
}

bool Scene::updateSource(uint32_t id, const SourceEntry& src) {
    std::unique_lock lock(mutex_);
    auto it = sources_.find(id);
    if (it == sources_.end()) return false;
    it->second = src;
    return true;
}

const SourceEntry* Scene::getSource(uint32_t id) const {
    std::shared_lock lock(mutex_);
    auto it = sources_.find(id);
    return (it != sources_.end()) ? &it->second : nullptr;
}

/* ------------------------------------------------------------------ */
/* Listeners                                                          */
/* ------------------------------------------------------------------ */
uint32_t Scene::registerListener(const ListenerEntry& lis) {
    uint32_t id = nextListenerID_.fetch_add(1, std::memory_order_relaxed);
    std::unique_lock lock(mutex_);
    listeners_[id] = lis;
    return id;
}

bool Scene::unregisterListener(uint32_t id) {
    std::unique_lock lock(mutex_);
    return listeners_.erase(id) > 0;
}

bool Scene::updateListener(uint32_t id, const ListenerEntry& lis) {
    std::unique_lock lock(mutex_);
    auto it = listeners_.find(id);
    if (it == listeners_.end()) return false;
    it->second = lis;
    return true;
}

const ListenerEntry* Scene::getListener(uint32_t id) const {
    std::shared_lock lock(mutex_);
    auto it = listeners_.find(id);
    return (it != listeners_.end()) ? &it->second : nullptr;
}

/* ------------------------------------------------------------------ */
/* Bulk accessors                                                     */
/* ------------------------------------------------------------------ */
std::vector<uint32_t> Scene::getActiveSourceIDs() const {
    std::shared_lock lock(mutex_);
    std::vector<uint32_t> ids;
    ids.reserve(sources_.size());
    for (const auto& [id, s] : sources_) {
        if (s.active) ids.push_back(id);
    }
    return ids;
}

std::vector<uint32_t> Scene::getActiveListenerIDs() const {
    std::shared_lock lock(mutex_);
    std::vector<uint32_t> ids;
    ids.reserve(listeners_.size());
    for (const auto& [id, l] : listeners_) {
        if (l.active) ids.push_back(id);
    }
    return ids;
}

std::vector<uint32_t> Scene::getAllGeometryIDs() const {
    std::shared_lock lock(mutex_);
    std::vector<uint32_t> ids;
    ids.reserve(geometries_.size());
    for (const auto& [id, g] : geometries_) {
        ids.push_back(id);
    }
    return ids;
}

uint32_t Scene::getActiveSourceCount() const {
    std::shared_lock lock(mutex_);
    uint32_t count = 0;
    for (const auto& [id, s] : sources_) {
        if (s.active) ++count;
    }
    return count;
}

} // namespace magnaundasoni
