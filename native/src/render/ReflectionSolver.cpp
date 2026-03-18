/**
 * @file ReflectionSolver.cpp
 * @brief Monte Carlo reflection tracing with multi-order bounces.
 */

#include "ReflectionSolver.h"
#include "BandProcessor.h"

#include <algorithm>
#include <cmath>

namespace magnaundasoni {

static constexpr float kPi    = 3.14159265358979323846f;
static constexpr float kGoldenAngle = 2.39996322972865332f; // pi*(1+sqrt(5))

Vec3 ReflectionSolver::fibonacciSphereDir(uint32_t index, uint32_t total) {
    float theta = kGoldenAngle * static_cast<float>(index);
    float phi   = std::acos(1.0f - 2.0f * (static_cast<float>(index) + 0.5f)
                            / static_cast<float>(total));
    return Vec3{
        std::sin(phi) * std::cos(theta),
        std::sin(phi) * std::sin(theta),
        std::cos(phi)
    };
}

Vec3 ReflectionSolver::biasedRayDir(uint32_t index, uint32_t total,
                                     const Vec3& toListener) const {
    Vec3 baseDir = fibonacciSphereDir(index, total);

    // For a fraction of rays, blend toward listener direction
    float biasThreshold = config_.importanceBias * static_cast<float>(total);
    if (static_cast<float>(index) < biasThreshold) {
        Vec3 listDir = toListener.normalized();
        float t = 0.5f; // blend factor
        Vec3 blended{
            baseDir.x * (1.0f - t) + listDir.x * t,
            baseDir.y * (1.0f - t) + listDir.y * t,
            baseDir.z * (1.0f - t) + listDir.z * t
        };
        return blended.normalized();
    }
    return baseDir;
}

Vec3 ReflectionSolver::reflect(const Vec3& incident, const Vec3& normal) {
    float d = incident.dot(normal);
    return incident - normal * (2.0f * d);
}

Vec3 ReflectionSolver::scatterDir(const Vec3& normal, uint32_t seed) {
    // Deterministic pseudo-random scatter using hash
    auto hashU = [](uint32_t x) -> float {
        x = ((x >> 16) ^ x) * 0x45d9f3bU;
        x = ((x >> 16) ^ x) * 0x45d9f3bU;
        x = (x >> 16) ^ x;
        return static_cast<float>(x) / static_cast<float>(0xFFFFFFFF);
    };

    float u1 = hashU(seed);
    float u2 = hashU(seed * 2654435761U + 1);

    // Cosine-weighted hemisphere sampling
    float r     = std::sqrt(u1);
    float theta = 2.0f * kPi * u2;
    float x = r * std::cos(theta);
    float y = r * std::sin(theta);
    float z = std::sqrt(std::max(0.0f, 1.0f - u1));

    // Build tangent frame from normal
    Vec3 tangent, bitangent;
    if (std::abs(normal.x) < 0.9f) {
        tangent = Vec3{1, 0, 0}.cross(normal).normalized();
    } else {
        tangent = Vec3{0, 1, 0}.cross(normal).normalized();
    }
    bitangent = normal.cross(tangent);

    return (tangent * x + bitangent * y + normal * z).normalized();
}

std::vector<ReflectionTapInternal> ReflectionSolver::solve(
    const Vec3& sourcePos,
    const Vec3& listenerPos,
    const BVH& bvh,
    const Scene& scene) {

    lastStats_ = ReflectionStats{};
    std::vector<ReflectionTapInternal> taps;

    Vec3 toListener = listenerPos - sourcePos;
    float srcListDist = toListener.length();
    if (srcListDist < 1e-6f) return taps;

    BandArray bandMask = BandProcessor::getEffectiveBandMask(config_.effectiveBands);
    uint32_t nextTapID = 0;

    for (uint32_t r = 0; r < config_.raysPerSource; ++r) {
        lastStats_.totalRays++;

        Vec3 rayDir = biasedRayDir(r, config_.raysPerSource, toListener);

        Vec3      currentOrigin = sourcePos;
        Vec3      currentDir    = rayDir;
        BandArray energy        = BandProcessor::bandFill(1.0f);
        float     totalPath     = 0.0f;

        for (uint32_t bounce = 0; bounce < config_.maxReflectionOrder; ++bounce) {
            Ray ray;
            ray.origin    = currentOrigin;
            ray.direction = currentDir;
            ray.tMin      = 0.001f;
            ray.tMax      = config_.maxPropagationDist - totalPath;

            if (ray.tMax <= 0.0f) break;

            HitResult hit = bvh.intersect(ray);
            if (!hit.hit) break;

            lastStats_.totalHits++;
            totalPath += hit.distance;
            lastStats_.totalPathLength += hit.distance;
            lastStats_.totalBounces++;

            // Apply material absorption and scattering
            const MaterialEntry* mat = scene.getMaterial(hit.materialID);

            BandArray reflectivity;
            float scatterCoeff = 0.0f;

            if (mat) {
                for (int b = 0; b < 8; ++b) {
                    reflectivity[b] = 1.0f - mat->absorption[b];
                    lastStats_.accumulatedAbsorption[b] += mat->absorption[b];
                }
                // Average scattering across bands
                scatterCoeff = 0.0f;
                for (int b = 0; b < 8; ++b) scatterCoeff += mat->scattering[b];
                scatterCoeff /= 8.0f;
            } else {
                reflectivity.fill(0.7f);
                scatterCoeff = 0.3f;
            }

            energy = BandProcessor::bandMultiply(energy, reflectivity);

            // Air absorption for this leg
            BandArray airAtten = BandProcessor::computeAirAbsorption(
                hit.distance, config_.humidity, config_.temperature);
            energy = BandProcessor::bandMultiply(energy, airAtten);

            // Check if energy is negligible
            if (BandProcessor::bandMax(energy) < 1e-6f) break;

            // Test visibility from hit point to listener
            Vec3 hitToListener = listenerPos - hit.hitPoint;
            float hitListDist  = hitToListener.length();

            if (hitListDist > 0.01f) {
                Vec3 hitListDir = hitToListener / hitListDist;

                // Cone test: dot product with normal must be positive (same hemisphere)
                float normalDot = hit.normal.dot(hitListDir);

                if (normalDot > 0.0f) {
                    Ray visRay;
                    visRay.origin    = hit.hitPoint + hit.normal * 0.01f;
                    visRay.direction = hitListDir;
                    visRay.tMin      = 0.0f;
                    visRay.tMax      = hitListDist - 0.01f;

                    if (!bvh.intersectAny(visRay)) {
                        float fullPath = totalPath + hitListDist;
                        float distAtten = BandProcessor::computeDistanceAttenuation(fullPath);

                        BandArray finalAir = BandProcessor::computeAirAbsorption(
                            hitListDist, config_.humidity, config_.temperature);

                        ReflectionTapInternal tap;
                        tap.tapID      = nextTapID++;
                        tap.delay      = fullPath / config_.speedOfSound;
                        tap.direction  = hitListDir;
                        tap.order      = bounce + 1;
                        tap.pathLength = fullPath;
                        tap.lastHitPoint = hit.hitPoint;

                        // Geometric term: cosine weighting at reflection point
                        float cosWeight = normalDot;

                        for (int b = 0; b < 8; ++b) {
                            tap.perBandEnergy[b] = energy[b] * distAtten *
                                finalAir[b] * cosWeight * bandMask[b];
                        }

                        // Stability heuristic: earlier bounces are more stable
                        tap.stability = 1.0f / (1.0f + 0.3f * static_cast<float>(bounce));

                        taps.push_back(tap);
                    }
                }
            }

            // Compute next bounce direction
            Vec3 specularDir = reflect(currentDir, hit.normal);

            // Blend specular and scattered direction
            float scatter = scatterCoeff * config_.scatteringBlend;
            uint32_t scatterSeed = r * 7919 + bounce * 6271;

            if (scatter > 0.01f) {
                Vec3 diffuseDir = scatterDir(hit.normal, scatterSeed);
                // Interpolate between specular and diffuse
                currentDir = Vec3{
                    specularDir.x * (1.0f - scatter) + diffuseDir.x * scatter,
                    specularDir.y * (1.0f - scatter) + diffuseDir.y * scatter,
                    specularDir.z * (1.0f - scatter) + diffuseDir.z * scatter
                }.normalized();
            } else {
                currentDir = specularDir;
            }

            currentOrigin = hit.hitPoint + hit.normal * 0.005f;
        }
    }

    // Compute mean free path estimate
    if (lastStats_.totalHits > 0) {
        lastStats_.meanFreePathEstimate =
            lastStats_.totalPathLength / static_cast<float>(lastStats_.totalHits);
    }

    // Cluster nearby taps
    clusterTaps(taps);

    // Sort by energy (descending) and keep top-K
    std::sort(taps.begin(), taps.end(),
        [](const ReflectionTapInternal& a, const ReflectionTapInternal& b) {
            return BandProcessor::bandSum(a.perBandEnergy) >
                   BandProcessor::bandSum(b.perBandEnergy);
        });

    if (taps.size() > config_.maxTaps) {
        taps.resize(config_.maxTaps);
    }

    return taps;
}

void ReflectionSolver::clusterTaps(std::vector<ReflectionTapInternal>& taps) const {
    if (taps.size() < 2) return;

    constexpr float kTimeTolerance  = 0.002f;  // 2ms
    constexpr float kSpaceTolerance = 0.5f;    // 0.5m

    // Simple O(n²) clustering for typical tap counts (< 100)
    for (size_t i = 0; i < taps.size(); ++i) {
        if (taps[i].merged) continue;

        for (size_t j = i + 1; j < taps.size(); ++j) {
            if (taps[j].merged) continue;

            float dt = std::abs(taps[i].delay - taps[j].delay);
            Vec3 dp  = taps[i].lastHitPoint - taps[j].lastHitPoint;
            float ds = dp.length();

            if (dt < kTimeTolerance && ds < kSpaceTolerance) {
                // Merge j into i (energy-weighted average)
                float ei = BandProcessor::bandSum(taps[i].perBandEnergy);
                float ej = BandProcessor::bandSum(taps[j].perBandEnergy);
                float total = ei + ej;
                if (total < 1e-12f) continue;

                float wi = ei / total;
                float wj = ej / total;

                for (int b = 0; b < 8; ++b) {
                    taps[i].perBandEnergy[b] += taps[j].perBandEnergy[b];
                }
                taps[i].delay = taps[i].delay * wi + taps[j].delay * wj;
                taps[i].stability = std::max(taps[i].stability, taps[j].stability);

                taps[j].merged = true;
            }
        }
    }

    // Remove merged taps
    taps.erase(
        std::remove_if(taps.begin(), taps.end(),
            [](const ReflectionTapInternal& t) { return t.merged; }),
        taps.end());
}

} // namespace magnaundasoni
