# Magnaundasoni – Unity Quick Start Guide

## Installation

### Via Unity Package Manager (local)

1. Clone or download this repository.
2. In Unity, open **Window → Package Manager**.
3. Click **+** → **Add package from disk…**
4. Navigate to `unity/plugin/package.json` and select it.

### Via Git URL

In `Packages/manifest.json`, add:

```json
"com.magnaundasoni.acoustics": "https://github.com/<org>/Project-Magnaundasoni.git?path=unity/plugin"
```

## Setup Steps

1. **Add the Engine** – Place a GameObject in your scene and add the
   `Magnaundasoni/Engine` component. Configure quality, backend, and thread
   settings in the Inspector.

2. **Tag Geometry** – On every mesh that should interact with acoustics, add
   `Magnaundasoni/Acoustic Geometry`. Assign an `Acoustic Material` asset and
   choose the appropriate dynamic flag (Static, QuasiStatic, DynamicImportant,
   DynamicMinor).

3. **Create Materials** – Use **Assets → Create → Magnaundasoni → Acoustic
   Material** to create `MagnaundasoniMaterial` ScriptableObjects. Tweak the
   8-band absorption, transmission, and scattering curves in the custom editor
   or load a built-in preset.

4. **Add Sources** – Attach `Magnaundasoni/Acoustic Source` to any GameObject
   with an `AudioSource`. The component automatically registers with the
   engine and applies occlusion, reverb, and spatialization.

5. **Add Listener** – Attach `Magnaundasoni/Acoustic Listener` to your camera
   or player head. Only one active listener is supported at a time.

6. **Play** – Enter Play Mode. The engine initializes, traces rays, and feeds
   acoustic results back to Unity's audio system each frame.

## Components Overview

| Component | Purpose |
|---|---|
| **MagnaundasoniEngine** | Singleton that creates and drives the native engine. |
| **MagnaundasoniSource** | Registers an audio source for acoustic simulation. |
| **MagnaundasoniListener** | Registers the player/camera as the listener. |
| **MagnaundasoniGeometry** | Feeds mesh data to the engine for ray tracing. |
| **MagnaundasoniMaterial** | ScriptableObject defining surface acoustic properties (8-band). |
| **MagnaundasoniDebugVisualizer** | Draws debug rays and diffraction edges in the Scene view. |

## Rendering Modes

- **Integration** – Maps engine results (occlusion, reverb, direction) onto
  Unity's built-in `AudioSource` parameters. Recommended for most projects.
- **BuiltIn** – The native engine handles audio output directly (advanced).

## Samples

See `unity/samples/MagnaundasoniDemo/` for an example scene with a door
controller, moving enemy, and performance stats overlay.

## Troubleshooting

- **"Engine not initialized"** – Ensure the native library (`magnaundasoni`)
  is placed in `Plugins/` for your target platform.
- **No acoustic effect** – Verify at least one Geometry, Source, and Listener
  are active in the scene.
- **Performance** – Lower quality or reduce `raysPerSource` on the Engine
  component.
