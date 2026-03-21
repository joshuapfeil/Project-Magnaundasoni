#include "BinauralConvolver.h"
#include "Quaternion.h"

#include <algorithm>

namespace magnaundasoni {

void BinauralConvolver::setHeadPose(const float quaternion[4]) {
    normalizeQuaternion(quaternion, quaternion_);
}

Vec3 BinauralConvolver::rotateToHeadSpace(const Vec3& worldDirection) const {
    float inverse[4] = {-quaternion_[0], -quaternion_[1], -quaternion_[2], quaternion_[3]};
    return rotateByQuaternion(worldDirection.normalized(), inverse).normalized();
}

BinauralConvolver::EarGains BinauralConvolver::evaluate(const Vec3& worldDirection,
                                                        float baseDelaySec,
                                                        float gain) const {
    Vec3 headDirection = rotateToHeadSpace(worldDirection);
    float azimuth = std::atan2(headDirection.x, headDirection.z);
    float pan = 0.5f * (std::sin(azimuth) + 1.0f);

    EarGains out;
    out.leftGain = std::sqrt(std::max(0.0f, 1.0f - pan)) * gain;
    out.rightGain = std::sqrt(std::max(0.0f, pan)) * gain;

    float itd = 0.00035f * std::sin(azimuth);
    out.leftDelaySec = baseDelaySec + std::max(0.0f, -itd);
    out.rightDelaySec = baseDelaySec + std::max(0.0f, itd);
    return out;
}

} // namespace magnaundasoni
