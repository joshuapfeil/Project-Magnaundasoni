# Build Instructions

Step-by-step instructions for building Project Magnaundasoni natively on
Linux, Windows, and macOS.

---

## Table of Contents

1. [Required Toolchain](#required-toolchain)
2. [Linux](#linux)
3. [Windows](#windows)
4. [macOS](#macos)
5. [CMake Options](#cmake-options)
6. [Running Unit Tests](#running-unit-tests)
7. [Troubleshooting](#troubleshooting)

---

## Required Toolchain

| Tool         | Minimum version | Notes                                       |
|--------------|-----------------|---------------------------------------------|
| C++ compiler | C++17 capable   | GCC 9+, Clang 10+, MSVC 2019+               |
| CMake        | 3.21            | Required for preset support                 |
| Ninja        | 1.10            | Recommended generator (faster than Make)    |
| Git          | 2.30            | LFS required for binary test assets         |
| Python       | 3.8             | Build helper scripts                        |

### Optional backends

| Tool        | Version  | Purpose                         |
|-------------|----------|---------------------------------|
| Vulkan SDK  | 1.3+     | Vulkan ray-query RT backend     |
| Windows SDK | 20348+   | DirectX Raytracing (DXR) backend |
| clang-format| 15+      | Auto-formatting                 |
| clang-tidy  | 15+      | Static analysis                 |

---

## Linux

```bash
# 1. Install dependencies (Ubuntu / Debian example)
sudo apt update
sudo apt install -y build-essential cmake ninja-build git python3

# Optional RT backend
sudo apt install -y libvulkan-dev

# 2. Clone
git clone https://github.com/joshuapfeil/Project-Magnaundasoni.git
cd Project-Magnaundasoni
git submodule update --init --recursive

# 3. Configure (Debug)
cmake -S native -B build/debug -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DMAGNAUNDASONI_BUILD_TESTS=ON

# 4. Build
cmake --build build/debug

# 5. Run tests
ctest --test-dir build/debug --output-on-failure
```

### Release build

```bash
cmake -S native -B build/release -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DMAGNAUNDASONI_BUILD_TESTS=ON
cmake --build build/release
ctest --test-dir build/release --output-on-failure
```

---

## Windows

> Tested with Visual Studio 2019/2022 and the bundled MSVC toolchain.

```powershell
# 1. Install CMake + Ninja via winget (or chocolatey / manual download)
winget install Kitware.CMake Ninja-build.Ninja

# 2. Open a Visual Studio Developer Command Prompt (x64)
#    OR use "Developer PowerShell for VS 2022"

# 3. Clone
git clone https://github.com/joshuapfeil/Project-Magnaundasoni.git
cd Project-Magnaundasoni
git submodule update --init --recursive

# 4. Configure
cmake -S native -B build\debug -G Ninja `
    -DCMAKE_BUILD_TYPE=Debug `
    -DMAGNAUNDASONI_BUILD_TESTS=ON

# 5. Build
cmake --build build\debug

# 6. Run tests
ctest --test-dir build\debug --output-on-failure
```

### Visual Studio IDE (alternative)

```powershell
# Open the native/ folder directly in Visual Studio 2022.
# CMake integration is built-in; select a preset from the toolbar.
cmake -S native -B build\vs2022 -G "Visual Studio 17 2022" -A x64 `
    -DMAGNAUNDASONI_BUILD_TESTS=ON
start build\vs2022\Magnaundasoni.sln
```

---

## macOS

> Tested with Apple Clang (Xcode 14+) on both Intel and Apple Silicon.

```bash
# 1. Install Xcode Command Line Tools
xcode-select --install

# 2. Install CMake + Ninja (Homebrew)
brew install cmake ninja

# 3. Clone
git clone https://github.com/joshuapfeil/Project-Magnaundasoni.git
cd Project-Magnaundasoni
git submodule update --init --recursive

# 4. Configure (Debug)
cmake -S native -B build/debug -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DMAGNAUNDASONI_BUILD_TESTS=ON

# 5. Build
cmake --build build/debug

# 6. Run tests
ctest --test-dir build/debug --output-on-failure
```

### Apple Silicon note

CMake auto-detects the host architecture.  For a universal binary:

```bash
cmake -S native -B build/universal -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build/universal
```

---

## CMake Options

| Option                        | Default | Description                                   |
|-------------------------------|---------|-----------------------------------------------|
| `MAGNAUNDASONI_BUILD_TESTS`   | `OFF`   | Build unit and integration tests              |
| `MAGNAUNDASONI_ENABLE_RT`     | `OFF`   | Enable hardware ray-tracing backends          |
| `MAGNAUNDASONI_ENABLE_SIMD`   | `ON`    | Enable SSE4.1 / NEON SIMD optimisations       |
| `MAGNAUNDASONI_ENABLE_ASAN`   | `OFF`   | Address Sanitizer (Debug only)                |
| `MAGNAUNDASONI_ENABLE_TSAN`   | `OFF`   | Thread Sanitizer (Debug only)                 |
| `MAGNAUNDASONI_ENABLE_COVERAGE` | `OFF` | Enable GCC/Clang coverage instrumentation     |

Example – enable Address Sanitizer on Linux/macOS:

```bash
cmake -S native -B build/asan -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DMAGNAUNDASONI_BUILD_TESTS=ON \
    -DMAGNAUNDASONI_ENABLE_ASAN=ON
cmake --build build/asan
ctest --test-dir build/asan --output-on-failure
```

---

## Running Unit Tests

```bash
# Run all tests
ctest --test-dir build/debug --output-on-failure

# Run with verbose output
ctest --test-dir build/debug -V

# Run a specific test binary directly
./build/debug/tests/magn_test_unit_placeholder

# Generate coverage report (requires MAGNAUNDASONI_ENABLE_COVERAGE=ON)
gcovr --html-details coverage/index.html -r native/src/
```

Test categories and their locations:

| Category    | Directory            | Description                            |
|-------------|----------------------|----------------------------------------|
| Unit        | `tests/unit/`        | Isolated component tests               |
| Integration | `tests/integration/` | Multi-component pipeline tests         |
| Regression  | `tests/regression/`  | Known-bug reproduction cases           |
| Benchmarks  | `benchmarks/`        | Performance measurement (Release only) |

---

## Troubleshooting

### `CMake Error: CMAKE_CXX_COMPILER not found`

Ensure your compiler is on `PATH`.  On Linux: `sudo apt install build-essential`.
On Windows: run CMake from a Visual Studio Developer Command Prompt.

### Linker errors on Linux (`undefined reference to pthread_*`)

The `pthread` library is linked automatically.  If you see this, ensure you
are not overriding `CMAKE_EXE_LINKER_FLAGS` in a way that strips `-lpthread`.

### Tests not built

Pass `-DMAGNAUNDASONI_BUILD_TESTS=ON` to the configure step.

### Vulkan backend not found

Install the Vulkan SDK from <https://vulkan.lunarg.com/> and make sure
`VULKAN_SDK` is set in your environment, then re-run CMake with
`-DMAGNAUNDASONI_ENABLE_RT=ON`.
