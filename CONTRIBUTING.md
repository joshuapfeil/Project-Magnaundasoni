# Contributing to Project Magnaundasoni

Thank you for your interest in contributing!  A quick-start summary is below;
see [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md) for the complete guide.

---

## Branch Strategy

| Branch pattern      | Purpose                                     |
|---------------------|---------------------------------------------|
| `main`              | Stable, always passing CI                   |
| `feat/<name>`       | New features — branch from `main`           |
| `fix/<name>`        | Bug fixes — branch from `main`              |
| `docs/<name>`       | Documentation-only changes                  |
| `chore/<name>`      | Maintenance, build, or tooling changes      |

Create feature/fix branches from `main` and open a pull request back to `main`.

---

## Quick Contribution Steps

1. Fork the repository and create your branch:
   ```bash
   git checkout -b feat/my-feature main
   ```
2. Make your changes following the [code style guide](docs/STYLEGUIDE.md).
3. Build and test locally (see [BUILD.md](BUILD.md)):
   ```bash
   cmake -S native -B build/debug -G Ninja \
       -DCMAKE_BUILD_TYPE=Debug \
       -DMAGNAUNDASONI_BUILD_TESTS=ON
   cmake --build build/debug
   ctest --test-dir build/debug --output-on-failure
   ```
4. Format changed files:
   ```bash
   git diff --name-only main | grep -E '\.(cpp|h)$' | xargs clang-format -i
   ```
5. Commit using [Conventional Commits](https://www.conventionalcommits.org/):
   ```
   feat(diffraction): add UTD single-edge solver
   fix(streaming): prevent stall on chunk boundary
   docs(readme): update getting-started section
   ```
6. Push and open a pull request against `main`.

---

## Code Style Highlights

- **C++17** minimum; C++20 features behind `#if __cplusplus >= 202002L` guards.
- **4-space indentation**, 100-column line limit, Allman-style braces.
- `PascalCase` for types/classes, `camelCase` for functions/variables,
  `m_` prefix for member variables.
- No `std::shared_ptr` without justification; no heap allocations on the audio
  thread.
- Wrap SIMD intrinsics inside `magn::simd` abstractions.

Full rules: [`docs/STYLEGUIDE.md`](docs/STYLEGUIDE.md)

---

## Running Tests

```bash
# All unit tests (Debug)
ctest --test-dir build/debug --output-on-failure

# Native unit test binary
./build/debug/magnaundasoni_tests
```

Coverage reporting is not yet exposed as a CMake option in this repository.

Minimum coverage target: **80 %** on core library code.

---

## Pull Request Checklist

- [ ] Code follows the [style guide](docs/STYLEGUIDE.md)
- [ ] No heap allocations on the audio thread path
- [ ] Tests added or updated
- [ ] Documentation updated if the API changed
- [ ] No new compiler warnings (`-Wall -Wextra -Werror`)
- [ ] Performance impact assessed for hot paths

---

## More Information

- Full contribution guide: [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md)
- Architecture decisions: [`docs/adr/`](docs/adr/)
- Build instructions: [`BUILD.md`](BUILD.md)
- IDE setup: [`docs/SETUP_DEVELOPMENT.md`](docs/SETUP_DEVELOPMENT.md)

Questions?  Open a
[GitHub Discussion](https://github.com/joshuapfeil/Project-Magnaundasoni/discussions)
or tag `@core-team` in issues.
