# ADR-0003: Material Schema

| Field       | Value                        |
|-------------|------------------------------|
| Status      | Accepted                     |
| Date        | 2025-01-15                   |
| Deciders    | Core Team                    |
| Category    | Data Model                   |

## Context

Acoustic simulation quality depends heavily on surface material properties.
We need a compact, extensible per-material record that is:

1. Rich enough to drive occlusion, transmission, scattering, and diffraction.
2. Small enough to store thousands of surfaces without blowing the cache.
3. Pre-populated with sensible defaults so users can prototype without
   hand-tuning every surface.

The record must align with the 8-band layout defined in ADR-0002.

## Decision

### Per-Material Record

```c
typedef struct MagnMaterial {
    /* Random-incidence absorption coefficient per band [0, 1] */
    float absorption[8];

    /* Transmission loss per band (linear, 0 = opaque, 1 = transparent) */
    float transmission[8];

    /* Scattering coefficient per band [0, 1] (0 = specular, 1 = diffuse) */
    float scattering[8];

    /* Surface roughness [0, 1]; affects diffuse scattering distribution */
    float roughness;

    /* Thickness class: THIN, MEDIUM, THICK, SOLID */
    MagnThicknessClass thicknessClass;

    /* Leakage hint [0, 1]; models imperfect seals (doors, windows) */
    float leakageHint;

    /* Human-readable category tag, null-terminated, max 31 chars + null */
    char  categoryTag[32];
} MagnMaterial;
```

| Field             | Type / Size | Description |
|-------------------|-------------|-------------|
| `absorption[8]`   | float × 8   | Fraction of incident energy absorbed at each band |
| `transmission[8]` | float × 8   | Fraction of energy passing through the surface |
| `scattering[8]`   | float × 8   | Fraction scattered diffusely vs. specularly |
| `roughness`        | float        | Micro-scale surface roughness for scattering lobe |
| `thicknessClass`   | enum         | `THIN` (≤5 mm), `MEDIUM` (5–50 mm), `THICK` (50–200 mm), `SOLID` (>200 mm) |
| `leakageHint`      | float        | Energy leak around imperfect boundaries |
| `categoryTag`      | char[32]     | Free-form label for tooling / debug |

### Band Frequencies

| Index | 0     | 1      | 2      | 3      | 4     | 5     | 6     | 7     |
|-------|-------|--------|--------|--------|-------|-------|-------|-------|
| Freq  | 63 Hz | 125 Hz | 250 Hz | 500 Hz | 1 kHz | 2 kHz | 4 kHz | 8 kHz |

### Default Material Presets

All values are **linear coefficients** in the range [0, 1].

#### Absorption Coefficients

| Material | 63 Hz | 125 Hz | 250 Hz | 500 Hz | 1 kHz | 2 kHz | 4 kHz | 8 kHz |
|----------|-------|--------|--------|--------|-------|-------|-------|-------|
| General  | 0.10   | 0.10   | 0.10   | 0.10  | 0.10  | 0.10  | 0.10  | 0.10   |
| Metal    | 0.01   | 0.01   | 0.02   | 0.02  | 0.02  | 0.03  | 0.04  | 0.04   |
| Wood     | 0.15   | 0.11   | 0.10   | 0.07  | 0.06  | 0.07  | 0.08  | 0.08   |
| Fabric   | 0.03   | 0.04   | 0.11   | 0.17  | 0.24  | 0.35  | 0.44  | 0.45   |
| Rock     | 0.02   | 0.02   | 0.03   | 0.04  | 0.05  | 0.05  | 0.05  | 0.06   |
| Dirt     | 0.15   | 0.25   | 0.40   | 0.55  | 0.60  | 0.60  | 0.60  | 0.55   |
| Grass    | 0.11   | 0.26   | 0.60   | 0.69  | 0.92  | 0.99  | 0.99  | 0.95   |
| Carpet   | 0.08   | 0.24   | 0.57   | 0.69  | 0.71  | 0.73  | 0.72  | 0.70   |

#### Transmission Coefficients

| Material | 63 Hz | 125 Hz | 250 Hz | 500 Hz | 1 kHz | 2 kHz | 4 kHz | 8 kHz |
|----------|-------|--------|--------|--------|-------|-------|-------|-------|
| General  | 0.10   | 0.08   | 0.06   | 0.04  | 0.03  | 0.02  | 0.02  | 0.01   |
| Metal    | 0.001  | 0.001  | 0.001  | 0.001 | 0.001 | 0.001 | 0.002 | 0.002  |
| Wood     | 0.06   | 0.04   | 0.03   | 0.02  | 0.02  | 0.01  | 0.01  | 0.01   |
| Fabric   | 0.30   | 0.25   | 0.20   | 0.15  | 0.12  | 0.10  | 0.08  | 0.07   |
| Rock     | 0.001  | 0.001  | 0.001  | 0.001 | 0.001 | 0.001 | 0.001 | 0.001  |
| Dirt     | 0.08   | 0.06   | 0.04   | 0.03  | 0.02  | 0.02  | 0.01  | 0.01   |
| Grass    | 0.20   | 0.15   | 0.10   | 0.08  | 0.06  | 0.04  | 0.03  | 0.02   |
| Carpet   | 0.12   | 0.09   | 0.06   | 0.04  | 0.03  | 0.02  | 0.02  | 0.01   |

#### Scattering Coefficients

| Material | 63 Hz | 125 Hz | 250 Hz | 500 Hz | 1 kHz | 2 kHz | 4 kHz | 8 kHz |
|----------|-------|--------|--------|--------|-------|-------|-------|-------|
| General  | 0.10   | 0.12   | 0.15   | 0.20  | 0.25  | 0.30  | 0.35  | 0.40   |
| Metal    | 0.05   | 0.05   | 0.05   | 0.05  | 0.06  | 0.07  | 0.08  | 0.10   |
| Wood     | 0.10   | 0.12   | 0.15   | 0.18  | 0.22  | 0.28  | 0.35  | 0.40   |
| Fabric   | 0.30   | 0.35   | 0.40   | 0.50  | 0.55  | 0.60  | 0.65  | 0.70   |
| Rock     | 0.20   | 0.25   | 0.30   | 0.35  | 0.40  | 0.45  | 0.50  | 0.55   |
| Dirt     | 0.25   | 0.30   | 0.40   | 0.50  | 0.55  | 0.60  | 0.65  | 0.70   |
| Grass    | 0.30   | 0.40   | 0.50   | 0.60  | 0.70  | 0.80  | 0.85  | 0.90   |
| Carpet   | 0.20   | 0.30   | 0.40   | 0.50  | 0.60  | 0.65  | 0.70  | 0.75   |

#### Roughness, Thickness, and Leakage

| Material | Roughness | Thickness Class | Leakage Hint |
|----------|-----------|-----------------|--------------|
| General  | 0.30      | MEDIUM          | 0.05         |
| Metal    | 0.05      | MEDIUM          | 0.00         |
| Wood     | 0.25      | MEDIUM          | 0.02         |
| Fabric   | 0.70      | THIN            | 0.20         |
| Rock     | 0.40      | SOLID           | 0.00         |
| Dirt     | 0.80      | SOLID           | 0.00         |
| Grass    | 0.90      | THIN            | 0.30         |
| Carpet   | 0.65      | THIN            | 0.10         |

### Preset Initialization

```c
/* Returns a read-only pointer to a built-in preset by tag name.
   Returns NULL if tag is not recognized. */
const MagnMaterial* magn_material_preset(const char* tag);

/* Copies a preset into a mutable material, allowing per-instance overrides. */
MagnResult magn_material_from_preset(const char* tag, MagnMaterial* out);
```

Users are encouraged to start from a preset and tweak individual bands rather
than specifying all 32 coefficients from scratch.

## Consequences

### Positive

- 8-band layout matches the solver output contract (ADR-0002)—no lossy
  resampling between material data and propagation results.
- Preset library lowers the barrier to entry; new users get plausible acoustics
  immediately.
- `leakageHint` handles common architectural gaps (door frames, open windows)
  without requiring explicit portal geometry.
- `thicknessClass` lets the transmission solver select appropriate models
  without requiring a continuous thickness parameter.

### Negative

- Fixed 8 bands may be insufficient for specialized scenarios (e.g., sub-bass
  heavy environments).  Mitigated by contract versioning (ADR-0002).
- Preset values are approximate and based on published acoustic data; real-world
  materials vary significantly.  Documentation must make this clear.

### Neutral

- The `categoryTag` is informational only; the solver never branches on it.

## References

- ADR-0002: Solver Output Contract
- ISO 354 – Measurement of sound absorption in a reverberation room
- ISO 10140 – Measurement of sound insulation of building elements
- `docs/Material-Presets.md` – user-facing preset reference
