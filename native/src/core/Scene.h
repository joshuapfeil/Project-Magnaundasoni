/**
 * @file Scene.h
 * @brief Central world-model that owns all geometry, materials, sources, and listeners.
 */

#ifndef MAGNAUNDASONI_CORE_SCENE_H
#define MAGNAUNDASONI_CORE_SCENE_H

#include "Types.h"
#include "Magnaundasoni.h"

#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace magnaundasoni {

/* ------------------------------------------------------------------ */
/* Internal entry structs                                             */
/* ------------------------------------------------------------------ */
struct MaterialEntry {
    BandArray   absorption{};
    BandArray   transmission{};
    BandArray   scattering{};
    float       roughness      = 0.5f;
    uint32_t    thicknessClass = 1;
    float       leakageHint    = 0.0f;
    std::string categoryTag;
};

struct GeometryEntry {
    std::vector<float>    vertices;   // x,y,z interleaved
    std::vector<uint32_t> indices;
    uint32_t              materialID  = 0;
    Mat4x4                transform   = Mat4x4::identity();
    AABB                  bounds;
    Importance            importance  = Importance::Static;
    int32_t               chunkID     = -1;
    bool                  active      = true;
};

struct SourceEntry {
    Vec3     position;
    Vec3     direction{0, 0, 1};
    float    radius     = 0.0f;
    uint32_t importance = 1;
    bool     active     = true;
};

struct ListenerEntry {
    Vec3 position;
    Vec3 forward{0, 0, 1};
    Vec3 up{0, 1, 0};
    bool active = true;
};

/* ------------------------------------------------------------------ */
/* Scene                                                              */
/* ------------------------------------------------------------------ */
class Scene {
public:
    Scene();
    ~Scene() = default;

    uint64_t geometryRevision() const {
        return geometryRevision_.load(std::memory_order_relaxed);
    }

    // Materials
    uint32_t registerMaterial(const MaterialEntry& mat);
    const MaterialEntry* getMaterial(uint32_t id) const;

    // Geometry
    uint32_t registerGeometry(const GeometryEntry& geo);
    bool     unregisterGeometry(uint32_t id);
    bool     updateTransform(uint32_t id, const Mat4x4& xform);
    const GeometryEntry* getGeometry(uint32_t id) const;

    // Sources
    uint32_t registerSource(const SourceEntry& src);
    bool     unregisterSource(uint32_t id);
    bool     updateSource(uint32_t id, const SourceEntry& src);
    const SourceEntry* getSource(uint32_t id) const;

    // Listeners
    uint32_t registerListener(const ListenerEntry& lis);
    bool     unregisterListener(uint32_t id);
    bool     updateListener(uint32_t id, const ListenerEntry& lis);
    const ListenerEntry* getListener(uint32_t id) const;

    // Bulk accessors
    std::vector<uint32_t> getActiveSourceIDs() const;
    std::vector<uint32_t> getActiveListenerIDs() const;
    std::vector<uint32_t> getAllGeometryIDs() const;

    uint32_t getActiveSourceCount() const;

private:
    mutable std::shared_mutex mutex_;

    std::atomic<uint32_t> nextMaterialID_{1};
    std::atomic<uint32_t> nextGeometryID_{1};
    std::atomic<uint32_t> nextSourceID_{1};
    std::atomic<uint32_t> nextListenerID_{1};
    std::atomic<uint64_t> geometryRevision_{1};

    std::unordered_map<uint32_t, MaterialEntry>  materials_;
    std::unordered_map<uint32_t, GeometryEntry>  geometries_;
    std::unordered_map<uint32_t, SourceEntry>    sources_;
    std::unordered_map<uint32_t, ListenerEntry>  listeners_;
};

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_CORE_SCENE_H
