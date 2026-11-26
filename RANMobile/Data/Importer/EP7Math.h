//
// EP7Math.h — Minimal D3DX-compatible math used by the authentic EP7 animation
// pipeline (Phase 3J).  Only the operations the original source demonstrably
// performs are implemented, with matched conventions.
//
// Source evidence:
//   enginelib/Meshs/SAnimation.cpp  — DecompressionQuaternion, SetPosQuat, SetTime, GetThisKey
//   enginelib/Meshs/DxBoneCollector.cpp — DxSkeleton::UpdateBones (quaternion path)
//   enginelib/Meshs/DxAniKeys.h     — SQuatPos / SQuatPosKey / QUATCOMP
//
// Conventions verified from the original code:
//   * D3DXQuaternionMultiply(out, a, b) == out = a * b  (Hamilton product, a then b)
//     -- D3DXMatrixMultiply(a,b) in DirectX is row-major and composes so that
//        D3DXMatrixMultiply(&out, &A, &B) means transform by A then B.
//     UpdateBones does:  matCombined = matRot(local) * matCur(parent),
//     i.e. it builds child-to-world by premultiplying the parent transform.
//   * D3DXMatrixRotationQuaternion builds a row-major rotation matrix from a quaternion.
//   * QUATCOMP decompression (SAnimation.cpp:52): x,y,z are 16-bit signed deltas divided
//     by 32768, w is 16-bit unsigned divided by 65535 -- note the *different* divisor
//     for w, which is reproduced exactly.
//
#pragma once

#include <cmath>
#include <cstdint>

namespace RANMobile {
namespace ep7math {

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vec3() = default;
    Vec3(float ax, float ay, float az) : x(ax), y(ay), z(az) {}
};

struct Quat {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
    Quat() = default;
    Quat(float ax, float ay, float az, float aw) : x(ax), y(ay), z(az), w(aw) {}
};

// Row-major 4x4, matches D3DXMATRIX memory layout (m[row][col]); translation is
// carried in _41.._43 exactly like D3DX (row 3).
struct Mat4 {
    float m[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}};

    static Mat4 Identity() { return Mat4(); }

    void SetIdentity() {
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c) m[r][c] = (r == c) ? 1.0f : 0.0f;
    }

    float* operator[](int row) { return m[row]; }
    const float* operator[](int row) const { return m[row]; }
};

// D3DXMatrixMultiply(out, a, b) -> out = a * b
inline Mat4 Multiply(const Mat4& a, const Mat4& b) {
    Mat4 out;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += a.m[r][k] * b.m[k][c];
            out.m[r][c] = sum;
        }
    }
    return out;
}

// D3DXMatrixMultiply(&out, &out, &in) for RHS accumulation, exactly as the
// original animation sampling code composes scale * rotation * translation.
inline void MultiplyIntoRhs(Mat4& inout, const Mat4& rhs) {
    inout = Multiply(inout, rhs);
}

inline Mat4 Translation(float x, float y, float z) {
    Mat4 out;
    out.m[3][0] = x;
    out.m[3][1] = y;
    out.m[3][2] = z;
    return out;
}

inline Mat4 Scaling(float x, float y, float z) {
    Mat4 out;
    out.m[0][0] = x;
    out.m[1][1] = y;
    out.m[2][2] = z;
    return out;
}

// D3DXMatrixRotationQuaternion (row-vector convention as used by D3DX)
inline Mat4 RotationQuaternion(const Quat& q) {
    const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    const float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

    Mat4 out;
    out.m[0][0] = 1.0f - 2.0f * (yy + zz);
    out.m[0][1] = 2.0f * (xy + wz);
    out.m[0][2] = 2.0f * (xz - wy);
    out.m[0][3] = 0.0f;

    out.m[1][0] = 2.0f * (xy - wz);
    out.m[1][1] = 1.0f - 2.0f * (xx + zz);
    out.m[1][2] = 2.0f * (yz + wx);
    out.m[1][3] = 0.0f;

    out.m[2][0] = 2.0f * (xz + wy);
    out.m[2][1] = 2.0f * (yz - wx);
    out.m[2][2] = 1.0f - 2.0f * (xx + yy);
    out.m[2][3] = 0.0f;

    out.m[3][0] = 0.0f;
    out.m[3][1] = 0.0f;
    out.m[3][2] = 0.0f;
    out.m[3][3] = 1.0f;
    return out;
}

// D3DXQuaternionMultiply(out, a, b)
inline Quat Multiply(const Quat& a, const Quat& b) {
    Quat out;
    out.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
    out.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
    out.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
    out.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
    return out;
}

inline Quat Normalize(const Quat& q) {
    const float len2 = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    if (len2 <= 0.0f) return Quat(0.0f, 0.0f, 0.0f, 1.0f);
    const float inv = 1.0f / std::sqrt(len2);
    return Quat(q.x * inv, q.y * inv, q.z * inv, q.w * inv);
}

inline Quat Inverse(const Quat& q) {
    const float len2 = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    if (len2 <= 0.0f) return Quat(0.0f, 0.0f, 0.0f, 1.0f);
    const float inv = 1.0f / len2;
    return Quat(-q.x * inv, -q.y * inv, -q.z * inv, q.w * inv);
}

inline Vec3 Lerp(const Vec3& a, const Vec3& b, float t) {
    return Vec3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
}

inline Quat Slerp(const Quat& a, const Quat& b, float t) {
    float cosom = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    Quat bb = b;
    if (cosom < 0.0f) {
        cosom = -cosom;
        bb = Quat(-b.x, -b.y, -b.z, -b.w);
    }

    float scale0 = 1.0f;
    float scale1 = 0.0f;
    if ((1.0f - cosom) > 1e-6f) {
        const float omega = std::acos(cosom);
        const float sinom = std::sin(omega);
        if (std::fabs(sinom) > 1e-9f) {
            scale0 = std::sin((1.0f - t) * omega) / sinom;
            scale1 = std::sin(t * omega) / sinom;
        }
    } else {
        // Nearly identical: linear blend is numerically safer.
        scale0 = 1.0f - t;
        scale1 = t;
    }

    Quat out;
    out.x = a.x * scale0 + bb.x * scale1;
    out.y = a.y * scale0 + bb.y * scale1;
    out.z = a.z * scale0 + bb.z * scale1;
    out.w = a.w * scale0 + bb.w * scale1;
    return out;
}

// D3DXVec3TransformCoord: transform a point by a row-vector matrix.
inline Vec3 TransformCoord(const Vec3& v, const Mat4& mat) {
    Vec3 out;
    const float w = v.x * mat.m[0][3] + v.y * mat.m[1][3] + v.z * mat.m[2][3] + mat.m[3][3];
    const float invW = (std::fabs(w) > 1e-9f) ? (1.0f / w) : 1.0f;
    out.x = (v.x * mat.m[0][0] + v.y * mat.m[1][0] + v.z * mat.m[2][0] + mat.m[3][0]) * invW;
    out.y = (v.x * mat.m[0][1] + v.y * mat.m[1][1] + v.z * mat.m[2][1] + mat.m[3][1]) * invW;
    out.z = (v.x * mat.m[0][2] + v.y * mat.m[1][2] + v.z * mat.m[2][2] + mat.m[3][2]) * invW;
    return out;
}

// D3DXVec3TransformNormal: transform a direction (ignores translation, no w divide).
inline Vec3 TransformNormal(const Vec3& v, const Mat4& mat) {
    Vec3 out;
    out.x = v.x * mat.m[0][0] + v.y * mat.m[1][0] + v.z * mat.m[2][0];
    out.y = v.x * mat.m[0][1] + v.y * mat.m[1][1] + v.z * mat.m[2][1];
    out.z = v.x * mat.m[0][2] + v.y * mat.m[1][2] + v.z * mat.m[2][2];
    return out;
}

inline Vec3 NormalizeVec3(const Vec3& v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len <= 1e-9f) return Vec3(0.0f, 0.0f, 0.0f);
    const float inv = 1.0f / len;
    return Vec3(v.x * inv, v.y * inv, v.z * inv);
}

// Quaternion compression/decompression constants (SAnimation.cpp:24-26).
inline constexpr float kDiv1_65535 = 0.000015259021896f;
inline constexpr float kDiv1_32768 = 0.000030517578125f;

// Authentic QUATCOMP -> quaternion (SAnimation.cpp DecompressionQuaternion).
inline Quat DecompressQuaternion(uint32_t dwFirst, uint32_t dwSecond) {
    Quat out;
    int nTemp = static_cast<int>(dwFirst >> 16);
    nTemp -= 32767;
    out.x = static_cast<float>(nTemp) * kDiv1_32768;

    nTemp = static_cast<int>(dwFirst & 0xffffu);
    nTemp -= 32767;
    out.y = static_cast<float>(nTemp) * kDiv1_32768;

    nTemp = static_cast<int>(dwSecond >> 16);
    nTemp -= 32767;
    out.z = static_cast<float>(nTemp) * kDiv1_32768;

    nTemp = static_cast<int>(dwSecond & 0xffffu);
    out.w = static_cast<float>(nTemp) * kDiv1_65535;
    return out;
}

}  // namespace ep7math
}  // namespace RANMobile
