# Magnaundasoni – C ABI Reference

> **Header**: `magn_api.h`
> **ABI Version**: 1
> **Calling Convention**: `__cdecl` (Windows), default (POSIX)
> **Namespace prefix**: `magn_` (functions), `Magn` (types), `MAGN_` (constants)

All functions are `extern "C"` and exported with `MAGN_API` visibility.

---

## Table of Contents

1. [Result Codes](#1-result-codes)
2. [Engine Lifecycle](#2-engine-lifecycle)
3. [Scene Management – Geometry](#3-scene-management--geometry)
4. [Scene Management – Materials](#4-scene-management--materials)
5. [Scene Management – Sources & Listeners](#5-scene-management--sources--listeners)
6. [Simulation Tick](#6-simulation-tick)
7. [Query Acoustic State](#7-query-acoustic-state)
8. [Render Output Retrieval](#8-render-output-retrieval)
9. [Quality Settings](#9-quality-settings)
10. [Debug & Profiling](#10-debug--profiling)
11. [Type Definitions](#11-type-definitions)

---

## 1. Result Codes

```c
typedef enum MagnResult {
    MAGN_OK                    = 0,
    MAGN_ERROR_INVALID_ARG     = -1,
    MAGN_ERROR_NOT_INITIALIZED = -2,
    MAGN_ERROR_OUT_OF_MEMORY   = -3,
    MAGN_ERROR_BACKEND_FAILURE = -4,
    MAGN_ERROR_VERSION_MISMATCH= -5,
    MAGN_ERROR_CHUNK_NOT_FOUND = -6,
    MAGN_ERROR_ID_NOT_FOUND    = -7,
    MAGN_ERROR_LIMIT_EXCEEDED  = -8,
    MAGN_ERROR_UNKNOWN         = -99
} MagnResult;
```

All functions return `MagnResult` unless otherwise noted.  On failure, output
parameters are left unchanged.

---

## 2. Engine Lifecycle

### `magn_init`

Initialize the Magnaundasoni runtime.

```c
MAGN_API MagnResult magn_init(const MagnInitConfig* config);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `config`  | `const MagnInitConfig*` | Configuration struct (see below). Pass `NULL` for defaults. |

**`MagnInitConfig`**:

```c
typedef struct MagnInitConfig {
    uint32_t          structSize;        /* = sizeof(MagnInitConfig) */
    MagnBackend       forceBackend;      /* MAGN_BACKEND_AUTO (default) */
    uint32_t          workerThreadCount; /* 0 = auto-detect */
    uint32_t          maxSources;        /* Default: 256 */
    uint32_t          maxListeners;      /* Default: 1 */
    uint32_t          maxChunks;         /* Default: 512 */
    uint32_t          framePoolSizeMB;   /* Default: 16 */
    MagnLogCallback   logCallback;       /* Optional logging callback */
    void*             logUserData;       /* Passed to logCallback */
    MagnAllocator*    allocator;         /* Custom allocator; NULL = default */
} MagnInitConfig;
```

**Returns**: `MAGN_OK` on success.  `MAGN_ERROR_BACKEND_FAILURE` if the
requested backend cannot be initialized.

**Thread safety**: Must be called from the main thread before any other
`magn_*` call.

---

### `magn_shutdown`

Tear down the runtime and release all resources.

```c
MAGN_API MagnResult magn_shutdown(void);
```

**Thread safety**: Must be called from the main thread.  No other `magn_*`
calls may be in flight.

---

## 3. Scene Management – Geometry

### `magn_register_geometry`

Register a static geometry chunk.

```c
MAGN_API MagnResult magn_register_geometry(
    uint64_t                 chunkID,
    const MagnGeometryDesc*  desc
);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `chunkID` | `uint64_t` | Unique chunk identifier |
| `desc`    | `const MagnGeometryDesc*` | Geometry descriptor |

**`MagnGeometryDesc`**:

```c
typedef struct MagnGeometryDesc {
    uint32_t        structSize;
    const float*    vertices;       /* Packed xyz, float[vertexCount*3] */
    uint32_t        vertexCount;
    const uint32_t* indices;        /* Triangle indices, uint32_t[indexCount] */
    uint32_t        indexCount;     /* Must be divisible by 3 */
    const uint32_t* materialIDs;    /* Per-triangle material ID, uint32_t[indexCount/3] */
    MagnAABB        bounds;         /* Axis-aligned bounding box */
    const MagnEdgeCache* edgeCache; /* Optional pre-extracted edges; NULL to skip */
} MagnGeometryDesc;
```

**Returns**: `MAGN_OK`.  `MAGN_ERROR_LIMIT_EXCEEDED` if `maxChunks` reached.

---

### `magn_unregister_geometry`

Remove a previously registered geometry chunk.

```c
MAGN_API MagnResult magn_unregister_geometry(uint64_t chunkID);
```

---

### `magn_register_dynamic_object`

Register a dynamic (moving) object with proxy geometry.

```c
MAGN_API MagnResult magn_register_dynamic_object(
    uint32_t                    objectID,
    const MagnDynamicObjectDesc* desc
);
```

**`MagnDynamicObjectDesc`**:

```c
typedef struct MagnDynamicObjectDesc {
    uint32_t        structSize;
    const float*    vertices;
    uint32_t        vertexCount;
    const uint32_t* indices;
    uint32_t        indexCount;
    const uint32_t* materialIDs;
    MagnTransform   transform;      /* Initial world-space transform */
} MagnDynamicObjectDesc;
```

---

### `magn_update_dynamic_object`

Update the transform of a dynamic object.

```c
MAGN_API MagnResult magn_update_dynamic_object(
    uint32_t              objectID,
    const MagnTransform*  transform
);
```

---

### `magn_deregister_dynamic_object`

Remove a dynamic object.

```c
MAGN_API MagnResult magn_deregister_dynamic_object(uint32_t objectID);
```

---

## 4. Scene Management – Materials

### `magn_set_material`

Register or update a material.

```c
MAGN_API MagnResult magn_set_material(
    uint32_t              materialID,
    const MagnMaterial*   material
);
```

See ADR-0003 for the `MagnMaterial` struct definition.

---

### `magn_set_material_from_preset`

Set a material from a built-in preset.

```c
MAGN_API MagnResult magn_set_material_from_preset(
    uint32_t    materialID,
    const char* presetTag       /* e.g., "Wood", "Metal", "Carpet" */
);
```

---

### `magn_get_material`

Retrieve the current material definition.

```c
MAGN_API MagnResult magn_get_material(
    uint32_t        materialID,
    MagnMaterial*   outMaterial
);
```

---

## 5. Scene Management – Sources & Listeners

### `magn_register_source`

Register an acoustic source.

```c
MAGN_API MagnResult magn_register_source(
    uint32_t                sourceID,
    const MagnSourceDesc*   desc
);
```

**`MagnSourceDesc`**:

```c
typedef struct MagnSourceDesc {
    uint32_t        structSize;
    float           position[3];        /* World-space position */
    float           forward[3];         /* Forward direction (unit vector) */
    float           up[3];              /* Up direction (unit vector) */
    uint32_t        directivityPattern; /* 0 = omnidirectional */
    float           priority;           /* [0,1]; higher = more ray budget */
    uint8_t         active;             /* 1 = active, 0 = inactive */
} MagnSourceDesc;
```

---

### `magn_update_source`

Update source position and orientation.

```c
MAGN_API MagnResult magn_update_source(
    uint32_t        sourceID,
    const float     position[3],
    const float     forward[3],
    const float     up[3]
);
```

---

### `magn_set_source_active`

Enable or disable a source without deregistering it.

```c
MAGN_API MagnResult magn_set_source_active(
    uint32_t sourceID,
    uint8_t  active    /* 1 = active, 0 = inactive */
);
```

---

### `magn_deregister_source`

Remove a source.

```c
MAGN_API MagnResult magn_deregister_source(uint32_t sourceID);
```

---

### `magn_register_listener`

Register a listener.

```c
MAGN_API MagnResult magn_register_listener(
    uint32_t                  listenerID,
    const MagnListenerDesc*   desc
);
```

**`MagnListenerDesc`**:

```c
typedef struct MagnListenerDesc {
    uint32_t structSize;
    float    position[3];
    float    forward[3];
    float    up[3];
} MagnListenerDesc;
```

---

### `magn_update_listener`

Update listener position and orientation.

```c
MAGN_API MagnResult magn_update_listener(
    uint32_t    listenerID,
    const float position[3],
    const float forward[3],
    const float up[3]
);
```

---

### `magn_deregister_listener`

Remove a listener.

```c
MAGN_API MagnResult magn_deregister_listener(uint32_t listenerID);
```

---

## 6. Simulation Tick

### `magn_tick`

Advance the acoustic simulation by one frame.

```c
MAGN_API MagnResult magn_tick(float deltaTime);
```

| Parameter   | Type    | Description |
|-------------|---------|-------------|
| `deltaTime` | `float` | Elapsed time in seconds since last tick |

This function:

1. Swaps the scene double buffer.
2. Updates fidelity zones and activates pending chunks.
3. Rebuilds the TLAS if the chunk set changed.
4. Dispatches ray batches and runs all sub-solvers.
5. Publishes the new `MagnAcousticState` to the triple buffer.

**Thread safety**: Must be called from the main thread (or a dedicated
simulation thread).  Must not be called concurrently with scene modification
functions.

---

## 7. Query Acoustic State

### `magn_get_acoustic_state`

Retrieve the most recently computed acoustic state for a listener.

```c
MAGN_API MagnResult magn_get_acoustic_state(
    uint32_t               listenerID,
    const MagnAcousticState** outState
);
```

Returns a pointer to an internally-owned `MagnAcousticState`.  The pointer
remains valid until the next `magn_tick()` completes (triple-buffered).

**Thread safety**: Safe to call from the audio thread.  Lock-free.

---

### `magn_get_direct`

Retrieve only the direct component for a specific source.

```c
MAGN_API MagnResult magn_get_direct(
    uint32_t                    listenerID,
    uint32_t                    sourceID,
    const MagnDirectComponent** outDirect
);
```

---

### `magn_get_reflections`

Retrieve early reflections for a specific source.

```c
MAGN_API MagnResult magn_get_reflections(
    uint32_t                      listenerID,
    uint32_t                      sourceID,
    const MagnEarlyReflections**  outReflections
);
```

---

### `magn_get_diffraction`

Retrieve diffraction taps for a specific source.

```c
MAGN_API MagnResult magn_get_diffraction(
    uint32_t                  listenerID,
    uint32_t                  sourceID,
    const MagnDiffraction**   outDiffraction
);
```

---

### `magn_get_late_field`

Retrieve the late reverberant field descriptor.

```c
MAGN_API MagnResult magn_get_late_field(
    uint32_t              listenerID,
    const MagnLateField** outLateField
);
```

---

## 8. Render Output Retrieval

For users of the built-in renderer or adapters that need pre-mixed output.

### `magn_get_submix`

Retrieve a spatialized submix buffer for the built-in renderer.

```c
MAGN_API MagnResult magn_get_submix(
    uint32_t              listenerID,
    MagnSubmixFormat      format,     /* STEREO, QUAD, SURROUND_51, SURROUND_71 */
    float*                outBuffer,  /* Interleaved float samples */
    uint32_t              frameCount  /* Number of audio frames requested */
);
```

---

### `magn_get_source_parameters`

Retrieve adapter-friendly parameter sets for a source (gain, filter, send
levels).

```c
MAGN_API MagnResult magn_get_source_parameters(
    uint32_t                      listenerID,
    uint32_t                      sourceID,
    MagnSourceParameterSet*       outParams
);
```

**`MagnSourceParameterSet`**:

```c
typedef struct MagnSourceParameterSet {
    float directGainDB;          /* Combined direct gain in dB */
    float occlusionLPFCutoff;    /* Low-pass filter cutoff in Hz */
    float reverbSendDB;          /* Reverb send level in dB */
    float earlyReflSendDB;       /* Early reflection send level in dB */
    float spreadDegrees;         /* Source apparent width (0-360) */
    float panAzimuth;            /* Horizontal angle in degrees */
    float panElevation;          /* Vertical angle in degrees */
    float distance;              /* Source-listener distance in metres */
} MagnSourceParameterSet;
```

---

## 9. Quality Settings

### `magn_set_quality`

Set the global quality level.

```c
MAGN_API MagnResult magn_set_quality(MagnQualityLevel level);
```

**`MagnQualityLevel`**:

```c
typedef enum MagnQualityLevel {
    MAGN_QUALITY_LOW    = 0,
    MAGN_QUALITY_MEDIUM = 1,
    MAGN_QUALITY_HIGH   = 2,
    MAGN_QUALITY_ULTRA  = 3,
    MAGN_QUALITY_CUSTOM = 4
} MagnQualityLevel;
```

---

### `magn_get_quality`

Query the current quality level.

```c
MAGN_API MagnResult magn_get_quality(MagnQualityLevel* outLevel);
```

---

### `magn_set_quality_settings`

Fine-tune individual quality parameters.

```c
MAGN_API MagnResult magn_set_quality_settings(
    const MagnQualitySettings* settings
);
```

**`MagnQualitySettings`**:

```c
typedef struct MagnQualitySettings {
    uint32_t structSize;
    uint32_t raysPerSource;         /* Total ray budget per source per tick */
    uint32_t maxReflectionOrder;    /* Max early reflection order (1-4) */
    uint32_t maxDiffractionDepth;   /* 1 or 2 */
    uint32_t maxEdgesPerSource;     /* Diffraction edge budget */
    float    detailedZoneRadius;    /* Metres */
    float    reducedZoneRadius;     /* Metres */
    float    summarizedZoneRadius;  /* Metres */
    float    maxPropagationDist;    /* Metres; rays beyond this are culled */
    uint32_t maxActiveSources;      /* Sources simulated per tick */
    float    temporalSmoothingAlpha;/* EMA factor for output smoothing */
} MagnQualitySettings;
```

---

### `magn_get_quality_settings`

Query the current quality settings.

```c
MAGN_API MagnResult magn_get_quality_settings(
    MagnQualitySettings* outSettings
);
```

---

## 10. Debug & Profiling

### `magn_debug_get_stats`

Retrieve per-frame performance statistics.

```c
MAGN_API MagnResult magn_debug_get_stats(MagnStats* outStats);
```

**`MagnStats`**:

```c
typedef struct MagnStats {
    float    tickTimeMs;         /* Total magn_tick() wall time */
    float    rayTraceTimeMs;     /* Time spent in ray backend */
    float    bvhBuildTimeMs;     /* Time spent building/refitting BVHs */
    uint32_t totalRaysCast;      /* Rays dispatched this frame */
    uint32_t activeSources;      /* Sources simulated this frame */
    uint32_t activeChunks;       /* Chunks in the top-level structure */
    uint32_t detailedChunks;     /* Chunks at detailed fidelity */
    uint32_t reducedChunks;      /* Chunks at reduced fidelity */
    uint32_t summarizedChunks;   /* Chunks at summarized fidelity */
    uint64_t framePoolUsedBytes; /* Frame allocator high watermark */
    MagnBackend backendUsed;     /* Active ray backend */
} MagnStats;
```

---

### `magn_debug_visualize`

Request debug visualization data (ray paths, BVH wireframes, diffraction
edges).

```c
MAGN_API MagnResult magn_debug_visualize(
    MagnDebugFlags          flags,
    MagnDebugDrawCallback   drawCallback,
    void*                   userData
);
```

**`MagnDebugFlags`**:

```c
typedef enum MagnDebugFlags {
    MAGN_DEBUG_RAYS           = 0x01,
    MAGN_DEBUG_BVH            = 0x02,
    MAGN_DEBUG_DIFFRACTION    = 0x04,
    MAGN_DEBUG_REFLECTIONS    = 0x08,
    MAGN_DEBUG_FIDELITY_ZONES = 0x10,
    MAGN_DEBUG_ALL            = 0xFF
} MagnDebugFlags;
```

The `drawCallback` is invoked synchronously with line/triangle primitives.
Adapters translate these into engine debug draw calls (Gizmos in Unity,
DrawDebugLine in Unreal).

---

### `magn_debug_set_log_level`

Control runtime log verbosity.

```c
MAGN_API MagnResult magn_debug_set_log_level(MagnLogLevel level);
```

**`MagnLogLevel`**:

```c
typedef enum MagnLogLevel {
    MAGN_LOG_SILENT  = 0,
    MAGN_LOG_ERROR   = 1,
    MAGN_LOG_WARNING = 2,
    MAGN_LOG_INFO    = 3,
    MAGN_LOG_DEBUG   = 4,
    MAGN_LOG_TRACE   = 5
} MagnLogLevel;
```

---

## 11. Type Definitions

### Common Types

```c
typedef struct MagnAABB {
    float min[3];
    float max[3];
} MagnAABB;

typedef struct MagnTransform {
    float position[3];
    float rotation[4];  /* Quaternion (x, y, z, w) */
    float scale[3];
} MagnTransform;

typedef enum MagnBackend {
    MAGN_BACKEND_AUTO       = 0,
    MAGN_BACKEND_DXR        = 1,
    MAGN_BACKEND_VULKAN_RT  = 2,
    MAGN_BACKEND_SOFTWARE   = 3
} MagnBackend;

typedef enum MagnSubmixFormat {
    MAGN_SUBMIX_STEREO      = 2,
    MAGN_SUBMIX_QUAD        = 4,
    MAGN_SUBMIX_SURROUND_51 = 6,
    MAGN_SUBMIX_SURROUND_71 = 8
} MagnSubmixFormat;

typedef void (*MagnLogCallback)(
    MagnLogLevel level,
    const char*  message,
    void*        userData
);

typedef struct MagnAllocator {
    void* (*alloc)(size_t size, size_t alignment, void* userData);
    void  (*free)(void* ptr, void* userData);
    void* userData;
} MagnAllocator;

typedef struct MagnFilterDescriptor {
    float cutoffHz;       /* Low-pass cutoff frequency */
    float resonanceQ;     /* Filter Q factor */
    float gainDB;         /* Output gain adjustment */
} MagnFilterDescriptor;

typedef struct MagnEdgeMeta {
    float    edgeLength;
    float    openingAngle;   /* Wedge angle in radians */
    uint32_t materialA;      /* Material ID of face A */
    uint32_t materialB;      /* Material ID of face B */
} MagnEdgeMeta;

typedef struct MagnReverbSendDescriptor {
    float wetLevelDB;
    float preDelayMs;
    float decayTimeScale;    /* Multiplier on RT60 for reverb preset */
} MagnReverbSendDescriptor;

typedef struct MagnEdgeCache {
    uint32_t              edgeCount;
    const MagnEdgeMeta*   edges;
    const float*          edgeEndpoints;  /* float[edgeCount * 6] (2 × xyz) */
} MagnEdgeCache;

typedef void (*MagnDebugDrawCallback)(
    const float* lineStart,  /* xyz */
    const float* lineEnd,    /* xyz */
    uint32_t     color,      /* RGBA packed */
    void*        userData
);
```

---

## References

- ADR-0001: Product Boundary
- ADR-0002: Solver Output Contract
- ADR-0003: Material Schema
- `docs/design/architecture.md` – System architecture
- `docs/Integration-Unity.md` – Unity adapter usage
- `docs/Integration-Unreal.md` – Unreal adapter usage
