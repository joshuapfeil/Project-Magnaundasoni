/**
 * @file placeholder_test.cpp
 * @brief Unit tests for the Magnaundasoni native acoustics engine.
 *
 * Tests cover the public C ABI:
 *  - Compile-time constants and enum values
 *  - Engine lifecycle (create / destroy)
 *  - Parameter validation (null / invalid inputs)
 *  - Material preset lookup
 */

#include <catch2/catch_test_macros.hpp>

// Include the public C ABI
#include "Magnaundasoni.h"

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

    // Null config must not succeed
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
    MagSourceID   id = 0;
    REQUIRE(mag_source_register(nullptr, &desc, &id) != MAG_OK);
}

TEST_CASE("mag_listener_register returns error for null engine", "[listener][validation]") {
    MagListenerDesc desc{};
    MagListenerID   id = 0;
    REQUIRE(mag_listener_register(nullptr, &desc, &id) != MAG_OK);
}

TEST_CASE("mag_geometry_register returns error for null engine", "[geometry][validation]") {
    MagGeometryDesc desc{};
    MagGeometryID   id = 0;
    REQUIRE(mag_geometry_register(nullptr, &desc, &id) != MAG_OK);
}

// ---------------------------------------------------------------------------
// Material presets
// ---------------------------------------------------------------------------

TEST_CASE("Known material preset 'concrete' can be retrieved", "[material][preset]") {
    MagMaterialDesc desc{};
    MagStatus status = mag_material_get_preset("concrete", &desc);

    REQUIRE(status == MAG_OK);

    // Absorption values must be in [0, 1]
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
    REQUIRE(state.activeSourceCount == 0); // no sources registered

    mag_engine_destroy(engine);
}

TEST_CASE("mag_set_quality accepts all valid levels", "[engine][quality]") {
    MagEngineConfig cfg = defaultConfig();
    MagEngine engine    = nullptr;

    REQUIRE(mag_engine_create(&cfg, &engine) == MAG_OK);

    REQUIRE(mag_set_quality(engine, MAG_QUALITY_LOW)    == MAG_OK);
    REQUIRE(mag_set_quality(engine, MAG_QUALITY_MEDIUM) == MAG_OK);
    REQUIRE(mag_set_quality(engine, MAG_QUALITY_HIGH)   == MAG_OK);
    REQUIRE(mag_set_quality(engine, MAG_QUALITY_ULTRA)  == MAG_OK);

    mag_engine_destroy(engine);
}
