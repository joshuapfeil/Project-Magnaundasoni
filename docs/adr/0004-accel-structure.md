# ADR-0004: Acceleration Structure Strategy

| Field       | Value                        |
|-------------|------------------------------|
| Status      | Accepted                     |
| Date        | 2025-01-15                   |
| Deciders    | Core Team                    |
| Category    | Architecture / Performance   |

> **Naming note**: Type and function names in this ADR (e.g. `Magn*`, `magn_*`)
> reflect the original design drafts.  The implementation uses the `Mag` / `mag_`
> / `MAG_` prefix — see [`docs/API.md`](../API.md).

## Context

The acoustic solver issues thousands of ray and path queries per frame against
potentially millions of triangles.  A naïve brute-force approach is
prohibitively expensive.  We need spatial acceleration structures that:

1. Handle both static and dynamic geometry efficiently.
2. Exploit hardware ray-tracing (RT) cores when available.
3. Degrade gracefully to software on platforms without RT hardware.
4. Integrate with the engine's streaming / world-partition model (ADR-0005).

## Decision

### Hybrid Three-Level Hierarchy

```
┌───────────────────────────────────────────────────────┐
│                    Top-Level BVH                      │
│            (chunk-level bounding volumes)             │
├───────────┬───────────┬───────────┬───────────────────┤
│  Chunk A  │  Chunk B  │  Chunk C  │  …                │
│  (static) │  (static) │  (static) │                   │
│  Per-chunk│  Per-chunk│  Per-chunk│                   │
│  BVH      │  BVH      │  BVH      │                   │
└───────────┴───────────┴───────────┴───────────────────┘
         ↕ broadphase overlap test
┌───────────────────────────────────────────────────────┐
│              Dynamic Object Layer                     │
│    Broadphase uniform grid + local dynamic BVHs       │
└───────────────────────────────────────────────────────┘
```

#### Level 1 – Chunk-Level Top Structure

- One AABB per loaded acoustic chunk (mirrors engine streaming chunks).
- Updated only when chunks are loaded / unloaded—effectively free at runtime.
- Used for coarse culling: skip chunks whose bounding volume is behind the
  listener or outside the propagation radius.

#### Level 2 – Per-Chunk Static BVH

- Built **once** when a chunk is loaded; rebuilt only if static geometry is
  edited at authoring time.
- Uses a high-quality SAH (Surface Area Heuristic) build.
- Triangle count per chunk is bounded by geometry fidelity limits (see below).
- On hardware-RT platforms this is stored as a Bottom-Level Acceleration
  Structure (BLAS).

#### Level 3 – Dynamic Object Layer

- A broadphase **uniform spatial grid** (cell size tuned to average dynamic
  object radius) detects which dynamics overlap which chunks.
- Each dynamic object maintains a **local BVH** over its proxy geometry.
- Local BVHs are rebuilt incrementally using a refit strategy when the object
  moves but its topology does not change, and fully rebuilt when topology changes.

### Hardware RTAS Path (DXR / Vulkan RT)

When RT hardware is detected at init time:

| Concept              | Mapping |
|----------------------|---------|
| Per-chunk static BVH | BLAS (bottom-level acceleration structure) |
| Chunk-level top BVH  | TLAS (top-level acceleration structure) with per-chunk instance transforms |
| Dynamic objects      | Separate BLAS per object, added as TLAS instances each frame |

- TLAS is rebuilt every frame (cheap—instance count is small).
- Static BLAS is built once.  Dynamic BLAS is refit or rebuilt depending on a
  deformation threshold.
- Ray queries use `TraceRayInline` (DXR 1.1) / `rayQueryEXT` (Vulkan) from
  compute shaders, keeping the pipeline simple (no hit/miss shader complexity).

### Software BVH Fallback

When no RT hardware is available:

- The same logical hierarchy is used, but stored as CPU-side BVH2 trees with
  quantized nodes (8 bytes/node).
- Traversal uses a stackless algorithm with SIMD 4-wide ray–AABB tests
  (SSE4.1 / NEON).
- A thread pool (see `docs/design/architecture.md`) distributes ray batches
  across available cores.

### Geometry Fidelity Limits and Proxies

To keep per-chunk BVH builds and traversals cheap:

| Fidelity Zone | Max Triangles / Chunk | Strategy |
|---------------|----------------------|----------|
| Detailed      | 50,000               | Full acoustic mesh |
| Reduced       | 10,000               | Simplified mesh (LOD1 equivalent) |
| Summarized    | 1,000                | Convex hull or box proxy per room |

- Dynamic objects always use **proxy geometry**: convex hulls, oriented bounding
  boxes, or artist-authored low-poly acoustic meshes.  Typical proxy budget:
  64–256 triangles.
- Proxy generation can be offline (build tool) or runtime (convex hull of
  render LOD2).

### Capability-Driven Selection

At `magn_init()`, the runtime probes for available backends in order:

1. **DXR 1.1** (Windows, Xbox) – preferred if D3D12 device supports
   `D3D12_RAYTRACING_TIER_1_1`.
2. **Vulkan Ray Query** – preferred on Linux / Steam Deck / Android with
   `VK_KHR_ray_query`.
3. **Software BVH** – universal fallback; always available.

The selected backend is reported in `MagnFrameGlobal.backendUsed` (ADR-0002).

Users may force a specific backend via `MagnInitConfig.forceBackend`.

## Consequences

### Positive

- Static geometry never causes per-frame BVH rebuild cost.
- Hardware RT path gives 5–10× ray throughput improvement where available.
- Software fallback ensures every platform is supported from Day 1.
- Geometry fidelity limits keep worst-case chunk build time under 50 ms on
  mid-range hardware.
- Broadphase grid keeps dynamic object overhead O(n) in object count, not
  O(n × chunk_count).

### Negative

- Three-level hierarchy adds implementation complexity.
- Proxy geometry requires either offline tooling or runtime convex hull
  generation.
- Hardware RT path requires maintaining both DXR and Vulkan code paths.

### Neutral

- The software BVH must be kept competitive; it is the reference
  implementation and the only path on consoles without RT (Nintendo Switch,
  older mobile).

## References

- ADR-0005: Streaming Model (chunk lifecycle triggers BVH build/destroy)
- ADR-0006: Diffraction Strategy (uses RTAS for edge visibility)
- Aila & Laine, "Understanding the Efficiency of Ray Traversal on GPUs" (2009)
- `docs/design/architecture.md` – backend selection and thread pool design
