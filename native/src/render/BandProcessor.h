/**
 * @file BandProcessor.h
 * @brief 8-band frequency processing utilities for acoustic computations.
 */

#ifndef MAGNAUNDASONI_RENDER_BAND_PROCESSOR_H
#define MAGNAUNDASONI_RENDER_BAND_PROCESSOR_H

#include "../core/Types.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace magnaundasoni {

/// Center frequencies for the 8 octave bands (Hz).
static constexpr float kBandCenterFrequencies[8] = {
    63.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f
};

/// Approximate air absorption coefficients in dB/km per band
/// (at ~20°C, 50% humidity).
static constexpr float kDefaultAirAbsorptionDbPerKm[8] = {
    0.05f, 0.1f, 0.3f, 0.7f, 1.5f, 3.0f, 6.0f, 12.0f
};

/// A-weighting corrections in dB per band.
static constexpr float kAWeighting[8] = {
    -26.2f, -16.1f, -8.6f, -3.2f, 0.0f, 1.2f, 1.0f, -1.1f
};

enum class FrequencyWeighting { Flat, AWeighted };

namespace BandProcessor {

/// Convert a linear gain value to decibels.
inline float linearToDb(float linear) {
    return (linear > 1e-12f) ? 20.0f * std::log10(linear) : -240.0f;
}

/// Convert decibels to linear gain.
inline float dbToLinear(float db) {
    return std::pow(10.0f, db / 20.0f);
}

/// Convert a per-band gain array from dB to linear.
inline BandArray bandFromDecibels(const BandArray& db) {
    BandArray out;
    for (int i = 0; i < 8; ++i) out[i] = dbToLinear(db[i]);
    return out;
}

/// Convert a per-band gain array from linear to dB.
inline BandArray bandToDecibels(const BandArray& linear) {
    BandArray out;
    for (int i = 0; i < 8; ++i) out[i] = linearToDb(linear[i]);
    return out;
}

/// Element-wise multiply two BandArrays.
inline BandArray bandMultiply(const BandArray& a, const BandArray& b) {
    BandArray out;
    for (int i = 0; i < 8; ++i) out[i] = a[i] * b[i];
    return out;
}

/// Element-wise add two BandArrays.
inline BandArray bandAdd(const BandArray& a, const BandArray& b) {
    BandArray out;
    for (int i = 0; i < 8; ++i) out[i] = a[i] + b[i];
    return out;
}

/// Scale all bands by a scalar.
inline BandArray bandScale(const BandArray& a, float s) {
    BandArray out;
    for (int i = 0; i < 8; ++i) out[i] = a[i] * s;
    return out;
}

/// Linearly interpolate between two BandArrays.
inline BandArray bandInterpolate(const BandArray& a, const BandArray& b, float t) {
    BandArray out;
    float oneMinusT = 1.0f - t;
    for (int i = 0; i < 8; ++i) out[i] = a[i] * oneMinusT + b[i] * t;
    return out;
}

/// Sum all bands (total energy).
inline float bandSum(const BandArray& a) {
    float s = 0.0f;
    for (int i = 0; i < 8; ++i) s += a[i];
    return s;
}

/// Maximum value across all bands.
inline float bandMax(const BandArray& a) {
    float m = a[0];
    for (int i = 1; i < 8; ++i) m = std::max(m, a[i]);
    return m;
}

/// Clamp all bands to [lo, hi].
inline BandArray bandClamp(const BandArray& a, float lo, float hi) {
    BandArray out;
    for (int i = 0; i < 8; ++i) out[i] = std::max(lo, std::min(hi, a[i]));
    return out;
}

/// Create a BandArray filled with a single value.
inline BandArray bandFill(float v) {
    BandArray out;
    out.fill(v);
    return out;
}

/// Compute per-band air absorption attenuation (linear) for a given distance.
BandArray computeAirAbsorption(float distanceMeters,
                               float humidity = 50.0f,
                               float temperatureCelsius = 20.0f);

/// Compute distance attenuation (inverse-square law with near-field clamping).
float computeDistanceAttenuation(float distanceMeters, float nearFieldRadius = 0.1f);

/// Collapse a BandArray to a single gain using frequency weighting.
float bandToSingleGain(const BandArray& bands, FrequencyWeighting weighting = FrequencyWeighting::Flat);

/// Get which bands are active for a given effective band count (4, 6, or 8).
/// Returns a BandArray mask: 1.0 for active, 0.0 for inactive.
BandArray getEffectiveBandMask(uint32_t effectiveBandCount);

} // namespace BandProcessor
} // namespace magnaundasoni

#endif // MAGNAUNDASONI_RENDER_BAND_PROCESSOR_H
