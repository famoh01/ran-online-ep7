#pragma once

#include <cstdint>

class VirtualJoystick {
public:
    void SetCenter(float x, float y);
    void SetRadius(float radius);
    void SetDeadzone(float deadzone);

    bool TouchDown(int32_t pointerId, float x, float y);
    void TouchMove(int32_t pointerId, float x, float y);
    void TouchUp(int32_t pointerId);

    // Convenience single-touch overloads
    void TouchDown(float x, float y) { TouchDown(0, x, y); }
    void TouchMove(float x, float y) { TouchMove(0, x, y); }
    void TouchUp() { TouchUp(m_activePointerId); }

    float GetAxisX() const { return m_axisX; }
    float GetAxisY() const { return m_axisY; }
    float GetMagnitude() const { return m_magnitude; }
    float GetCenterX() const { return m_centerX; }
    float GetCenterY() const { return m_centerY; }
    float GetRadius() const { return m_radius; }
    bool IsRunning() const { return m_magnitude > 0.65f; }
    bool IsActive() const { return m_active; }

private:
    float m_centerX = 180.0f;
    float m_centerY = 540.0f;
    float m_radius = 100.0f;
    float m_deadzone = 0.1f;

    float m_axisX = 0.0f;
    float m_axisY = 0.0f;
    float m_magnitude = 0.0f;
    bool m_active = false;
    int32_t m_activePointerId = -1;
};
