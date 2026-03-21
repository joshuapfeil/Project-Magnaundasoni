# Magnaundasoni – Material Presets Reference

This document lists all built-in material presets with their numeric
coefficients across 8 octave bands.

---

## Band Frequencies

| Index | 0     | 1      | 2      | 3      | 4     | 5     | 6     | 7     |
|-------|-------|--------|--------|--------|-------|-------|-------|-------|
| Freq  | 63 Hz | 125 Hz | 250 Hz | 500 Hz | 1 kHz | 2 kHz | 4 kHz | 8 kHz |

All coefficients are **linear** values in the range [0, 1].

---

## Absorption Coefficients

Fraction of incident sound energy absorbed by the surface at each frequency
band.  Higher values = more absorption (less reflection).

| Material     | 63 Hz  | 125 Hz | 250 Hz | 500 Hz | 1 kHz | 2 kHz | 4 kHz | 8 kHz |
|--------------|--------|--------|--------|--------|-------|-------|-------|-------|
| **General**  | 0.10   | 0.12   | 0.14   | 0.16   | 0.18  | 0.20  | 0.22  | 0.24  |
| **Metal**    | 0.01   | 0.01   | 0.02   | 0.02   | 0.03  | 0.04  | 0.05  | 0.05  |
| **Wood**     | 0.15   | 0.11   | 0.10   | 0.07   | 0.06  | 0.07  | 0.08  | 0.09  |
| **Fabric**   | 0.03   | 0.04   | 0.11   | 0.17   | 0.24  | 0.35  | 0.45  | 0.50  |
| **Rock**     | 0.02   | 0.02   | 0.03   | 0.04   | 0.05  | 0.05  | 0.05  | 0.05  |
| **Dirt**     | 0.15   | 0.25   | 0.40   | 0.55   | 0.60  | 0.60  | 0.60  | 0.60  |
| **Grass**    | 0.11   | 0.26   | 0.60   | 0.69   | 0.92  | 0.99  | 0.99  | 0.99  |
| **Carpet**   | 0.05   | 0.10   | 0.20   | 0.35   | 0.50  | 0.65  | 0.60  | 0.55  |
| **Glass**    | 0.35   | 0.25   | 0.18   | 0.12   | 0.07  | 0.05  | 0.05  | 0.05  |
| **Concrete** | 0.01   | 0.01   | 0.02   | 0.02   | 0.02  | 0.03  | 0.04  | 0.04  |
| **Plaster**  | 0.01   | 0.02   | 0.02   | 0.03   | 0.04  | 0.05  | 0.05  | 0.05  |
| **Water**    | 0.01   | 0.01   | 0.01   | 0.02   | 0.02  | 0.03  | 0.03  | 0.04  |

### Interpretation

- **Metal** and **Concrete** have very low absorption—most energy is reflected,
  making these environments highly reverberant.
- **Grass** and **Dirt** are highly absorptive at mid/high frequencies,
  simulating open outdoor environments with short reverb times.
- **Carpet** peaks around 2 kHz then decreases, typical of furnished indoor
  spaces.
- **Glass** has high absorption at low frequencies due to panel resonance, then
  drops off at higher frequencies.
- **General** rises gradually from 10% to 24% across the band range.

---

## Transmission Coefficients

Fraction of sound energy that passes through the surface.  Higher values = more
transmission (less isolation).

| Material     | 63 Hz  | 125 Hz | 250 Hz | 500 Hz | 1 kHz | 2 kHz | 4 kHz | 8 kHz |
|--------------|--------|--------|--------|--------|-------|-------|-------|-------|
| **General**  | 0.05   | 0.04   | 0.03   | 0.02   | 0.02  | 0.01  | 0.01  | 0.01  |
| **Metal**    | 0.00   | 0.00   | 0.00   | 0.00   | 0.00  | 0.00  | 0.00  | 0.00  |
| **Wood**     | 0.04   | 0.03   | 0.02   | 0.02   | 0.01  | 0.01  | 0.01  | 0.01  |
| **Fabric**   | 0.15   | 0.12   | 0.10   | 0.08   | 0.06  | 0.04  | 0.03  | 0.02  |
| **Rock**     | 0.00   | 0.00   | 0.00   | 0.00   | 0.00  | 0.00  | 0.00  | 0.00  |
| **Dirt**     | 0.02   | 0.02   | 0.01   | 0.01   | 0.01  | 0.00  | 0.00  | 0.00  |
| **Grass**    | 0.10   | 0.08   | 0.05   | 0.03   | 0.02  | 0.01  | 0.01  | 0.01  |
| **Carpet**   | 0.10   | 0.08   | 0.06   | 0.04   | 0.03  | 0.02  | 0.02  | 0.01  |
| **Glass**    | 0.08   | 0.06   | 0.04   | 0.03   | 0.02  | 0.02  | 0.01  | 0.01  |
| **Concrete** | 0.00   | 0.00   | 0.00   | 0.00   | 0.00  | 0.00  | 0.00  | 0.00  |
| **Plaster**  | 0.01   | 0.01   | 0.01   | 0.00   | 0.00  | 0.00  | 0.00  | 0.00  |
| **Water**    | 0.90   | 0.85   | 0.80   | 0.70   | 0.60  | 0.50  | 0.40  | 0.30  |

### Interpretation

- **Metal**, **Rock**, and **Concrete** are nearly opaque—minimal energy passes
  through.
- **Fabric** has moderate transmission, modeling thin curtains, tapestries, or
  cloth dividers.
- **Grass** transmits some low-frequency energy, modeling thin ground cover
  over open terrain.
- **Water** has very high transmission, especially at low frequencies, modeling
  the acoustic properties of water surfaces and bodies.

---

## Scattering Coefficients

Fraction of reflected energy that is scattered diffusely (as opposed to
specularly reflected).  Higher values = more diffuse scattering.

| Material     | 63 Hz  | 125 Hz | 250 Hz | 500 Hz | 1 kHz | 2 kHz | 4 kHz | 8 kHz |
|--------------|--------|--------|--------|--------|-------|-------|-------|-------|
| **General**  | 0.10   | 0.12   | 0.15   | 0.18   | 0.22  | 0.25  | 0.28  | 0.30  |
| **Metal**    | 0.10   | 0.10   | 0.12   | 0.15   | 0.18  | 0.22  | 0.28  | 0.30  |
| **Wood**     | 0.10   | 0.12   | 0.14   | 0.18   | 0.22  | 0.28  | 0.32  | 0.35  |
| **Fabric**   | 0.10   | 0.15   | 0.20   | 0.30   | 0.40  | 0.50  | 0.55  | 0.60  |
| **Rock**     | 0.12   | 0.14   | 0.16   | 0.20   | 0.24  | 0.28  | 0.32  | 0.35  |
| **Dirt**     | 0.20   | 0.30   | 0.40   | 0.50   | 0.55  | 0.60  | 0.60  | 0.60  |
| **Grass**    | 0.30   | 0.40   | 0.50   | 0.60   | 0.70  | 0.80  | 0.85  | 0.90  |
| **Carpet**   | 0.10   | 0.15   | 0.25   | 0.40   | 0.55  | 0.65  | 0.70  | 0.75  |
| **Glass**    | 0.05   | 0.05   | 0.06   | 0.08   | 0.10  | 0.12  | 0.14  | 0.15  |
| **Concrete** | 0.10   | 0.11   | 0.12   | 0.14   | 0.16  | 0.20  | 0.24  | 0.28  |
| **Plaster**  | 0.10   | 0.12   | 0.14   | 0.16   | 0.20  | 0.24  | 0.28  | 0.30  |
| **Water**    | 0.05   | 0.06   | 0.08   | 0.10   | 0.12  | 0.15  | 0.18  | 0.20  |

### Interpretation

- **Glass** and **Water** are highly specular (smooth surfaces produce clear
  reflections).
- **Grass** and **Carpet** scatter aggressively, diffusing reflected energy.
- **Metal** has moderate scattering that increases with frequency.
- Scattering generally increases with frequency across all materials
  (shorter wavelengths interact more with surface texture).

---

## Roughness, Thickness Class, and Leakage Hint

| Material     | Roughness | Thickness Class | Leakage Hint |
|--------------|-----------|-----------------|--------------|
| **General**  | 0.50      | MEDIUM          | 0.05         |
| **Metal**    | 0.20      | MEDIUM          | 0.00         |
| **Wood**     | 0.40      | MEDIUM          | 0.02         |
| **Fabric**   | 0.80      | THIN            | 0.10         |
| **Rock**     | 0.60      | THICK           | 0.00         |
| **Dirt**     | 0.90      | THICK           | 0.01         |
| **Grass**    | 0.95      | THIN            | 0.05         |
| **Carpet**   | 0.85      | THIN            | 0.08         |
| **Glass**    | 0.10      | THIN            | 0.03         |
| **Concrete** | 0.30      | THICK           | 0.00         |
| **Plaster**  | 0.35      | MEDIUM          | 0.01         |
| **Water**    | 0.05      | THICK           | 0.50         |

### Field Descriptions

| Field | Range | Description |
|-------|-------|-------------|
| **Roughness** | 0.0–1.0 | Micro-scale surface irregularity. Affects the width of the diffuse scattering lobe. 0 = mirror-smooth, 1 = maximally rough. |
| **Thickness Class** | Enum | `THIN` (0): curtains, carpet, thin panels, glass. `MEDIUM` (1): wood panels, metal sheets, drywall, plaster. `THICK` (2): brick walls, bedrock, concrete walls, earthen banks. |
| **Leakage Hint** | 0.0–1.0 | Models imperfect acoustic seals. 0 = perfectly sealed. Higher values simulate gaps around doors, open window frames, or porous materials. Applied as additional transmission energy. |

---

## Using Presets in Code

### C ABI

```c
/* Load a preset by name and register it */
MagMaterialDesc mat;
mag_material_get_preset("Wood", &mat);
MagMaterialID matID = 0;
mag_material_register(engine, &mat, &matID);

/* Or get a copy, modify it, and register */
MagMaterialDesc custom;
mag_material_get_preset("Metal", &custom);
custom.absorption[0] = 0.05f;  /* Override 63 Hz absorption */
MagMaterialID customID = 0;
mag_material_register(engine, &custom, &customID);
```

### Unity (C#)

```csharp
var mat = MagAPI.MaterialGetPreset("Wood");
uint matID = MagAPI.MaterialRegister(engine, mat);

var custom = MagAPI.MaterialGetPreset("Metal");
custom.absorption[0] = 0.05f;
uint customID = MagAPI.MaterialRegister(engine, custom);
```

### Unreal (C++)

```cpp
FMagMaterialDescNative Mat;
Bridge.MaterialGetPreset("Wood", &Mat);
MagMaterialIDNative MatID = 0;
Bridge.MaterialRegister(Engine, &Mat, &MatID);

FMagMaterialDescNative Custom;
Bridge.MaterialGetPreset("Metal", &Custom);
Custom.absorption[0] = 0.05f;
MagMaterialIDNative CustomID = 0;
Bridge.MaterialRegister(Engine, &Custom, &CustomID);
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
