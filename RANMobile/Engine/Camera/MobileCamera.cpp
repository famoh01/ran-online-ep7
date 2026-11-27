#include "MobileCamera.h"
#include <algorithm>
#include <cmath>

namespace RANMobile {

namespace {
constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

void Normalize3(float& x, float& y, float& z) {
    const float len = std::sqrt(x * x + y * y + z * z);
    if (len > 1e-6f) {
        const float inv = 1.0f / len;
        x *= inv;
        y *= inv;
        z *= inv;
    }
}

void Cross3(float ax, float ay, float az, float bx, float by, float bz,
            float& ox, float& oy, float& oz) {
    ox = ay * bz - az * by;
    oy = az * bx - ax * bz;
    oz = ax * by - ay * bx;
}

float Dot3(float ax, float ay, float az, float bx, float by, float bz) {
    return ax * bx + ay * by + az * bz;
}
} // namespace

void MobileCamera::OnDrag(float deltaX, float deltaY) {
    m_targetYaw += deltaX * m_sensitivity;
    m_targetPitch = std::clamp(m_targetPitch - deltaY * m_sensitivity, -15.0f, 75.0f);
}

void MobileCamera::OnPan(float deltaX, float deltaY) {
    const float yawRad = m_yaw * kDegToRad;
    const float rightX = std::cos(yawRad);
    const float rightZ = -std::sin(yawRad);
    const float fwdX = std::sin(yawRad);
    const float fwdZ = std::cos(yawRad);
    const float panScale = std::max(0.05f, m_distance * 0.002f);
    m_targetX += (-deltaX * rightX + deltaY * fwdX) * panScale;
    m_targetZ += (-deltaX * rightZ + deltaY * fwdZ) * panScale;
}

void MobileCamera::OnPinch(float scaleDelta) {
    m_distance = std::clamp(m_distance - scaleDelta * 2.0f, m_minDistance, m_maxDistance);
}

void MobileCamera::SetTarget(float x, float y, float z) {
    m_targetX = x;
    m_targetY = y;
    m_targetZ = z;
}

void MobileCamera::Reset(float yaw, float pitch, float distance, float tx, float ty, float tz) {
    m_yaw = yaw;
    m_targetYaw = yaw;
    m_pitch = std::clamp(pitch, -15.0f, 75.0f);
    m_targetPitch = m_pitch;
    m_distance = std::clamp(distance, m_minDistance, m_maxDistance);
    m_targetX = tx;
    m_targetY = ty;
    m_targetZ = tz;
}

void MobileCamera::SetDistanceBounds(float minDist, float maxDist) {
    m_minDistance = std::max(0.25f, minDist);
    m_maxDistance = std::max(m_minDistance, maxDist);
    m_distance = std::clamp(m_distance, m_minDistance, m_maxDistance);
}

void MobileCamera::SetProjectionParams(float fovDeg, float aspect, float nearZ, float farZ) {
    m_fovDeg = std::clamp(fovDeg, 20.0f, 110.0f);
    m_aspect = (aspect > 0.05f) ? aspect : 16.0f / 9.0f;
    m_nearZ = std::max(0.05f, nearZ);
    m_farZ = std::max(m_nearZ + 1.0f, farZ);
}

void MobileCamera::SetViewportAspect(float aspect) {
    if (aspect > 0.05f) m_aspect = aspect;
}

void MobileCamera::Update(float deltaTime) {
    float t = std::min(1.0f, std::max(0.0f, m_smoothing * deltaTime));
    m_yaw += (m_targetYaw - m_yaw) * t;
    m_pitch += (m_targetPitch - m_pitch) * t;
}

void MobileCamera::GetEyePosition(float& outX, float& outY, float& outZ) const {
    const float yawRad = m_yaw * kDegToRad;
    const float pitchRad = m_pitch * kDegToRad;
    const float horiz = m_distance * std::cos(pitchRad);
    outX = m_targetX - horiz * std::sin(yawRad);
    outY = m_targetY + m_distance * std::sin(pitchRad);
    outZ = m_targetZ - horiz * std::cos(yawRad);
}

void MobileCamera::ComputeViewMatrix(float out16[16]) const {
    float eyeX = 0.0f, eyeY = 0.0f, eyeZ = 0.0f;
    GetEyePosition(eyeX, eyeY, eyeZ);

    float fx = m_targetX - eyeX;
    float fy = m_targetY - eyeY;
    float fz = m_targetZ - eyeZ;
    if (fx * fx + fy * fy + fz * fz < 1e-8f) {
        fz = 1.0f;
    }
    Normalize3(fx, fy, fz);

    float ux = 0.0f, uy = 1.0f, uz = 0.0f;
    if (std::fabs(Dot3(fx, fy, fz, ux, uy, uz)) > 0.999f) {
        ux = 0.0f; uy = 0.0f; uz = 1.0f;
    }

    float rx = 0.0f, ry = 0.0f, rz = 0.0f;
    Cross3(ux, uy, uz, fx, fy, fz, rx, ry, rz);
    Normalize3(rx, ry, rz);

    float vx = 0.0f, vy = 0.0f, vz = 0.0f;
    Cross3(fx, fy, fz, rx, ry, rz, vx, vy, vz);

    // Column-major LH View Matrix
    out16[0] = rx;  out16[1] = vx;  out16[2]  = fx; out16[3]  = 0.0f;
    out16[4] = ry;  out16[5] = vy;  out16[6]  = fy; out16[7]  = 0.0f;
    out16[8] = rz;  out16[9] = vz;  out16[10] = fz; out16[11] = 0.0f;
    out16[12] = -Dot3(rx, ry, rz, eyeX, eyeY, eyeZ);
    out16[13] = -Dot3(vx, vy, vz, eyeX, eyeY, eyeZ);
    out16[14] = -Dot3(fx, fy, fz, eyeX, eyeY, eyeZ);
    out16[15] = 1.0f;
}

void MobileCamera::ComputeProjectionMatrix(float out16[16]) const {
    for (int i = 0; i < 16; ++i) out16[i] = 0.0f;
    const float halfRad = 0.5f * m_fovDeg * kDegToRad;
    const float f = 1.0f / std::tan(halfRad);
    const float invRange = 1.0f / (m_farZ - m_nearZ);

    // Column-major LH Perspective mapping z_view in [near, far] -> GL NDC [-1, 1]
    out16[0]  = f / m_aspect;
    out16[5]  = f;
    out16[10] = (m_farZ + m_nearZ) * invRange;
    out16[11] = 1.0f;
    out16[14] = -2.0f * m_farZ * m_nearZ * invRange;
}

void MobileCamera::ComputeViewProjectionMatrix(float out16[16]) const {
    float v[16]{};
    float p[16]{};
    ComputeViewMatrix(v);
    ComputeProjectionMatrix(p);
    // out = P * V (column-major)
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            out16[col * 4 + row] =
                p[0 * 4 + row] * v[col * 4 + 0] +
                p[1 * 4 + row] * v[col * 4 + 1] +
                p[2 * 4 + row] * v[col * 4 + 2] +
                p[3 * 4 + row] * v[col * 4 + 3];
        }
    }
}

void MobileCamera::ExtractFrustumPlanes(std::array<FrustumPlane, 6>& outPlanes) const {
    float m[16]{};
    ComputeViewProjectionMatrix(m);
    auto row = [&](int r, int c) -> float { return m[c * 4 + r]; };
    auto setPlane = [&](int idx, float a, float b, float c, float d) {
        const float len = std::sqrt(a * a + b * b + c * c);
        if (len > 1e-6f) {
            const float inv = 1.0f / len;
            outPlanes[idx] = {a * inv, b * inv, c * inv, d * inv};
        } else {
            outPlanes[idx] = {0.0f, 0.0f, 1.0f, 1e6f};
        }
    };
    setPlane(0, row(3, 0) + row(0, 0), row(3, 1) + row(0, 1), row(3, 2) + row(0, 2), row(3, 3) + row(0, 3)); // Left
    setPlane(1, row(3, 0) - row(0, 0), row(3, 1) - row(0, 1), row(3, 2) - row(0, 2), row(3, 3) - row(0, 3)); // Right
    setPlane(2, row(3, 0) + row(1, 0), row(3, 1) + row(1, 1), row(3, 2) + row(1, 2), row(3, 3) + row(1, 3)); // Bottom
    setPlane(3, row(3, 0) - row(1, 0), row(3, 1) - row(1, 1), row(3, 2) - row(1, 2), row(3, 3) - row(1, 3)); // Top
    setPlane(4, row(3, 0) + row(2, 0), row(3, 1) + row(2, 1), row(3, 2) + row(2, 2), row(3, 3) + row(2, 3)); // Near
    setPlane(5, row(3, 0) - row(2, 0), row(3, 1) - row(2, 1), row(3, 2) - row(2, 2), row(3, 3) - row(2, 3)); // Far
}

} // namespace RANMobile
