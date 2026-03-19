/**
 * @file placeholder_test.cpp
 * @brief Unit tests for the Magnaundasoni native acoustics engine.
 *
 * Tests cover the public C ABI plus a minimal set of band-math sanity checks
 * so CI can validate both the exported runtime surface and the canonical
 * 8-band frequency model.
 */

#include <catch2/catch_test_macros.hpp>

#include "Magnaundasoni.h"
#include "render/BandProcessor.h"

#include <cmath>

using namespace magnaundasoni;
using namespace magnaundasoni::BandProcessor;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static MagEngineConfig defaultConfig() {
    MagEngineConfig cfg{};
    cfg.quality             = MAG_QUALITY_MEDIUM;
    cfg.preferredBackend    = MAG_BACKEND_SOFTWARE_BVH;
    cfg.maxSources          = 16;
    cfg.maxReflectionOrder  = 2;
    cfg.maxDiffractionDepth = 2;
    cfg.raysPerSource       = 64;
    cfg.threadCount         = 1;
    cfg.worldChunkSize      = 50.0f;
    cfg.effectiveBandCount  = 8;
    return cfg;
}

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

TEST_CASE("MAG_MAX_BANDS is 8", "[constants]") {
    REQUIRE(MAG_MAX_BANDS == 8);
}

TEST_CASE("Status codes have expected values", "[constants]") {
    REQUIRE(MAG_OK              ==  0);
    REQUIRE(MAG_ERROR           == -1);
    REQUIRE(MAG_INVALID_PARAM   == -2);
    REQUIRE(MAG_OUT_OF_MEMORY   == -3);
    REQUIRE(MAG_NOT_INITIALIZED == -4);
}

TEST_CASE("Quality enum values are ordered", "[constants]") {
    REQUIRE(MAG_QUALITY_LOW    < MAG_QUALITY_MEDIUM);
    REQUIRE(MAG_QUALITY_MEDIUM < MAG_QUALITY_HIGH);
    REQUIRE(MAG_QUALITY_HIGH   < MAG_QUALITY_ULTRA);
}

// ---------------------------------------------------------------------------
// Engine lifecycle
// ---------------------------------------------------------------------------

TEST_CASE("Engine create and destroy succeeds with valid config", "[engine][lifecycle]") {
    MagEngineConfig cfg = defaultConfig();
    MagEngine engine    = nullptr;

    MagStatus status = mag_engine_create(&cfg, &engine);

    REQUIRE(status == MAG_OK);
    REQUIRE(engine != nullptr);

    status = mag_engine_destroy(engine);
    REQUIRE(status == MAG_OK);
}

TEST_CASE("Engine create returns error for null config", "[engine][lifecycle]") {
    MagEngine engine = nullptr;
    MagStatus status = mag_engine_create(nullptr, &engine);

    REQUIRE(status != MAG_OK);
    REQUIRE(engine == nullptr);
}

TEST_CASE("Engine create returns error for null output pointer", "[engine][lifecycle]") {
    MagEngineConfig cfg = defaultConfig();
    MagStatus status    = mag_engine_create(&cfg, nullptr);

    REQUIRE(status != MAG_OK);
}

TEST_CASE("Engine destroy returns error for null engine", "[engine][lifecycle]") {
    MagStatus status = mag_engine_destroy(nullptr);
    REQUIRE(status != MAG_OK);
}

// ---------------------------------------------------------------------------
// Parameter validation
// ---------------------------------------------------------------------------

TEST_CASE("mag_update returns error for null engine", "[engine][validation]") {
    REQUIRE(mag_update(nullptr, 0.016f) != MAG_OK);
}

TEST_CASE("mag_set_quality returns error for null engine", "[engine][validation]") {
    REQUIRE(mag_set_quality(nullptr, MAG_QUALITY_HIGH) != MAG_OK);
}

TEST_CASE("mag_get_global_state returns error for null engine", "[engine][validation]") {
    MagGlobalState state{};
    REQUIRE(mag_get_global_state(nullptr, &state) != MAG_OK);
}

TEST_CASE("mag_source_register returns error for null engine", "[source][validation]") {
    MagSourceDesc desc{};
    MagSourceID id = 0;
    REQUIRE(mag_source_register(nullptr, &desc, &id) != MAG_OK);
}

TEST_CASE("mag_listener_register returns error for null engine", "[listener][validation]") {
    MagListenerDesc desc{};
    MagListenerID id = 0;
    REQUIRE(mag_listener_register(nullptr, &desc, &id) != MAG_OK);
}

TEST_CASE("mag_geometry_register returns error for null engine", "[geometry][validation]") {
    MagGeometryDesc desc{};
    MagGeometryID id = 0;
    REQUIRE(mag_geometry_register(nullptr, &desc, &id) != MAG_OK);
}

// ---------------------------------------------------------------------------
// Material presets
// ---------------------------------------------------------------------------

TEST_CASE("Known material preset 'concrete' can be retrieved", "[material][preset]") {
    MagMaterialDesc desc{};
    MagStatus status = mag_material_get_preset("concrete", &desc);

    REQUIRE(status == MAG_OK);

    for (int i = 0; i < MAG_MAX_BANDS; ++i) {
        REQUIRE(desc.absorption[i] >= 0.0f);
        REQUIRE(desc.absorption[i] <= 1.0f);
    }
}

TEST_CASE("Unknown preset returns non-OK status", "[material][preset]") {
    MagMaterialDesc desc{};
    MagStatus status = mag_material_get_preset("__nonexistent_preset__", &desc);
    REQUIRE(status != MAG_OK);
}

TEST_CASE("mag_material_get_preset returns error for null name", "[material][preset]") {
    MagMaterialDesc desc{};
    MagStatus status = mag_material_get_preset(nullptr, &desc);
    REQUIRE(status != MAG_OK);
}

// ---------------------------------------------------------------------------
// Engine operations (post-create)
// ---------------------------------------------------------------------------

TEST_CASE("mag_update succeeds on valid engine", "[engine][update]") {
    MagEngineConfig cfg = defaultConfig();
    MagEngine engine    = nullptr;

    REQUIRE(mag_engine_create(&cfg, &engine) == MAG_OK);
    REQUIRE(engine != nullptr);

    MagStatus status = mag_update(engine, 0.016f);
    REQUIRE(status == MAG_OK);

    mag_engine_destroy(engine);
}

TEST_CASE("mag_get_global_state returns valid data after update", "[engine][state]") {
    MagEngineConfig cfg = defaultConfig();
    MagEngine engine    = nullptr;

    REQUIRE(mag_engine_create(&cfg, &engine) == MAG_OK);
    REQUIRE(mag_update(engine, 0.016f) == MAG_OK);

    MagGlobalState state{};
    MagStatus status = mag_get_global_state(engine, &state);
    REQUIRE(status == MAG_OK);
    REQUIRE(state.activeSourceCount == 0);

    mag_engine_destroy(engine);
}

TEST_CASE("mag_set_quality accepts all valid levels", "[engine][quality]") {
    MagEngineConfig cfg = defaultConfig();
    MagEngine engine    = nullptr;

    REQUIRE(mag_engine_create(&cfg, &engine) == MAG_OK);

    REQUIRE(mag_set_quality(engine, MAG_QUALITY_LOW) == MAG_OK);
    REQUIRE(mag_set_quality(engine, MAG_QUALITY_MEDIUM) == MAG_OK);
    REQUIRE(mag_set_quality(engine, MAG_QUALITY_HIGH) == MAG_OK);
    REQUIRE(mag_set_quality(engine, MAG_QUALITY_ULTRA) == MAG_OK);

    mag_engine_destroy(engine);
}

// ---------------------------------------------------------------------------
// Band-math placeholder coverage
// ---------------------------------------------------------------------------

TEST_CASE("Band center frequencies match the canonical 8-band schema", "[bands][placeholder]") {
    static constexpr float kExpected[8] = {
        63.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f
    };

    for (int i = 0; i < 8; ++i) {
        REQUIRE(std::fabs(kBandCenterFrequencies[i] - kExpected[i]) <= 0.01f);
    }

    for (int i = 1; i < 8; ++i) {
        float ratio = kBandCenterFrequencies[i] / kBandCenterFrequencies[i - 1];
        REQUIRE(std::fabs(ratio - 2.0f) <= 0.05f);
    }
}

TEST_CASE("Sum of unity band gains stays within the expected range", "[bands][placeholder]") {
    BandArray ones = bandFill(1.0f);
    REQUIRE(bandSum(ones) == 8.0f);
}

TEST_CASE("Band helpers preserve basic gain math invariants", "[bands][placeholder]") {
    BandArray a = bandFill(0.5f);
    BandArray b = bandFill(0.3f);
    BandArray ones = bandFill(1.0f);

    BandArray added = bandAdd(a, b);
    BandArray scaled = bandScale(ones, 0.5f);
    BandArray multiplied = bandMultiply(added, ones);
    BandArray interpolated = bandInterpolate(a, ones, 0.5f);
    BandArray clamped = bandClamp({-1.0f, 0.0f, 0.5f, 1.0f, 1.5f, 2.0f, -0.5f, 0.9f}, 0.0f, 1.0f);

    for (int i = 0; i < 8; ++i) {
        REQUIRE(std::fabs(added[i] - 0.8f) <= 1e-5f);
        REQUIRE(std::fabs(scaled[i] - 0.5f) <= 1e-6f);
        REQUIRE(std::fabs(multiplied[i] - added[i]) <= 1e-6f);
        REQUIRE(std::fabs(interpolated[i] - 0.75f) <= 1e-6f);
        REQUIRE(clamped[i] >= 0.0f);
        REQUIRE(clamped[i] <= 1.0f);
    }

    REQUIRE(std::fabs(bandMax({0.1f, 0.2f, 0.9f, 0.4f, 0.3f, 0.8f, 0.5f, 0.6f}) - 0.9f) <= 1e-6f);
    REQUIRE(std::fabs(linearToDb(dbToLinear(-6.0f)) + 6.0f) <= 1e-3f);
}
