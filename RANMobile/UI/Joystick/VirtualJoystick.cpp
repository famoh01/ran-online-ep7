#include "VirtualJoystick.h"
#include <cmath>
#include <algorithm>

void VirtualJoystick::SetCenter(float x, float y) {
    m_centerX = x;
    m_centerY = y;
}

void VirtualJoystick::SetRadius(float radius) {
    m_radius = std::max(10.0f, radius);
}

void VirtualJoystick::SetDeadzone(float deadzone) {
    m_deadzone = std::clamp(deadzone, 0.0f, 0.9f);
}

bool VirtualJoystick::TouchDown(int32_t pointerId, float x, float y) {
    float dx = x - m_centerX;
    float dy = y - m_centerY;
    float dist = std::sqrt(dx * dx + dy * dy);
    if (dist <= m_radius * 1.5f) {
        m_active = true;
        m_activePointerId = pointerId;
        TouchMove(pointerId, x, y);
        return true;
    }
    return false;
}

void VirtualJoystick::TouchMove(int32_t pointerId, float x, float y) {
    if (!m_active || pointerId != m_activePointerId) return;

    float dx = x - m_centerX;
    float dy = y - m_centerY;
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist < 1e-4f) {
        m_axisX = 0.0f;
        m_axisY = 0.0f;
        m_magnitude = 0.0f;
        return;
    }

    float norm = std::min(dist / m_radius, 1.0f);
    if (norm < m_deadzone) {
        m_axisX = 0.0f;
        m_axisY = 0.0f;
        m_magnitude = 0.0f;
        return;
    }

    m_axisX = (dx / dist) * norm;
    m_axisY = (dy / dist) * norm;
    m_magnitude = norm;
}

void VirtualJoystick::TouchUp(int32_t pointerId) {
    if (pointerId != m_activePointerId) return;
    m_active = false;
    m_activePointerId = -1;
    m_axisX = 0.0f;
    m_axisY = 0.0f;
    m_magnitude = 0.0f;
}
