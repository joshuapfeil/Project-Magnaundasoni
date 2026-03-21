# ADR-0006: Diffraction Strategy

| Field       | Value                        |
|-------------|------------------------------|
| Status      | Accepted                     |
| Date        | 2025-01-15                   |
| Deciders    | Core Team                    |
| Category    | Acoustics / Algorithm         |

> **Naming note**: Type and function names in this ADR (e.g. `Magn*`, `magn_*`)
> reflect the original design drafts.  The implementation uses the `Mag` / `mag_`
> / `MAG_` prefix — see [`docs/API.md`](../API.md).

## Context

Sound bends around obstacles—through doorways, over walls, around pillars.
Without diffraction modelling, occluded sources abruptly cut to silence, which
is perceptually jarring and physically incorrect.

Full wave-based diffraction (BEM/FEM) is far too expensive for real-time
(ADR-0001 excludes these).  We need an efficient geometric approach that:

1. Produces plausible per-band attenuation.
2. Runs within the per-frame ray/compute budget.
3. Scales from low-end (mobile) to high-end (PC with RT hardware).

## Decision

### Approach: Edge-Based Analytic Diffraction (UTD-Inspired)

We implement diffraction as **virtual edge taps** using the Uniform Theory of
Diffraction (UTD) as the physical foundation, with progressive simplifications
at lower quality tiers.

Each diffracting edge produces a tap in the output contract (ADR-0002,
`MagnDiffractionTap`) with per-band attenuation, delay, and direction.

### Edge Extraction

#### Static Geometry (Offline)

1. At asset build time, extract **silhouette / crease edges** from the acoustic
   mesh where the dihedral angle between adjacent faces exceeds a threshold
   (default: 30°).
2. Merge collinear edges within tolerance.
3. Store edge data (endpoints, adjacent face normals, adjacent material IDs) in
   the chunk's **edge cache** (ADR-0005).
4. Typical edge count per chunk: 200–2,000 after merging.

#### Dynamic Objects (Runtime Proxy)

1. Dynamic objects use **proxy edges** derived from their convex hull or
   artist-authored acoustic proxy mesh.
2. Proxy edges are computed once when the object is registered and cached.
3. Typical proxy edge count: 8–32 per object.

### Visibility Testing

Before computing diffraction coefficients, we must verify that both the
source and the listener can "see" the diffracting edge (i.e., the edge is on
the geometric shadow boundary).

| Backend          | Method |
|------------------|--------|
| DXR / Vulkan RT  | Two inline ray queries per candidate edge (source→edge, edge→listener) |
| Software BVH     | Two ray casts through the CPU BVH; batched for SIMD efficiency |

Edge candidates are **pre-culled** using a conservative angular test against
the source–listener axis to avoid issuing rays for obviously irrelevant edges.

### Diffraction Coefficient Computation

The core computation follows the UTD wedge diffraction model:

```
D(ν) ≈ -(e^(-jπ/4)) / (2ν√(2πk)) × Σ cot((π ± (φ-φ'))/2n) × F(kLa±)
```

Where:
- `ν` = wedge index (π / wedge angle)
- `k` = wavenumber = 2πf / c
- `φ, φ'` = angles of incidence and diffraction
- `L` = distance parameter
- `F` = transition function (Fresnel integral)
- `a±` = detour parameters

For real-time use, we make the following simplifications:

1. **Pre-tabulated transition function** `F(x)` using a 256-entry LUT with
   linear interpolation.
2. **Per-band evaluation**: compute `D` at each of the 8 centre frequencies
   rather than continuously.
3. **Magnitude only**: we discard the phase term for the base quality tiers
   (phase is only used at Ultra tier for interference patterns).

### Cascade Depth

| Cascade Depth | Meaning |
|---------------|---------|
| Depth 1       | Sound diffracts around a single edge from source to listener |
| Depth 2       | Sound diffracts around two successive edges (e.g., around a corner and then over a wall) |

- **Default: Depth 1** – handles the vast majority of perceptually significant
  diffraction scenarios (doorways, single corners).
- **Configurable: Depth 2** – can be enabled via `MagnQualitySettings` for
  complex environments (e.g., L-shaped corridors).
- Depth 2 requires finding edge pairs, which multiplies candidate count.
  A spatial hash accelerates pair lookup to keep cost manageable.

### Quality Tiers

| Tier     | Method                        | Edge Budget | Cascade | Typical Cost |
|----------|-------------------------------|-------------|---------|-------------|
| **Low**  | Fresnel zone heuristic        | 4 / source  | 1       | ~5 µs / source |
| **Medium** | Single-edge UTD (magnitude) | 8 / source  | 1       | ~15 µs / source |
| **High** | Full UTD (magnitude)          | 16 / source | 1       | ~40 µs / source |
| **Ultra** | Cascaded UTD (mag + phase)   | 32 / source | 2       | ~120 µs / source |

#### Low Tier – Fresnel Zone Heuristic

Instead of full UTD, approximate diffraction loss using:

```
attenuation(f) ≈ clamp(1 - (detour / fresnel_radius(f)), 0, 1)
```

Where `fresnel_radius(f) = √(λ × d_s × d_l / (d_s + d_l))` and `detour` is
the path length difference via the edge vs. direct.  This is extremely cheap
(no trig, no LUT) and perceptually adequate for ambient sources.

#### Medium Tier – Single-Edge UTD

Full UTD magnitude computation for the single strongest diffracting edge per
source.  Remaining edges (up to budget) use the Fresnel heuristic.

#### High Tier – Full UTD

UTD magnitude for all edges within budget.  Produces the most accurate
per-band attenuation without phase.

#### Ultra Tier – Cascaded UTD with Phase

Full UTD with phase for depth-1 edges, magnitude-only for depth-2 cascades.
Enables constructive/destructive interference patterns at edges, which can be
audible for tonal sources (alarms, drones).

### RTAS Integration for Edge Visibility

When hardware RT is available (ADR-0004), edge visibility rays are dispatched
as part of the main ray batch in a single `DispatchRays` / compute pass.  This
amortizes the dispatch overhead and takes advantage of hardware ray–triangle
intersection.

The ray payload is minimal (1 bit: hit / no-hit), keeping bandwidth low.

### Output

Each visible, contributing edge produces a `MagnDiffractionTap` in the solver
output (ADR-0002):

```c
typedef struct MagnDiffractionTap {
    uint32_t edgeID;
    float    delay;              /* seconds, relative to direct sound */
    float    direction[3];       /* listener-space arrival direction */
    float    perBandAttenuation[8]; /* linear, 0 = fully attenuated */
    MagnEdgeMeta edgeMeta;       /* edge length, opening angle, material IDs */
} MagnDiffractionTap;
```

## Consequences

### Positive

- Perceptually convincing around-corner sound propagation without wave solvers.
- Four quality tiers cover the full hardware spectrum from mobile to PC.
- Pre-extracted static edges amortize the most expensive step (edge finding) to
  build time.
- RTAS integration keeps the GPU path coherent with the rest of the ray
  pipeline.

### Negative

- UTD is an asymptotic high-frequency approximation; it loses accuracy below
  ~200 Hz.  For game audio this is acceptable since low-frequency content is
  typically non-directional.
- Cascade depth 2 is significantly more expensive; must be gated by quality
  settings.
- Edge extraction requires good mesh topology; degenerate meshes (T-junctions,
  duplicate vertices) need pre-cleaning in the asset pipeline.

### Neutral

- The Fresnel heuristic (Low tier) is not physically rigorous but is a
  well-known game-audio approximation (used by Wwise Spatial Audio, Steam
  Audio, etc.).

## References

- Kouyoumjian & Pathak, "A Uniform Geometrical Theory of Diffraction for an
  Edge in a Perfectly Conducting Surface" (1974)
- Tsingos et al., "Perceptual Audio Rendering of Complex Virtual Environments"
  (2004)
- ADR-0002: Solver Output Contract (`MagnDiffractionTap`)
- ADR-0004: Acceleration Structure Strategy (RTAS for edge visibility)
- ADR-0005: Streaming Model (edge cache in chunk data)
- `docs/Performance-Tuning.md` – quality tier configuration
