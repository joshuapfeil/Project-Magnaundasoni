#ifndef MAGNAUNDASONI_SPATIAL_QUATERNION_H
#define MAGNAUNDASONI_SPATIAL_QUATERNION_H

#include "../core/Types.h"

#include <cmath>

namespace magnaundasoni {

inline Vec3 rotateByQuaternion(const Vec3& v, const float q[4]) {
    Vec3 u{q[0], q[1], q[2]};
    float s = q[3];
    return 2.0f * u.dot(v) * u +
           (s * s - u.dot(u)) * v +
           2.0f * s * u.cross(v);
}

inline bool normaliseQuaternion(const float in[4], float out[4]) {
    if (!in || !out) return false;
    float lenSq = in[0] * in[0] + in[1] * in[1] +
                  in[2] * in[2] + in[3] * in[3];
    if (lenSq <= 1e-12f) return false;
    float invLen = 1.0f / std::sqrt(lenSq);
    for (int i = 0; i < 4; ++i) out[i] = in[i] * invLen;
    return true;
}

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_SPATIAL_QUATERNION_H
