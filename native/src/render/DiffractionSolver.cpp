/**
 * @file DiffractionSolver.cpp
 * @brief UTD-inspired diffraction computation with quality-level dispatch.
 */

#include "DiffractionSolver.h"
#include "BandProcessor.h"

#include <algorithm>
#include <cmath>

namespace magnaundasoni {

static constexpr float kPi      = 3.14159265358979323846f;
static constexpr float kTwoPi   = 6.28318530717958647692f;
static constexpr float kSpeedOfSoundDefault = 343.0f;

bool DiffractionSolver::isVisible(const Vec3& from, const Vec3& to,
                                   const BVH& bvh) const {
    Vec3  dir  = to - from;
    float dist = dir.length();
    if (dist < 1e-6f) return true;

    Ray ray;
    ray.origin    = from;
    ray.direction = dir / dist;
    ray.tMin      = 0.001f;
    ray.tMax      = dist - 0.001f;

    return !bvh.intersectAny(ray);
}

BandArray DiffractionSolver::computeFresnelApprox(
    const DiffractionEdge& edge,
    const Vec3& sourcePos,
    const Vec3& listenerPos) const {

    // Simple Fresnel zone approximation: scalar attenuation based on
    // how far into the shadow region the listener is.
    Vec3 edgeCenter = edge.centerPoint;

    Vec3 srcToEdge = edgeCenter - sourcePos;
    Vec3 edgeToLis = listenerPos - edgeCenter;
    float dSrc = srcToEdge.length();
    float dLis = edgeToLis.length();

    if (dSrc < 1e-6f || dLis < 1e-6f) return BandProcessor::bandFill(1.0f);

    // Approximate Fresnel number for each band
    BandArray atten;
    for (int b = 0; b < 8; ++b) {
        float freq       = kBandCenterFrequencies[b];
        float wavelength = config_.speedOfSound / freq;
        float pathDiff   = (dSrc + dLis) - (listenerPos - sourcePos).length();
        float fresnelN   = 2.0f * pathDiff / wavelength;

        // Simplified diffraction loss from Fresnel number
        float loss;
        if (fresnelN <= 0.0f) {
            loss = 1.0f; // no shadow
        } else {
            // Approximate: attenuation ≈ 1 / (1 + sqrt(N))
            loss = 1.0f / (1.0f + std::sqrt(fresnelN));
        }

        atten[b] = std::max(0.0f, std::min(1.0f, loss));
    }

    return atten;
}

BandArray DiffractionSolver::computeUTDCoefficients(
    const DiffractionEdge& edge,
    const Vec3& sourcePos,
    const Vec3& listenerPos) const {

    Vec3 edgeCenter = edge.centerPoint;
    Vec3 edgeDir    = (edge.endpoint1 - edge.endpoint0);
    float edgeLen   = edgeDir.length();
    if (edgeLen < 1e-6f) return BandProcessor::bandFill(0.5f);
    edgeDir = edgeDir / edgeLen;

    // Project source and listener onto the plane perpendicular to the edge
    Vec3 srcToEdge = edgeCenter - sourcePos;
    Vec3 edgeToLis = listenerPos - edgeCenter;

    // Remove edge-parallel component
    Vec3 srcPerp = srcToEdge - edgeDir * srcToEdge.dot(edgeDir);
    Vec3 lisPerp = edgeToLis - edgeDir * edgeToLis.dot(edgeDir);

    float rSrc = srcPerp.length();
    float rLis = lisPerp.length();

    if (rSrc < 1e-6f || rLis < 1e-6f) return BandProcessor::bandFill(0.5f);

    // Wedge parameter n = wedgeAngle / pi
    float n = edge.wedgeAngle / kPi;
    if (n < 0.01f) n = 0.01f;

    // Incidence angle (alpha) and diffraction angle (beta)
    Vec3 srcPerpNorm = srcPerp / rSrc;
    Vec3 lisPerpNorm = lisPerp / rLis;

    float cosAlpha = srcPerpNorm.dot(lisPerpNorm);
    cosAlpha = std::max(-1.0f, std::min(1.0f, cosAlpha));
    float alpha = std::acos(cosAlpha);

    // Distance parameters
    float dSrc = (edgeCenter - sourcePos).length();
    float dLis = (listenerPos - edgeCenter).length();

    // UTD distance parameter
    float L = (rSrc * rLis) / (rSrc + rLis);

    BandArray atten;
    for (int b = 0; b < 8; ++b) {
        float freq       = kBandCenterFrequencies[b];
        float wavelength = config_.speedOfSound / freq;
        float k          = kTwoPi / wavelength;

        // Kouyoumjian-Pathak UTD coefficient magnitude (simplified)
        // |D| ≈ 1/(2n√(2πkL)) * |cot((π+α)/(2n))·F(kLa+) + cot((π-α)/(2n))·F(kLa-)|
        float sqrtTerm = std::sqrt(kTwoPi * k * std::max(L, 0.01f));
        float prefactor = 1.0f / (2.0f * n * std::max(sqrtTerm, 0.01f));

        // Cotangent terms with safe evaluation
        auto safeCot = [](float x) -> float {
            float s = std::sin(x);
            if (std::abs(s) < 1e-6f) return 0.0f;
            return std::cos(x) / s;
        };

        float arg1 = (kPi + alpha) / (2.0f * n);
        float arg2 = (kPi - alpha) / (2.0f * n);

        float cot1 = safeCot(arg1);
        float cot2 = safeCot(arg2);

        // Fresnel transition function approximation: F(X) ≈ 1 for large X
        float kLaPlus  = k * L * (1.0f + std::cos(alpha));
        float kLaMinus = k * L * (1.0f - std::cos(alpha));

        auto fresnelF = [](float X) -> float {
            if (X > 10.0f) return 1.0f;
            if (X < 0.0f) X = 0.0f;
            // Approximate: F(X) ≈ 1 - exp(-sqrt(X))
            return 1.0f - std::exp(-std::sqrt(X));
        };

        float D = prefactor * std::abs(cot1 * fresnelF(kLaPlus) +
                                        cot2 * fresnelF(kLaMinus));

        // Spreading factor for edge diffraction
        float spreading = 1.0f / std::sqrt(std::max(dSrc * dLis * (dSrc + dLis), 0.01f));

        float totalAtten = D * spreading * edgeLen;

        // Clamp to physically meaningful range
        atten[b] = std::max(0.0f, std::min(1.0f, totalAtten));
    }

    return atten;
}

BandArray DiffractionSolver::computeCascadedUTD(
    const DiffractionEdge& edge1,
    const DiffractionEdge& edge2,
    const Vec3& sourcePos,
    const Vec3& listenerPos) const {

    // Cascaded: source → edge1 → edge2 → listener
    BandArray atten1 = computeUTDCoefficients(edge1, sourcePos, edge2.centerPoint);
    BandArray atten2 = computeUTDCoefficients(edge2, edge1.centerPoint, listenerPos);

    return BandProcessor::bandMultiply(atten1, atten2);
}

std::vector<DiffractionTapInternal> DiffractionSolver::solve(
    const Vec3& sourcePos,
    const Vec3& listenerPos,
    const std::vector<DiffractionEdge>& edges,
    const BVH& bvh) const {

    std::vector<DiffractionTapInternal> taps;
    if (edges.empty()) return taps;

    // Single-edge diffraction
    for (size_t i = 0; i < edges.size(); ++i) {
        const auto& edge = edges[i];

        // Visibility checks: source → edge and edge → listener
        if (!isVisible(sourcePos, edge.centerPoint, bvh)) continue;
        if (!isVisible(edge.centerPoint, listenerPos, bvh)) continue;

        BandArray atten;
        switch (config_.quality) {
            case QualityLevel::Low:
                atten = computeFresnelApprox(edge, sourcePos, listenerPos);
                break;
            case QualityLevel::Medium:
            case QualityLevel::High:
                atten = computeUTDCoefficients(edge, sourcePos, listenerPos);
                break;
            case QualityLevel::Ultra:
                atten = computeUTDCoefficients(edge, sourcePos, listenerPos);
                break;
        }

        // Apply effective band mask
        BandArray bandMask = BandProcessor::getEffectiveBandMask(config_.effectiveBands);
        atten = BandProcessor::bandMultiply(atten, bandMask);

        // Skip negligible contributions
        if (BandProcessor::bandMax(atten) < 1e-6f) continue;

        float dSrc = (edge.centerPoint - sourcePos).length();
        float dLis = (listenerPos - edge.centerPoint).length();
        float totalDist = dSrc + dLis;

        DiffractionTapInternal tap;
        tap.edgeID     = static_cast<uint32_t>(i);
        tap.delay      = totalDist / config_.speedOfSound;
        tap.direction  = (listenerPos - edge.centerPoint).normalized();
        tap.perBandAttenuation = atten;
        tap.pathLength = totalDist;
        tap.depth      = 1;

        taps.push_back(tap);
    }

    // Cascaded diffraction (Ultra quality, depth >= 2)
    if (config_.quality == QualityLevel::Ultra && config_.maxDiffractionDepth >= 2) {
        for (size_t i = 0; i < edges.size() && taps.size() < config_.maxTaps * 2; ++i) {
            for (size_t j = i + 1; j < edges.size() && taps.size() < config_.maxTaps * 2; ++j) {
                const auto& e1 = edges[i];
                const auto& e2 = edges[j];

                // Check path: source → e1 → e2 → listener
                if (!isVisible(sourcePos, e1.centerPoint, bvh)) continue;
                if (!isVisible(e1.centerPoint, e2.centerPoint, bvh)) continue;
                if (!isVisible(e2.centerPoint, listenerPos, bvh)) continue;

                BandArray atten = computeCascadedUTD(e1, e2, sourcePos, listenerPos);
                BandArray bandMask = BandProcessor::getEffectiveBandMask(config_.effectiveBands);
                atten = BandProcessor::bandMultiply(atten, bandMask);

                if (BandProcessor::bandMax(atten) < 1e-6f) continue;

                float d1 = (e1.centerPoint - sourcePos).length();
                float d2 = (e2.centerPoint - e1.centerPoint).length();
                float d3 = (listenerPos - e2.centerPoint).length();
                float totalDist = d1 + d2 + d3;

                DiffractionTapInternal tap;
                tap.edgeID     = static_cast<uint32_t>(i);
                tap.delay      = totalDist / config_.speedOfSound;
                tap.direction  = (listenerPos - e2.centerPoint).normalized();
                tap.perBandAttenuation = atten;
                tap.pathLength = totalDist;
                tap.depth      = 2;

                taps.push_back(tap);
            }
        }
    }

    // Sort by total attenuation energy (descending) and keep top-K
    std::sort(taps.begin(), taps.end(),
        [](const DiffractionTapInternal& a, const DiffractionTapInternal& b) {
            return BandProcessor::bandSum(a.perBandAttenuation) >
                   BandProcessor::bandSum(b.perBandAttenuation);
        });

    if (taps.size() > config_.maxTaps) {
        taps.resize(config_.maxTaps);
    }

    return taps;
}

} // namespace magnaundasoni
