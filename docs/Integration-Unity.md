# Magnaundasoni – Unity Integration Guide

This guide walks you through integrating the Magnaundasoni acoustics runtime
into a Unity project.

---

## Table of Contents

1. [Requirements](#requirements)
2. [Installation](#installation)
3. [Quick Start](#quick-start)
4. [Scene Setup](#scene-setup)
5. [Source Configuration](#source-configuration)
6. [Listener Configuration](#listener-configuration)
7. [Material Assignment](#material-assignment)
8. [Streaming Integration](#streaming-integration)
9. [Applying Acoustic Results](#applying-acoustic-results)
10. [Quality Settings](#quality-settings)
11. [Debug Visualization](#debug-visualization)
12. [Best Practices](#best-practices)
13. [Troubleshooting](#troubleshooting)

---

## Requirements

| Requirement       | Version            |
|-------------------|--------------------|
| Unity             | 2022.3 LTS or newer |
| Scripting Backend | IL2CPP (recommended) or Mono |
| Platform          | Windows, Linux, macOS, Android, iOS |

The Magnaundasoni native library (`magnaundasoni.dll` / `.so` / `.dylib`) must
be in the `Plugins/` folder of the Unity adapter package.

---

## Installation

### Via Unity Package Manager (Recommended)

1. Open **Window → Package Manager**.
2. Click **+ → Add package from git URL…**
3. Enter: `https://github.com/<org>/magnaundasoni-unity.git`
4. Click **Add**.

### Manual Installation

1. Download the latest release from the
   [Releases page](https://github.com/<org>/Project-Magnaundasoni/releases).
2. Copy the `Magnaundasoni/` folder into your project's `Packages/` directory.

---

## Quick Start

```csharp
using Magnaundasoni;
using UnityEngine;

public class AcousticsDemo : MonoBehaviour
{
    void Start()
    {
        // Initialize with default settings
        MagnEngine.Initialize();

        // Register materials
        MagnEngine.SetMaterialFromPreset(1, "Wood");
        MagnEngine.SetMaterialFromPreset(2, "Metal");
    }

    void Update()
    {
        // Tick the simulation each frame
        MagnEngine.Tick(Time.deltaTime);
    }

    void OnDestroy()
    {
        MagnEngine.Shutdown();
    }
}
```

---

## Scene Setup

### Fully Automatic Geometry Registration (Recommended)

By default, the Magnaundasoni engine **automatically discovers and registers**
all `MeshFilter` and `MeshRenderer` geometry in the scene—no manual component
placement required. When `MagnaundasoniEngine` initializes, it scans the scene
for all renderable meshes marked as **Static** (or with any `StaticFlags` set)
and registers them using the "General" material preset. Dynamic objects with
a `Rigidbody` are registered as `DynamicImportant`.

```csharp
// In the engine settings (Inspector), enable auto-scan:
// [✓] Auto Register Scene Geometry
// [✓] Auto Register On Scene Load

// That's it! All meshes are registered automatically.
// No component placement needed for basic setups.
```

**Auto-scan behavior:**
- All `MeshFilter` objects with `MeshRenderer` in the scene are discovered.
- Geometry marked as `Static` in the editor is registered as static acoustic
  geometry with the "General" material preset.
- Non-static objects with a `Rigidbody` are registered as dynamic geometry.
- When a new scene loads (additively or otherwise), new geometry is
  automatically registered.
- Objects destroyed at runtime are automatically unregistered.

### Per-Object Overrides (Optional)

For objects that need specific material presets, dynamic flags, or exclusion
from acoustic processing, add the `MagnaundasoniGeometry` component to
override auto-scan defaults:

```csharp
// Adding MagnaundasoniGeometry to a wall lets you set:
// - Material preset: "Wood", "Metal", "Carpet", etc.
// - Dynamic flag: Static, QuasiStatic, DynamicImportant, DynamicMinor
// - Exclude from acoustics: toggle off to skip this mesh
```

### Manual Geometry Registration

For custom meshes, procedural geometry, or full manual control:

```csharp
var desc = new MagnGeometryDesc
{
    Vertices = myVertexArray,     // Vector3[]
    Indices = myIndexArray,       // int[]
    MaterialIDs = myMatIDArray,   // uint[]
    Bounds = myAABB               // Bounds
};

MagnEngine.RegisterGeometry(myChunkID, desc);
```

---

## Source Configuration

Add the `MagnAcousticSource` component to any GameObject that emits sound.

```csharp
using Magnaundasoni;
using UnityEngine;

[RequireComponent(typeof(AudioSource))]
public class MagnAcousticSource : MonoBehaviour
{
    [Range(0f, 1f)]
    public float priority = 0.5f;

    public MagnDirectivityPattern directivity = MagnDirectivityPattern.Omnidirectional;

    private uint sourceID;
    private AudioSource audioSource;

    void OnEnable()
    {
        audioSource = GetComponent<AudioSource>();
        sourceID = MagnIDAllocator.NextSourceID();

        var desc = new MagnSourceDesc
        {
            Position = transform.position,
            Forward = transform.forward,
            Up = transform.up,
            DirectivityPattern = (uint)directivity,
            Priority = priority,
            Active = true
        };

        MagnEngine.RegisterSource(sourceID, desc);
    }

    void Update()
    {
        MagnEngine.UpdateSource(sourceID, transform.position, transform.forward, transform.up);

        // Apply acoustic results to the Unity AudioSource
        var parameters = MagnEngine.GetSourceParameters(0, sourceID);
        ApplyParameters(parameters);
    }

    void OnDisable()
    {
        MagnEngine.DeregisterSource(sourceID);
    }

    private void ApplyParameters(MagnSourceParameterSet p)
    {
        // Volume (convert from dB to linear)
        audioSource.volume = Mathf.Pow(10f, p.DirectGainDB / 20f);

        // Occlusion low-pass filter
        var lpf = audioSource.GetComponent<AudioLowPassFilter>();
        if (lpf != null)
        {
            lpf.cutoffFrequency = p.OcclusionLPFCutoff;
        }

        // Spatial blend (ensure 3D)
        audioSource.spatialBlend = 1.0f;

        // Reverb zone mix (maps to reverb send)
        audioSource.reverbZoneMix = Mathf.Pow(10f, p.ReverbSendDB / 20f);

        // Spread
        audioSource.spread = p.SpreadDegrees;
    }
}
```

---

## Listener Configuration

Add `MagnAcousticListener` to the main camera or audio listener.

```csharp
using Magnaundasoni;
using UnityEngine;

[RequireComponent(typeof(AudioListener))]
public class MagnAcousticListener : MonoBehaviour
{
    private uint listenerID;

    void OnEnable()
    {
        listenerID = MagnIDAllocator.NextListenerID();

        var desc = new MagnListenerDesc
        {
            Position = transform.position,
            Forward = transform.forward,
            Up = transform.up
        };

        MagnEngine.RegisterListener(listenerID, desc);
    }

    void Update()
    {
        MagnEngine.UpdateListener(listenerID, transform.position, transform.forward, transform.up);
    }

    void OnDisable()
    {
        MagnEngine.DeregisterListener(listenerID);
    }
}
```

---

## Material Assignment

### Using the Unity Editor (Recommended)

Materials can be created and assigned entirely within the Unity Editor:

1. **Create a material asset:** Right-click in the Project panel →
   **Create → Magnaundasoni → Acoustic Material**.
2. **Select a preset** from the dropdown in the Inspector (General, Metal,
   Wood, Fabric, Rock, Dirt, Grass, Carpet, etc.).
3. **Fine-tune per-band values** using the visual sliders and frequency
   response curve display in the Inspector.
4. **Assign the material** by dragging it onto any `MagnaundasoniGeometry`
   component's "Acoustic Material" field—or use the auto-scan's per-layer
   material mapping in the engine settings.

Material editing works in **both Edit mode and Play mode**.  Changes made in
Edit mode are saved as asset modifications.  Changes in Play mode update the
simulation immediately for live preview.

### Editor Menu Tools

- **Magnaundasoni → Create Material Preset** – batch-create materials from
  all built-in presets.
- **Magnaundasoni → Assign Materials by Layer** – opens a window to map Unity
  layers to acoustic material presets (e.g., Layer "Walls" → "Concrete").
- **Magnaundasoni → Material Auditor** – shows all acoustic materials in the
  scene and flags objects without an assigned material.

### Using Code (Presets)

```csharp
// Assign a built-in preset
MagnEngine.SetMaterialFromPreset(materialID: 1, preset: "Wood");
MagnEngine.SetMaterialFromPreset(materialID: 2, preset: "Metal");
MagnEngine.SetMaterialFromPreset(materialID: 3, preset: "Carpet");
```

### Using Code (Custom Materials)

```csharp
var customMaterial = new MagnMaterial
{
    Absorption = new float[] { 0.05f, 0.06f, 0.08f, 0.10f, 0.12f, 0.14f, 0.16f, 0.18f },
    Transmission = new float[] { 0.001f, 0.001f, 0.001f, 0.001f, 0.001f, 0.001f, 0.002f, 0.002f },
    Scattering = new float[] { 0.10f, 0.12f, 0.14f, 0.16f, 0.18f, 0.20f, 0.22f, 0.24f },
    Roughness = 0.1f,
    ThicknessClass = MagnThicknessClass.Thick,
    LeakageHint = 0.0f,
    CategoryTag = "CustomGlass"
};

MagnEngine.SetMaterial(materialID: 10, customMaterial);
```

---

## Streaming Integration

### Addressables-Based Streaming

```csharp
using UnityEngine.AddressableAssets;
using UnityEngine.ResourceManagement.AsyncOperations;
using Magnaundasoni;

public class MagnStreamingManager : MonoBehaviour
{
    void OnEnable()
    {
        // Hook into Addressables scene loading
        Addressables.ResourceManager.RegisterDiagnosticCallback(OnResourceEvent);
    }

    public void LoadAcousticChunk(string sceneAddress, ulong chunkID)
    {
        var handle = Addressables.LoadSceneAsync(sceneAddress, 
            UnityEngine.SceneManagement.LoadSceneMode.Additive);
        handle.Completed += (op) =>
        {
            if (op.Status == AsyncOperationStatus.Succeeded)
            {
                // Extract geometry from loaded scene and register
                var roots = op.Result.Scene.GetRootGameObjects();
                foreach (var root in roots)
                {
                    var meshes = root.GetComponentsInChildren<MeshFilter>();
                    foreach (var mf in meshes)
                    {
                        var desc = MagnMeshUtility.CreateGeometryDesc(
                            mf.sharedMesh, mf.transform, "General");
                        MagnEngine.RegisterGeometry(chunkID, desc);
                    }
                }
            }
        };
    }

    public void UnloadAcousticChunk(ulong chunkID)
    {
        MagnEngine.UnregisterGeometry(chunkID);
    }
}
```

---

## Applying Acoustic Results

### Simple Parameter Mode

The `MagnSourceParameterSet` provides pre-computed values suitable for driving
Unity's built-in audio system:

```csharp
var p = MagnEngine.GetSourceParameters(listenerID, sourceID);

// Direct gain → AudioSource.volume
audioSource.volume = MagnMath.DBToLinear(p.DirectGainDB);

// Occlusion → AudioLowPassFilter.cutoffFrequency
lowPassFilter.cutoffFrequency = p.OcclusionLPFCutoff;

// Reverb send → AudioSource.reverbZoneMix
audioSource.reverbZoneMix = MagnMath.DBToLinear(p.ReverbSendDB);

// Panning → AudioSource position override (for custom spatializers)
```

### Advanced: Raw Acoustic State

For maximum control (custom spatializers, ambisonics):

```csharp
var state = MagnEngine.GetAcousticState(listenerID);

// Direct component per source
for (int i = 0; i < state.SourceCount; i++)
{
    var direct = state.DirectComponents[i];
    // Access: direct.PerBandDirectGain[0..7]
    // Access: direct.DirectDirection (Vector3)
    // Access: direct.Confidence
}

// Late field
var late = state.LateField;
// Access: late.RT60[0..7], late.RoomSizeEstimate
```

---

## Quality Settings

```csharp
// Use a built-in quality preset
MagnEngine.SetQuality(MagnQualityLevel.High);

// Or fine-tune individual parameters
var settings = MagnEngine.GetQualitySettings();
settings.RaysPerSource = 128;
settings.MaxReflectionOrder = 2;
settings.MaxDiffractionDepth = 1;
settings.DetailedZoneRadius = 25.0f;
MagnEngine.SetQualitySettings(settings);
```

---

## Debug Visualization

```csharp
using Magnaundasoni;
using UnityEngine;

public class MagnDebugDraw : MonoBehaviour
{
    [SerializeField] private bool showRays = true;
    [SerializeField] private bool showDiffraction = true;
    [SerializeField] private bool showReflections = true;

    void OnDrawGizmos()
    {
        if (!Application.isPlaying) return;

        MagnDebugFlags flags = MagnDebugFlags.None;
        if (showRays) flags |= MagnDebugFlags.Rays;
        if (showDiffraction) flags |= MagnDebugFlags.Diffraction;
        if (showReflections) flags |= MagnDebugFlags.Reflections;

        MagnEngine.DebugVisualize(flags, (start, end, color) =>
        {
            Gizmos.color = MagnColorUtility.FromPacked(color);
            Gizmos.DrawLine(start, end);
        });
    }
}
```

---

## Best Practices

1. **Initialize early** – Call `MagnEngine.Initialize()` in `Awake()` or a
   bootstrap scene.
2. **Tick in `Update()`** – Not `FixedUpdate()`.  The acoustic simulation
   benefits from matching the visual frame rate.
3. **Limit active sources** – Only the highest-priority sources consume
   simulation budget.  Use `MagnAcousticSource.priority` and
   `MagnEngine.SetSourceActive()` to manage.
4. **Use proxy geometry for dynamics** – Don't pass full render meshes for
   moving objects; use simplified colliders or convex hulls.
5. **Match chunk boundaries to Addressables** – One acoustic chunk per
   Addressable scene group minimizes management complexity.
6. **Profile regularly** – Use `MagnEngine.GetStats()` to monitor ray budgets,
   tick times, and chunk counts.

---

## Troubleshooting

| Problem | Cause | Solution |
|---------|-------|----------|
| No acoustic effect | Engine not initialized | Ensure `MagnEngine.Initialize()` is called before `Tick()` |
| Sounds cut out abruptly | Geometry not registered | Enable auto-scan or add `MagnaundasoniGeometry` to wall/floor meshes |
| High GPU usage from ray tracing | Too many active sources or rays on GPU RT backend | Reduce `RaysPerSource`, lower quality level, or limit active sources |
| Falling back to CPU BVH | GPU RT not available on hardware | Expected on non-RTX/non-RDNA2 hardware; the software BVH fallback is automatic and correct. Upgrade GPU for best performance |
| Popping on scene load | Chunk not loaded before listener enters | Use predictive loading (see Streaming section) |
| `MAGN_ERROR_BACKEND_FAILURE` | GPU driver issue | Try `MagnInitConfig.ForceBackend = SoftwareBVH` as a temporary workaround |

> **Note:** Magnaundasoni uses **GPU-accelerated ray tracing** (DXR / Vulkan RT)
> as the primary backend whenever the hardware supports it. The CPU-based
> software BVH is an automatic fallback for non-RT-capable hardware. The engine
> detects hardware capabilities at startup and selects the best available path.
> You can force a specific backend via `MagnEngineConfig.preferredBackend`.

---

## References

- [`docs/API.md`](API.md) – Full API reference
- [`docs/Material-Presets.md`](Material-Presets.md) – Material preset values
- [`docs/Performance-Tuning.md`](Performance-Tuning.md) – Performance guide
- [`docs/design/architecture.md`](design/architecture.md) – Architecture overview
