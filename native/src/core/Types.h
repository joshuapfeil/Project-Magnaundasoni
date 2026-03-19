/**
 * @file Types.h
 * @brief Internal math primitives and utility types used throughout the engine.
 */

#ifndef MAGNAUNDASONI_CORE_TYPES_H
#define MAGNAUNDASONI_CORE_TYPES_H

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>

namespace magnaundasoni {

/* ------------------------------------------------------------------ */
/* Band array                                                         */
/* ------------------------------------------------------------------ */
using BandArray = std::array<float, 8>;

/* ------------------------------------------------------------------ */
/* Vec3                                                               */
/* ------------------------------------------------------------------ */
struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s)       const { return {x * s,   y * s,   z * s};   }
    Vec3 operator/(float s)       const { float inv = 1.0f / s; return *this * inv; }
    Vec3 operator-()              const { return {-x, -y, -z}; }

    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s)       { x *= s;   y *= s;   z *= s;   return *this; }

    float dot(const Vec3& o)   const { return x * o.x + y * o.y + z * o.z; }
    Vec3  cross(const Vec3& o) const {
        return {y * o.z - z * o.y,
                z * o.x - x * o.z,
                x * o.y - y * o.x};
    }

    float lengthSq()  const { return dot(*this); }
    float length()    const { return std::sqrt(lengthSq()); }

    Vec3 normalized() const {
        float len = length();
        if (len < 1e-12f) return {0, 0, 0};
        return *this / len;
    }

    float& operator[](int i)       { return (&x)[i]; }
    float  operator[](int i) const { return (&x)[i]; }
};

inline Vec3 operator*(float s, const Vec3& v) { return v * s; }

/* ------------------------------------------------------------------ */
/* Mat4x4                                                             */
/* ------------------------------------------------------------------ */
struct Mat4x4 {
    float m[4][4]{};

    Mat4x4() = default;

    static Mat4x4 identity() {
        Mat4x4 r;
        r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.0f;
        return r;
    }

    Vec3 transformPoint(const Vec3& p) const {
        float w = m[3][0] * p.x + m[3][1] * p.y + m[3][2] * p.z + m[3][3];
        if (std::abs(w) < 1e-12f) w = 1.0f;
        return {(m[0][0] * p.x + m[0][1] * p.y + m[0][2] * p.z + m[0][3]) / w,
                (m[1][0] * p.x + m[1][1] * p.y + m[1][2] * p.z + m[1][3]) / w,
                (m[2][0] * p.x + m[2][1] * p.y + m[2][2] * p.z + m[2][3]) / w};
    }

    Vec3 transformDir(const Vec3& d) const {
        return {m[0][0] * d.x + m[0][1] * d.y + m[0][2] * d.z,
                m[1][0] * d.x + m[1][1] * d.y + m[1][2] * d.z,
                m[2][0] * d.x + m[2][1] * d.y + m[2][2] * d.z};
    }

    Mat4x4 operator*(const Mat4x4& o) const {
        Mat4x4 r;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j) {
                r.m[i][j] = 0;
                for (int k = 0; k < 4; ++k)
                    r.m[i][j] += m[i][k] * o.m[k][j];
            }
        return r;
    }

    /** Load from a column-major 16-float array (OpenGL / Unity convention). */
    static Mat4x4 fromColumnMajor(const float* p) {
        Mat4x4 r;
        for (int c = 0; c < 4; ++c)
            for (int row = 0; row < 4; ++row)
                r.m[row][c] = p[c * 4 + row];
        return r;
    }
};

/* ------------------------------------------------------------------ */
/* AABB                                                               */
/* ------------------------------------------------------------------ */
struct AABB {
    Vec3 min{std::numeric_limits<float>::max(),
             std::numeric_limits<float>::max(),
             std::numeric_limits<float>::max()};
    Vec3 max{std::numeric_limits<float>::lowest(),
             std::numeric_limits<float>::lowest(),
             std::numeric_limits<float>::lowest()};

    AABB() = default;
    AABB(const Vec3& mn, const Vec3& mx) : min(mn), max(mx) {}

    void expand(const Vec3& p) {
        if (p.x < min.x) min.x = p.x;
        if (p.y < min.y) min.y = p.y;
        if (p.z < min.z) min.z = p.z;
        if (p.x > max.x) max.x = p.x;
        if (p.y > max.y) max.y = p.y;
        if (p.z > max.z) max.z = p.z;
    }

    void expand(const AABB& o) { expand(o.min); expand(o.max); }

    bool contains(const Vec3& p) const {
        return p.x >= min.x && p.x <= max.x &&
               p.y >= min.y && p.y <= max.y &&
               p.z >= min.z && p.z <= max.z;
    }

    bool intersects(const AABB& o) const {
        return min.x <= o.max.x && max.x >= o.min.x &&
               min.y <= o.max.y && max.y >= o.min.y &&
               min.z <= o.max.z && max.z >= o.min.z;
    }

    Vec3  center()     const { return (min + max) * 0.5f; }
    Vec3  extent()     const { return max - min; }
    float surfaceArea() const {
        Vec3 d = extent();
        return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
    }

    bool valid() const { return min.x <= max.x && min.y <= max.y && min.z <= max.z; }
};

/* ------------------------------------------------------------------ */
/* Ray                                                                */
/* ------------------------------------------------------------------ */
struct Ray {
    Vec3  origin;
    Vec3  direction;
    float tMin = 0.0f;
    float tMax = std::numeric_limits<float>::max();
};

/* ------------------------------------------------------------------ */
/* HitResult                                                          */
/* ------------------------------------------------------------------ */
struct HitResult {
    float    distance   = std::numeric_limits<float>::max();
    Vec3     normal;
    Vec3     hitPoint;
    uint32_t materialID  = 0;
    uint32_t geometryID  = 0;
    bool     hit         = false;
};

/* ------------------------------------------------------------------ */
/* Importance enum                                                    */
/* ------------------------------------------------------------------ */
enum class Importance : uint32_t {
    Static          = 0,
    QuasiStatic     = 1,
    DynamicImportant= 2,
    DynamicMinor    = 3
};

/* ------------------------------------------------------------------ */
/* SpinLock – lightweight, audio-thread-safe lock                     */
/* ------------------------------------------------------------------ */
class SpinLock {
public:
    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire)) {
            /* spin */
        }
    }
    void unlock() { flag_.clear(std::memory_order_release); }

    bool try_lock() { return !flag_.test_and_set(std::memory_order_acquire); }

private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

} // namespace magnaundasoni

#endif // MAGNAUNDASONI_CORE_TYPES_H
