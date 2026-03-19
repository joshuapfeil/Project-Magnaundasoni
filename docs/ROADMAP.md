# Magnaundasoni – Roadmap

## Vision

Deliver a production-quality, cross-platform real-time acoustics runtime that
integrates seamlessly with major game engines and custom applications.

---

## Phase 1 — Foundation (Months 1–4)

**Goal**: Core runtime with software BVH, direct sound, and basic reflections.

| Milestone | Deliverable | Target |
|-----------|-------------|--------|
| **M1.1** – Project bootstrap | CMake build system, CI pipeline, coding standards enforced | Month 1 |
| **M1.2** – C ABI scaffold | `magn_init`, `magn_shutdown`, `magn_tick` stubs with result codes | Month 1 |
| **M1.3** – Software BVH | CPU BVH2 builder (SAH) + SIMD traversal (SSE4.1 / NEON) | Month 2 |
| **M1.4** – Scene graph | Source / listener / geometry registration, double-buffered updates | Month 2 |
| **M1.5** – Direct path solver | Distance attenuation, air absorption, occlusion ray test | Month 3 |
| **M1.6** – Early reflections | Image-source method (order 1–2) + stochastic validation rays | Month 3–4 |
| **M1.7** – Material system | `MagnMaterial` struct, 8 built-in presets, per-triangle material IDs | Month 3 |
| **M1.8** – Output contract v1 | `MagnAcousticState` assembly, triple-buffered query path | Month 4 |
| **M1.9** – Test suite | Unit tests (≥80% coverage on core), integration test harness | Month 4 |

**Exit criteria**: Standalone demo renders direct sound + first-order
reflections in a static scene using the software BVH, queryable via C ABI.

---

## Phase 2 — Feature Complete Core (Months 5–8)

**Goal**: Full acoustic feature set, hardware RT, streaming, and first engine
adapter.

| Milestone | Deliverable | Target |
|-----------|-------------|--------|
| **M2.1** – Late field estimator | Per-band RT60 estimation, diffuse field descriptor | Month 5 |
| **M2.2** – Diffraction solver | Edge extraction, UTD-based single-edge diffraction (Medium tier) | Month 5–6 |
| **M2.3** – DXR backend | D3D12 + DXR 1.1 BLAS/TLAS management, inline ray tracing | Month 6 |
| **M2.4** – Vulkan RT backend | `VK_KHR_ray_query` implementation | Month 6–7 |
| **M2.5** – Streaming system | Chunk lifecycle, fidelity zones, async loading, activation budget | Month 7 |
| **M2.6** – Quality tiers | Low / Medium / High / Ultra presets, per-setting overrides | Month 7 |
| **M2.7** – Unity adapter (beta) | C# wrapper package, automatic chunk loading, parameter mapping | Month 7–8 |
| **M2.8** – Source directivity | Polar/balloon pattern support | Month 8 |
| **M2.9** – Performance profiler | `magn_debug_get_stats`, on-screen HUD overlay | Month 8 |

**Exit criteria**: Full acoustic pipeline runs on hardware RT + software
fallback.  Unity adapter drives audio sources in a sample scene with streaming.

---

## Phase 3 — Production Hardening (Months 9–12)

**Goal**: Unreal adapter, console support, optimization, and public beta.

| Milestone | Deliverable | Target |
|-----------|-------------|--------|
| **M3.1** – Unreal adapter | UE5 plug-in module, World Partition integration | Month 9–10 |
| **M3.2** – Console backends | Xbox Series X/S (DXR), PlayStation 5 (software + custom RT) | Month 9–10 |
| **M3.3** – Cascaded diffraction | Depth-2 edge cascades, spatial hash for pair lookup | Month 10 |
| **M3.4** – Built-in renderer | HRTF binaural + speaker panning + convolution reverb | Month 10–11 |
| **M3.5** – Memory optimization | Pool tuning, allocation-free audio path audit | Month 11 |
| **M3.6** – Multi-listener | Support for split-screen / multiple listener scenarios | Month 11 |
| **M3.7** – Documentation | Complete API docs, integration guides, material reference | Month 12 |
| **M3.8** – Public beta release | Versioned packages for Unity + Unreal + standalone | Month 12 |

**Exit criteria**: Both Unity and Unreal adapters are production-usable.
Console targets pass platform certification requirements.  Public beta
available.

---

## Phase 4 — Ecosystem & Scale (Months 13–18)

**Goal**: Third-party middleware bridges, advanced features, and community
growth.

| Milestone | Deliverable | Target |
|-----------|-------------|--------|
| **M4.1** – FMOD adapter | Bridge plug-in: Magnaundasoni output → FMOD spatializer parameters | Month 13–14 |
| **M4.2** – Wwise adapter | Bridge plug-in: Magnaundasoni output → Wwise spatial audio | Month 14–15 |
| **M4.3** – Metal RT backend | Apple Silicon ray tracing for macOS / iOS | Month 14–15 |
| **M4.4** – Nintendo Switch optimization | ARM NEON tuning, reduced-memory mode | Month 15 |
| **M4.5** – Offline bake tools | Pre-compute probes, edge caches, BVH blobs from CLI | Month 15–16 |
| **M4.6** – Editor extensions | Unity custom inspector, Unreal editor mode for material painting | Month 16–17 |
| **M4.7** – Open source community | Contributor onboarding, plugin API for custom backends | Month 17 |
| **M4.8** – v1.0 stable release | Semantic versioning, LTS commitment, migration guides | Month 18 |

**Exit criteria**: Stable 1.0 release with FMOD/Wwise bridges, all major
platforms supported, active contributor community.

---

## Risk Register

| Risk | Impact | Mitigation |
|------|--------|------------|
| Hardware RT adoption slower than expected | Medium | Software BVH is always the fallback; optimize it aggressively |
| Unity/Unreal API breaking changes | Medium | Adapter layers are thin; pin to LTS versions |
| Performance targets not met on consoles | High | Quality tier system allows graceful degradation |
| Scope creep into mixer/music territory | High | ADR-0001 enforces product boundary |
| Patent risks around UTD implementations | Low | UTD is public-domain physics; implementation is novel |

---

## References

- ADR-0001 through ADR-0006
- `docs/design/architecture.md`
- `docs/Performance-Tuning.md`
