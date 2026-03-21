#ifndef MAGNAUNDASONI_SPATIAL_BINAURALCONVOLVER_H
#define MAGNAUNDASONI_SPATIAL_BINAURALCONVOLVER_H

#include "../core/Types.h"

namespace magnaundasoni {

class BinauralConvolver {
public:
    struct EarGains {
        float leftGain = 0.0f;
        float rightGain = 0.0f;
        float leftDelaySec = 0.0f;
        float rightDelaySec = 0.0f;
    };

    void configure(uint32_t sampleRate) { sampleRate_ = sampleRate; }
    void setHeadPose(const float quaternion[4]);
    EarGains evaluate(const Vec3& worldDirection, float baseDelaySec,
                      float gain) const;

private:
    Vec3 rotateToHeadSpace(const Vec3& worldDirection) const;

    uint32_t sampleRate_ = 48000;
    float quaternion_[4] = {0.0f, 0.0f, 0.0f, 1.0f};
};

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_SPATIAL_BINAURALCONVOLVER_H
