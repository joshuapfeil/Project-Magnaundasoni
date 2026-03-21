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
#include "render/OutputMixer.h"
#include "spatial/SpatialConfig.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

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

static MagAcousticResult directOnlyResult(float x, float y, float z, float gain = 1.0f) {
    MagAcousticResult result{};
    result.direct.delay = 0.0f;
    result.direct.direction[0] = x;
    result.direct.direction[1] = y;
    result.direct.direction[2] = z;
    for (int i = 0; i < MAG_MAX_BANDS; ++i) {
        result.direct.perBandGain[i] = gain;
    }
    return result;
}

static float channelEnergy(const std::vector<float>& buffer,
                           uint32_t channels,
                           uint32_t channel) {
    float sum = 0.0f;
    for (size_t i = channel; i < buffer.size(); i += channels) {
        sum += std::fabs(buffer[i]);
    }
    return sum;
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
// Default configuration helper
// ---------------------------------------------------------------------------

TEST_CASE("mag_engine_config_defaults returns error for null pointer", "[engine][config]") {
    REQUIRE(mag_engine_config_defaults(nullptr) != MAG_OK);
}

TEST_CASE("mag_engine_config_defaults populates sensible defaults", "[engine][config]") {
    MagEngineConfig cfg{};
    MagStatus status = mag_engine_config_defaults(&cfg);

    REQUIRE(status == MAG_OK);
    REQUIRE(cfg.quality == MAG_QUALITY_MEDIUM);
    REQUIRE(cfg.preferredBackend == MAG_BACKEND_AUTO);
    REQUIRE(cfg.maxSources > 0);
    REQUIRE(cfg.maxReflectionOrder > 0);
    REQUIRE(cfg.maxDiffractionDepth > 0);
    REQUIRE(cfg.raysPerSource > 0);
    REQUIRE(cfg.worldChunkSize > 0.0f);
    REQUIRE(cfg.effectiveBandCount == 8);
}

TEST_CASE("Engine created from defaults works end-to-end", "[engine][config]") {
    MagEngineConfig cfg{};
    REQUIRE(mag_engine_config_defaults(&cfg) == MAG_OK);

    MagEngine engine = nullptr;
    REQUIRE(mag_engine_create(&cfg, &engine) == MAG_OK);
    REQUIRE(engine != nullptr);

    REQUIRE(mag_update(engine, 0.016f) == MAG_OK);

    MagGlobalState state{};
    REQUIRE(mag_get_global_state(engine, &state) == MAG_OK);
    REQUIRE(state.activeSourceCount == 0);

    mag_engine_destroy(engine);
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

TEST_CASE("Spatial config defaults to auto with stereo layout", "[spatial][config]") {
    MagEngine engine = nullptr;
    MagEngineConfig cfg = defaultConfig();
    REQUIRE(mag_engine_create(&cfg, &engine) == MAG_OK);

    MagSpatialConfig spatial{};
    REQUIRE(mag_get_spatial_config(engine, &spatial) == MAG_OK);
    REQUIRE(spatial.mode == MAG_SPATIAL_AUTO);
    REQUIRE(spatial.speakerLayout == MAG_SPEAKERS_STEREO);
    REQUIRE(spatial.hrtfPreset == MAG_HRTF_PRESET_DEFAULT_KEMAR);
    REQUIRE(spatial.maxBinauralSources == 16);

    MagSpatialBackendInfo backend{};
    REQUIRE(mag_get_spatial_backend_info(engine, &backend) == MAG_OK);
    REQUIRE(backend.type == MAG_SPATIAL_BACKEND_BUILTIN_BINAURAL);
    REQUIRE(backend.outputChannels == 2);
    REQUIRE(backend.hasCustomHRTFDataset == 0);

    REQUIRE(mag_engine_destroy(engine) == MAG_OK);
}

TEST_CASE("Spatial config and HRTF controls round-trip through the C ABI", "[spatial][abi]") {
    MagEngine engine = nullptr;
    MagEngineConfig cfg = defaultConfig();
    REQUIRE(mag_engine_create(&cfg, &engine) == MAG_OK);

    MagSpatialConfig spatial{};
    spatial.mode = MAG_SPATIAL_SURROUND_51;
    spatial.speakerLayout = MAG_SPEAKERS_51;
    spatial.hrtfPreset = MAG_HRTF_PRESET_DEFAULT_KEMAR;
    spatial.maxBinauralSources = 4;
    REQUIRE(mag_set_spatial_config(engine, &spatial) == MAG_OK);

    MagSpatialConfig roundTrip{};
    REQUIRE(mag_get_spatial_config(engine, &roundTrip) == MAG_OK);
    REQUIRE(roundTrip.mode == MAG_SPATIAL_SURROUND_51);
    REQUIRE(roundTrip.speakerLayout == MAG_SPEAKERS_51);
    REQUIRE(roundTrip.maxBinauralSources == 4);

    uint8_t fakeSofa[] = {1, 2, 3, 4};
    REQUIRE(mag_set_hrtf_dataset(engine, fakeSofa, sizeof(fakeSofa)) == MAG_OK);

    MagSpatialBackendInfo backend{};
    REQUIRE(mag_get_spatial_backend_info(engine, &backend) == MAG_OK);
    REQUIRE(backend.type == MAG_SPATIAL_BACKEND_BUILTIN_SURROUND);
    REQUIRE(backend.outputChannels == 6);
    REQUIRE(backend.hasCustomHRTFDataset == 1);

    REQUIRE(mag_set_hrtf_preset(engine, MAG_HRTF_PRESET_DEFAULT_KEMAR) == MAG_OK);
    REQUIRE(mag_get_spatial_backend_info(engine, &backend) == MAG_OK);
    REQUIRE(backend.hasCustomHRTFDataset == 0);

    REQUIRE(mag_engine_destroy(engine) == MAG_OK);
}

TEST_CASE("Listener head pose API validates listener IDs and quaternion data", "[spatial][headpose]") {
    MagEngine engine = nullptr;
    MagEngineConfig cfg = defaultConfig();
    REQUIRE(mag_engine_create(&cfg, &engine) == MAG_OK);

    MagListenerDesc listener{};
    listener.forward[2] = 1.0f;
    listener.up[1] = 1.0f;
    MagListenerID listenerID = 0;
    REQUIRE(mag_listener_register(engine, &listener, &listenerID) == MAG_OK);

    const float yaw90[4] = {0.0f, 0.70710677f, 0.0f, 0.70710677f};
    REQUIRE(mag_set_listener_head_pose(engine, listenerID, yaw90) == MAG_OK);
    REQUIRE(mag_set_listener_head_pose(engine, listenerID + 99, yaw90) == MAG_ERROR);

    const float invalidQuat[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    REQUIRE(mag_set_listener_head_pose(engine, listenerID, invalidQuat) == MAG_INVALID_PARAM);

    REQUIRE(mag_engine_destroy(engine) == MAG_OK);
}

TEST_CASE("OutputMixer binaural mode responds to direction and head pose", "[spatial][mixer]") {
    OutputMixer mixer;
    OutputMixer::Config cfg{};
    cfg.channels = 2;
    cfg.maxBlockSize = 64;
    cfg.spatializationMode = OutputMixer::SpatializationMode::Binaural;
    mixer.configure(cfg);

    mixer.stageResult(1, 1, directOnlyResult(1.0f, 0.0f, 1.0f));
    mixer.commitStaged();

    std::vector<float> output(64 * 2, 0.0f);
    mixer.mix(output.data(), 64);
    float leftEnergy = channelEnergy(output, 2, 0);
    float rightEnergy = channelEnergy(output, 2, 1);
    REQUIRE(rightEnergy > leftEnergy);

    const float yaw180[4] = {0.0f, 1.0f, 0.0f, 0.0f};
    mixer.setListenerHeadPose(yaw180);
    mixer.stageResult(2, 1, directOnlyResult(1.0f, 0.0f, 1.0f));
    mixer.commitStaged();

    std::fill(output.begin(), output.end(), 0.0f);
    mixer.mix(output.data(), 64);
    float rotatedLeftEnergy = channelEnergy(output, 2, 0);
    float rotatedRightEnergy = channelEnergy(output, 2, 1);
    REQUIRE(rotatedLeftEnergy > rotatedRightEnergy);
}

TEST_CASE("OutputMixer surround mode distributes energy across configured speakers", "[spatial][surround]") {
    OutputMixer mixer;
    OutputMixer::Config cfg{};
    cfg.channels = 6;
    cfg.maxBlockSize = 32;
    cfg.spatializationMode = OutputMixer::SpatializationMode::Surround;
    cfg.speakerLayout = defaultSpeakerLayout(MAG_SPEAKERS_51);
    mixer.configure(cfg);

    mixer.stageResult(1, 1, directOnlyResult(1.0f, 0.0f, 1.0f));
    mixer.commitStaged();

    std::vector<float> output(32 * 6, 0.0f);
    mixer.mix(output.data(), 32);

    REQUIRE(channelEnergy(output, 6, 1) > channelEnergy(output, 6, 0));
    REQUIRE(channelEnergy(output, 6, 1) > 0.0f);
    REQUIRE(std::accumulate(output.begin(), output.end(), 0.0f,
                            [](float acc, float sample) { return acc + std::fabs(sample); }) > 0.0f);
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
