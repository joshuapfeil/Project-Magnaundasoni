# ADR-0001: Product Boundary

| Field       | Value                        |
|-------------|------------------------------|
| Status      | Accepted                     |
| Date        | 2025-01-15                   |
| Deciders    | Core Team                    |
| Category    | Architecture / Scope         |

## Context

When building an acoustics middleware library, the temptation is to grow into a
full audio engine—mixing, music playback, spatial UI, etc.  Every feature added
outside the core acoustic simulation dilutes focus and increases the integration
surface with host engines (Unity, Unreal, proprietary).

We need a clear, enforceable line between what Magnaundasoni **is** and what it
**is not**, so that every contributor, every API review, and every roadmap
decision can be measured against this boundary.

## Decision

### Magnaundasoni IS

A **real-time acoustics runtime** that computes physically-inspired propagation
results and exposes them through a stable C ABI.

| Capability                | Description |
|---------------------------|-------------|
| **Direct sound**          | Distance attenuation, air absorption, source directivity |
| **Occlusion**             | Ray/path-based line-of-sight obstruction with per-band filtering |
| **Transmission**          | Through-wall energy transfer governed by material transmission loss |
| **Early reflections**     | Specular and clustered reflection taps up to configurable order |
| **Diffraction**           | Edge-based analytic (UTD-inspired) bending around geometry |
| **Late reverb**           | Per-band decay estimation, RT60, diffuse field descriptors |
| **Source directivity**    | Polar/balloon pattern support per source |

### Magnaundasoni is NOT

| Excluded Scope                  | Rationale |
|---------------------------------|-----------|
| Mixer / DSP graph               | Host engines already have mature mixers (Unity Audio, Wwise, FMOD). We output parameters, not mixed audio. |
| UI / editor tooling             | Editor integration belongs in per-engine adapter packages, not the core. |
| Music system / sequencer        | Orthogonal concern; no value in coupling. |
| FMOD / Wwise plug-in (initial)  | Deferred to Phase 3+. Core must stabilise first. |
| FEM / BEM wave solvers          | Far too expensive for real-time; out of scope permanently for the runtime (offline pre-compute tools may exist separately). |

### Primary Delivery Model

```
┌──────────────────────────────────────────────────────┐
│                   Host Application                   │
│         (Unity, Unreal, Custom Engine)               │
├──────────────┬───────────────────────┬───────────────┤
│  Unity       │   Unreal              │  Raw C ABI    │
│  Adapter     │   Adapter             │  (direct)     │
├──────────────┴───────────────────────┴───────────────┤
│              Magnaundasoni Native Core                │
│  ┌───────────┐  ┌────────────┐  ┌─────────────────┐ │
│  │ Simulation│  │  Built-in  │  │   Integration   │ │
│  │   Engine  │  │  Renderer  │  │    Adapters     │ │
│  └───────────┘  └────────────┘  └─────────────────┘ │
└──────────────────────────────────────────────────────┘
```

1. **Shared native core** – Single C/C++ library compiled per platform.
   Contains the simulation engine, acceleration structures, material database,
   and quality-tier logic.

2. **Built-in rendering mode** – A lightweight spatial audio renderer that can
   drive platform audio output directly.  Useful for quick prototyping and
   standalone demos where no host mixer exists.

3. **Integration adapters** – Thin, per-engine packages (Unity C# wrapper,
   Unreal plug-in module) that translate Magnaundasoni output into the host's
   audio system (e.g., Unity AudioSource parameters, Unreal Submix effects).

## Consequences

### Positive

- Contributors have a single, testable checklist for "does this belong here?"
- Adapters stay thin—reduces per-engine maintenance burden.
- The built-in renderer keeps the project usable without any third-party engine.
- Avoids feature creep that historically sinks acoustics middleware projects.

### Negative

- Users who want an all-in-one audio solution must still integrate a mixer.
- FMOD/Wwise users will need to wait for Phase 3 adapters or write their own
  bridge using the C ABI.
- The built-in renderer must be maintained even if most users prefer host-engine
  rendering.

### Neutral

- The C ABI contract becomes the single most important API surface; changes to
  it require an ADR amendment.

## References

- ADR-0002: Solver Output Contract (defines what the core produces)
- `docs/design/architecture.md` – full system diagram
