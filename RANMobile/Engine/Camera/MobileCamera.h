#pragma once

#include <array>

namespace RANMobile {

enum class CameraMode {
    ThirdPerson,
    Free,
    Target,
    Combat
};

struct FrustumPlane {
    float nx = 0.0f;
    float ny = 0.0f;
    float nz = 0.0f;
    float d = 0.0f;
};

class MobileCamera {
public:
    void SetMode(CameraMode mode) { m_mode = mode; }
    CameraMode GetMode() const { return m_mode; }

    void OnDrag(float deltaX, float deltaY);
    void OnPan(float deltaX, float deltaY);
    void OnPinch(float scaleDelta);

    void SetTarget(float x, float y, float z);
    void Reset(float yaw, float pitch, float distance, float tx, float ty, float tz);
    void SetSensitivity(float sensitivity) { m_sensitivity = sensitivity; }
    void SetSmoothing(float smoothing) { m_smoothing = smoothing; }
    void SetDistanceBounds(float minDist, float maxDist);
    void SetProjectionParams(float fovDeg, float aspect, float nearZ, float farZ);
    void SetViewportAspect(float aspect);

    void Update(float deltaTime);

    float GetYaw() const { return m_yaw; }
    float GetPitch() const { return m_pitch; }
    float GetDistance() const { return m_distance; }
    float GetTargetX() const { return m_targetX; }
    float GetTargetY() const { return m_targetY; }
    float GetTargetZ() const { return m_targetZ; }
    float GetFovDeg() const { return m_fovDeg; }
    float GetAspect() const { return m_aspect; }
    float GetNearZ() const { return m_nearZ; }
    float GetFarZ() const { return m_farZ; }

    void GetEyePosition(float& outX, float& outY, float& outZ) const;

    // Column-major 4x4 OpenGL matrices using Left-Handed (EP7 D3D9) world coordinates.
    void ComputeViewMatrix(float out16[16]) const;
    void ComputeProjectionMatrix(float out16[16]) const;
    void ComputeViewProjectionMatrix(float out16[16]) const;

    // Extracts 6 normalized Gribb-Hartmann frustum planes (Left, Right, Bottom, Top, Near, Far).
    void ExtractFrustumPlanes(std::array<FrustumPlane, 6>& outPlanes) const;

private:
    CameraMode m_mode = CameraMode::ThirdPerson;
    float m_yaw = 0.0f;
    float m_pitch = 25.0f;
    float m_targetYaw = 0.0f;
    float m_targetPitch = 25.0f;
    float m_distance = 12.0f;
    float m_minDistance = 4.0f;
    float m_maxDistance = 30.0f;
    float m_sensitivity = 0.25f;
    float m_smoothing = 10.0f;
    float m_targetX = 0.0f;
    float m_targetY = 0.0f;
    float m_targetZ = 0.0f;
    float m_fovDeg = 55.0f;
    float m_aspect = 16.0f / 9.0f;
    float m_nearZ = 0.5f;
    float m_farZ = 1800.0f;
};

} // namespace RANMobile
