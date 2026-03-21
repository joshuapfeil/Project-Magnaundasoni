#include "BinauralConvolver.h"

#include <algorithm>
#include <cmath>

namespace magnaundasoni {

namespace {

Vec3 rotateByQuaternion(const Vec3& v, const float q[4]) {
    Vec3 u{q[0], q[1], q[2]};
    float s = q[3];
    return 2.0f * u.dot(v) * u +
           (s * s - u.dot(u)) * v +
           2.0f * s * u.cross(v);
}

} // namespace

void BinauralConvolver::setHeadPose(const float quaternion[4]) {
    if (!quaternion) return;

    float lenSq = quaternion[0] * quaternion[0] +
                  quaternion[1] * quaternion[1] +
                  quaternion[2] * quaternion[2] +
                  quaternion[3] * quaternion[3];
    if (lenSq <= 1e-12f) return;

    float invLen = 1.0f / std::sqrt(lenSq);
    for (int i = 0; i < 4; ++i) {
        quaternion_[i] = quaternion[i] * invLen;
    }
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
