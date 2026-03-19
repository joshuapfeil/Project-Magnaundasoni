/**
 * @file TemporalSmoother.h
 * @brief Per-source temporal smoothing to prevent audio artifacts between frames.
 */

#ifndef MAGNAUNDASONI_RENDER_TEMPORAL_SMOOTHER_H
#define MAGNAUNDASONI_RENDER_TEMPORAL_SMOOTHER_H

#include "../core/Types.h"
#include "Magnaundasoni.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace magnaundasoni {

/// Smoothed acoustic state per source-listener pair.
struct SmoothedState {
    // Direct path
    float     directDelay         = 0.0f;
    Vec3      directDirection;
    BandArray directGain          = {};
    float     directLPF           = 0.0f;

    // Reflection taps (smoothed copies)
    struct SmoothReflTap {
        uint32_t  tapID     = 0;
        float     delay     = 0.0f;
        Vec3      direction;
        BandArray energy    = {};
        float     fadeGain  = 1.0f;   // 0→1 fade-in, 1→0 fade-out
        bool      active    = false;
    };
    std::vector<SmoothReflTap> reflectionTaps;

    // Diffraction taps
    struct SmoothDiffTap {
        uint32_t  edgeID    = 0;
        float     delay     = 0.0f;
        Vec3      direction;
        BandArray attenuation = {};
        float     fadeGain  = 1.0f;
        bool      active    = false;
    };
    std::vector<SmoothDiffTap> diffractionTaps;

    // Late field
    BandArray lateRT60        = {};
    BandArray lateDecay       = {};
    float     roomSize        = 0.0f;
    float     diffuseDir      = 0.0f;

    bool initialised = false;
};

class TemporalSmoother {
public:
    struct Config {
        float directSmoothRate      = 0.2f;   // fast: 0.1–0.3
        float reflectionSmoothRate  = 0.4f;   // moderate: 0.3–0.5
        float diffractionSmoothRate = 0.4f;   // moderate: 0.3–0.5
        float lateFieldSmoothRate   = 0.7f;   // slow: 0.5–0.8
        float tapFadeInRate         = 0.15f;  // seconds to fade in new tap
        float tapFadeOutRate        = 0.25f;  // seconds to fade out removed tap
        float teleportThreshold     = 10.0f;  // position change in metres that triggers reset
    };

    void configure(const Config& cfg) { config_ = cfg; }

    /// Apply smoothing to an acoustic result for a given source-listener pair.
    /// Modifies the result in-place, blending with the previous frame.
    void smooth(uint64_t pairKey,
                MagAcousticResult& result,
                const Vec3& sourcePos,
                float deltaTime);

    /// Reset all smoothing state (e.g., on scene change).
    void resetAll();

    /// Reset state for a specific pair.
    void resetPair(uint64_t pairKey);

private:
    /// Exponential smoothing helper.
    static float expSmooth(float current, float target, float rate, float dt);

    Config config_;
    std::unordered_map<uint64_t, SmoothedState> states_;
    std::unordered_map<uint64_t, Vec3> lastSourcePositions_;
};

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_RENDER_TEMPORAL_SMOOTHER_H
