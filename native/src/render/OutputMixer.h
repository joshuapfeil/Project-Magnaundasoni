/**
 * @file OutputMixer.h
 * @brief Lock-free double-buffered output mixer for the audio callback.
 */

#ifndef MAGNAUNDASONI_RENDER_OUTPUT_MIXER_H
#define MAGNAUNDASONI_RENDER_OUTPUT_MIXER_H

#include "../core/Types.h"
#include "../spatial/BinauralConvolver.h"
#include "../spatial/SurroundPanner.h"
#include "../spatial/SpatialConfig.h"
#include "Magnaundasoni.h"

#include <atomic>
#include <cstdint>
#include <vector>

namespace magnaundasoni {

class OutputMixer {
public:
    enum class SpatializationMode : uint32_t {
        Passthrough = 0,
        Binaural,
        Surround
    };

    struct Config {
        uint32_t sampleRate    = 48000;
        uint32_t channels      = 2;        // stereo
        uint32_t maxBlockSize  = 1024;
        float    maxDelayMs    = 2000.0f;  // max delay line length
        float    masterGain    = 1.0f;
        float    feedbackSmoothRate = 0.001f;
        SpatializationMode spatializationMode = SpatializationMode::Passthrough;
        uint32_t maxBinauralSources = 16;
        MagSpeakerLayout speakerLayout = defaultSpeakerLayout(MAG_SPEAKERS_STEREO);
    };

    OutputMixer();
    ~OutputMixer() = default;

    void configure(const Config& cfg);

    /// Called by the simulation thread: stage a new result for mixing.
    void stageResult(uint32_t sourceID, uint32_t listenerID,
                     const MagAcousticResult& result);

    /// Swap the staged buffer into the active buffer (call once per sim frame).
    void commitStaged();

    /// Called by the audio thread: mix all staged results into the output buffer.
    /// @param outputBuffer Interleaved float samples [frames * channels].
    /// @param numFrames Number of audio frames to produce.
    void mix(float* outputBuffer, uint32_t numFrames);

    void setListenerHeadPose(const float quaternion[4]);

    /// Get the currently configured sample rate.
    uint32_t getSampleRate() const { return config_.sampleRate; }

private:
    /// Write a delayed sample into the delay line.
    void writeDelayLine(float delaySec, float sample, uint32_t channel);

    /// Read from the delay line at the current position.
    float readDelayLine(uint32_t channel) const;

    /// Advance the delay line write head by one sample.
    void advanceDelayLine();

    /// Synthesise simple parametric reverb tail from RT60/decay.
    void synthesiseReverb(float* outputBuffer, uint32_t numFrames,
                          const MagLateField& lateField);

    void writeSpatialisedTap(const Vec3& direction, float baseDelay, float gain);

    Config config_;

    // Delay line per channel
    struct DelayLine {
        std::vector<float> buffer;
        uint32_t writePos = 0;
        uint32_t length   = 0;
    };
    std::vector<DelayLine> delayLines_;

    // Double-buffer: simulation writes to staging, audio reads from active
    struct StagedResult {
        uint32_t sourceID   = 0;
        uint32_t listenerID = 0;
        MagAcousticResult result{};
        // Owned copies of dynamic arrays
        std::vector<MagReflectionTap> reflCopy;
        std::vector<MagDiffractionTap> diffCopy;
    };

    std::vector<StagedResult> staging_;
    std::vector<StagedResult> active_;
    SpinLock                  swapLock_;

    // Pre-allocated mix scratch buffers
    std::vector<float> scratchBuffer_;

    // Reverb state: simple feedback delay network
    struct FDNState {
        std::vector<float> delays[4];  // 4 delay lines
        uint32_t writePos[4] = {};
        float    feedback = 0.0f;
    };
    FDNState fdnState_;
    BinauralConvolver binauralConvolver_;
    SurroundPanner    surroundPanner_;

    void initialiseFDN();
};

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_RENDER_OUTPUT_MIXER_H
