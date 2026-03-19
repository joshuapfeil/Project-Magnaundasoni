# Development Environment Setup

This guide covers recommended IDE and editor configurations for working on
Project Magnaundasoni's C++ native core and the Unity / Unreal Engine plugin
layers.

---

## Table of Contents

1. [General Requirements](#general-requirements)
2. [VS Code](#vs-code)
3. [CLion](#clion)
4. [Visual Studio 2019/2022](#visual-studio-20192022)
5. [Xcode (macOS)](#xcode-macos)
6. [Unity Plugin Development](#unity-plugin-development)
7. [Unreal Plugin Development](#unreal-plugin-development)
8. [Recommended Extensions & Tools](#recommended-extensions--tools)

---

## General Requirements

Before opening any IDE, complete the native build at least once so that CMake
generates compile commands:

```bash
cmake -S native -B build/debug -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DMAGNAUNDASONI_BUILD_TESTS=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
# Symlink for tools that expect compile_commands.json at the repo root
ln -sf build/debug/compile_commands.json compile_commands.json
```

---

## VS Code

### Recommended extensions

| Extension                       | Publisher       | Purpose                         |
|---------------------------------|-----------------|---------------------------------|
| C/C++                           | Microsoft       | IntelliSense, debugging         |
| CMake Tools                     | Microsoft       | CMake integration               |
| clangd                          | llvm-vs-code-ext| Fast code completion & analysis |
| CodeLLDB                        | Vadim Chugunov  | LLDB debugger (Linux/macOS)     |
| EditorConfig for VS Code        | EditorConfig    | Honour `.editorconfig`          |

### Workspace settings (`.vscode/settings.json`)

```json
{
    "cmake.sourceDirectory": "${workspaceFolder}/native",
    "cmake.buildDirectory": "${workspaceFolder}/build/${buildType}",
    "cmake.configureArgs": [
        "-DMAGNAUNDASONI_BUILD_TESTS=ON",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    ],
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "clangd.arguments": [
        "--compile-commands-dir=${workspaceFolder}/build/debug",
        "--clang-tidy",
        "--header-insertion=iwyu"
    ],
    "editor.formatOnSave": true,
    "[cpp]": {
        "editor.defaultFormatter": "xaver.clang-format"
    }
}
```

### Launch configuration (`.vscode/launch.json`)

```json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Unit Tests (LLDB)",
            "type": "lldb",
            "request": "launch",
            "program": "${workspaceFolder}/build/debug/tests/magn_test_unit_placeholder",
            "cwd": "${workspaceFolder}"
        }
    ]
}
```

---

## CLion

1. Open CLion → **Open** → select the `native/` directory.
2. CLion detects `CMakeLists.txt` automatically.
3. Add CMake profiles in **Settings → Build, Execution, Deployment → CMake**:

   | Profile  | Build type | CMake options                                          |
   |----------|------------|--------------------------------------------------------|
   | Debug    | Debug      | `-DMAGNAUNDASONI_BUILD_TESTS=ON -DMAGNAUNDASONI_ENABLE_ASAN=ON` |
   | Release  | Release    | `-DMAGNAUNDASONI_BUILD_TESTS=ON`                       |

4. Enable **clang-format** integration: **Settings → Editor → Code Style → C/C++ → Enable ClangFormat**.
5. Run/debug unit tests via the gutter icons next to `TEST` macros or through
   the **CTest** tab.

---

## Visual Studio 2019/2022

1. **File → Open → Folder** → select the `native/` directory.
2. VS detects `CMakeLists.txt` and shows **CMake Overview**.
3. Click **Manage Configurations** (gear icon) and add:
   - **Debug** – set CMake variables `MAGNAUNDASONI_BUILD_TESTS=ON`.
   - **Release** – same, with `CMAKE_BUILD_TYPE=Release`.
4. Build via **Build → Build All** or `Ctrl+Shift+B`.
5. Tests appear in **Test Explorer** (run via `Ctrl+R,A`).

### Recommended extensions (VS Marketplace)

- **clangd** – for better IntelliSense than the default MSVC parser.
- **EditorConfig** – respects `.editorconfig` rules.

---

## Xcode (macOS)

```bash
# Generate Xcode project
cmake -S native -B build/xcode -G Xcode \
    -DMAGNAUNDASONI_BUILD_TESTS=ON
open build/xcode/Magnaundasoni.xcodeproj
```

Select the **magnaundasoni** scheme and build with `⌘B`.
To run tests, select the test scheme and press `⌘U`.

---

## Unity Plugin Development

The Unity plugin lives in `unity/plugin/`.

### Requirements

- Unity **2022.3 LTS** or **2023.x** (Unity 6 compatible)
- **.NET SDK 6+** for `dotnet` CLI tooling
- The native `magnaundasoni` shared library must be built first and copied into
  `unity/plugin/Runtime/Plugins/<platform>/` before opening the Unity project.

### Setup steps

1. Build the native library:
   ```bash
   cmake -S native -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build/release
   ```
2. Copy the output library:
   ```bash
   # Linux
   cp build/release/libmagnaundasoni.so \
       unity/plugin/Runtime/Plugins/Linux/x86_64/

   # Windows
   copy build\release\magnaundasoni.dll \
       unity\plugin\Runtime\Plugins\Windows\x86_64\

   # macOS
   cp build/release/libmagnaundasoni.dylib \
       unity/plugin/Runtime/Plugins/macOS/
   ```
3. Open `unity/samples/MagnaundasoniDemo/` in the Unity Editor.
4. The `MagnaundasoniEngine` MonoBehaviour auto-registers scene geometry on
   start (see [`docs/Integration-Unity.md`](../Integration-Unity.md)).

### Recommended VS Code extensions for C# / Unity

- **C# Dev Kit** (Microsoft)
- **Unity** (Unity Technologies)
- **Debugger for Unity** (Unity Technologies)

---

## Unreal Plugin Development

The Unreal plugin lives in `unreal/Plugin/`.

### Requirements

- Unreal Engine **5.1+** (built from source or Epic Games Launcher install)
- Visual Studio 2022 with **Game Development with C++** workload (Windows)
  or Xcode 14+ (macOS)
- The native `magnaundasoni` library must be built and placed in
  `unreal/Plugin/Source/ThirdParty/Magnaundasoni/<platform>/`.

### Setup steps

1. Build the native library (see [BUILD.md](../BUILD.md)).
2. Copy the shared library into the plugin's `ThirdParty` directory.
3. Right-click `unreal/Plugin/Magnaundasoni.uplugin` → **Generate Visual Studio
   Project Files** (Windows) or equivalent on macOS.
4. Open the generated solution / workspace and build the `MagnaundasoniEditor`
   target.
5. Copy or symlink `unreal/Plugin/` into your UE5 project's `Plugins/` folder.
6. Enable the plugin in **Edit → Plugins → Audio** within the Unreal Editor.

### Tips

- Enable **Live Coding** (`Ctrl+Alt+F11`) for fast C++ iteration inside the
  Unreal Editor.
- The plugin's `Source/` tree follows the standard Unreal module layout; add
  new files through the **New C++ Class** wizard to keep `.Build.cs` in sync.
- See [`docs/Integration-Unreal.md`](../Integration-Unreal.md) for the full
  Unreal-specific integration guide.

---

## Recommended Extensions & Tools

| Tool / Extension        | Platform        | Install                                  |
|-------------------------|-----------------|------------------------------------------|
| clang-format 15+        | All             | `apt install clang-format` / Homebrew / LLVM installer |
| clang-tidy 15+          | All             | `apt install clang-tidy` / Homebrew      |
| Ninja                   | All             | `apt install ninja-build` / Homebrew / winget |
| Vulkan SDK              | All (optional)  | <https://vulkan.lunarg.com/>             |
| Git LFS                 | All             | `git lfs install` after installing Git LFS |
| WSL 2                   | Windows         | For Linux-native builds on Windows       |

---

## Code Formatting

The repository ships with both a `.clang-format` config (inside `native/`) and
an `.editorconfig` at the repo root.  Most IDEs pick these up automatically.

To format all changed files from the command line:

```bash
git diff --name-only main | grep -E '\.(cpp|h|cs)$' | xargs clang-format -i
```
