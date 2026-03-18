/**
 * @file ReverbEstimator.h
 * @brief Late reverberation field estimation from ray tracing statistics.
 */

#ifndef MAGNAUNDASONI_RENDER_REVERB_ESTIMATOR_H
#define MAGNAUNDASONI_RENDER_REVERB_ESTIMATOR_H

#include "../core/Types.h"
#include "Magnaundasoni.h"
#include "ReflectionSolver.h"

#include <cstdint>

namespace magnaundasoni {

/// Room size classification.
enum class RoomSizeClass : uint32_t {
    Small   = 0,  // < 100 m³
    Medium  = 1,  // 100–1000 m³
    Large   = 2,  // 1000–10000 m³
    Outdoor = 3   // > 10000 m³ or no enclosure
};

/// Internal late-field result.
struct LateFieldResult {
    BandArray perBandDecay          = {};
    BandArray rt60                  = {};
    float     roomSizeEstimate      = 0.0f;    // m³
    float     surfaceAreaEstimate   = 0.0f;    // m²
    float     diffuseDirectionality = 0.0f;    // [0,1]
    float     reverbDensity         = 0.0f;    // [0,1]
    RoomSizeClass roomClass         = RoomSizeClass::Medium;
};

class ReverbEstimator {
public:
    struct Config {
        float    smoothingFactor   = 0.85f;  // temporal smoothing for RT60
        bool     useEyring         = true;   // Eyring vs Sabine
        float    minRT60           = 0.05f;  // seconds
        float    maxRT60           = 10.0f;  // seconds
        float    outdoorThreshold  = 50.0f;  // mean free path threshold for outdoor
        uint32_t effectiveBands    = 8;
    };

    void configure(const Config& cfg) { config_ = cfg; }

    /// Estimate late-field from reflection ray stats and scene materials.
    LateFieldResult estimate(const ReflectionStats& stats,
                             const Scene& scene);

    /// Get the smoothed result (call after estimate).
    const LateFieldResult& getSmoothedResult() const { return smoothedResult_; }

    /// Reset temporal state (e.g., after teleportation).
    void reset();

private:
    /// Estimate room volume and surface area from ray stats.
    void estimateRoomGeometry(const ReflectionStats& stats,
                              float& outVolume,
                              float& outSurfaceArea) const;

    /// Classify room size.
    RoomSizeClass classifyRoom(float volume, float meanFreePath) const;

    /// Compute average absorption per band from the scene.
    BandArray computeAverageAbsorption(const Scene& scene) const;

    Config          config_;
    LateFieldResult smoothedResult_;
    bool            hasHistory_ = false;
};

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_RENDER_REVERB_ESTIMATOR_H
