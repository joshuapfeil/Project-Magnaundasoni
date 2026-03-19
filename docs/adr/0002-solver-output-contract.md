# ADR-0002: Solver Output Contract

| Field       | Value                        |
|-------------|------------------------------|
| Status      | Accepted                     |
| Date        | 2025-01-15                   |
| Deciders    | Core Team                    |
| Category    | API / Data Contract          |

## Context

Every downstream consumer—built-in renderer, Unity adapter, Unreal adapter,
third-party integrations—needs a **single, stable, versioned** description of
what the acoustic solver produces each frame.  Without a canonical contract,
each adapter invents its own interpretation, leading to drift and bugs.

The contract must be rich enough for high-quality spatialization yet cheap
enough to copy/serialize every frame at real-time rates.

## Decision

All solver output is organized into a single `MagnAcousticState` structure per
listener, containing per-source results and a global acoustic field descriptor.

### Band Layout

All per-band arrays use **8 octave bands**:

| Index | 0     | 1      | 2      | 3      | 4     | 5     | 6     | 7     |
|-------|-------|--------|--------|--------|-------|-------|-------|-------|
| Freq  | 63 Hz | 125 Hz | 250 Hz | 500 Hz | 1 kHz | 2 kHz | 4 kHz | 8 kHz |

### Per-Source Direct Component

```c
typedef struct MagnDirectComponent {
    uint32_t sourceID;

    /* Propagation delay in seconds from source to listener */
    float    directDelay;

    /* Unit direction vector (listener-space) of the direct arrival */
    float    directDirection[3];

    /* Per-band linear gain after distance, air absorption, directivity */
    float    perBandDirectGain[8];

    /* Descriptor for the occlusion low-pass / band-reject filter */
    MagnFilterDescriptor occlusionFilterDescriptor;

    /* Index into the directivity pattern table; 0 = omnidirectional */
    uint32_t directivityPatternDescriptor;

    /* Solver confidence [0,1]; low when source is in a transitional zone */
    float    confidence;
} MagnDirectComponent;
```

### Early Reflections

A variable-length list of reflection taps or clusters, sorted by delay.

```c
typedef struct MagnReflectionTap {
    uint32_t tapID;

    /* Delay in seconds relative to direct sound */
    float    delay;

    /* Arrival direction (listener-space) */
    float    direction[3];

    /* Per-band energy (linear) of this tap */
    float    perBandEnergy[8];

    /* Reflection order (1 = first order, 2 = second order, …) */
    uint8_t  order;

    /* Temporal stability [0,1]; high = consistent across frames */
    float    stability;
} MagnReflectionTap;

typedef struct MagnEarlyReflections {
    uint32_t          sourceID;
    uint32_t          tapCount;
    MagnReflectionTap taps[];   /* flexible array member */
} MagnEarlyReflections;
```

### Diffraction

Edge-based taps produced by the diffraction solver.

```c
typedef struct MagnDiffractionTap {
    uint32_t edgeID;

    /* Delay in seconds relative to direct sound */
    float    delay;

    /* Arrival direction (listener-space) */
    float    direction[3];

    /* Per-band attenuation coefficients (linear, 0-1) */
    float    perBandAttenuation[8];

    /* Metadata: edge length, opening angle, material IDs of adjacent faces */
    MagnEdgeMeta edgeMeta;
} MagnDiffractionTap;

typedef struct MagnDiffraction {
    uint32_t            sourceID;
    uint32_t            edgeCount;
    MagnDiffractionTap  edges[];  /* flexible array member */
} MagnDiffraction;
```

### Late Field Descriptor

One per listener, describing the diffuse reverberant field.

```c
typedef struct MagnLateField {
    /* Per-band exponential decay rate (1/seconds) */
    float perBandDecay[8];

    /* RT60 per band in seconds */
    float RT60[8];

    /* Estimated room volume (m³) or equivalent room radius */
    float roomSizeEstimate;

    /* Directional diffuseness [0,1]; 0 = highly directional, 1 = isotropic */
    float diffuseDirectionality;

    /* Descriptor for driving a reverb send (wet level, pre-delay, etc.) */
    MagnReverbSendDescriptor reverbSendDescriptor;
} MagnLateField;
```

### Global Frame Metadata

```c
typedef struct MagnFrameGlobal {
    /* Active quality level for this frame (may adapt dynamically) */
    MagnQualityLevel activeQualityLevel;

    /* Which compute backend was used: MAGN_BACKEND_DXR, _VK_RT, _SW_BVH */
    MagnBackend      backendUsed;

    /* Monotonic timestamp in microseconds */
    uint64_t         timestamp;
} MagnFrameGlobal;
```

### Top-Level State

```c
typedef struct MagnAcousticState {
    MagnFrameGlobal       global;
    MagnLateField         lateField;

    uint32_t              sourceCount;
    MagnDirectComponent*  directComponents;   /* [sourceCount] */
    MagnEarlyReflections* earlyReflections;   /* [sourceCount] */
    MagnDiffraction*      diffractions;       /* [sourceCount] */
} MagnAcousticState;
```

### Versioning

The contract carries a `MAGN_OUTPUT_CONTRACT_VERSION` integer (currently `1`).
Any breaking change to field layout, band count, or semantics increments this
version.  Adapters must check the version on `magn_init()` and fail clearly if
mismatched.

## Consequences

### Positive

- Single source of truth consumed by all adapters.
- Flexible array members keep memory contiguous—good for cache and SIMD.
- Band count (8) balances perceptual resolution against compute cost.
- Confidence and stability fields allow adapters to implement smoothing heuristics.

### Negative

- 8 bands is a fixed choice; some use-cases (e.g., ultra-low frequency
  simulation) may want more.  Mitigated by versioning—a future contract version
  could extend.
- Flexible array members require careful allocation; adapters must use the
  provided allocator or copy into their own buffers.

### Neutral

- All numeric values are **linear scale** unless explicitly noted.  Adapters
  converting to dB must apply `20*log10()` themselves.

## References

- ADR-0001: Product Boundary
- ADR-0003: Material Schema (absorption/transmission bands align with this
  contract)
- `docs/design/api.md` – C ABI functions that populate and query this contract
