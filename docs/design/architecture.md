# Magnaundasoni – System Architecture

## Overview

Magnaundasoni is a real-time acoustics runtime that computes physically-inspired
sound propagation and exposes results through a stable C ABI.  This document
describes the internal architecture, data flow, threading model, and backend
abstraction.

## System Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          HOST APPLICATION                                  │
│                  (Unity / Unreal / Custom Engine)                           │
├───────────────┬─────────────────────────┬───────────────────────────────────┤
│  Unity C#     │   Unreal C++ Module     │  Direct C ABI Consumer           │
│  Adapter Pkg  │   Adapter Plug-in       │  (standalone / proprietary)      │
├───────────────┴─────────────────────────┴───────────────────────────────────┤
│                                                                             │
│                      ┌─────────────────────┐                                │
│                      │   PUBLIC C ABI      │   magn_*.h                     │
│                      │  (magn_init, tick,  │                                │
│                      │   query, shutdown)  │                                │
│                      └────────┬────────────┘                                │
│                               │                                             │
│  ┌────────────────────────────┼────────────────────────────────────────┐    │
│  │                    NATIVE CORE (C++17)                              │    │
│  │                                                                    │    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────────┐  │    │
│  │  │ Scene Graph  │  │  Material    │  │  Streaming / Chunk Mgr   │  │    │
│  │  │  Manager     │  │  Database    │  │  (load/unload/fidelity)  │  │    │
│  │  └──────┬───────┘  └──────┬───────┘  └────────────┬─────────────┘  │    │
│  │         │                 │                        │                │    │
│  │         ▼                 ▼                        ▼                │    │
│  │  ┌─────────────────────────────────────────────────────────────┐   │    │
│  │  │               ACCELERATION STRUCTURES                       │   │    │
│  │  │  ┌─────────────┐  ┌──────────────┐  ┌───────────────────┐  │   │    │
│  │  │  │ Top-Level   │  │ Per-Chunk    │  │  Dynamic Object   │   │   │    │
│  │  │  │ Chunk BVH   │  │ Static BVHs  │  │  Broadphase+BVH  │   │   │    │
│  │  │  └─────────────┘  └──────────────┘  └───────────────────┘  │   │    │
│  │  └────────────────────────┬────────────────────────────────────┘   │    │
│  │                           │                                        │    │
│  │                           ▼                                        │    │
│  │  ┌─────────────────────────────────────────────────────────────┐   │    │
│  │  │            COMPUTE / RAY BACKEND (abstracted)               │   │    │
│  │  │                                                             │   │    │
│  │  │  ┌───────────┐   ┌──────────────┐   ┌────────────────┐     │   │    │
│  │  │  │  DXR 1.1  │   │ Vulkan RT    │   │ Software BVH   │     │   │    │
│  │  │  │  Backend  │   │ Backend      │   │ (CPU, SIMD)    │     │   │    │
│  │  │  └───────────┘   └──────────────┘   └────────────────┘     │   │    │
│  │  └────────────────────────┬────────────────────────────────────┘   │    │
│  │                           │                                        │    │
│  │                           ▼                                        │    │
│  │  ┌─────────────────────────────────────────────────────────────┐   │    │
│  │  │             ACOUSTIC RENDERING CORE                         │   │    │
│  │  │                                                             │   │    │
│  │  │  ┌──────────┐ ┌───────────┐ ┌────────────┐ ┌────────────┐  │   │    │
│  │  │  │ Direct   │ │ Early     │ │ Diffraction│ │ Late Field │  │   │    │
│  │  │  │ Path     │ │ Reflect.  │ │ Solver     │ │ Estimator  │  │   │    │
│  │  │  └──────────┘ └───────────┘ └────────────┘ └────────────┘  │   │    │
│  │  └────────────────────────┬────────────────────────────────────┘   │    │
│  │                           │                                        │    │
│  │                           ▼                                        │    │
│  │  ┌─────────────────────────────────────────────────────────────┐   │    │
│  │  │           OUTPUT ASSEMBLY & CACHE                           │   │    │
│  │  │    MagnAcousticState (per-listener, per-frame)              │   │    │
│  │  └─────────────────────────────────────────────────────────────┘   │    │
│  │                                                                    │    │
│  └────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
│  ┌────────────────────────────────────────────────────────────────────┐    │
│  │           OPTIONAL BUILT-IN SPATIAL RENDERER                       │    │
│  │    (binaural / HRTF / amplitude panning → platform audio out)      │    │
│  └────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Component Descriptions

### 1. Public C ABI (`magn_*.h`)

The sole entry point for all consumers.  A flat C API (no C++ in headers)
ensuring ABI stability across compiler versions.  See `docs/design/api.md` for
the full function reference.

Key surface areas:

| Area               | Functions |
|--------------------|-----------|
| Lifecycle          | `magn_init`, `magn_shutdown` |
| Scene              | `magn_register_geometry`, `magn_set_material`, `magn_register_source`, `magn_register_listener` |
| Simulation         | `magn_tick` |
| Query              | `magn_get_acoustic_state`, `magn_get_direct`, `magn_get_reflections` |
| Quality            | `magn_set_quality`, `magn_get_quality` |
| Debug              | `magn_debug_get_stats`, `magn_debug_visualize` |

### 2. Scene Graph Manager

Maintains the runtime scene representation:

- **Source registry** – position, orientation, directivity pattern, priority,
  active/inactive flag.
- **Listener registry** – position, orientation, HRTF profile.
- **Geometry registry** – per-chunk triangle meshes with material IDs.
- **Dynamic object registry** – moving objects with proxy geometry.

The scene graph is **double-buffered**: the simulation thread reads from the
"front" buffer while the main thread writes to the "back" buffer.  A swap
occurs at the start of each `magn_tick()`.

### 3. Material Database

Stores all registered `MagnMaterial` records (ADR-0003).  Includes the built-in
preset table.  Materials are referenced by `uint32_t materialID` from geometry
triangles.

### 4. Streaming / Chunk Manager

Implements the chunk lifecycle (ADR-0005):

- Accepts load/unload requests from the host via the C ABI.
- Dispatches BVH build/deserialization to the worker thread pool.
- Manages fidelity zone assignment based on listener proximity.
- Enforces the per-frame activation budget (max 2 chunks per tick).

### 5. Acceleration Structures

Three-level hierarchy described in ADR-0004:

1. **Top-level chunk BVH** – AABB per loaded chunk.
2. **Per-chunk static BVH** – SAH-built BVH over chunk triangles.
3. **Dynamic broadphase + local BVH** – uniform grid for overlap detection,
   per-object BVH for ray intersection.

### 6. Compute / Ray Backend

An abstract interface (`IRayBackend`) with three implementations:

```cpp
class IRayBackend {
public:
    virtual ~IRayBackend() = default;

    virtual void buildBLAS(ChunkID id, const TriMesh& mesh) = 0;
    virtual void destroyBLAS(ChunkID id) = 0;
    virtual void rebuildTLAS(Span<const InstanceDesc> instances) = 0;

    virtual void traceRays(Span<const Ray> rays,
                           Span<HitResult> results) = 0;

    virtual void traceOcclusionRays(Span<const Ray> rays,
                                    Span<bool> occluded) = 0;

    virtual BackendType type() const = 0;
};
```

| Implementation   | Platform            | Notes |
|------------------|---------------------|-------|
| `DxrBackend`     | Windows, Xbox       | Uses D3D12 + DXR 1.1 inline ray tracing |
| `VulkanRtBackend`| Linux, Steam Deck, Android | Uses `VK_KHR_ray_query` |
| `SoftwareBvhBackend` | All platforms  | CPU BVH2, SIMD traversal (SSE4.1 / NEON) |

Backend selection is automatic (best available) but can be overridden via
`MagnInitConfig.forceBackend`.

### 7. Acoustic Rendering Core

Four parallel sub-solvers that consume ray results and produce the output
contract (ADR-0002):

| Sub-solver           | Input | Output |
|----------------------|-------|--------|
| **Direct Path**      | Single ray per source→listener | `MagnDirectComponent` |
| **Early Reflections**| Image-source + stochastic rays | `MagnEarlyReflections` |
| **Diffraction Solver** | Edge cache + visibility rays | `MagnDiffraction` |
| **Late Field Estimator** | Energy decay from stochastic ray set | `MagnLateField` |

Sub-solvers run as parallel tasks within a single `magn_tick()` invocation.

### 8. Output Assembly & Cache

Collects sub-solver outputs into a single `MagnAcousticState` per listener.
Applies temporal smoothing (EMA filter) to prevent frame-to-frame jitter.
The assembled state is stored in a triple-buffered cache so that query calls
from the audio thread never block the simulation thread.

### 9. Built-in Spatial Renderer (Optional)

A lightweight audio renderer for standalone use:

- HRTF-based binaural rendering (built-in MIT KEMAR dataset).
- Amplitude panning for speaker layouts (stereo, quad, 5.1, 7.1).
- Convolution reverb driven by `MagnLateField` parameters.
- Outputs PCM to the platform audio API (WASAPI, CoreAudio, PulseAudio/ALSA).

This component is **optional**; most users will use their engine's audio
system and consume `MagnAcousticState` directly.

## Data Flow

```
1. Host calls magn_register_*() to populate the scene graph.
2. Host calls magn_tick(dt):
   a. Double-buffer swap (scene graph front ↔ back).
   b. Streaming manager updates fidelity zones, activates pending chunks.
   c. Ray backend rebuilds TLAS if chunk set changed.
   d. Sub-solvers dispatch ray batches through the backend.
   e. Sub-solver results are assembled into MagnAcousticState.
   f. Triple-buffer publish (new state becomes available to queries).
3. Host (or audio thread) calls magn_get_acoustic_state() at any time.
4. Host applies results to its audio system (or built-in renderer processes
   them automatically).
```

## Threading Model

```
┌──────────────┐     ┌──────────────┐     ┌──────────────────────┐
│  Main Thread │     │  Sim Thread  │     │  Worker Thread Pool  │
│  (host)      │     │  (magn_tick) │     │  (N = core_count-2)  │
├──────────────┤     ├──────────────┤     ├──────────────────────┤
│ register_*() │────▶│ buffer swap  │     │                      │
│ tick()       │     │ zone update  │     │ chunk BVH builds     │
│              │     │ TLAS rebuild │     │ ray batch dispatch   │
│              │     │ sub-solvers ─┼────▶│ sub-solver tasks     │
│              │     │ output assem │◀────│                      │
│              │     │              │     │                      │
│ get_state()  │◀ ─ ─│─ triple buf ─│     │                      │
│ (audio thrd) │     │              │     │                      │
└──────────────┘     └──────────────┘     └──────────────────────┘
```

### Thread Ownership Rules

| Thread            | May Do | Must Not Do |
|-------------------|--------|-------------|
| **Main thread**   | Call any `magn_*` API function | Block on simulation |
| **Sim thread**    | Read front scene buffer, dispatch tasks, write output buffer | Allocate heap memory (use pre-allocated pools) |
| **Worker threads**| Execute dispatched tasks | Access scene graph directly (only through task payloads) |
| **Audio thread**  | Call `magn_get_acoustic_state()` | Call `magn_tick()` or modify scene |

### Synchronization Primitives

| Primitive         | Purpose |
|-------------------|---------|
| Atomic swap       | Double-buffer (scene) and triple-buffer (output) exchange |
| Lock-free queue   | Task dispatch to worker pool |
| Counting semaphore| Worker thread wake/sleep |
| Read-write lock   | Chunk array modification (write-rare, read-frequent) |

No mutexes are held across frame boundaries.  No lock is ever acquired on the
audio thread's query path.

## Memory Architecture

| Pool            | Lifetime | Size | Purpose |
|-----------------|----------|------|---------|
| **Frame pool**  | Per-tick | 16 MB (configurable) | Temporary allocations for ray batches, intermediate results |
| **Chunk pool**  | Per-chunk load | Varies | BVH nodes, triangle data, edge caches |
| **Output pool** | Persistent | 2 MB | Triple-buffered `MagnAcousticState` |
| **Material pool**| Persistent | 64 KB | Material table |

All pools use bump allocation internally; no per-object `free()`.  The frame
pool is reset at the start of each `magn_tick()`.

## Platform Support Matrix

| Platform       | Primary Backend | Fallback | Notes |
|----------------|----------------|----------|-------|
| Windows 10+    | DXR 1.1        | Software BVH | Requires NVIDIA RTX / AMD RDNA2+ for HW RT |
| Linux (Vulkan) | Vulkan RT      | Software BVH | Steam Deck supported |
| macOS          | Software BVH   | —        | Metal ray tracing backend planned (Phase 3) |
| Xbox Series X/S| DXR 1.1        | Software BVH | |
| PlayStation 5  | Software BVH   | —        | Custom RT backend planned (Phase 3) |
| Nintendo Switch| Software BVH   | —        | Reduced quality tier enforced |
| Android (Vulkan)| Vulkan RT     | Software BVH | High-end devices only for HW RT |
| iOS            | Software BVH   | —        | Metal RT planned |

## References

- ADR-0001 through ADR-0006
- `docs/design/api.md` – complete C ABI reference
- `docs/Performance-Tuning.md` – quality and budget configuration
- `docs/STYLEGUIDE.md` – C++ coding standards
