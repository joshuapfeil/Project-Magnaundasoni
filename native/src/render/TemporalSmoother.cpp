/**
 * @file TemporalSmoother.cpp
 * @brief Temporal smoothing with per-component rates and tap crossfading.
 */

#include "TemporalSmoother.h"
#include "BandProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace magnaundasoni {

float TemporalSmoother::expSmooth(float current, float target, float rate, float dt) {
    // Exponential approach: current + (target - current) * (1 - e^(-dt/rate))
    if (rate < 1e-6f) return target;
    float alpha = 1.0f - std::exp(-dt / rate);
    return current + (target - current) * alpha;
}

void TemporalSmoother::resetAll() {
    states_.clear();
    lastSourcePositions_.clear();
}

void TemporalSmoother::resetPair(uint64_t pairKey) {
    states_.erase(pairKey);
    lastSourcePositions_.erase(pairKey);
}

void TemporalSmoother::smooth(uint64_t pairKey,
                               MagAcousticResult& result,
                               const Vec3& sourcePos,
                               float deltaTime) {
    auto& state = states_[pairKey];

    // Teleportation detection
    auto posIt = lastSourcePositions_.find(pairKey);
    if (posIt != lastSourcePositions_.end()) {
        Vec3 delta = sourcePos - posIt->second;
        if (delta.length() > config_.teleportThreshold) {
            state = SmoothedState{};
        }
    }
    lastSourcePositions_[pairKey] = sourcePos;

    if (!state.initialised) {
        // First frame: copy everything directly without smoothing
        state.directDelay = result.direct.delay;
        state.directDirection = Vec3{result.direct.direction[0],
                                     result.direct.direction[1],
                                     result.direct.direction[2]};
        std::memcpy(state.directGain.data(), result.direct.perBandGain, sizeof(float) * 8);
        state.directLPF = result.direct.occlusionLPF;

        std::memcpy(state.lateRT60.data(), result.lateField.rt60, sizeof(float) * 8);
        std::memcpy(state.lateDecay.data(), result.lateField.perBandDecay, sizeof(float) * 8);
        state.roomSize = result.lateField.roomSizeEstimate;
        state.diffuseDir = result.lateField.diffuseDirectionality;

        state.initialised = true;
        return;
    }

    float dt = std::max(deltaTime, 0.001f);

    // --- Direct path smoothing (fast) ---
    float dRate = config_.directSmoothRate;
    state.directDelay = expSmooth(state.directDelay, result.direct.delay, dRate, dt);
    state.directLPF   = expSmooth(state.directLPF, result.direct.occlusionLPF, dRate, dt);

    Vec3 targetDir{result.direct.direction[0],
                   result.direct.direction[1],
                   result.direct.direction[2]};
    state.directDirection.x = expSmooth(state.directDirection.x, targetDir.x, dRate, dt);
    state.directDirection.y = expSmooth(state.directDirection.y, targetDir.y, dRate, dt);
    state.directDirection.z = expSmooth(state.directDirection.z, targetDir.z, dRate, dt);

    for (int b = 0; b < 8; ++b) {
        state.directGain[b] = expSmooth(state.directGain[b],
                                         result.direct.perBandGain[b], dRate, dt);
    }

    // Write back smoothed direct
    result.direct.delay = state.directDelay;
    result.direct.direction[0] = state.directDirection.x;
    result.direct.direction[1] = state.directDirection.y;
    result.direct.direction[2] = state.directDirection.z;
    std::memcpy(result.direct.perBandGain, state.directGain.data(), sizeof(float) * 8);
    result.direct.occlusionLPF = state.directLPF;

    // --- Reflection tap smoothing (moderate) ---
    float rRate = config_.reflectionSmoothRate;

    // Match existing smoothed taps to new taps by tapID
    for (auto& sTap : state.reflectionTaps) {
        sTap.active = false;
    }

    for (uint32_t i = 0; i < result.reflectionCount; ++i) {
        auto& newTap = result.reflections[i];
        bool found = false;

        for (auto& sTap : state.reflectionTaps) {
            if (sTap.tapID == newTap.tapID) {
                // Smooth existing tap
                sTap.delay = expSmooth(sTap.delay, newTap.delay, rRate, dt);
                sTap.direction.x = expSmooth(sTap.direction.x, newTap.direction[0], rRate, dt);
                sTap.direction.y = expSmooth(sTap.direction.y, newTap.direction[1], rRate, dt);
                sTap.direction.z = expSmooth(sTap.direction.z, newTap.direction[2], rRate, dt);
                for (int b = 0; b < 8; ++b) {
                    sTap.energy[b] = expSmooth(sTap.energy[b], newTap.perBandEnergy[b], rRate, dt);
                }
                sTap.fadeGain = std::min(1.0f, sTap.fadeGain + dt / config_.tapFadeInRate);
                sTap.active = true;
                found = true;

                // Write back
                newTap.delay = sTap.delay;
                newTap.direction[0] = sTap.direction.x;
                newTap.direction[1] = sTap.direction.y;
                newTap.direction[2] = sTap.direction.z;
                for (int b = 0; b < 8; ++b) {
                    newTap.perBandEnergy[b] = sTap.energy[b] * sTap.fadeGain;
                }
                break;
            }
        }

        if (!found) {
            // New tap: start with fade-in
            SmoothedState::SmoothReflTap sTap;
            sTap.tapID = newTap.tapID;
            sTap.delay = newTap.delay;
            sTap.direction = Vec3{newTap.direction[0], newTap.direction[1], newTap.direction[2]};
            std::memcpy(sTap.energy.data(), newTap.perBandEnergy, sizeof(float) * 8);
            sTap.fadeGain = dt / config_.tapFadeInRate;
            sTap.active = true;
            state.reflectionTaps.push_back(sTap);

            // Apply fade to output
            for (int b = 0; b < 8; ++b) {
                newTap.perBandEnergy[b] *= sTap.fadeGain;
            }
        }
    }

    // Fade out removed taps (mark inactive ones)
    for (auto& sTap : state.reflectionTaps) {
        if (!sTap.active) {
            sTap.fadeGain -= dt / config_.tapFadeOutRate;
        }
    }
    // Remove fully faded taps
    state.reflectionTaps.erase(
        std::remove_if(state.reflectionTaps.begin(), state.reflectionTaps.end(),
                        [](const SmoothedState::SmoothReflTap& t) {
                            return !t.active && t.fadeGain <= 0.0f;
                        }),
        state.reflectionTaps.end());

    // --- Late field smoothing (slow) ---
    float lRate = config_.lateFieldSmoothRate;
    for (int b = 0; b < 8; ++b) {
        state.lateRT60[b] = expSmooth(state.lateRT60[b], result.lateField.rt60[b], lRate, dt);
        state.lateDecay[b] = expSmooth(state.lateDecay[b],
                                        result.lateField.perBandDecay[b], lRate, dt);
    }
    state.roomSize = expSmooth(state.roomSize, result.lateField.roomSizeEstimate, lRate, dt);
    state.diffuseDir = expSmooth(state.diffuseDir,
                                  result.lateField.diffuseDirectionality, lRate, dt);

    std::memcpy(result.lateField.rt60, state.lateRT60.data(), sizeof(float) * 8);
    std::memcpy(result.lateField.perBandDecay, state.lateDecay.data(), sizeof(float) * 8);
    result.lateField.roomSizeEstimate = state.roomSize;
    result.lateField.diffuseDirectionality = state.diffuseDir;
}

} // namespace magnaundasoni
