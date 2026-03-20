# Magnaundasoni – Unreal Engine Integration Guide

This guide walks you through integrating the Magnaundasoni acoustics runtime
into an Unreal Engine 5 project.

---

## Table of Contents

1. [Requirements](#requirements)
2. [Installation](#installation)
3. [Enabling the Plugin in a Project](#enabling-the-plugin-in-a-project)
4. [Runtime Components Overview](#runtime-components-overview)
5. [Quick Start](#quick-start)
6. [Scene Setup](#scene-setup)
7. [Source Configuration](#source-configuration)
8. [Listener Configuration](#listener-configuration)
9. [Material Assignment](#material-assignment)
10. [World Partition Streaming](#world-partition-streaming)
11. [Applying Acoustic Results](#applying-acoustic-results)
12. [Quality Settings](#quality-settings)
13. [Debug Visualization](#debug-visualization)
14. [Demo Map](#demo-map)
15. [Best Practices](#best-practices)
16. [Troubleshooting](#troubleshooting)

---

## Requirements

| Requirement    | Version            |
|----------------|--------------------|
| Unreal Engine  | 5.2 or newer       |
| Platform       | Windows, Linux, Xbox Series X/S, PlayStation 5, Android |
| Build system   | C++ project (Blueprint-only projects must convert) |

---

## Installation

### Plugin folder structure

```
MyProject/
└── Plugins/
    └── Magnaundasoni/
        ├── Magnaundasoni.uplugin
        ├── Source/
        │   ├── Magnaundasoni/          (base module: native API bridge, engine lifecycle)
        │   └── MagnaundasoniRuntime/   (runtime module: Actor Components for use in game)
        └── Binaries/
            ├── Win64/magnaundasoni.dll
            ├── Linux/libmagnaundasoni.so
            └── Mac/libmagnaundasoni.dylib
```

1. Download the latest Magnaundasoni Unreal plugin from the
   [Releases page](https://github.com/joshuapfeil/Project-Magnaundasoni/releases).
2. Copy the `unreal/Plugin/` folder from the repo into your project's
   `Plugins/Magnaundasoni/` directory (see structure above).
3. Place the compiled native library binaries in the `Binaries/<Platform>/`
   folder (built from `native/CMakeLists.txt`).
4. Regenerate project files and rebuild in your IDE.

### Build.cs Configuration

In your game module's `.Build.cs`, add the **MagnaundasoniRuntime** module
(which pulls in the base module automatically):

```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core",
    "CoreUObject",
    "Engine",
    "MagnaundasoniRuntime"   // Actor Components (UMagSourceComponent, etc.)
});
```

---

## Enabling the Plugin in a Project

1. Open your project in Unreal Engine.
2. Go to **Edit → Plugins**.
3. Search for **Magnaundasoni** in the search box.
4. Tick the **Enabled** checkbox.
5. Restart the editor when prompted.

Alternatively, add this to your project's `DefaultEngine.ini`:

```ini
[/Script/Engine.PluginsSection]
+EnabledPlugins=Magnaundasoni
```

Verify the plugin loaded successfully by checking the **Output Log** for:

```
LogMagnaundasoniRuntime: Started. Function table resolved. Native engine: valid.
```

If you see `Native engine: null`, the native DLL is missing — see
[Troubleshooting](#troubleshooting).

---

## Runtime Components Overview

The `MagnaundasoniRuntime` module provides three Unreal Actor Components:

| Component | Class | Purpose |
|-----------|-------|---------|
| **Mag Source** | `UMagSourceComponent` | Registers an Actor as an acoustic sound source. Pushes position each tick; queries and applies results to a sibling `UAudioComponent`. |
| **Mag Listener** | `UMagListenerComponent` | Registers the player camera or character as the acoustic listener. |
| **Mag Geometry** | `UMagGeometryComponent` | Extracts the Actor's static mesh and registers it as acoustic geometry. Supports dynamic occluders (doors, vehicles). |

All three components are **Blueprint spawnable** — search for "Mag Source",
"Mag Listener", or "Mag Geometry" in the Add Component dialog.

### Module tick order

```
TG_PrePhysics  ← UMagSourceComponent::TickComponent    (push source position)
               ← UMagListenerComponent::TickComponent  (push listener position)
               ← UMagGeometryComponent::TickComponent  (push dynamic transforms)
               ↓
World post-actor-tick ← FMagnaundasoniRuntimeModule::OnWorldPostActorTick
                        calls mag_update(DeltaTime) to advance the simulation
               ↓
Next frame TG_PrePhysics ← components read LastAcousticResult
```

---

## Quick Start

```cpp
// MyGameMode.cpp
#include "MagnaundasoniSubsystem.h"

void AMyGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Get the Magnaundasoni subsystem
    auto* MagnSubsystem = GetWorld()->GetSubsystem<UMagnaundasoniSubsystem>();
    if (MagnSubsystem)
    {
        // Initialize with default settings
        FMagnInitConfig Config;
        Config.MaxSources = 128;
        Config.QualityLevel = EMagnQualityLevel::High;
        MagnSubsystem->Initialize(Config);
    }
}
```

---

## Scene Setup

### Automatic Geometry Registration via Actor Component

Add the `UMagnGeometryComponent` to any actor with static mesh components.

```cpp
// In your actor's constructor or Blueprint
UMagnGeometryComponent* GeomComp = CreateDefaultSubobject<UMagnGeometryComponent>(
    TEXT("MagnGeometry"));
GeomComp->MaterialPreset = TEXT("Wood");
GeomComp->bAutoRegister = true;
```

The component automatically:
1. Extracts collision geometry from `UStaticMeshComponent` children.
2. Registers the geometry as an acoustic chunk on `BeginPlay()`.
3. Unregisters on `EndPlay()`.

### Manual Geometry Registration

```cpp
#include "MagnaundasoniSubsystem.h"

void AMyActor::RegisterAcousticGeometry()
{
    auto* MagnSubsystem = GetWorld()->GetSubsystem<UMagnaundasoniSubsystem>();
    if (!MagnSubsystem) return;

    // Get mesh data from a static mesh component
    UStaticMeshComponent* MeshComp = FindComponentByClass<UStaticMeshComponent>();
    if (!MeshComp) return;

    FMagnGeometryDesc Desc;
    MagnSubsystem->ExtractGeometryFromMesh(MeshComp, Desc);
    Desc.MaterialID = 1; // Wood preset

    ChunkID = MagnSubsystem->RegisterGeometry(Desc);
}

void AMyActor::DestroyAcousticGeometry()
{
    auto* MagnSubsystem = GetWorld()->GetSubsystem<UMagnaundasoniSubsystem>();
    if (MagnSubsystem)
    {
        MagnSubsystem->UnregisterGeometry(ChunkID);
    }
}
```

---

## Source Configuration

### Using the MagnAcousticSourceComponent

```cpp
// In your sound-emitting actor
UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
UAudioComponent* AudioComp;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
UMagnAcousticSourceComponent* AcousticSourceComp;

// Constructor
AMyEmitter::AMyEmitter()
{
    AudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("Audio"));
    RootComponent = AudioComp;

    AcousticSourceComp = CreateDefaultSubobject<UMagnAcousticSourceComponent>(
        TEXT("AcousticSource"));
    AcousticSourceComp->SetupAttachment(RootComponent);
    AcousticSourceComp->Priority = 0.8f;
    AcousticSourceComp->DirectivityPattern = EMagnDirectivity::Omnidirectional;
}
```

### Blueprint Setup

1. Add an **Audio Component** to your actor.
2. Add a **Magn Acoustic Source Component** to the same actor.
3. Configure **Priority** and **Directivity** in the Details panel.
4. The component automatically syncs position each tick.

### Source Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `Priority` | float | 0.5 | Source importance [0, 1]; higher = more ray budget |
| `DirectivityPattern` | enum | Omnidirectional | Sound radiation pattern |
| `bAutoApplyToAudio` | bool | true | Automatically apply results to sibling AudioComponent |
| `bActive` | bool | true | Whether the source is simulated |

---

## Listener Configuration

### Automatic (Recommended)

The `UMagnaundasoniSubsystem` automatically tracks the player's camera/pawn
position as the primary listener.  No additional setup is needed.

### Manual Listener

```cpp
// For custom listener positions (e.g., spectator cameras)
UMagnAcousticListenerComponent* ListenerComp =
    CreateDefaultSubobject<UMagnAcousticListenerComponent>(TEXT("Listener"));
ListenerComp->SetupAttachment(CameraComponent);
ListenerComp->bIsPrimaryListener = true;
```

---

## Material Assignment

### In C++

```cpp
auto* MagnSubsystem = GetWorld()->GetSubsystem<UMagnaundasoniSubsystem>();

// From preset
MagnSubsystem->SetMaterialFromPreset(1, TEXT("Wood"));
MagnSubsystem->SetMaterialFromPreset(2, TEXT("Metal"));
MagnSubsystem->SetMaterialFromPreset(3, TEXT("Carpet"));

// Custom material
FMagnMaterial CustomMat;
CustomMat.Absorption = { 0.05f, 0.06f, 0.08f, 0.10f, 0.12f, 0.14f, 0.16f, 0.18f };
CustomMat.Transmission = { 0.001f, 0.001f, 0.001f, 0.001f, 0.001f, 0.001f, 0.002f, 0.002f };
CustomMat.Scattering = { 0.10f, 0.12f, 0.14f, 0.16f, 0.18f, 0.20f, 0.22f, 0.24f };
CustomMat.Roughness = 0.1f;
CustomMat.ThicknessClass = EMagnThickness::Thick;
CustomMat.LeakageHint = 0.0f;
CustomMat.CategoryTag = TEXT("CustomGlass");
MagnSubsystem->SetMaterial(10, CustomMat);
```

### Via Data Asset

Create a `UMagnMaterialDataAsset` in the Content Browser:

1. Right-click → **Magnaundasoni → Material Data Asset**.
2. Configure absorption, transmission, and scattering bands.
3. Reference the asset in your `UMagnGeometryComponent`.

---

## World Partition Streaming

The Magnaundasoni Unreal adapter integrates directly with World Partition.

### Automatic Mode (Default)

The `UMagnaundasoniSubsystem` automatically hooks into World Partition streaming
delegates:

```cpp
// This happens automatically in the subsystem, but here's what it does:
void UMagnaundasoniSubsystem::OnWorldPartitionCellLoaded(
    const UWorldPartitionRuntimeCell* Cell)
{
    // Extract geometry from loaded actors
    FMagnGeometryDesc Desc;
    ExtractGeometryFromCell(Cell, Desc);

    // Register with acoustics runtime (async, background thread)
    uint64 ChunkID = CellToChunkID(Cell);
    MagnEngine::RegisterGeometryAsync(ChunkID, Desc);
}

void UMagnaundasoniSubsystem::OnWorldPartitionCellUnloaded(
    const UWorldPartitionRuntimeCell* Cell)
{
    uint64 ChunkID = CellToChunkID(Cell);
    MagnEngine::UnregisterGeometry(ChunkID);
}
```

### Predictive Loading

Enable ahead-of-time loading for seamless transitions:

```cpp
// In your project settings or subsystem config
MagnSubsystem->SetPredictiveLoadingEnabled(true);
MagnSubsystem->SetPredictiveLoadDistance(50.0f); // metres ahead of WP streaming
```

### Chunk-to-Cell Mapping

| Mode | Description |
|------|-------------|
| `OneToOne` | Each World Partition cell maps to one acoustic chunk (default) |
| `ManyToOne` | Multiple WP cells merge into a single acoustic chunk (reduces overhead for small cells) |
| `Custom` | User-defined mapping via `IMagnChunkMapper` interface |

---

## Applying Acoustic Results

### Automatic Mode

When `UMagnAcousticSourceComponent::bAutoApplyToAudio` is `true`, the
component automatically applies acoustic parameters to its sibling
`UAudioComponent` every tick:

- Attenuation volume
- Low-pass filter (occlusion)
- Reverb send
- Source spread

### Manual Mode

```cpp
void AMyEmitter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    auto* MagnSubsystem = GetWorld()->GetSubsystem<UMagnaundasoniSubsystem>();
    if (!MagnSubsystem) return;

    FMagnSourceParameterSet Params;
    MagnSubsystem->GetSourceParameters(ListenerID, SourceID, Params);

    // Apply to your audio system
    AudioComp->SetVolumeMultiplier(FMath::Pow(10.0f, Params.DirectGainDB / 20.0f));

    // Custom occlusion filter (via Audio Modulation or Sound Cue)
    OcclusionLPFCutoff = Params.OcclusionLPFCutoff;

    // Reverb send via Submix
    if (ReverbSubmix)
    {
        AudioComp->SetSubmixSend(ReverbSubmix,
            FMath::Pow(10.0f, Params.ReverbSendDB / 20.0f));
    }
}
```

### Raw Acoustic State (Advanced)

```cpp
const FMagnAcousticState* State = MagnSubsystem->GetAcousticState(ListenerID);
if (State)
{
    // Per-source direct components
    for (uint32 i = 0; i < State->SourceCount; i++)
    {
        const auto& Direct = State->DirectComponents[i];
        // Access: Direct.PerBandDirectGain[0..7]
        // Access: Direct.DirectDirection (FVector)
    }

    // Late field
    const auto& Late = State->LateField;
    // Access: Late.RT60[0..7], Late.RoomSizeEstimate
}
```

---

## Quality Settings

### Via C++

```cpp
auto* MagnSubsystem = GetWorld()->GetSubsystem<UMagnaundasoniSubsystem>();

// Preset
MagnSubsystem->SetQuality(EMagnQualityLevel::High);

// Fine-tuned
FMagnQualitySettings Settings;
MagnSubsystem->GetQualitySettings(Settings);
Settings.RaysPerSource = 128;
Settings.MaxReflectionOrder = 2;
Settings.MaxDiffractionDepth = 1;
Settings.DetailedZoneRadius = 25.0f;
MagnSubsystem->SetQualitySettings(Settings);
```

### Via Console Variable

```
magn.Quality 2          ; 0=Low, 1=Medium, 2=High, 3=Ultra
magn.RaysPerSource 128
magn.MaxReflectionOrder 2
```

### Via Project Settings

Navigate to **Project Settings → Plugins → Magnaundasoni** to configure default
quality levels per platform.

---

## Debug Visualization

### In-Editor

Enable the **Magnaundasoni** show flag in the viewport:

1. Click the **Show** dropdown in the viewport.
2. Enable **Magnaundasoni → Rays / Reflections / Diffraction / Fidelity Zones**.

### Runtime Console Commands

```
magn.Debug.ShowRays 1
magn.Debug.ShowReflections 1
magn.Debug.ShowDiffraction 1
magn.Debug.ShowFidelityZones 1
magn.Debug.ShowStats 1        ; On-screen performance HUD
```

### Programmatic

```cpp
#include "MagnaundasoniDebug.h"

void AMyDebugActor::DrawAcousticDebug()
{
    auto* MagnSubsystem = GetWorld()->GetSubsystem<UMagnaundasoniSubsystem>();

    FMagnStats Stats;
    MagnSubsystem->GetStats(Stats);

    GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green,
        FString::Printf(TEXT("Magn Tick: %.2f ms | Rays: %d | Chunks: %d"),
            Stats.TickTimeMs, Stats.TotalRaysCast, Stats.ActiveChunks));
}
```

---

## Demo Map

The repository ships a ready-to-run demo that illustrates the key acoustic
features of the runtime components: a patrolling enemy with a live sound source,
a sliding door as a dynamic occluder, and a simple concrete room.

### Location

```
unreal/Content/MagnaundasoniDemo/
├── README.md           ← step-by-step setup guide
└── setup_demo.py       ← Unreal Python automation script
```

### Opening the demo

1. **Install the plugin** (see [Installation](#installation) above).
2. **Create a new empty level** in your project:
   `File → New Level → Empty Level`.
3. **Enable the Python Script Plugin** if it is not already on:
   `Edit → Plugins → Scripting → Python Script Plugin`.
4. **Run the setup script** from the Output Log Python console:

   ```python
   import sys, importlib
   sys.path.append(r'/path/to/repo/unreal/Content/MagnaundasoniDemo')
   import setup_demo; importlib.reload(setup_demo)
   setup_demo.run()
   ```

   The script creates four Blueprint classes and places them in the level:
   - **BP_MagWalls** – concrete-material room shell (static geometry)
   - **BP_MagDoor** – wood-material sliding door (dynamic occluder)
   - **BP_MagEnemy** – patrolling enemy with `UMagSourceComponent`
   - **BP_MagPlayer** – player character with `UMagListenerComponent`

5. Press **Alt+P** to play.  Walk around and notice how the enemy sound changes
   as it moves behind the door.

### Replacing placeholder meshes

The script assigns engine-built-in primitive meshes (`/Engine/BasicShapes/Cube`,
`/Engine/BasicShapes/Sphere`) as placeholders.  Replace them in the Blueprint
editor with your own assets before shipping.

### Debugging the demo

```
magn.Debug.ShowRays 1          -- draw per-source ray paths
magn.Debug.ShowReflections 1   -- visualise reflection taps
magn.Debug.ShowDiffraction 1   -- visualise diffraction edges around the door
magn.Debug.ShowStats 1         -- HUD with tick time and active source count
```

---

## Best Practices

1. **Use collision geometry** – `UStaticMeshComponent` collision meshes are
   typically much simpler than render meshes, making them ideal for acoustic
   simulation.

2. **Leverage World Partition** – The automatic streaming integration keeps
   memory usage bounded in large worlds.

3. **Tag acoustic-relevant actors** – Not every mesh needs acoustic geometry.
   Tag walls, floors, and ceilings; skip small decorations.

4. **Set priorities wisely** – Give important gameplay sources (player weapons,
   dialogue, alarms) high priority values.

5. **Profile in Development builds** – Use `magn.Debug.ShowStats` to track per-
   frame costs during playtesting.

6. **Use platform quality scaling** – Set `MAGN_QUALITY_LOW` on Switch and
   mobile, `MAGN_QUALITY_HIGH` on PC/console.

7. **Pre-bake edge caches** – Run the offline bake tool to pre-extract
   diffraction edges; avoids runtime edge extraction costs.

---

## Troubleshooting

| Problem | Cause | Solution |
|---------|-------|----------|
| Plugin not found | Missing `.uplugin` or library path | Verify `Plugins/Magnaundasoni/` structure; rebuild |
| Linker errors | Missing `MagnaundasoniRuntime` module dependency | Add `"MagnaundasoniRuntime"` to `Build.cs` `PublicDependencyModuleNames` |
| `Native engine: null` in log | Native DLL missing | Place `magnaundasoni.dll` / `.so` / `.dylib` in `Plugins/Magnaundasoni/Binaries/<Platform>/` |
| Components inactive (no sound changes) | Native function table not resolved | Check log for `ResolveFunctionPointers` errors; verify DLL version matches header |
| No acoustic effect | Subsystem not ticking | Ensure `UMagnaundasoniSubsystem` is registered in your `DefaultEngine.ini` |
| World Partition chunks not loading | Streaming delegates not bound | Verify `bAutoStreamingEnabled` is `true` on the subsystem |
| High CPU usage | Ray budget too high | Reduce `magn.RaysPerSource` or use lower quality level |
| GPU backend not activating | Driver doesn't support RT | Falls back to Software BVH; check logs for backend selection message |
| Crashes on shutdown | Order-of-destruction issue | Ensure `MagnSubsystem` shuts down before audio engine |
| Demo script fails | Python Script Plugin disabled | Enable `Edit → Plugins → Python Script Plugin` and restart the editor |

---

## References

- [`docs/API.md`](API.md) – Full API reference
- [`docs/Material-Presets.md`](Material-Presets.md) – Material preset values
- [`docs/Performance-Tuning.md`](Performance-Tuning.md) – Performance guide
- [`docs/design/architecture.md`](design/architecture.md) – Architecture overview
- [`unreal/Content/MagnaundasoniDemo/README.md`](../unreal/Content/MagnaundasoniDemo/README.md) – Demo map setup
- [Unreal Engine World Partition Documentation](https://docs.unrealengine.com/5.0/en-US/world-partition-in-unreal-engine/)
