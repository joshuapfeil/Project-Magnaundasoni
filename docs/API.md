# Magnaundasoni – API Reference

> **Version**: 1.0
> **Header**: `Magnaundasoni.h`

This is the primary API reference for the Magnaundasoni real-time acoustics
runtime.  For early design notes and architecture context, see
[`docs/design/api.md`](design/api.md).

---

## Quick Start

```c
#include "Magnaundasoni.h"

int main(void)
{
    /* 1. Create engine with sensible defaults */
    MagEngineConfig cfg;
    mag_engine_config_defaults(&cfg);
    /* Override only what you need: */
    cfg.quality    = MAG_QUALITY_HIGH;
    cfg.maxSources = 32;

    MagEngine engine = NULL;
    MagStatus status = mag_engine_create(&cfg, &engine);
    if (status != MAG_OK) return 1;

    /* 2. Register a material from a built-in preset */
    MagMaterialDesc matDesc;
    mag_material_get_preset("Wood", &matDesc);
    MagMaterialID matID = 0;
    mag_material_register(engine, &matDesc, &matID);

    /* 3. Register geometry (simplified example) */
    float verts[] = { /* ... x,y,z interleaved triangle vertices ... */ };
    uint32_t indices[] = { /* ... triangle indices ... */ };
    MagGeometryDesc geo = {0};
    geo.vertices     = verts;
    geo.vertexCount  = 8;
    geo.indices      = indices;
    geo.indexCount    = 12;
    geo.materialID   = matID;
    geo.dynamicFlag  = 0;  /* static */

    MagGeometryID geoID = 0;
    mag_geometry_register(engine, &geo, &geoID);

    /* 4. Register source and listener */
    MagSourceDesc src = {0};
    src.position[0] = 2.0f; src.position[1] = 1.5f; src.position[2] = 3.0f;
    src.direction[0] = 1.0f;
    src.radius     = 0.1f;
    src.importance = 2;  /* high */

    MagSourceID srcID = 0;
    mag_source_register(engine, &src, &srcID);

    MagListenerDesc lis = {0};
    lis.position[0] = 8.0f; lis.position[1] = 1.5f; lis.position[2] = 3.0f;
    lis.forward[0]  = -1.0f;
    lis.up[1]       = 1.0f;

    MagListenerID lisID = 0;
    mag_listener_register(engine, &lis, &lisID);

    /* 5. Run simulation */
    mag_update(engine, 0.016f);

    /* 6. Query results */
    MagAcousticResult result = {0};
    mag_get_acoustic_result(engine, srcID, lisID, &result);
    /* Use result.direct, result.reflections, result.lateField, etc. */

    /* 7. Shut down */
    mag_engine_destroy(engine);
    return 0;
}
```

---

## Function Reference

### Engine Lifecycle

| Function | Signature | Description |
|----------|-----------|-------------|
| `mag_engine_config_defaults` | `MagStatus mag_engine_config_defaults(MagEngineConfig* outConfig)` | Populate a config with sensible defaults. Call first, then override only the fields you need. |
| `mag_engine_create` | `MagStatus mag_engine_create(const MagEngineConfig* config, MagEngine* outEngine)` | Create an engine instance. |
| `mag_engine_destroy` | `MagStatus mag_engine_destroy(MagEngine engine)` | Destroy the engine and release all resources. |

### Material Management

| Function | Signature | Description |
|----------|-----------|-------------|
| `mag_material_register` | `MagStatus mag_material_register(MagEngine engine, const MagMaterialDesc* desc, MagMaterialID* outID)` | Register a material definition. |
| `mag_material_get_preset` | `MagStatus mag_material_get_preset(const char* presetName, MagMaterialDesc* outDesc)` | Retrieve a built-in material preset (e.g., "Wood", "Metal"). |

### Scene Management – Geometry

| Function | Signature | Description |
|----------|-----------|-------------|
| `mag_geometry_register` | `MagStatus mag_geometry_register(MagEngine engine, const MagGeometryDesc* desc, MagGeometryID* outID)` | Register a geometry chunk. |
| `mag_geometry_unregister` | `MagStatus mag_geometry_unregister(MagEngine engine, MagGeometryID id)` | Remove a geometry chunk. |
| `mag_geometry_update_transform` | `MagStatus mag_geometry_update_transform(MagEngine engine, MagGeometryID id, const float* transform4x4)` | Update a geometry's world transform (16 floats, column-major). |

### Scene Management – Sources & Listeners

| Function | Signature | Description |
|----------|-----------|-------------|
| `mag_source_register` | `MagStatus mag_source_register(MagEngine engine, const MagSourceDesc* desc, MagSourceID* outID)` | Register an acoustic source. |
| `mag_source_unregister` | `MagStatus mag_source_unregister(MagEngine engine, MagSourceID id)` | Remove a source. |
| `mag_source_update` | `MagStatus mag_source_update(MagEngine engine, MagSourceID id, const MagSourceDesc* desc)` | Update source position/orientation. |
| `mag_listener_register` | `MagStatus mag_listener_register(MagEngine engine, const MagListenerDesc* desc, MagListenerID* outID)` | Register a listener. |
| `mag_listener_unregister` | `MagStatus mag_listener_unregister(MagEngine engine, MagListenerID id)` | Remove a listener. |
| `mag_listener_update` | `MagStatus mag_listener_update(MagEngine engine, MagListenerID id, const MagListenerDesc* desc)` | Update listener position/orientation. |
| `mag_set_listener_head_pose` | `MagStatus mag_set_listener_head_pose(MagEngine engine, uint32_t listenerID, const float quaternion[4])` | Store a listener's head-tracked `(x, y, z, w)` quaternion without overwriting the listener basis last set via `mag_listener_register` / `mag_listener_update`. |

### Spatialisation

| Function | Signature | Description |
|----------|-----------|-------------|
| `mag_set_spatial_config` | `MagStatus mag_set_spatial_config(MagEngine engine, const MagSpatialConfig* config)` | Configure built-in spatial routing (auto, binaural, passthrough, or surround presets). Invalid speaker layout presets are rejected, and surround modes resolve to the matching surround layout preset. |
| `mag_get_spatial_config` | `MagStatus mag_get_spatial_config(MagEngine engine, MagSpatialConfig* outConfig)` | Query the active spatialisation configuration. |
| `mag_set_hrtf_dataset` | `MagStatus mag_set_hrtf_dataset(MagEngine engine, const void* sofaData, size_t sofaSize)` | Provide a runtime HRTF dataset blob for the built-in binaural renderer. |
| `mag_set_hrtf_preset` | `MagStatus mag_set_hrtf_preset(MagEngine engine, MagHRTFPreset preset)` | Revert to a built-in HRTF preset. |
| `mag_set_speaker_layout` | `MagStatus mag_set_speaker_layout(MagEngine engine, const MagSpeakerLayout* layout)` | Override the current speaker layout with an explicit channel map. For the built-in default `x.1` presets, channel index `3` is reserved as the non-directional LFE channel and is excluded from directional surround panning. |
| `mag_get_spatial_backend_info` | `MagStatus mag_get_spatial_backend_info(MagEngine engine, MagSpatialBackendInfo* outInfo)` | Report which built-in or platform spatial backend the current configuration resolves to. |

### Simulation

| Function | Signature | Description |
|----------|-----------|-------------|
| `mag_update` | `MagStatus mag_update(MagEngine engine, float deltaTime)` | Advance simulation by one frame. |

### Query Acoustic State

| Function | Signature | Description |
|----------|-----------|-------------|
| `mag_get_acoustic_result` | `MagStatus mag_get_acoustic_result(MagEngine engine, MagSourceID sourceID, MagListenerID listenerID, MagAcousticResult* outResult)` | Get full acoustic result for a source-listener pair. |
| `mag_get_global_state` | `MagStatus mag_get_global_state(MagEngine engine, MagGlobalState* outState)` | Get per-frame engine statistics. |

### Quality Settings

| Function | Signature | Description |
|----------|-----------|-------------|
| `mag_set_quality` | `MagStatus mag_set_quality(MagEngine engine, MagQualityLevel level)` | Set quality preset (Low, Medium, High, Ultra). |

### Debug & Profiling

| Function | Signature | Description |
|----------|-----------|-------------|
| `mag_debug_get_ray_count` | `MagStatus mag_debug_get_ray_count(MagEngine engine, uint32_t* outCount)` | Get the number of rays cast in the last tick. |
| `mag_debug_get_active_edges` | `MagStatus mag_debug_get_active_edges(MagEngine engine, uint32_t* outCount)` | Get the number of active diffraction edges. |

---

## Error Codes

| Code | Name | Description |
|------|------|-------------|
| 0    | `MAG_OK` | Success |
| -1   | `MAG_ERROR` | General error |
| -2   | `MAG_INVALID_PARAM` | A parameter is null or out of range |
| -3   | `MAG_OUT_OF_MEMORY` | Allocation failed |
| -4   | `MAG_NOT_INITIALIZED` | Engine not initialized |

---

## Thread Safety Summary

| Function Group | Main Thread | Audio Thread | Worker Thread |
|----------------|:-----------:|:------------:|:-------------:|
| `mag_engine_create` / `mag_engine_destroy` | ✅ | ❌ | ❌ |
| `mag_*_register` / `mag_*_update` | ✅ | ❌ | ❌ |
| `mag_update` | ✅ | ❌ | ❌ |
| `mag_get_acoustic_result` | ✅ | ❌ | ❌ |
| `mag_get_global_state` | ✅ | ❌ | ❌ |
| `mag_debug_*` | ✅ | ❌ | ❌ |

All query functions (`mag_get_*`) are thread-safe when used from the main
thread, but are **not guaranteed to be lock-free or real-time safe**. In
particular, `mag_get_acoustic_result` and `mag_get_global_state` may take
internal locks and **must not** be called from the audio callback.

---

## Data Types Reference

For complete struct definitions, see the public header
[`native/include/Magnaundasoni.h`](../native/include/Magnaundasoni.h) and the
design notes in [`docs/design/api.md`](design/api.md).

---

## See Also

- [`docs/design/api.md`](design/api.md) – Full C ABI with struct definitions
- [`docs/design/architecture.md`](design/architecture.md) – System architecture
- [`docs/Integration-Unity.md`](Integration-Unity.md) – Unity integration guide
- [`docs/Integration-Unreal.md`](Integration-Unreal.md) – Unreal integration guide
- [`docs/Material-Presets.md`](Material-Presets.md) – Material preset reference
- [`docs/Performance-Tuning.md`](Performance-Tuning.md) – Performance tuning guide
