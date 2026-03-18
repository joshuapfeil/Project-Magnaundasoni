# Magnaundasoni – Material Presets Reference

This document lists all built-in material presets with their numeric
coefficients across 8 octave bands.

---

## Band Frequencies

| Index | 0      | 1      | 2      | 3     | 4     | 5     | 6     | 7      |
|-------|--------|--------|--------|-------|-------|-------|-------|--------|
| Freq  | 125 Hz | 250 Hz | 500 Hz | 1 kHz | 2 kHz | 4 kHz | 8 kHz | 16 kHz |

All coefficients are **linear** values in the range [0, 1].

---

## Absorption Coefficients

Fraction of incident sound energy absorbed by the surface at each frequency
band.  Higher values = more absorption (less reflection).

| Material   | 125 Hz | 250 Hz | 500 Hz | 1 kHz | 2 kHz | 4 kHz | 8 kHz | 16 kHz |
|------------|--------|--------|--------|-------|-------|-------|-------|--------|
| **General**| 0.10   | 0.10   | 0.10   | 0.10  | 0.10  | 0.10  | 0.10  | 0.10   |
| **Metal**  | 0.01   | 0.01   | 0.02   | 0.02  | 0.02  | 0.03  | 0.04  | 0.04   |
| **Wood**   | 0.15   | 0.11   | 0.10   | 0.07  | 0.06  | 0.07  | 0.08  | 0.08   |
| **Fabric** | 0.03   | 0.04   | 0.11   | 0.17  | 0.24  | 0.35  | 0.44  | 0.45   |
| **Rock**   | 0.02   | 0.02   | 0.03   | 0.04  | 0.05  | 0.05  | 0.05  | 0.06   |
| **Dirt**   | 0.15   | 0.25   | 0.40   | 0.55  | 0.60  | 0.60  | 0.60  | 0.55   |
| **Grass**  | 0.11   | 0.26   | 0.60   | 0.69  | 0.92  | 0.99  | 0.99  | 0.95   |
| **Carpet** | 0.08   | 0.24   | 0.57   | 0.69  | 0.71  | 0.73  | 0.72  | 0.70   |

### Interpretation

- **Metal** has very low absorption—most energy is reflected, making metallic
  environments highly reverberant.
- **Grass** and **Dirt** are highly absorptive at mid/high frequencies,
  simulating open outdoor environments with short reverb times.
- **Carpet** is strongly absorptive above 500 Hz, typical of furnished indoor
  spaces.
- **General** is a flat 10% default for quick prototyping.

---

## Transmission Coefficients

Fraction of sound energy that passes through the surface.  Higher values = more
transmission (less isolation).

| Material   | 125 Hz | 250 Hz | 500 Hz | 1 kHz | 2 kHz | 4 kHz | 8 kHz | 16 kHz |
|------------|--------|--------|--------|-------|-------|-------|-------|--------|
| **General**| 0.10   | 0.08   | 0.06   | 0.04  | 0.03  | 0.02  | 0.02  | 0.01   |
| **Metal**  | 0.001  | 0.001  | 0.001  | 0.001 | 0.001 | 0.001 | 0.002 | 0.002  |
| **Wood**   | 0.06   | 0.04   | 0.03   | 0.02  | 0.02  | 0.01  | 0.01  | 0.01   |
| **Fabric** | 0.30   | 0.25   | 0.20   | 0.15  | 0.12  | 0.10  | 0.08  | 0.07   |
| **Rock**   | 0.001  | 0.001  | 0.001  | 0.001 | 0.001 | 0.001 | 0.001 | 0.001  |
| **Dirt**   | 0.08   | 0.06   | 0.04   | 0.03  | 0.02  | 0.02  | 0.01  | 0.01   |
| **Grass**  | 0.20   | 0.15   | 0.10   | 0.08  | 0.06  | 0.04  | 0.03  | 0.02   |
| **Carpet** | 0.12   | 0.09   | 0.06   | 0.04  | 0.03  | 0.02  | 0.02  | 0.01   |

### Interpretation

- **Metal** and **Rock** are nearly opaque—minimal energy passes through.
- **Fabric** has high transmission, modeling thin curtains, tapestries, or
  cloth dividers.
- **Grass** transmits some low-frequency energy, modeling thin ground cover
  over open terrain.

---

## Scattering Coefficients

Fraction of reflected energy that is scattered diffusely (as opposed to
specularly reflected).  Higher values = more diffuse scattering.

| Material   | 125 Hz | 250 Hz | 500 Hz | 1 kHz | 2 kHz | 4 kHz | 8 kHz | 16 kHz |
|------------|--------|--------|--------|-------|-------|-------|-------|--------|
| **General**| 0.10   | 0.12   | 0.15   | 0.20  | 0.25  | 0.30  | 0.35  | 0.40   |
| **Metal**  | 0.05   | 0.05   | 0.05   | 0.05  | 0.06  | 0.07  | 0.08  | 0.10   |
| **Wood**   | 0.10   | 0.12   | 0.15   | 0.18  | 0.22  | 0.28  | 0.35  | 0.40   |
| **Fabric** | 0.30   | 0.35   | 0.40   | 0.50  | 0.55  | 0.60  | 0.65  | 0.70   |
| **Rock**   | 0.20   | 0.25   | 0.30   | 0.35  | 0.40  | 0.45  | 0.50  | 0.55   |
| **Dirt**   | 0.25   | 0.30   | 0.40   | 0.50  | 0.55  | 0.60  | 0.65  | 0.70   |
| **Grass**  | 0.30   | 0.40   | 0.50   | 0.60  | 0.70  | 0.80  | 0.85  | 0.90   |
| **Carpet** | 0.20   | 0.30   | 0.40   | 0.50  | 0.60  | 0.65  | 0.70  | 0.75   |

### Interpretation

- **Metal** is highly specular (smooth surfaces produce clear reflections).
- **Grass** and **Carpet** scatter aggressively, diffusing reflected energy.
- Scattering generally increases with frequency across all materials
  (shorter wavelengths interact more with surface texture).

---

## Roughness, Thickness Class, and Leakage Hint

| Material   | Roughness | Thickness Class | Leakage Hint |
|------------|-----------|-----------------|--------------|
| **General**| 0.30      | MEDIUM          | 0.05         |
| **Metal**  | 0.05      | MEDIUM          | 0.00         |
| **Wood**   | 0.25      | MEDIUM          | 0.02         |
| **Fabric** | 0.70      | THIN            | 0.20         |
| **Rock**   | 0.40      | SOLID           | 0.00         |
| **Dirt**   | 0.80      | SOLID           | 0.00         |
| **Grass**  | 0.90      | THIN            | 0.30         |
| **Carpet** | 0.65      | THIN            | 0.10         |

### Field Descriptions

| Field | Range | Description |
|-------|-------|-------------|
| **Roughness** | 0.0–1.0 | Micro-scale surface irregularity. Affects the width of the diffuse scattering lobe. 0 = mirror-smooth, 1 = maximally rough. |
| **Thickness Class** | Enum | `THIN` (≤5 mm): curtains, carpet, thin panels. `MEDIUM` (5–50 mm): wood panels, metal sheets, drywall. `THICK` (50–200 mm): brick walls, thick glass. `SOLID` (>200 mm): bedrock, concrete walls, earthen banks. |
| **Leakage Hint** | 0.0–1.0 | Models imperfect acoustic seals. 0 = perfectly sealed. Higher values simulate gaps around doors, open window frames, or porous materials. Applied as additional transmission energy. |

---

## Using Presets in Code

### C ABI

```c
/* Load a preset by name */
MagnResult r = magn_set_material_from_preset(1, "Wood");

/* Or get a copy to modify */
MagnMaterial mat;
magn_material_from_preset("Metal", &mat);
mat.absorption[0] = 0.05f;  /* Override 125 Hz absorption */
magn_set_material(2, &mat);
```

### Unity (C#)

```csharp
MagnEngine.SetMaterialFromPreset(1, "Wood");

var mat = MagnEngine.GetPresetMaterial("Metal");
mat.Absorption[0] = 0.05f;
MagnEngine.SetMaterial(2, mat);
```

### Unreal (C++)

```cpp
auto* Subsystem = GetWorld()->GetSubsystem<UMagnaundasoniSubsystem>();
Subsystem->SetMaterialFromPreset(1, TEXT("Wood"));

FMagnMaterial Mat;
Subsystem->GetPresetMaterial(TEXT("Metal"), Mat);
Mat.Absorption[0] = 0.05f;
Subsystem->SetMaterial(2, Mat);
```

---

## Custom Material Guidelines

When creating custom materials:

1. **Start from a preset** – Copy the closest matching preset and adjust
   individual bands.
2. **Ensure energy conservation** – For each band:
   `absorption + reflection ≤ 1.0` where `reflection = 1.0 - absorption`.
   Transmission is additional energy transfer, so
   `absorption + transmission ≤ 1.0` should generally hold.
3. **Test with the debug visualizer** – Use ray visualization to verify that
   reflections and transmission behave as expected.
4. **Profile scattering** – High scattering on large surfaces significantly
   increases the number of diffuse ray bounces.

---

## References

- [ADR-0003: Material Schema](adr/0003-material-schema.md) – Technical
  specification
- [ADR-0002: Solver Output Contract](adr/0002-solver-output-contract.md) –
  How materials feed into simulation output
- [`docs/API.md`](API.md) – Material API functions
- ISO 354: Sound absorption measurement
- ISO 10140: Sound insulation measurement
