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
