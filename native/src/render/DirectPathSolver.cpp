/**
 * @file DirectPathSolver.cpp
 * @brief Direct-path computation with multi-layer occlusion and air absorption.
 */

#include "DirectPathSolver.h"
#include "BandProcessor.h"

#include <algorithm>
#include <cmath>

namespace magnaundasoni {

DirectPathResult DirectPathSolver::solve(const Vec3& sourcePos,
                                         const Vec3& listenerPos,
                                         const BVH& bvh,
                                         const Scene& scene) const {
    DirectPathResult result;

    Vec3  toListener = listenerPos - sourcePos;
    float dist       = toListener.length();
    result.distance  = dist;

    if (dist < 1e-6f) {
        result.direction  = Vec3{0.0f, 0.0f, 1.0f};
        result.delay      = 0.0f;
        result.perBandGain = BandProcessor::bandFill(1.0f);
        result.confidence = 1.0f;
        return result;
    }

    Vec3 dir = toListener / dist;
    result.direction = dir;
    result.delay     = dist / config_.speedOfSound;

    // Distance attenuation
    float distAtten = BandProcessor::computeDistanceAttenuation(dist, config_.nearFieldRadius);

    // Air absorption
    BandArray airAtten = BandProcessor::computeAirAbsorption(
        dist, config_.humidity, config_.temperature);

    // Apply band mask
    BandArray bandMask = BandProcessor::getEffectiveBandMask(config_.effectiveBands);

    // Occlusion: trace through geometry accumulating transmission
    uint32_t occluderCount = 0;
    BandArray transmission = accumulateTransmission(
        sourcePos, dir, dist, bvh, scene, occluderCount);

    result.occluded      = (occluderCount > 0);
    result.occluderCount = occluderCount;

    // Combine gains: distance × air × transmission × band mask
    for (int b = 0; b < 8; ++b) {
        result.perBandGain[b] = distAtten * airAtten[b] * transmission[b] * bandMask[b];
    }

    // Compute occlusion LPF cutoff from total transmission loss
    if (result.occluded) {
        float avgTransmission = BandProcessor::bandToSingleGain(
            transmission, FrequencyWeighting::Flat);
        // Map transmission [0,1] to cutoff [500, 20000] Hz
        float lpf = 500.0f + avgTransmission * 19500.0f;
        result.occlusionLPF = std::max(500.0f, std::min(20000.0f, lpf));
    } else {
        result.occlusionLPF = 0.0f; // no filter
    }

    // Confidence: high when unoccluded and close, drops with distance/occlusion
    float distConfidence = std::max(0.0f, 1.0f - dist / config_.maxDistance);
    float occConfidence  = result.occluded ? 0.7f : 1.0f;
    result.confidence    = distConfidence * occConfidence;

    return result;
}

BandArray DirectPathSolver::accumulateTransmission(
    const Vec3& origin,
    const Vec3& direction,
    float maxDist,
    const BVH& bvh,
    const Scene& scene,
    uint32_t& occluderCount) const {

    BandArray totalTransmission = BandProcessor::bandFill(1.0f);
    occluderCount = 0;

    // March through geometry collecting transmission from each occluder
    constexpr uint32_t kMaxOccluders = 8;
    float currentT = 0.001f;

    for (uint32_t i = 0; i < kMaxOccluders; ++i) {
        Ray ray;
        ray.origin    = origin + direction * currentT;
        ray.direction = direction;
        ray.tMin      = 0.0f;
        ray.tMax      = maxDist - currentT - 0.001f;

        if (ray.tMax <= 0.0f) break;

        HitResult hit = bvh.intersect(ray);
        if (!hit.hit) break;

        occluderCount++;

        // Get material transmission coefficients
        const MaterialEntry* mat = scene.getMaterial(hit.materialID);
        if (mat) {
            for (int b = 0; b < 8; ++b) {
                totalTransmission[b] *= mat->transmission[b];
            }
        } else {
            // Default: 50% transmission across all bands
            for (int b = 0; b < 8; ++b) {
                totalTransmission[b] *= 0.5f;
            }
        }

        // Advance past this hit
        currentT += hit.distance + 0.01f;
        if (currentT >= maxDist) break;
    }

    return totalTransmission;
}

} // namespace magnaundasoni
