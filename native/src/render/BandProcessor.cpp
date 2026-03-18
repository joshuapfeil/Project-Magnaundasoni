/**
 * @file BandProcessor.cpp
 * @brief Implementation of 8-band frequency processing utilities.
 */

#include "BandProcessor.h"
#include <algorithm>
#include <cmath>

namespace magnaundasoni {
namespace BandProcessor {

BandArray computeAirAbsorption(float distanceMeters,
                               float humidity,
                               float temperatureCelsius) {
    BandArray attenuation;

    // Scale factor based on environmental conditions.
    // Higher humidity slightly reduces HF absorption; higher temperature increases it.
    float humidityScale  = 1.0f + (50.0f - humidity) * 0.004f;
    float tempScale      = 1.0f + (temperatureCelsius - 20.0f) * 0.002f;
    float envScale       = std::max(0.2f, humidityScale * tempScale);

    float distKm = distanceMeters * 0.001f;

    for (int i = 0; i < 8; ++i) {
        float absorptionDb = kDefaultAirAbsorptionDbPerKm[i] * envScale * distKm;
        // Convert dB loss to linear attenuation
        attenuation[i] = std::pow(10.0f, -absorptionDb / 20.0f);
        attenuation[i] = std::max(0.0f, std::min(1.0f, attenuation[i]));
    }

    return attenuation;
}

float computeDistanceAttenuation(float distanceMeters, float nearFieldRadius) {
    // Clamp to near-field radius to avoid singularity
    float effectiveDist = std::max(distanceMeters, nearFieldRadius);
    // Inverse-square law: 1 / (4π r²), normalised so 1.0 at reference distance
    return 1.0f / (effectiveDist * effectiveDist);
}

float bandToSingleGain(const BandArray& bands, FrequencyWeighting weighting) {
    float sum = 0.0f;
    float weightSum = 0.0f;

    for (int i = 0; i < 8; ++i) {
        float w = 1.0f;
        if (weighting == FrequencyWeighting::AWeighted) {
            w = dbToLinear(kAWeighting[i]);
        }
        sum += bands[i] * w;
        weightSum += w;
    }

    return (weightSum > 1e-12f) ? (sum / weightSum) : 0.0f;
}

BandArray getEffectiveBandMask(uint32_t effectiveBandCount) {
    BandArray mask;
    mask.fill(0.0f);

    if (effectiveBandCount >= 8) {
        // All 8 bands active
        mask.fill(1.0f);
    } else if (effectiveBandCount >= 6) {
        // Bands 1-6 active (250 Hz – 8 kHz)
        for (int i = 1; i <= 6; ++i) mask[i] = 1.0f;
    } else {
        // 4 bands: 2,3,4,5 (500 Hz – 4 kHz)
        for (int i = 2; i <= 5; ++i) mask[i] = 1.0f;
    }

    return mask;
}

} // namespace BandProcessor
} // namespace magnaundasoni
