# Magnaundasoni – API Reference

> **Version**: 1.0
> **Header**: `magn_api.h`

This is the primary API reference for the Magnaundasoni real-time acoustics
runtime.  For implementation details and architecture context, see
[`docs/design/api.md`](design/api.md).

---

## Quick Start

```c
#include "magn_api.h"

int main(void)
{
    /* 1. Initialize with defaults */
    MagnResult result = magn_init(NULL);
    if (result != MAGN_OK) return 1;

    /* 2. Register a material */
    magn_set_material_from_preset(1, "Wood");

    /* 3. Register geometry (simplified example) */
    float verts[] = { /* ... triangle vertices ... */ };
    uint32_t indices[] = { /* ... triangle indices ... */ };
    uint32_t matIDs[] = { /* ... per-triangle material IDs ... */ };
    MagnGeometryDesc geo = {
        .structSize = sizeof(MagnGeometryDesc),
        .vertices = verts,
        .vertexCount = 8,
        .indices = indices,
        .indexCount = 12,
        .materialIDs = matIDs,
        .bounds = { .min = {0,0,0}, .max = {10,5,10} },
        .edgeCache = NULL
    };
    magn_register_geometry(1, &geo);

    /* 4. Register source and listener */
    MagnSourceDesc src = {
        .structSize = sizeof(MagnSourceDesc),
        .position = {2.0f, 1.5f, 3.0f},
        .forward = {1, 0, 0},
        .up = {0, 1, 0},
        .directivityPattern = 0,
        .priority = 1.0f,
        .active = 1
    };
    magn_register_source(1, &src);

    MagnListenerDesc lis = {
        .structSize = sizeof(MagnListenerDesc),
        .position = {8.0f, 1.5f, 3.0f},
        .forward = {-1, 0, 0},
        .up = {0, 1, 0}
    };
    magn_register_listener(1, &lis);

    /* 5. Run simulation */
    magn_tick(0.016f);

    /* 6. Query results */
    const MagnAcousticState* state = NULL;
    magn_get_acoustic_state(1, &state);

    /* Use state->directComponents, state->earlyReflections, etc. */

    /* 7. Shut down */
    magn_shutdown();
    return 0;
}
```

---

## Function Reference

### Engine Lifecycle

| Function | Signature | Description |
|----------|-----------|-------------|
| `magn_init` | `MagnResult magn_init(const MagnInitConfig* config)` | Initialize the runtime. Pass `NULL` for defaults. |
| `magn_shutdown` | `MagnResult magn_shutdown(void)` | Shut down and release all resources. |

### Scene Management – Geometry

| Function | Signature | Description |
|----------|-----------|-------------|
| `magn_register_geometry` | `MagnResult magn_register_geometry(uint64_t chunkID, const MagnGeometryDesc* desc)` | Register a static geometry chunk. |
| `magn_unregister_geometry` | `MagnResult magn_unregister_geometry(uint64_t chunkID)` | Remove a geometry chunk. |
| `magn_register_dynamic_object` | `MagnResult magn_register_dynamic_object(uint32_t objectID, const MagnDynamicObjectDesc* desc)` | Register a dynamic object with proxy geometry. |
| `magn_update_dynamic_object` | `MagnResult magn_update_dynamic_object(uint32_t objectID, const MagnTransform* transform)` | Update a dynamic object's transform. |
| `magn_deregister_dynamic_object` | `MagnResult magn_deregister_dynamic_object(uint32_t objectID)` | Remove a dynamic object. |

### Scene Management – Materials

| Function | Signature | Description |
|----------|-----------|-------------|
| `magn_set_material` | `MagnResult magn_set_material(uint32_t materialID, const MagnMaterial* material)` | Register or update a material definition. |
| `magn_set_material_from_preset` | `MagnResult magn_set_material_from_preset(uint32_t materialID, const char* presetTag)` | Set a material from a built-in preset (e.g., "Wood", "Metal"). |
| `magn_get_material` | `MagnResult magn_get_material(uint32_t materialID, MagnMaterial* outMaterial)` | Retrieve a material definition. |

### Scene Management – Sources & Listeners

| Function | Signature | Description |
|----------|-----------|-------------|
| `magn_register_source` | `MagnResult magn_register_source(uint32_t sourceID, const MagnSourceDesc* desc)` | Register an acoustic source. |
| `magn_update_source` | `MagnResult magn_update_source(uint32_t sourceID, const float pos[3], const float fwd[3], const float up[3])` | Update source position/orientation. |
| `magn_set_source_active` | `MagnResult magn_set_source_active(uint32_t sourceID, uint8_t active)` | Enable or disable a source. |
| `magn_deregister_source` | `MagnResult magn_deregister_source(uint32_t sourceID)` | Remove a source. |
| `magn_register_listener` | `MagnResult magn_register_listener(uint32_t listenerID, const MagnListenerDesc* desc)` | Register a listener. |
| `magn_update_listener` | `MagnResult magn_update_listener(uint32_t listenerID, const float pos[3], const float fwd[3], const float up[3])` | Update listener position/orientation. |
| `magn_deregister_listener` | `MagnResult magn_deregister_listener(uint32_t listenerID)` | Remove a listener. |

### Simulation

| Function | Signature | Description |
|----------|-----------|-------------|
| `magn_tick` | `MagnResult magn_tick(float deltaTime)` | Advance simulation by one frame. |

### Query Acoustic State

| Function | Signature | Description |
|----------|-----------|-------------|
| `magn_get_acoustic_state` | `MagnResult magn_get_acoustic_state(uint32_t listenerID, const MagnAcousticState** outState)` | Get full acoustic state for a listener. Lock-free, audio-thread safe. |
| `magn_get_direct` | `MagnResult magn_get_direct(uint32_t listenerID, uint32_t sourceID, const MagnDirectComponent** out)` | Get direct component for a specific source. |
| `magn_get_reflections` | `MagnResult magn_get_reflections(uint32_t listenerID, uint32_t sourceID, const MagnEarlyReflections** out)` | Get early reflections for a specific source. |
| `magn_get_diffraction` | `MagnResult magn_get_diffraction(uint32_t listenerID, uint32_t sourceID, const MagnDiffraction** out)` | Get diffraction taps for a specific source. |
| `magn_get_late_field` | `MagnResult magn_get_late_field(uint32_t listenerID, const MagnLateField** out)` | Get the late reverberant field descriptor. |

### Render Output

| Function | Signature | Description |
|----------|-----------|-------------|
| `magn_get_submix` | `MagnResult magn_get_submix(uint32_t listenerID, MagnSubmixFormat fmt, float* outBuf, uint32_t frameCount)` | Get spatialized audio submix (built-in renderer). |
| `magn_get_source_parameters` | `MagnResult magn_get_source_parameters(uint32_t listenerID, uint32_t sourceID, MagnSourceParameterSet* out)` | Get adapter-friendly parameters (gain, filter, panning). |

### Quality Settings

| Function | Signature | Description |
|----------|-----------|-------------|
| `magn_set_quality` | `MagnResult magn_set_quality(MagnQualityLevel level)` | Set quality preset (Low, Medium, High, Ultra). |
| `magn_get_quality` | `MagnResult magn_get_quality(MagnQualityLevel* outLevel)` | Query current quality level. |
| `magn_set_quality_settings` | `MagnResult magn_set_quality_settings(const MagnQualitySettings* settings)` | Fine-tune individual quality parameters. |
| `magn_get_quality_settings` | `MagnResult magn_get_quality_settings(MagnQualitySettings* out)` | Query current quality settings. |

### Debug & Profiling

| Function | Signature | Description |
|----------|-----------|-------------|
| `magn_debug_get_stats` | `MagnResult magn_debug_get_stats(MagnStats* outStats)` | Get per-frame performance statistics. |
| `magn_debug_visualize` | `MagnResult magn_debug_visualize(MagnDebugFlags flags, MagnDebugDrawCallback cb, void* ud)` | Request debug visualization data. |
| `magn_debug_set_log_level` | `MagnResult magn_debug_set_log_level(MagnLogLevel level)` | Set runtime log verbosity. |

---

## Error Codes

| Code | Name | Description |
|------|------|-------------|
| 0    | `MAGN_OK` | Success |
| -1   | `MAGN_ERROR_INVALID_ARG` | A parameter is null or out of range |
| -2   | `MAGN_ERROR_NOT_INITIALIZED` | `magn_init()` has not been called |
| -3   | `MAGN_ERROR_OUT_OF_MEMORY` | Allocation failed |
| -4   | `MAGN_ERROR_BACKEND_FAILURE` | Ray backend initialization or dispatch failed |
| -5   | `MAGN_ERROR_VERSION_MISMATCH` | Header/library version mismatch |
| -6   | `MAGN_ERROR_CHUNK_NOT_FOUND` | Chunk ID not registered |
| -7   | `MAGN_ERROR_ID_NOT_FOUND` | Source/listener/material ID not found |
| -8   | `MAGN_ERROR_LIMIT_EXCEEDED` | Maximum count exceeded (sources, chunks, etc.) |
| -99  | `MAGN_ERROR_UNKNOWN` | Unclassified internal error |

---

## Thread Safety Summary

| Function Group | Main Thread | Audio Thread | Worker Thread |
|----------------|:-----------:|:------------:|:-------------:|
| `magn_init` / `magn_shutdown` | ✅ | ❌ | ❌ |
| `magn_register_*` / `magn_update_*` | ✅ | ❌ | ❌ |
| `magn_tick` | ✅ | ❌ | ❌ |
| `magn_get_acoustic_state` | ✅ | ✅ | ❌ |
| `magn_get_direct` / `reflections` / etc. | ✅ | ✅ | ❌ |
| `magn_get_submix` | ✅ | ✅ | ❌ |
| `magn_debug_*` | ✅ | ❌ | ❌ |

All query functions (`magn_get_*`) are **lock-free** and safe to call from the
audio thread at any time.

---

## Data Types Reference

For complete struct definitions, see [`docs/design/api.md`](design/api.md),
sections 2–11.

---

## See Also

- [`docs/design/api.md`](design/api.md) – Full C ABI with struct definitions
- [`docs/design/architecture.md`](design/architecture.md) – System architecture
- [`docs/Integration-Unity.md`](Integration-Unity.md) – Unity integration guide
- [`docs/Integration-Unreal.md`](Integration-Unreal.md) – Unreal integration guide
- [`docs/Material-Presets.md`](Material-Presets.md) – Material preset reference
- [`docs/Performance-Tuning.md`](Performance-Tuning.md) – Performance tuning guide
