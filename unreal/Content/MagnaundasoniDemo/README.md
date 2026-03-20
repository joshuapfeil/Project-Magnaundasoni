# Magnaundasoni Demo Map – Unreal Engine

This folder contains the sample demo for the **MagnaundasoniRuntime** plugin module.
Because binary `.uasset` / `.umap` files cannot be source-controlled cleanly, the
demo is set up via a Python automation script that runs inside the Unreal Editor's
built-in Python interpreter.

---

## What the demo includes

| Actor | Components | Purpose |
|-------|-----------|---------|
| **BP_MagWalls** | `UStaticMeshComponent` + `UMagGeometryComponent` (Static, Concrete) | Room walls / floor / ceiling as acoustic geometry |
| **BP_MagDoor** | `UStaticMeshComponent` + `UMagGeometryComponent` (DynamicImportant, Wood) | Sliding door that opens/closes – demonstrates live occluder updates |
| **BP_MagEnemy** | `UStaticMeshComponent` + `UAudioComponent` + `UMagSourceComponent` (DynamicImportant) | Patrolling enemy whose footstep loop is processed by Magnaundasoni |
| **BP_MagPlayer** | `UCameraComponent` + `UMagListenerComponent` (Primary) | Player camera with acoustic listener |

---

## Quick setup

### Prerequisites
- Unreal Engine 5.2 or newer.
- The Magnaundasoni plugin installed (see `docs/Integration-Unreal.md`).
- Python scripting enabled in the editor:
  `Edit → Plugins → Python Script Plugin → enable`.
- A C++ UE project (blueprint-only projects must convert first).

### Steps

1. **Copy the plugin into your project's Plugins folder**

   ```
   MyProject/
   └── Plugins/
       └── Magnaundasoni/    ← copy the unreal/Plugin/ folder here
   ```

2. **Regenerate project files** and rebuild in your IDE.

3. **Enable the plugin** in the editor:
   `Edit → Plugins → Audio → Magnaundasoni → Enable`.
   Restart the editor when prompted.

4. **Create a new empty level** (or use an existing one):
   `File → New Level → Empty Level`, save as `MagnaundasoniDemo`.

5. **Run the setup script** from the Unreal Editor Python console
   (`Output Log → Python` tab, or via `Window → Developer Tools → Python`):

   ```python
   import importlib, sys
   sys.path.append(r'<AbsPathToRepo>/unreal/Content/MagnaundasoniDemo')
   import setup_demo
   importlib.reload(setup_demo)
   setup_demo.run()
   ```

   The script will:
   - Create the four Blueprint classes listed above.
   - Add the required Magnaundasoni components (source, listener, geometry) to each Blueprint.
   - Place default instances in the level.
   - Save all assets.

   > **Note:** Patrol waypoints and movement logic for BP_MagEnemy are **not**
   > automated by the script. See the **Manual Blueprint recreation** section
   > below to add patrol movement by hand.

6. **Play the level** (`Alt+P`).  Walk the player pawn around the room and listen
   for acoustic changes as the enemy moves around the door.

---

## Manual Blueprint recreation

If you prefer to set up the Blueprints by hand instead of running the script:

### BP_MagEnemy
1. Create a Blueprint Class inheriting `Pawn`.
2. Add a `Static Mesh Component` (any capsule or placeholder mesh).
3. Add an `Audio Component` (assign a looping footstep Sound Cue).
4. Add a **Mag Source** component (`UMagSourceComponent`):
   - `ImportanceClass` = Dynamic Important
   - `RadiusCm` = 30
   - `bAutoApplyToAudioComponent` = true
5. Add a custom `BeginPlay` graph that starts the audio component.
6. Add a `Tick` graph that moves the actor along a list of waypoints
   (array of `Vector` target positions, cycle on arrival).

### BP_MagDoor
1. Create a Blueprint Class inheriting `Actor`.
2. Add a `Static Mesh Component` (plane or thin box).
3. Add a **Mag Geometry** component (`UMagGeometryComponent`):
   - `MaterialPreset` = "Wood"
   - `ImportanceClass` = Dynamic Important
   - `bAutoRegister` = true
4. Add a `Timeline` that translates the door mesh on the Y axis (open/close).
5. Expose a `bOpen` boolean variable; toggle it from a `TriggerBox` overlap.

### BP_MagPlayer
1. Use or subclass your game's player character.
2. Add a **Mag Listener** component (`UMagListenerComponent`):
   - `bIsPrimaryListener` = true
3. Attach it to the camera boom or camera component.

### BP_MagWalls (static room geometry)
1. Place `StaticMeshActors` for walls, floor, ceiling.
2. On each, add a **Mag Geometry** component:
   - `MaterialPreset` = "Concrete"  (or "Wood", "Carpet" as appropriate)
   - `ImportanceClass` = Static

---

## Replacing placeholder meshes

The setup script assigns `Engine Content` primitive meshes as placeholders.
Replace them with your own assets:

| Placeholder | Suggested replacement |
|-------------|----------------------|
| `Cube` (1×1×1 m) for walls | Your architectural static mesh |
| `Cube` (0.1×1×2 m) for door | A door panel mesh with pivot at hinge |
| `Capsule` for enemy | Your character skeletal mesh |

---

## Debugging acoustics

After pressing Play:

```
# Console commands (~ key in-game or Output Log)
magn.Debug.ShowRays 1          -- draw per-source ray paths
magn.Debug.ShowReflections 1   -- visualise reflection taps
magn.Debug.ShowDiffraction 1   -- visualise diffraction edges
magn.Debug.ShowStats 1         -- HUD with tick time and ray count
```

See `docs/Integration-Unreal.md` for a full reference.
