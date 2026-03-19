/**
 * @file ReverbEstimator.cpp
 * @brief Late reverberation estimation using Sabine/Eyring equations.
 */

#include "ReverbEstimator.h"
#include "BandProcessor.h"

#include <algorithm>
#include <cmath>

namespace magnaundasoni {

void ReverbEstimator::reset() {
    smoothedResult_ = LateFieldResult{};
    hasHistory_ = false;
}

void ReverbEstimator::estimateRoomGeometry(const ReflectionStats& stats,
                                            float& outVolume,
                                            float& outSurfaceArea) const {
    // Estimate room geometry from ray statistics.
    // Mean free path: MFP = 4V/S  →  V/S = MFP/4
    // We need another relation: assume roughly cubic room where S ≈ 6 * V^(2/3)

    float mfp = stats.meanFreePathEstimate;
    if (mfp < 0.1f) mfp = 5.0f; // fallback

    // From MFP = 4V/S and S = 6V^(2/3):
    //   MFP = 4V / (6V^(2/3)) = (2/3)V^(1/3)
    //   V = (1.5 * MFP)³
    float cubeRoot = 1.5f * mfp;
    outVolume      = cubeRoot * cubeRoot * cubeRoot;

    // S ≈ 6 * V^(2/3)
    float vTwoThirds = std::pow(outVolume, 2.0f / 3.0f);
    outSurfaceArea   = 6.0f * vTwoThirds;

    // Sanity clamps
    outVolume      = std::max(1.0f, std::min(1000000.0f, outVolume));
    outSurfaceArea = std::max(6.0f, std::min(60000.0f, outSurfaceArea));
}

RoomSizeClass ReverbEstimator::classifyRoom(float volume, float meanFreePath) const {
    if (meanFreePath > config_.outdoorThreshold) return RoomSizeClass::Outdoor;
    if (volume < 100.0f)   return RoomSizeClass::Small;
    if (volume < 1000.0f)  return RoomSizeClass::Medium;
    if (volume < 10000.0f) return RoomSizeClass::Large;
    return RoomSizeClass::Outdoor;
}

BandArray ReverbEstimator::computeAverageAbsorption(const Scene& scene) const {
    BandArray avgAbsorption;
    avgAbsorption.fill(0.0f);

    auto geoIDs = scene.getAllGeometryIDs();
    int matCount = 0;

    for (uint32_t gid : geoIDs) {
        const GeometryEntry* geo = scene.getGeometry(gid);
        if (!geo) continue;

        const MaterialEntry* mat = scene.getMaterial(geo->materialID);
        if (!mat) continue;

        for (int b = 0; b < 8; ++b) {
            avgAbsorption[b] += mat->absorption[b];
        }
        matCount++;
    }

    if (matCount > 0) {
        float inv = 1.0f / static_cast<float>(matCount);
        for (int b = 0; b < 8; ++b) {
            avgAbsorption[b] *= inv;
        }
    } else {
        // Default absorption
        avgAbsorption.fill(0.15f);
    }

    return avgAbsorption;
}

LateFieldResult ReverbEstimator::estimate(const ReflectionStats& stats,
                                           const Scene& scene) {
    LateFieldResult result;

    // Estimate room geometry
    float volume, surfaceArea;
    estimateRoomGeometry(stats, volume, surfaceArea);
    result.roomSizeEstimate    = volume;
    result.surfaceAreaEstimate = surfaceArea;

    // Get average per-band absorption
    BandArray alpha = computeAverageAbsorption(scene);
    BandArray bandMask = BandProcessor::getEffectiveBandMask(config_.effectiveBands);

    // Compute RT60 per band
    for (int b = 0; b < 8; ++b) {
        float a = std::max(alpha[b], 0.001f);

        float rt60;
        if (config_.useEyring) {
            // Eyring: RT60 = 0.161 * V / (-S * ln(1-α))
            float logTerm = -std::log(std::max(1.0f - a, 0.001f));
            rt60 = 0.161f * volume / (surfaceArea * logTerm);
        } else {
            // Sabine: RT60 = 0.161 * V / (S * α)
            rt60 = 0.161f * volume / (surfaceArea * a);
        }

        rt60 = std::max(config_.minRT60, std::min(config_.maxRT60, rt60));
        result.rt60[b] = rt60 * bandMask[b];

        // Decay envelope: per-band absorption coefficient (used by mixer)
        result.perBandDecay[b] = a * bandMask[b];
    }

    // Room classification
    result.roomClass = classifyRoom(volume, stats.meanFreePathEstimate);

    // Diffuse directionality: narrow rooms are more directional
    // Estimate from ratio of volume to surface area (spherical rooms → 0)
    float compactness = std::pow(volume, 1.0f / 3.0f) / std::max(surfaceArea, 1.0f);
    result.diffuseDirectionality = std::max(0.0f, std::min(1.0f,
        1.0f - compactness * 36.0f));

    // Reverb density: higher in smaller rooms with varied materials
    float avgAbsorption = BandProcessor::bandSum(alpha) / 8.0f;
    float absorptionVariance = 0.0f;
    for (int b = 0; b < 8; ++b) {
        float diff = alpha[b] - avgAbsorption;
        absorptionVariance += diff * diff;
    }
    absorptionVariance /= 8.0f;

    float sizeScale = 1.0f / (1.0f + volume * 0.001f);
    result.reverbDensity = std::max(0.0f, std::min(1.0f,
        sizeScale * (1.0f + absorptionVariance * 10.0f)));

    // Temporal smoothing
    if (hasHistory_) {
        float s = config_.smoothingFactor;
        float oms = 1.0f - s;

        for (int b = 0; b < 8; ++b) {
            result.rt60[b] = smoothedResult_.rt60[b] * s + result.rt60[b] * oms;
            result.perBandDecay[b] = smoothedResult_.perBandDecay[b] * s +
                                     result.perBandDecay[b] * oms;
        }
        result.roomSizeEstimate = smoothedResult_.roomSizeEstimate * s +
                                  result.roomSizeEstimate * oms;
        result.diffuseDirectionality = smoothedResult_.diffuseDirectionality * s +
                                       result.diffuseDirectionality * oms;
        result.reverbDensity = smoothedResult_.reverbDensity * s +
                               result.reverbDensity * oms;
    }

    smoothedResult_ = result;
    hasHistory_ = true;

    return result;
}

} // namespace magnaundasoni
