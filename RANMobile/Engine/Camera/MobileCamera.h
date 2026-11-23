#pragma once

namespace RANMobile {

enum class CameraMode {
    ThirdPerson,
    Free,
    Target,
    Combat
};

class MobileCamera {
public:
    void SetMode(CameraMode mode) { m_mode = mode; }
    CameraMode GetMode() const { return m_mode; }

    void OnDrag(float deltaX, float deltaY);
    void OnPinch(float scaleDelta);

    void SetTarget(float x, float y, float z);
    void SetSensitivity(float sensitivity) { m_sensitivity = sensitivity; }
    void SetSmoothing(float smoothing) { m_smoothing = smoothing; }
    void SetDistanceBounds(float minDist, float maxDist);

    void Update(float deltaTime);

    float GetYaw() const { return m_yaw; }
    float GetPitch() const { return m_pitch; }
    float GetDistance() const { return m_distance; }

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
};

} // namespace RANMobile
