# Release Process

This document explains how to cut a Magnaundasoni release, what artifacts
are produced, how they are named, and what manual steps are required until
the CI pipeline is fully validated in production.

---

## Table of Contents

1. [Overview](#overview)
2. [Tag naming conventions](#tag-naming-conventions)
3. [Artifact inventory](#artifact-inventory)
4. [Automated release workflow](#automated-release-workflow)
5. [Manual release (step-by-step)](#manual-release-step-by-step)
6. [Installing artifacts](#installing-artifacts)
7. [Pre-release / beta builds](#pre-release--beta-builds)
8. [Troubleshooting](#troubleshooting)

---

## Overview

Each release is driven by a **git tag** pushed to `main`.  Pushing a tag that
matches the pattern `v<MAJOR>.<MINOR>.<PATCH>` triggers the
`.github/workflows/release.yml` workflow, which:

1. Builds the native shared library for **Linux x64**, **Windows x64**, and
   **macOS Apple Silicon (arm64)**.
2. Packages the **Unity UPM plugin** as a ZIP.
3. Packages the **Unreal Engine 5 plugin** source as a ZIP.
4. Creates a **GitHub Release** and attaches all artifacts.

> **Note:** The Unity and Unreal packages produced by CI contain *source and
> meta-files only* — they do **not** include pre-built editor/game binaries.
> Developers must build or copy the native libraries into the correct
> sub-directories before using the packages in a project (see
> [Installing artifacts](#installing-artifacts)).

---

## Tag naming conventions

| Pattern | Meaning |
|---------|---------|
| `v1.2.3` | Stable release |
| `v1.2.3-beta.1` | Beta pre-release (GitHub marks it as pre-release) |
| `v1.2.3-rc.1` | Release candidate |
| `v0.x.y` | Pre-1.0 development release |

Rules:
- Tags are **always prefixed with `v`**.
- The version portion must follow [Semantic Versioning 2.0](https://semver.org/).
- Tags must be created from the `main` branch (or a dedicated `release/*`
  branch for patch releases).
- Never re-use or force-push a release tag after it has been pushed.

---

## Artifact inventory

After a successful release workflow the following ZIP archives are attached to
the GitHub Release:

| Artifact | Contents |
|----------|----------|
| `magnaundasoni-native-linux-x64-<ver>.zip` | `lib/libmagnaundasoni.so`, `include/Magnaundasoni.h`, `VERSION` |
| `magnaundasoni-native-windows-x64-<ver>.zip` | `bin/magnaundasoni.dll`, `lib/magnaundasoni.lib`, `include/Magnaundasoni.h`, `VERSION` |
| `magnaundasoni-native-macos-arm64-<ver>.zip` | `lib/libmagnaundasoni.dylib`, `include/Magnaundasoni.h`, `VERSION` |
| `magnaundasoni-unity-<ver>.zip` | Full `unity/plugin/` tree (UPM layout) |
| `magnaundasoni-unreal-<ver>.zip` | Full `unreal/Plugin/` tree (source + bundled native headers; no pre-built binaries) |

---

## Automated release workflow

The workflow is defined in `.github/workflows/release.yml`.  To trigger it:

```bash
# Make sure the working tree is clean and you are on main
git checkout main
git pull

# Create and push the annotated tag
git tag -a v1.2.3 -m "Release v1.2.3"
git push origin v1.2.3
```

Monitor the workflow in the repository's **Actions** tab (for example:
`https://github.com/<org>/<repo>/actions`).

The workflow creates the release as a **draft** so you can review the
generated release notes and attached artifacts before publishing.  Open the
draft release in the GitHub UI, make any necessary edits, then click
**Publish release** when satisfied.

---

## Manual release (step-by-step)

Use this procedure if the automated workflow is not yet trusted or when
building locally for testing.

### Prerequisites

- CMake ≥ 3.16
- A C++17-capable compiler (GCC ≥ 9, Clang ≥ 10, or MSVC 2019+)
- `zip` (POSIX) or PowerShell 5+ (Windows)
- (Optional) `ninja` for faster builds

### Step 1 — Decide the version number

Agree on the version string (e.g., `1.2.3`) and record it.  This string is
passed to each packaging script via `--version`.

### Step 2 — Build native libraries on each target platform

Run from the repository root on **each** target OS:

```bash
# Linux / macOS
scripts/package-native.sh --version 1.2.3

# Windows (PowerShell)
scripts\package-native.ps1 -Version 1.2.3
```

Each invocation produces a ZIP in `dist/native/`:

```
dist/native/magnaundasoni-native-linux-x64-1.2.3.zip
dist/native/magnaundasoni-native-macos-arm64-1.2.3.zip
dist/native/magnaundasoni-native-windows-x64-1.2.3.zip
```

Transfer all three ZIPs to a single machine for the next steps.

### Step 3 — (Optional) Inject native binaries into Unity/Unreal plugin trees

If you want the Unity or Unreal packages to ship with pre-built native
binaries:

**Unity** — Extract the appropriate ZIP and copy the library file into:
```
unity/plugin/Runtime/Plugins/<platform>/
```

Supported subfolder names follow Unity's platform naming:
- `x86_64` (Linux)
- `x86_64` (Windows) — file is `magnaundasoni.dll`
- `AppleSilicon` or `x86_64` (macOS)

Add or update the corresponding `.meta` files if you edit these paths.

**Unreal** — The Unreal plugin ZIP already includes native headers under
`Plugin/Source/ThirdParty/Magnaundasoni/include/` (bundled by
`package-unreal.sh`).  To add pre-built native libraries for a specific
platform, extract the native ZIP for that platform and copy the library file
into the matching subdirectory inside the plugin:

```
Plugin/Source/ThirdParty/Magnaundasoni/Win64/magnaundasoni.dll
Plugin/Source/ThirdParty/Magnaundasoni/Win64/magnaundasoni.lib
Plugin/Source/ThirdParty/Magnaundasoni/Linux/libmagnaundasoni.so
Plugin/Source/ThirdParty/Magnaundasoni/Mac/libmagnaundasoni.dylib
```

`Magnaundasoni.Build.cs` automatically detects these paths and uses them
instead of the repo-relative `native/build/` path.

### Step 4 — Package the Unity plugin

```bash
scripts/package-unity.sh --version 1.2.3
```

Output: `dist/unity/magnaundasoni-unity-1.2.3.zip`

### Step 5 — Package the Unreal plugin

```bash
scripts/package-unreal.sh --version 1.2.3
```

Output: `dist/unreal/magnaundasoni-unreal-1.2.3.zip`

### Step 6 — Create the GitHub Release

```bash
# Create an annotated tag
git tag -a v1.2.3 -m "Release v1.2.3"
git push origin v1.2.3

# Use the GitHub CLI to create the release and attach artifacts
gh release create v1.2.3 \
  --title "Magnaundasoni v1.2.3" \
  --notes "See CHANGELOG for details." \
  dist/native/magnaundasoni-native-linux-x64-1.2.3.zip \
  dist/native/magnaundasoni-native-windows-x64-1.2.3.zip \
  dist/native/magnaundasoni-native-macos-arm64-1.2.3.zip \
  dist/unity/magnaundasoni-unity-1.2.3.zip \
  dist/unreal/magnaundasoni-unreal-1.2.3.zip
```

---

## Installing artifacts

### Unity

1. Download `magnaundasoni-unity-<ver>.zip`.
2. Extract it; the top-level directory is `plugin/`.
3. Copy `plugin/` into your Unity project's `Packages/` folder, renaming it
   to `com.magnaundasoni.acoustics` (or use UPM's
   *Window → Package Manager → + → Add package from disk* and point to
   `plugin/package.json`).
4. Place the appropriate native library into
   `Packages/com.magnaundasoni.acoustics/Runtime/Plugins/<platform>/`.
5. Configure the plugin importer for each library in the Unity Inspector.

### Unreal Engine 5

1. Download `magnaundasoni-unreal-<ver>.zip`.
2. Extract it; the top-level directory is `Plugin/`.
3. Copy `Plugin/` into your project's `Plugins/Magnaundasoni/` folder.
4. Download the native ZIP for your target platform and copy the library file
   into `Plugins/Magnaundasoni/Source/ThirdParty/Magnaundasoni/<Platform>/`
   (see [Step 3](#step-3--optional-inject-native-binaries-into-unityunreal-plugin-trees)
   for exact filenames).  `Magnaundasoni.Build.cs` picks up these files
   automatically.
5. Right-click your `.uproject` file → *Generate Visual Studio project files*.
6. Build the project; UBT will compile the plugin using the bundled headers
   and the pre-built library you placed in the ThirdParty directory.

> If you skip step 4 (no pre-built library), UBT will look for the native
> library in the development tree (`native/build/`).  This path only exists
> when building from within the repository.

---

## Pre-release / beta builds

To publish a pre-release (e.g., beta):

```bash
git tag -a v1.2.3-beta.1 -m "Beta release v1.2.3-beta.1"
git push origin v1.2.3-beta.1
```

The workflow detects the `-` in the version string and marks the GitHub
Release as a **pre-release** automatically.

---

## Troubleshooting

| Problem | Resolution |
|---------|-----------|
| CMake configure fails on Linux | Ensure `cmake`, `g++` (≥ 9), and `zip` are installed. |
| `package-native.ps1` fails on Windows | Run in a **Developer PowerShell for VS 2022** so that `cmake` and the MSVC toolchain are on `PATH`. |
| Unity package shows missing scripts | Re-import the package and ensure the `.meta` files were not deleted. |
| Unreal does not compile the plugin | Check that the engine version matches; minimum UE5.0 is required. |
| GitHub Release not created | Confirm the tag matches `v[0-9]*.[0-9]*.[0-9]*` and was pushed to `origin`. |
| Artifacts missing from the release | Check the workflow run in GitHub Actions for individual job failures. |
