# Contributing to Magnaundasoni

Thank you for your interest in contributing!  This guide covers everything you
need to get started.

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Getting the Source](#getting-the-source)
3. [Build Instructions](#build-instructions)
4. [Running Tests](#running-tests)
5. [Pull Request Process](#pull-request-process)
6. [Coding Standards](#coding-standards)
7. [Commit Messages](#commit-messages)
8. [Issue Guidelines](#issue-guidelines)
9. [Architecture Decision Records](#architecture-decision-records)

---

## Prerequisites

| Tool         | Version        | Notes |
|--------------|---------------|-------|
| C++ compiler | C++17 capable | GCC 9+, Clang 10+, MSVC 2019+ |
| CMake        | ≥ 3.16        | Matches `native/CMakeLists.txt`; newer OK |
| Python       | ≥ 3.8         | Build scripts and test tooling |
| Git          | ≥ 2.30        | LFS required for test assets |
| Vulkan SDK   | ≥ 1.3         | Optional; for Vulkan RT backend |
| Windows SDK  | ≥ 10.0.20348  | Optional; for DXR backend on Windows |

### Optional but Recommended

- `clang-format` 15+ (auto-formatting)
- `clang-tidy` 15+ (static analysis)
- `ninja` (faster builds)
- An IDE with CMake support (VS Code + CMake Tools, CLion, Visual Studio)

---

## Getting the Source

```bash
git clone https://github.com/<org>/Project-Magnaundasoni.git
cd Project-Magnaundasoni
```

---

## Build Instructions

### Configure + Build (Recommended)

```bash
# Debug
cmake -S native -B build/debug -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DMAGNAUNDASONI_BUILD_TESTS=ON
cmake --build build/debug

# Release
cmake -S native -B build/release -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DMAGNAUNDASONI_BUILD_TESTS=ON
cmake --build build/release
```

### Key CMake Options

| Option                        | Default | Description |
|------------------------------|---------|-------------|
| `MAGNAUNDASONI_BUILD_TESTS`  | `OFF`   | Build unit tests |
| `MAGNAUNDASONI_ENABLE_RT`    | `OFF`   | Enable hardware ray-tracing backends |
| `MAGNAUNDASONI_ENABLE_SIMD`  | `ON`    | Enable SIMD optimisations |

---

## Running Tests

```bash
# Run all tests
ctest --test-dir build/debug --output-on-failure

# Run the native unit test binary directly
./build/debug/magnaundasoni_tests

# Run with verbose output
ctest --test-dir build/debug -V
```

### Test Categories

| Category       | Directory              | Description |
|----------------|------------------------|-------------|
| Unit tests     | `tests/unit/`          | Isolated component tests |
| Integration    | `tests/integration/`   | Multi-component pipeline tests |
| Regression     | `tests/regression/`    | Known-bug reproduction tests |
| Benchmarks     | `benchmarks/`          | Performance measurement |

### Coverage

Coverage instrumentation is not yet exposed as a CMake option in this repo.

Minimum coverage target: **80%** on core library code.

---

## Pull Request Process

### Before You Start

1. Check existing issues and PRs to avoid duplicate work.
2. For large changes, open an issue or discussion first to align on approach.
3. Create a feature branch from `main`:
   ```bash
   git checkout -b feat/my-feature main
   ```

### Development Workflow

1. Make your changes following the [style guide](STYLEGUIDE.md).
2. Add or update tests for any new/changed functionality.
3. Run the full test suite locally:
   ```bash
   cmake -S native -B build/debug -G Ninja \
       -DCMAKE_BUILD_TYPE=Debug \
       -DMAGNAUNDASONI_BUILD_TESTS=ON
   cmake --build build/debug
   ctest --test-dir build/debug --output-on-failure
   ```
4. Run formatting and linting:
   ```bash
   # Format all changed files
   git diff --name-only main | grep -E '\.(cpp|h)$' | xargs clang-format -i

   # Run the native test target explicitly (optional)
   cmake --build build/debug --target magnaundasoni_tests
   ```
5. Commit with a clear message (see [Commit Messages](#commit-messages)).

### Submitting the PR

1. Push your branch and open a PR against `main`.
2. Fill in the PR template:
   - **What** does this change?
   - **Why** is it needed?
   - **How** was it tested?
   - **Breaking changes** (if any).
3. Ensure CI passes (builds, tests, formatting, linting).
4. Request review from at least one core team member.

### Review Criteria

- [ ] Code follows the [style guide](STYLEGUIDE.md).
- [ ] No heap allocations on audio thread path.
- [ ] Tests added or updated.
- [ ] Documentation updated (if API changed).
- [ ] No compiler warnings (`-Wall -Wextra -Werror`).
- [ ] Performance impact assessed (benchmarks for hot paths).
- [ ] C ABI compatibility maintained (no breaking changes without ADR).

### After Review

- Address review comments with fixup commits.
- Once approved, the maintainer will squash-merge the PR.

---

## Coding Standards

See [`docs/STYLEGUIDE.md`](STYLEGUIDE.md) for the full C++ style guide.

Key points:

- C++17 minimum, C++20 features behind guards.
- 4-space indentation, 100-column limit, Allman braces.
- `PascalCase` for types, `camelCase` for functions/variables, `m_` prefix for
  members.
- No `std::shared_ptr` without justification.
- No allocations on the audio thread.
- Wrap SIMD in `magn::simd` abstractions.

---

## Commit Messages

Follow the [Conventional Commits](https://www.conventionalcommits.org/)
specification:

```
<type>(<scope>): <short description>

<optional body>

<optional footer>
```

### Types

| Type       | Usage |
|------------|-------|
| `feat`     | New feature |
| `fix`      | Bug fix |
| `perf`     | Performance improvement |
| `refactor` | Code restructuring (no behavior change) |
| `test`     | Adding or updating tests |
| `docs`     | Documentation changes |
| `build`    | Build system / CI changes |
| `chore`    | Maintenance tasks |

### Examples

```
feat(diffraction): implement single-edge UTD solver

Adds the Medium-tier diffraction solver using UTD-based per-band
attenuation coefficients. Edge visibility is tested via the active
ray backend.

Closes #42
```

```
fix(streaming): prevent chunk activation stall on frame boundary

The activation budget was checked after insertion rather than before,
allowing an extra chunk to slip through and cause a 3ms spike.
```

---

## Issue Guidelines

### Bug Reports

Include:
- Magnaundasoni version / commit hash
- Platform and backend (DXR / Vulkan RT / Software)
- Minimal reproduction steps
- Expected vs. actual behavior
- Performance stats (`MagnStats`) if performance-related

### Feature Requests

Include:
- Use case description
- Relationship to existing ADRs
- Proposed API surface (if applicable)

---

## Architecture Decision Records

Significant architectural decisions are documented as ADRs in `docs/adr/`.

If your contribution requires a new architectural decision:

1. Copy `docs/adr/TEMPLATE.md` (or follow the format of existing ADRs).
2. Number it sequentially (`0007-your-topic.md`).
3. Include the ADR in your PR for review.
4. ADRs require approval from at least two core team members.

---

## Code of Conduct

All contributors are expected to follow the project's code of conduct.  Be
respectful, constructive, and inclusive.

---

## Questions?

- Open a [GitHub Discussion](https://github.com/<org>/Project-Magnaundasoni/discussions)
  for general questions.
- Tag `@core-team` in issues for architectural guidance.

Thank you for helping make Magnaundasoni better!
