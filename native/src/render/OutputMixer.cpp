/**
 * @file OutputMixer.cpp
 * @brief Lock-free double-buffered output mixer with delay lines and FDN reverb.
 */

#include "OutputMixer.h"
#include "BandProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace magnaundasoni {

// Prime-ish delay lengths in samples for a 4-tap FDN
static constexpr uint32_t kFDNDelayLengths[4] = {1559, 1777, 2003, 2281};

OutputMixer::OutputMixer() {
    configure(Config{});
}

void OutputMixer::configure(const Config& cfg) {
    config_ = cfg;
    if (config_.channels == 0) config_.channels = 1;
    if (config_.maxBinauralSources == 0) config_.maxBinauralSources = 16;
    if (config_.speakerLayout.channelCount == 0 ||
        config_.speakerLayout.channelCount > MAG_MAX_SPEAKERS) {
        config_.speakerLayout = defaultSpeakerLayout(MAG_SPEAKERS_STEREO);
    }

    // Allocate delay lines
    uint32_t maxDelaySamples = static_cast<uint32_t>(
        cfg.maxDelayMs * 0.001f * static_cast<float>(cfg.sampleRate)) + 1;

    delayLines_.resize(cfg.channels);
    for (auto& dl : delayLines_) {
        dl.buffer.assign(maxDelaySamples, 0.0f);
        dl.writePos = 0;
        dl.length   = maxDelaySamples;
    }

    // Pre-allocate scratch
    scratchBuffer_.resize(cfg.maxBlockSize * cfg.channels, 0.0f);

    // Initialise FDN
    initialiseFDN();
    binauralConvolver_.configure(config_.sampleRate);
    surroundPanner_.configure(config_.speakerLayout);
}

void OutputMixer::initialiseFDN() {
    for (int i = 0; i < 4; ++i) {
        fdnState_.delays[i].assign(kFDNDelayLengths[i], 0.0f);
        fdnState_.writePos[i] = 0;
    }
    fdnState_.feedback = 0.0f;
}

void OutputMixer::stageResult(uint32_t sourceID, uint32_t listenerID,
                               const MagAcousticResult& result) {
    StagedResult sr;
    sr.sourceID   = sourceID;
    sr.listenerID = listenerID;
    sr.result     = result;

    // Deep-copy the dynamic arrays
    if (result.reflections && result.reflectionCount > 0) {
        sr.reflCopy.assign(result.reflections,
                           result.reflections + result.reflectionCount);
        sr.result.reflections = sr.reflCopy.data();
    } else {
        sr.result.reflections = nullptr;
    }

    if (result.diffractions && result.diffractionCount > 0) {
        sr.diffCopy.assign(result.diffractions,
                           result.diffractions + result.diffractionCount);
        sr.result.diffractions = sr.diffCopy.data();
    } else {
        sr.result.diffractions = nullptr;
    }

    staging_.push_back(std::move(sr));
}

void OutputMixer::commitStaged() {
    swapLock_.lock();
    std::swap(active_, staging_);
    swapLock_.unlock();

    staging_.clear();
}

void OutputMixer::writeDelayLine(float delaySec, float sample, uint32_t channel) {
    if (channel >= delayLines_.size()) return;
    auto& dl = delayLines_[channel];

    uint32_t delaySamples = static_cast<uint32_t>(
        delaySec * static_cast<float>(config_.sampleRate));
    delaySamples = std::min(delaySamples, dl.length - 1);

    uint32_t writeIdx = (dl.writePos + delaySamples) % dl.length;
    dl.buffer[writeIdx] += sample;
}

float OutputMixer::readDelayLine(uint32_t channel) const {
    if (channel >= delayLines_.size()) return 0.0f;
    const auto& dl = delayLines_[channel];
    return dl.buffer[dl.writePos];
}

void OutputMixer::advanceDelayLine() {
    for (auto& dl : delayLines_) {
        dl.buffer[dl.writePos] = 0.0f;
        dl.writePos = (dl.writePos + 1) % dl.length;
    }
}

void OutputMixer::synthesiseReverb(float* outputBuffer, uint32_t numFrames,
                                    const MagLateField& lateField) {
    // Simple FDN reverb synthesis from RT60 parameters
    // Compute average RT60 for feedback gain
    float avgRT60 = 0.0f;
    for (int b = 0; b < 8; ++b) avgRT60 += lateField.rt60[b];
    avgRT60 /= 8.0f;

    if (avgRT60 < 0.01f) return;

    // Feedback gain from desired RT60: g = 10^(-3 * delayTime / RT60)
    // Use average FDN delay length
    float avgDelayTime = 0.0f;
    for (int i = 0; i < 4; ++i) {
        avgDelayTime += static_cast<float>(kFDNDelayLengths[i]) /
                        static_cast<float>(config_.sampleRate);
    }
    avgDelayTime /= 4.0f;

    float targetFeedback = std::pow(10.0f, -3.0f * avgDelayTime / avgRT60);
    targetFeedback = std::max(0.0f, std::min(0.99f, targetFeedback));

    fdnState_.feedback += (targetFeedback - fdnState_.feedback) * config_.feedbackSmoothRate;

    float fb = fdnState_.feedback;
    uint32_t ch = config_.channels;

    // Hadamard-like mixing matrix (4×4 normalised)
    static constexpr float kMix = 0.5f;

    for (uint32_t f = 0; f < numFrames; ++f) {
        // Read from each FDN delay line
        float taps[4];
        for (int i = 0; i < 4; ++i) {
            auto& delay = fdnState_.delays[i];
            taps[i] = delay[fdnState_.writePos[i]];
        }

        // Hadamard mixing
        float mixed[4];
        mixed[0] = kMix * ( taps[0] + taps[1] + taps[2] + taps[3]);
        mixed[1] = kMix * ( taps[0] - taps[1] + taps[2] - taps[3]);
        mixed[2] = kMix * ( taps[0] + taps[1] - taps[2] - taps[3]);
        mixed[3] = kMix * ( taps[0] - taps[1] - taps[2] + taps[3]);

        // Feed back with gain
        for (int i = 0; i < 4; ++i) {
            auto& delay = fdnState_.delays[i];
            delay[fdnState_.writePos[i]] = mixed[i] * fb;
            fdnState_.writePos[i] = (fdnState_.writePos[i] + 1) %
                                     static_cast<uint32_t>(delay.size());
        }

        // Mix FDN output into audio buffer.
        float reverbSample = (taps[0] + taps[1] + taps[2] + taps[3]) * 0.25f;
        if (config_.spatializationMode == SpatializationMode::Surround && ch > 2) {
            std::vector<float> gains(ch, 0.0f);
            surroundPanner_.diffuse(reverbSample * config_.masterGain, gains.data(), ch,
                                    lateField.diffuseDirectionality);
            for (uint32_t c = 0; c < ch; ++c) {
                outputBuffer[f * ch + c] += gains[c];
            }
        } else {
            for (uint32_t c = 0; c < ch; ++c) {
                outputBuffer[f * ch + c] += reverbSample * config_.masterGain;
            }
        }
    }
}

void OutputMixer::setListenerHeadPose(const float quaternion[4]) {
    binauralConvolver_.setHeadPose(quaternion);
}

void OutputMixer::writeSpatialisedTap(const Vec3& direction, float baseDelay, float gain) {
    uint32_t ch = config_.channels;
    switch (config_.spatializationMode) {
        case SpatializationMode::Binaural: {
            auto ears = binauralConvolver_.evaluate(direction, baseDelay, gain * config_.masterGain);
            if (ch >= 2) {
                writeDelayLine(ears.leftDelaySec, ears.leftGain, 0);
                writeDelayLine(ears.rightDelaySec, ears.rightGain, 1);
            } else {
                writeDelayLine(baseDelay, 0.5f * (ears.leftGain + ears.rightGain), 0);
            }
            break;
        }
        case SpatializationMode::Surround: {
            std::vector<float> gains(ch, 0.0f);
            surroundPanner_.pan(direction, gain * config_.masterGain, gains.data(), ch);
            for (uint32_t c = 0; c < ch; ++c) {
                writeDelayLine(baseDelay, gains[c], c);
            }
            break;
        }
        case SpatializationMode::Passthrough:
        default: {
            float pan = direction.x * 0.5f + 0.5f; // x → [0,1]
            if (ch >= 2) {
                writeDelayLine(baseDelay, (1.0f - pan) * gain * config_.masterGain, 0);
                writeDelayLine(baseDelay, pan * gain * config_.masterGain, 1);
            } else {
                writeDelayLine(baseDelay, gain * config_.masterGain, 0);
            }
            break;
        }
    }
}

void OutputMixer::mix(float* outputBuffer, uint32_t numFrames) {
    if (!outputBuffer || numFrames == 0) return;

    uint32_t ch = config_.channels;
    uint32_t totalSamples = numFrames * ch;

    // Zero output
    std::memset(outputBuffer, 0, totalSamples * sizeof(float));

    // Lock and read active results
    swapLock_.lock();
    // Take a local copy of active to minimise lock time
    auto localActive = active_;
    swapLock_.unlock();

    MagLateField combinedLateField{};
    uint32_t lateFieldCount = 0;
    uint32_t binauralSources = 0;

    for (const auto& sr : localActive) {
        const auto& res = sr.result;

        // --- Direct path ---
        float directGain = BandProcessor::bandToSingleGain(
            *reinterpret_cast<const BandArray*>(res.direct.perBandGain),
            FrequencyWeighting::Flat);

        bool binauralMode = config_.spatializationMode == SpatializationMode::Binaural;
        bool useBinaural = binauralMode && binauralSources < config_.maxBinauralSources;
        Vec3 directDir{res.direct.direction[0], res.direct.direction[1], res.direct.direction[2]};
        if (useBinaural) ++binauralSources;
        if (useBinaural) {
            writeSpatialisedTap(directDir, res.direct.delay, directGain);
        } else if (!binauralMode) {
            writeSpatialisedTap(directDir, res.direct.delay, directGain);
        } else {
            float pan = directDir.x * 0.5f + 0.5f;
            if (ch >= 2) {
                writeDelayLine(res.direct.delay, (1.0f - pan) * directGain * config_.masterGain, 0);
                writeDelayLine(res.direct.delay, pan * directGain * config_.masterGain, 1);
            } else {
                writeDelayLine(res.direct.delay, directGain * config_.masterGain, 0);
            }
        }

        // --- Reflections ---
        for (uint32_t i = 0; i < res.reflectionCount && res.reflections; ++i) {
            const auto& tap = res.reflections[i];
            float tapGain = BandProcessor::bandToSingleGain(
                *reinterpret_cast<const BandArray*>(tap.perBandEnergy),
                FrequencyWeighting::Flat);

            writeSpatialisedTap({tap.direction[0], tap.direction[1], tap.direction[2]},
                                tap.delay, tapGain);
        }

        // --- Diffraction ---
        for (uint32_t i = 0; i < res.diffractionCount && res.diffractions; ++i) {
            const auto& tap = res.diffractions[i];
            float tapGain = BandProcessor::bandToSingleGain(
                *reinterpret_cast<const BandArray*>(tap.perBandAttenuation),
                FrequencyWeighting::Flat);

            writeSpatialisedTap({tap.direction[0], tap.direction[1], tap.direction[2]},
                                tap.delay, tapGain);
        }

        // Accumulate late field
        for (int b = 0; b < 8; ++b) {
            combinedLateField.rt60[b] += res.lateField.rt60[b];
            combinedLateField.perBandDecay[b] += res.lateField.perBandDecay[b];
        }
        combinedLateField.roomSizeEstimate += res.lateField.roomSizeEstimate;
        lateFieldCount++;
    }

    // Average late field
    if (lateFieldCount > 0) {
        float inv = 1.0f / static_cast<float>(lateFieldCount);
        for (int b = 0; b < 8; ++b) {
            combinedLateField.rt60[b] *= inv;
            combinedLateField.perBandDecay[b] *= inv;
        }
        combinedLateField.roomSizeEstimate *= inv;
    }

    // Read from delay lines into output
    for (uint32_t f = 0; f < numFrames; ++f) {
        for (uint32_t c = 0; c < ch; ++c) {
            outputBuffer[f * ch + c] += readDelayLine(c);
        }
        advanceDelayLine();
    }

    // Synthesise reverb tail
    synthesiseReverb(outputBuffer, numFrames, combinedLateField);
}

} // namespace magnaundasoni
