# Magnaundasoni – C++ Style Guide

This guide governs all C++ source in the Magnaundasoni repository.  It is
enforced by CI (`clang-tidy` + `clang-format`) and code review.

---

## Language Standard

- **C++17** is the minimum.  C++20 features may be used when guarded by
  `#if __cplusplus >= 202002L` and a C++17 fallback exists.
- The public C ABI header (`magn_api.h`) is **pure C11**—no C++ constructs.

---

## Naming Conventions

| Entity            | Style               | Example |
|-------------------|----------------------|---------|
| Namespace         | `snake_case`         | `magn::core`, `magn::backend` |
| Class / Struct    | `PascalCase`         | `SceneGraph`, `BvhNode` |
| Function (C++)    | `camelCase`          | `buildTLAS()`, `traceRays()` |
| Function (C ABI)  | `magn_snake_case`    | `magn_init()`, `magn_tick()` |
| Local variable    | `camelCase`          | `rayCount`, `chunkIdx` |
| Member variable   | `m_camelCase`        | `m_nodeCount`, `m_backend` |
| Static member     | `s_camelCase`        | `s_instance` |
| Global constant   | `k_PascalCase`       | `k_MaxBands`, `k_DefaultRT60` |
| Enum value (C++)  | `PascalCase`         | `QualityLevel::High` |
| Enum value (C)    | `MAGN_UPPER_SNAKE`   | `MAGN_QUALITY_HIGH` |
| Macro             | `MAGN_UPPER_SNAKE`   | `MAGN_API`, `MAGN_ASSERT` |
| Template param    | `PascalCase`         | `template <typename AllocT>` |
| File name         | `snake_case.cpp/.h`  | `bvh_builder.cpp`, `scene_graph.h` |

---

## Formatting

Enforced by `.clang-format` in the repository root.  Key settings:

```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 100
BreakBeforeBraces: Allman
AllowShortFunctionsOnASingleLine: Inline
PointerAlignment: Left
NamespaceIndentation: None
SortIncludes: CaseSensitive
```

- **4-space indentation**, no tabs.
- **100-column limit**.
- **Allman braces** (opening brace on its own line for functions and classes).
- Run `clang-format -i <file>` or use the pre-commit hook.

---

## Header Rules

1. Every header has a `#pragma once` guard (preferred) or traditional include
   guard (`MAGN_<PATH>_<FILE>_H`).
2. Include order (enforced by `clang-format SortIncludes`):
   1. Corresponding `.h` for the `.cpp`
   2. Magnaundasoni headers (`magn/...`)
   3. Third-party headers
   4. Standard library headers
3. **Forward-declare** where possible to minimize header dependencies.
4. Never `using namespace` in a header.

---

## Memory Ownership Rules

### Heap Allocation Policy

| Context | Rule |
|---------|------|
| **Audio thread query path** | **Zero allocations**.  All data is pre-allocated in pools. |
| **Simulation tick** (`magn_tick`) | Use the **frame pool** (bump allocator, reset each tick). |
| **Chunk loading** (background) | Use the **chunk pool** via `MagnAllocator`. |
| **Initialization** (`magn_init`) | Heap allocation allowed (`new` / `malloc`). |

### Smart Pointers

| Type | Usage |
|------|-------|
| `std::unique_ptr<T>` | Owning pointers for long-lived objects (backends, managers). |
| `std::shared_ptr<T>` | **Avoid**.  If truly needed, justify in code review. |
| Raw `T*` | Non-owning observers only.  Must never outlive the owner. |
| `Span<T>` | Non-owning view of contiguous data.  Preferred for function parameters. |

### RAII

All resources (GPU buffers, file handles, thread pool) must be owned by RAII
wrappers.  No manual `delete` / `free` in application logic.

---

## SIMD Usage

- Wrap SIMD intrinsics in platform-abstracted types: `Vec4f` (SSE `__m128` /
  NEON `float32x4_t`).
- The `magn::simd` namespace provides common operations: `load`, `store`,
  `add`, `mul`, `min`, `max`, `dot3`, `cross3`, `rsqrt`.
- Scalar fallbacks must exist for every SIMD path (selected at compile time via
  `MAGN_SIMD_NONE`).
- **Alignment**: All SIMD-operated data is 16-byte aligned.  Use
  `MAGN_ALIGN(16)` on struct definitions and pool allocations.

```cpp
// Example: 4-wide ray–AABB intersection
MAGN_ALIGN(16) struct RayPacket4
{
    Vec4f originX, originY, originZ;
    Vec4f invDirX, invDirY, invDirZ;
    Vec4f tMin, tMax;
};

Vec4f hit = magn::simd::rayAABBTest4(packet, node.boundsMin, node.boundsMax);
```

---

## Threading Rules

1. **No `std::mutex` on the audio query path.**  Use atomic operations and
   lock-free structures only.
2. **No thread creation** outside `magn_init()`.  The worker thread pool is the
   only threading primitive.
3. Tasks submitted to the pool must be **self-contained**: capture data by value
   or via `Span<>`, never reference mutable shared state.
4. Use `std::atomic<>` with explicit memory ordering (`acquire` / `release`)—
   never default `seq_cst` unless proven necessary.
5. **No sleeping** in the simulation tick.  Use the lock-free task queue with
   spin-then-yield back-off.

---

## Error Handling

- The C ABI uses `MagnResult` return codes.  No exceptions cross the ABI
  boundary.
- Internal C++ code uses `MagnResult` propagation (early return on error).
- **No exceptions** in performance-critical paths.  Exceptions may be used
  during initialization/shutdown for truly exceptional conditions.
- Assertions (`MAGN_ASSERT`) are active in Debug and Development builds, removed
  in Release.
- Fatal errors (GPU device lost, out of memory) log via the user-provided
  callback and return the appropriate `MagnResult`.

---

## Comments and Documentation

- Use `///` (Doxygen) comments for all public API functions and types.
- Internal code: comment **why**, not **what**.  If the code needs a comment
  explaining what it does, consider renaming or refactoring.
- Every file begins with a one-line `/// @file` description.
- No commented-out code in `main` branch.  Use version control.

---

## Const Correctness

- Prefer `const` everywhere: parameters, local variables, member functions.
- Mark member functions `const` unless they modify state.
- Use `constexpr` for compile-time constants and simple functions.

---

## Enums

- Use `enum class` in C++ code.
- Use plain `enum` with `MAGN_` prefix in C ABI headers.
- Always provide an explicit underlying type: `enum class Foo : uint8_t`.

---

## Testing

- Unit tests use the project's test framework (Google Test or Catch2—decided at
  bootstrap).
- Every public function has at least one test.
- Performance-sensitive code has benchmarks (Google Benchmark or equivalent).
- Tests are in `tests/` mirroring the `src/` directory structure.

---

## Build Configuration

| Define                | Meaning |
|-----------------------|---------|
| `MAGN_DEBUG`          | Debug build: assertions, debug logging, slow checks |
| `MAGN_DEVELOPMENT`    | Development build: assertions, profiling hooks, no slow checks |
| `MAGN_RELEASE`        | Release build: no assertions, no debug logging |
| `MAGN_SIMD_SSE41`     | Enable SSE4.1 paths |
| `MAGN_SIMD_AVX2`      | Enable AVX2 paths |
| `MAGN_SIMD_NEON`      | Enable ARM NEON paths |
| `MAGN_SIMD_NONE`      | Scalar fallback only |

---

## References

- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)
- [LLVM Coding Standards](https://llvm.org/docs/CodingStandards.html)
- `docs/CONTRIBUTING.md` – setup and PR process
