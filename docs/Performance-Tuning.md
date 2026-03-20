# Magnaundasoni – Performance Tuning Guide

This guide explains how to configure Magnaundasoni for optimal performance
across different hardware targets and game scenarios.

---

## Table of Contents

1. [Quality Tiers](#quality-tiers)
2. [Ray Budgets](#ray-budgets)
3. [Memory Configuration](#memory-configuration)
4. [Thread Pool Configuration](#thread-pool-configuration)
5. [Fidelity Zone Tuning](#fidelity-zone-tuning)
6. [Diffraction Budget](#diffraction-budget)
7. [Streaming Budgets](#streaming-budgets)
8. [Platform-Specific Recommendations](#platform-specific-recommendations)
9. [Profiling & Diagnostics](#profiling--diagnostics)
10. [Common Performance Scenarios](#common-performance-scenarios)
11. [Budget Calculator](#budget-calculator)

---

## Quality Tiers

Magnaundasoni ships with four built-in quality presets.  Each preset configures
all simulation parameters as a coherent set.

| Parameter               | Low       | Medium    | High      | Ultra     |
|-------------------------|-----------|-----------|-----------|-----------|
| Rays per source         | 16        | 64        | 128       | 256       |
| Max reflection order    | 1         | 2         | 3         | 4         |
| Max diffraction depth   | 1         | 1         | 1         | 2         |
| Max edges per source    | 4         | 8         | 16        | 32        |
| Detailed zone radius    | 15 m      | 25 m      | 40 m      | 60 m      |
| Reduced zone radius     | 50 m      | 80 m      | 120 m     | 180 m     |
| Summarized zone radius  | 100 m     | 200 m     | 300 m     | 500 m     |
| Max propagation dist    | 80 m      | 150 m     | 250 m     | 500 m     |
| Max active sources      | 8         | 16        | 32        | 64        |
| Temporal smoothing α    | 0.3       | 0.2       | 0.15      | 0.1       |

### Selecting a Tier

```c
/* C ABI */
MagEngineConfig cfg;
mag_engine_config_defaults(&cfg);
MagEngine engine = NULL;
mag_engine_create(&cfg, &engine);
mag_set_quality(engine, MAG_QUALITY_HIGH);
```

### When to Use Each Tier

| Tier     | Target Hardware | Typical Scenarios |
|----------|-----------------|-------------------|
| **Low**  | Mobile, Nintendo Switch | Background ambience, non-critical audio |
| **Medium** | Steam Deck, last-gen consoles | Most gameplay scenarios |
| **High** | Current-gen consoles, mid-range PC | Competitive / narrative games where audio matters |
| **Ultra** | High-end PC with RT hardware | Audio showcase, VR (where spatial accuracy is critical) |

---

## Ray Budgets

### Per-Source Ray Budget

The `raysPerSource` parameter controls how many rays are cast for each active
source per simulation tick.  These rays are distributed across sub-solvers:

| Sub-solver         | Low  | Medium | High | Ultra |
|--------------------|------|--------|------|-------|
| Direct path        | 1    | 1      | 1    | 1     |
| Occlusion probes   | 3    | 7      | 15   | 31    |
| Early reflections  | 8    | 32     | 64   | 128   |
| Diffraction vis.   | 2    | 8      | 16   | 32    |
| Late field         | 2    | 16     | 32   | 64    |

### Total Ray Budget Per Frame

```
totalRays = raysPerSource × activeSourceCount + lateFieldGlobalRays
```

| Scenario                  | Sources | Rays/Source | Total Rays | Typical Cost (SW) | Typical Cost (HW RT) |
|---------------------------|---------|-------------|------------|--------------------|-----------------------|
| Mobile ambient            | 8       | 16          | 128        | ~0.3 ms            | ~0.1 ms              |
| Indoor FPS (Medium)       | 16      | 64          | 1,024      | ~1.5 ms            | ~0.4 ms              |
| Open world (High)         | 32      | 128         | 4,096      | ~4.0 ms            | ~1.0 ms              |
| VR showcase (Ultra)       | 64      | 256         | 16,384     | ~12 ms             | ~2.5 ms              |

### Reducing Ray Cost

1. **Lower `raysPerSource`** – Most impactful single knob.
2. **Reduce `maxActiveSources`** – Only simulate the N most important sources.
3. **Use source priorities** – Assign `priority = 0.0` to ambient background
   sources that don't need per-band accuracy.
4. **Reduce `maxReflectionOrder`** – Order 2 is sufficient for most indoor
   spaces.
5. **Use hardware RT** – 5-10× faster ray throughput on supported GPUs.

---

## Memory Configuration

### Pool Sizes

| Pool | Default | Min | Max | Notes |
|------|---------|-----|-----|-------|
| Frame pool | 16 MB | 2 MB | 64 MB | Temporary per-tick allocations. Increase if `MagnStats.framePoolUsedBytes` approaches the limit. |
| Output pool | 2 MB | 512 KB | 8 MB | Triple-buffered acoustic state. Scale with source count. |
| Chunk pool | Varies | — | — | Per-chunk; see fidelity zone table below. |

### Per-Chunk Memory Budgets

| Fidelity Zone | Geometry | BVH | Edge Cache | Total |
|---------------|----------|-----|------------|-------|
| Detailed      | ≤ 4 MB   | ≤ 2 MB | ≤ 1 MB | ≤ 8 MB |
| Reduced       | ≤ 1 MB   | ≤ 512 KB | ≤ 256 KB | ≤ 2 MB |
| Summarized    | ≤ 128 KB | ≤ 64 KB | ≤ 32 KB | ≤ 256 KB |

### Configuring Memory

```c
MagEngineConfig cfg;
mag_engine_config_defaults(&cfg);
cfg.maxSources     = 128;
cfg.worldChunkSize = 64.0f;  /* Increase for large scenes */

MagEngine engine = NULL;
mag_engine_create(&cfg, &engine);
```

### Custom Allocator

For engine integration, provide a custom allocator:

```c
void* myAlloc(size_t size, size_t alignment, void* userData) {
    return myEngine_alignedMalloc(size, alignment);
}
void myFree(void* ptr, void* userData) {
    myEngine_alignedFree(ptr);
}

MagnAllocator allocator = { myAlloc, myFree, NULL };
config.allocator = &allocator;
```

---

## Thread Pool Configuration

### Worker Thread Count

| Setting | Behaviour |
|---------|-----------|
| `0` (default) | Auto-detect: `max(1, core_count - 2)` |
| `1` | Single-threaded simulation (debugging) |
| `N` | Exactly N worker threads |

### Recommendations by Platform

| Platform | Physical Cores | Recommended Workers | Notes |
|----------|---------------|---------------------|-------|
| Desktop PC (8-core) | 8 | 6 (auto) | Leave 2 cores for engine + audio |
| Steam Deck | 4 | 2 | Shares CPU with game |
| Xbox Series X | 8 | 4 | Reserve cores for title requirements |
| PlayStation 5 | 8 | 4 | Reserve cores for system + audio |
| Nintendo Switch | 4 | 1–2 | Very limited; use Low quality |
| Mobile (high-end) | 8 | 2–3 | Battery and thermal considerations |

### Affinity & Priority

The thread pool does **not** set CPU affinity or thread priority by default.
Engine adapters may configure these through platform-specific APIs:

```cpp
// Unreal example: set worker threads to below-normal priority
MagnSubsystem->SetWorkerThreadPriority(EThreadPriority::TPri_BelowNormal);
```

---

## Fidelity Zone Tuning

Fidelity zones control the simulation detail based on distance from the
listener.

```
┌──────────────────────────────────────────┐
│             Summarized Zone              │
│  ┌────────────────────────────────────┐  │
│  │          Reduced Zone              │  │
│  │  ┌──────────────────────────────┐  │  │
│  │  │      Detailed Zone           │  │  │
│  │  │         🎧 Listener          │  │  │
│  │  └──────────────────────────────┘  │  │
│  └────────────────────────────────────┘  │
└──────────────────────────────────────────┘
```

### Tuning Guidelines

| Environment | Detailed | Reduced | Summarized |
|-------------|----------|---------|------------|
| Small indoor (room) | 10 m | 30 m | 50 m |
| Medium indoor (building) | 25 m | 80 m | 150 m |
| Large outdoor | 40 m | 150 m | 400 m |
| Open world | 30 m | 100 m | 300 m |

### Hysteresis

Zone boundaries use a 10% hysteresis band to prevent thrashing when the
listener is near a boundary:

- Enter detailed zone at `detailedZoneRadius`
- Exit detailed zone at `detailedZoneRadius × 1.1`

Zone evaluation frequency: every 4 frames (configurable).

---

## Diffraction Budget

Diffraction is the most variable cost center.  Key controls:

| Parameter | Effect | Performance Impact |
|-----------|--------|--------------------|
| `maxDiffractionDepth` | 1 = single edge, 2 = cascaded | Depth 2 is ~3× cost of depth 1 |
| `maxEdgesPerSource` | Max candidate edges evaluated | Linear scaling |
| Quality tier | Fresnel (Low) vs. full UTD (High/Ultra) | UTD is ~8× cost of Fresnel |

### Recommended Diffraction Settings

| Scenario | Depth | Edges | Tier |
|----------|-------|-------|------|
| Mobile / ambient | 1 | 4 | Low (Fresnel) |
| Standard gameplay | 1 | 8 | Medium |
| Narrative / stealth | 1 | 16 | High (full UTD) |
| Audio showcase / VR | 2 | 32 | Ultra (cascaded) |

---

## Streaming Budgets

### Chunk Activation

| Metric | Target |
|--------|--------|
| Chunks activated per tick | ≤ 2 |
| Per-chunk load time (amortized) | < 50 ms |
| Main thread stall | 0 ms |
| Audio thread stall | 0 ms |

### Reducing Streaming Cost

1. **Pre-bake BVH blobs** – Avoid runtime BVH construction by serializing
   BVHs during the build process.
2. **Use pre-extracted edge caches** – Store diffraction edges in the chunk
   data to skip runtime edge extraction.
3. **Increase chunk size** – Fewer, larger chunks reduce activation overhead
   (but increase per-chunk memory).
4. **Limit concurrent loads** – The default budget (2 per tick) prevents
   spikes; only increase on high-end hardware.

---

## Platform-Specific Recommendations

### Windows PC (High-End)

```c
MagEngineConfig cfg;
mag_engine_config_defaults(&cfg);
cfg.preferredBackend   = MAG_BACKEND_AUTO;  /* Will select DXR if available */
cfg.threadCount        = 0;                 /* Auto-detect */
cfg.maxSources         = 256;

MagEngine engine = NULL;
mag_engine_create(&cfg, &engine);
mag_set_quality(engine, MAG_QUALITY_HIGH);
```

### Console (Xbox Series X / PS5)

```c
MagEngineConfig cfg;
mag_engine_config_defaults(&cfg);
cfg.threadCount = 4;
cfg.maxSources  = 128;

MagEngine engine = NULL;
mag_engine_create(&cfg, &engine);
mag_set_quality(engine, MAG_QUALITY_MEDIUM);
```

### Nintendo Switch

```c
MagEngineConfig cfg;
mag_engine_config_defaults(&cfg);
cfg.preferredBackend = MAG_BACKEND_SOFTWARE_BVH;
cfg.threadCount      = 1;
cfg.maxSources       = 32;

MagEngine engine = NULL;
mag_engine_create(&cfg, &engine);
mag_set_quality(engine, MAG_QUALITY_LOW);
```

### Mobile (Android / iOS)

```c
MagEngineConfig cfg;
mag_engine_config_defaults(&cfg);
cfg.preferredBackend = MAG_BACKEND_SOFTWARE_BVH;
cfg.threadCount      = 2;
cfg.maxSources       = 16;
cfg.raysPerSource    = 8;

MagEngine engine = NULL;
mag_engine_create(&cfg, &engine);
mag_set_quality(engine, MAG_QUALITY_LOW);
```

### VR (Quest, PCVR)

```c
/* VR requires low latency; prioritize tick time */
MagEngineConfig cfg;
mag_engine_config_defaults(&cfg);
cfg.threadCount           = 0;   /* Max available */
cfg.maxSources            = 32;
cfg.raysPerSource         = 128;
cfg.maxReflectionOrder    = 2;
cfg.maxDiffractionDepth   = 1;

MagEngine engine = NULL;
mag_engine_create(&cfg, &engine);
```

---

## Profiling & Diagnostics

### Built-in Statistics

```c
MagGlobalState state;
mag_get_global_state(engine, &state);

uint32_t rayCount = 0;
mag_debug_get_ray_count(engine, &rayCount);

uint32_t edgeCount = 0;
mag_debug_get_active_edges(engine, &edgeCount);

printf("Active quality: %d\n", state.activeQuality);
printf("Backend used: %d\n", state.backendUsed);
printf("Active sources: %u\n", state.activeSourceCount);
printf("CPU time: %.2f ms\n", state.cpuTimeMs);
printf("Rays cast: %u\n", rayCount);
printf("Active edges: %u\n", edgeCount);
```

### Key Metrics to Monitor

| Metric | Target (60 FPS) | Target (90 FPS VR) | Action if Exceeded |
|--------|-----------------|---------------------|-------------------|
| `tickTimeMs` | < 4.0 ms | < 2.0 ms | Lower quality tier |
| `rayTraceTimeMs` | < 3.0 ms | < 1.5 ms | Reduce `raysPerSource` |
| `bvhBuildTimeMs` | < 0.5 ms | < 0.3 ms | Pre-bake BVH blobs |
| `totalRaysCast` | < 8,000 | < 4,000 | Reduce sources or rays |
| `framePoolUsedBytes` | < 80% of pool | < 80% of pool | Increase pool size |

### External Profiling

- **PIX** (Windows/Xbox): Capture GPU timeline for DXR backend.
- **RenderDoc**: Capture Vulkan RT dispatches.
- **Tracy / Superluminal**: CPU profiling with Magnaundasoni's built-in
  `MAGN_PROFILE_SCOPE()` markers.
- **Unity Profiler**: Custom markers appear under "Magnaundasoni" category.
- **Unreal Insights**: Custom traces via `TRACE_CPUPROFILER_EVENT_SCOPE`.

---

## Common Performance Scenarios

### Scenario 1: Indoor FPS – CPU Budget Exceeded

**Symptoms**: `tickTimeMs` > 5 ms on target hardware.

**Diagnosis**: Check `activeSources` and `totalRaysCast`.

**Solutions** (in order):
1. Reduce `maxActiveSources` from 32 to 16.
2. Reduce `raysPerSource` from 128 to 64.
3. Reduce `maxReflectionOrder` from 3 to 2.
4. Switch to hardware RT backend if available.

### Scenario 2: Open World – Streaming Spikes

**Symptoms**: Periodic 10+ ms spikes when entering new areas.

**Diagnosis**: Check `bvhBuildTimeMs` and chunk activation count.

**Solutions**:
1. Pre-bake BVH blobs (eliminates runtime build).
2. Reduce chunk geometry to < 10k triangles.
3. Increase predictive loading distance.
4. Lower chunk activation budget to 1 per tick.

### Scenario 3: VR – Latency Too High

**Symptoms**: Acoustic changes lag behind head movement.

**Diagnosis**: Check `temporalSmoothingAlpha` and tick frequency.

**Solutions**:
1. Reduce `temporalSmoothingAlpha` to 0.05.
2. Ensure `magn_tick()` runs every frame (not every other frame).
3. Reduce total ray budget to keep `tickTimeMs` under 2 ms.
4. Use hardware RT for fastest ray dispatch.

### Scenario 4: Memory Pressure on Console

**Symptoms**: Chunk loading fails or frame pool exhausted.

**Solutions**:
1. Reduce `framePoolSizeMB` and monitor `framePoolUsedBytes`.
2. Use Reduced/Summarized fidelity for distant chunks.
3. Reduce `maxChunks` and unload aggressively.
4. Use smaller acoustic meshes (simplify geometry offline).

---

## Budget Calculator

Use this formula to estimate your per-frame cost:

```
estimatedTickMs =
    (activeSourceCount × raysPerSource × costPerRay_us) / 1000
    + chunkCount × costPerChunkOverhead_us / 1000
    + diffractionCost_us / 1000
```

### Cost-Per-Ray Estimates (microseconds)

| Backend | costPerRay (µs) |
|---------|-----------------|
| DXR 1.1 (RTX 3070) | 0.15 |
| Vulkan RT (RX 6800) | 0.18 |
| Software BVH (Ryzen 7, 6 threads) | 0.8 |
| Software BVH (Switch, 1 thread) | 4.0 |

### Example: Console (Medium Quality)

```
activeSourceCount = 16
raysPerSource = 64
costPerRay = 0.8 µs (software BVH, 4 threads on PS5)
chunkCount = 20
costPerChunkOverhead = 5 µs
diffractionCost = 16 sources × 8 edges × 15 µs = 1920 µs

estimatedTickMs = (16 × 64 × 0.8) / 1000 + (20 × 5) / 1000 + 1920 / 1000
                = 0.82 + 0.10 + 1.92
                = 2.84 ms ✓ (under 4 ms budget)
```

---

## References

- [`docs/design/architecture.md`](design/architecture.md) – Threading and backend details
- [`docs/API.md`](API.md) – Quality settings API
- [ADR-0004: Acceleration Structure Strategy](adr/0004-accel-structure.md)
- [ADR-0005: Streaming Model](adr/0005-streaming-model.md)
- [ADR-0006: Diffraction Strategy](adr/0006-diffraction-strategy.md)
