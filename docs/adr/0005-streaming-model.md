# ADR-0005: Streaming Model

| Field       | Value                        |
|-------------|------------------------------|
| Status      | Accepted                     |
| Date        | 2025-01-15                   |
| Deciders    | Core Team                    |
| Category    | Architecture / Streaming     |

## Context

Modern game worlds are too large to fit in memory at once.  Host engines use
streaming systems—Unity Addressables / Scene loading, Unreal World Partition—to
load and unload regions dynamically.  The acoustic runtime must mirror this
behaviour: loading acoustic data when the engine loads geometry, and releasing
it when the engine unloads.

Key constraints:

- Acoustic data must be available before the listener enters a region (no
  popping).
- Loading must not stall the main thread or the audio thread.
- The system must handle hundreds of chunks in an open-world scenario.

## Decision

### Acoustic Chunk Concept

An **acoustic chunk** is the unit of streaming for Magnaundasoni.  It contains:

| Data                      | Description |
|---------------------------|-------------|
| Static geometry (trimesh) | Triangles + material IDs for the chunk's region |
| Pre-built BVH blob        | Serialized acceleration structure (ADR-0004) |
| Portal / opening hints    | Connectivity between this chunk and its neighbours |
| Material overrides        | Per-chunk material table delta (if any) |
| Edge cache (optional)     | Pre-extracted diffraction edges (ADR-0006) |

Chunks are identified by a `uint64_t chunkID` and an axis-aligned bounding box.

### Chunk Lifecycle

```
    ┌───────────┐
    │  UNLOADED │
    └─────┬─────┘
          │  magn_chunk_request_load(chunkID, data, priority)
          ▼
    ┌───────────┐
    │  LOADING  │  (async: BVH deserialization / build on worker thread)
    └─────┬─────┘
          │  background work complete
          ▼
    ┌───────────┐
    │  READY    │  (BVH inserted into top-level structure)
    └─────┬─────┘
          │  magn_chunk_unload(chunkID)
          ▼
    ┌───────────┐
    │  UNLOADING│  (removed from top-level; memory freed on next safe point)
    └─────┬─────┘
          │
          ▼
    ┌───────────┐
    │  UNLOADED │
    └───────────┘
```

- **LOADING** happens entirely on a background worker thread.  The main thread
  and audio thread are never blocked.
- **READY → active** transition is a single atomic pointer swap into the
  top-level chunk array, costing < 1 µs.
- **UNLOADING** defers memory release to a safe point (end of the current
  simulation tick) to avoid use-after-free.

### Fidelity Zones

Not all loaded chunks need full-detail simulation.  Chunks are assigned a
**fidelity zone** based on distance to the listener and to important sources.

| Zone         | Radius*        | Geometry Detail | Simulation Detail |
|--------------|---------------|-----------------|-------------------|
| **Detailed** | 0 – 30 m       | Full mesh (≤50k tris) | Full ray budget, all reflection orders |
| **Reduced**  | 30 – 100 m     | Simplified mesh (≤10k tris) | Reduced ray budget, first-order reflections only |
| **Summarized** | 100 – 300 m | Convex proxy (≤1k tris) | Late field estimation only, no per-source detail |

*\* Default radii; configurable via `MagnQualitySettings`.*

Zone assignment is re-evaluated every **N** frames (default N=4) to avoid
thrashing.  A hysteresis band of 10% is applied to zone boundaries.

### Integration with Engine Streaming

#### Unity (Addressables / Scene Loading)

```
Engine loads scene chunk
  → Unity adapter calls magn_chunk_request_load()
     with geometry extracted from MeshFilter/MeshCollider

Engine unloads scene chunk
  → Unity adapter calls magn_chunk_unload()
```

The Unity adapter registers a callback on `SceneManager.sceneLoaded` and
`Addressables.LoadSceneAsync().Completed` to trigger acoustic chunk loads
automatically.

#### Unreal Engine (World Partition)

```
World Partition streams in cell
  → Unreal adapter calls magn_chunk_request_load()
     with geometry from UStaticMeshComponent collision

World Partition streams out cell
  → Unreal adapter calls magn_chunk_unload()
```

The Unreal adapter hooks into the `UWorldPartitionSubsystem` streaming
delegates.  Acoustic chunks map 1:1 with World Partition cells by default,
but can be configured for N:1 (multiple WP cells per acoustic chunk) for
performance.

### Chunk Activation Budget

| Metric                    | Target |
|---------------------------|--------|
| Load (deserialize + BVH insert) | < 50 ms amortized per chunk |
| Unload (remove + deferred free) | < 1 ms |
| Main thread stall          | **0 ms** (all work on background threads) |
| Audio thread stall          | **0 ms** |
| Memory per chunk (detailed) | ≤ 8 MB |
| Memory per chunk (reduced)  | ≤ 2 MB |
| Memory per chunk (summarized) | ≤ 256 KB |

Activation is **budgeted per frame**: at most **2 chunks** may transition from
LOADING → READY in a single simulation tick to avoid spikes.  Remaining chunks
queue and activate in subsequent ticks.

### Pre-loading and Predictive Activation

The adapter may call `magn_chunk_request_load()` before the engine actually
loads the visual geometry, enabling **predictive pre-loading**:

- Unity: use `Addressables.DownloadDependenciesAsync` completion as a signal.
- Unreal: hook `OnWorldPartitionCellAboutToStream` for a few-hundred-ms head
  start.

This ensures acoustic data is READY before the listener physically enters the
new region, eliminating perceptible transitions.

## Consequences

### Positive

- Zero main-thread and audio-thread stalls by design.
- Fidelity zones let open-world games stay within ray and memory budgets.
- Engine-agnostic chunk API makes it straightforward to add new engine adapters.
- Budgeted activation prevents frame spikes even when many chunks load
  simultaneously.

### Negative

- Predictive pre-loading requires adapter-specific hooks that may break across
  engine versions.
- Fidelity zone hysteresis adds a few frames of latency when the listener moves
  rapidly between zones.
- Serialized BVH blobs are platform-specific; cross-platform asset pipelines
  must rebuild per target.

### Neutral

- Chunk boundaries can cause discontinuities at edges if portal/opening hints
  are missing.  This is mitigated by requiring overlap regions or portal
  annotations.

## References

- ADR-0004: Acceleration Structure Strategy (per-chunk BVH lifecycle)
- ADR-0006: Diffraction Strategy (edge cache in chunk data)
- `docs/design/architecture.md` – threading model for background loading
- Unity Addressables documentation
- Unreal Engine World Partition documentation
