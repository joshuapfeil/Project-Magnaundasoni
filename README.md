## DISCLAIMER: I am not an audio engineer, this is mostly AI generated. I am making this because I wanted to see the idea get the attention I felt it deserved nad I'm not sure if an easy to use or truly realtime for large projects version of what I am thinking exists. Audio tends to be low priority and I wanted to make something that makes it easy to add superemely high quality audio. I welcome the input of people that know more than me and I hope this helps fellow developers make amazing things!

# Project Magnaundasoni

A realtime acoustics and audio rendering runtime for games, built around physically grounded propagation, dynamic world support, and a hybrid built-in/integration workflow for Unity, Unreal, and other engine pipelines.

## Overview

Project Magnaundasoni aims to provide advanced audio rendering solutions focusing on realism and dynamic responsiveness in gaming environments.

## Vision

This project targets realtime physically grounded acoustics for games with dynamic objects, large streamed/procedural worlds, and both built-in acoustic rendering mode and integration mode.

## Core Goals
- Direct sound
- Occlusion
- Transmission
- Early reflections
- Diffraction
- Late reverberation
- Air absorption
- Source directivity
- Dynamic object support
- Large-world streaming support

## Architecture Direction
- Shared native core
- Hybrid RT/compute backend philosophy
- Canonical multi-band acoustics
- Built-in rendering mode as primary target
- Integration mode as a compatibility layer

## Material Presets
- General
- Metal
- Wood
- Fabric
- Rock
- Dirt
- Grass
- Carpet

## Status

The project is in early design/scaffolding, with architecture decisions and initial repository structure being established.

## Planned Repository Structure
- `docs/`: Documentation for users and developers.
- `native/`: Core native codebase for audio processing.
- `unity/`: Unity-specific assets and plugins.
- `unreal/`: Unreal Engine-specific assets and plugins.
- `tools/`: Utilities and scripts for development and testing.
- `tests/`: Test suite to ensure the reliability and performance of the codebase.

## Contributing

We invite discussion and early contributions while noting that major architecture decisions are still being documented.

---

## Getting Started

Clone the repository and build the native library in a few commands:

```bash
# Clone
git clone https://github.com/joshuapfeil/Project-Magnaundasoni.git
cd Project-Magnaundasoni

# Configure & build (Linux / macOS)
cmake -S native -B build/debug -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DMAGNAUNDASONI_BUILD_TESTS=ON
cmake --build build/debug

# Run unit tests
ctest --test-dir build/debug --output-on-failure
```

For Windows and additional CMake options see **[BUILD.md](BUILD.md)**.

### Minimal C Usage

```c
#include "Magnaundasoni.h"

MagEngineConfig cfg;
mag_engine_config_defaults(&cfg);   /* sensible defaults – override what you need */
cfg.quality = MAG_QUALITY_HIGH;

MagEngine engine = NULL;
mag_engine_create(&cfg, &engine);

/* … register geometry, sources, listener … */
mag_update(engine, 0.016f);

MagAcousticResult result = {0};
/* srcID and lisID are obtained when you register your source and listener */
/* mag_get_acoustic_result(engine, srcID, lisID, &result); */

mag_engine_destroy(engine);
```

### Engine-Specific Integration

| Engine | Guide |
|--------|-------|
| **Unity** | [docs/Integration-Unity.md](docs/Integration-Unity.md) – Add via UPM, drop components, press Play |
| **Unreal** | [docs/Integration-Unreal.md](docs/Integration-Unreal.md) – Copy plugin, add Actor Components, hit Alt+P |
| **Custom / C** | [docs/API.md](docs/API.md) – Single-header C ABI; link `magnaundasoni` and go |

### Key Documentation

| Document | Description |
|----------|-------------|
| [BUILD.md](BUILD.md) | Native build instructions for Linux, Windows, and macOS |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Branch strategy, code style, and PR process |
| [docs/SETUP_DEVELOPMENT.md](docs/SETUP_DEVELOPMENT.md) | IDE and editor setup for C++ and Unity/Unreal |
| [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) | Full contribution guide |
| [docs/API.md](docs/API.md) | Public C API reference |
| [docs/Integration-Unity.md](docs/Integration-Unity.md) | Unity plugin integration guide |
| [docs/Integration-Unreal.md](docs/Integration-Unreal.md) | Unreal plugin integration guide |
| [docs/adr/](docs/adr/) | Architecture Decision Records |
