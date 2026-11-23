#include "MobileButton.h"
#include <algorithm>

void MobileButton::SetPosition(float x, float y) { m_x = x; m_y = y; }
void MobileButton::SetSize(float width, float height) { m_width = width; m_height = height; }
void MobileButton::SetOpacity(float opacity) { m_opacity = std::clamp(opacity, 0.0f, 1.0f); }

void MobileButton::SetCooldown(float seconds) {
    m_cooldownRemaining = std::max(0.0f, seconds);
}

bool MobileButton::HitTest(float x, float y) const {
    return x >= m_x && x <= (m_x + m_width) && y >= m_y && y <= (m_y + m_height);
}

bool MobileButton::OnTouchDown(int32_t pointerId, float x, float y) {
    m_clicked = false;
    if (!IsEnabled()) return false;
    if (HitTest(x, y)) {
        m_pressed = true;
        m_activePointerId = pointerId;
        return true;
    }
    return false;
}

void MobileButton::OnTouchMove(int32_t pointerId, float x, float y) {
    if (!m_pressed || pointerId != m_activePointerId) return;
    if (!HitTest(x, y)) {
        m_pressed = false;
    }
}

void MobileButton::OnTouchUp(int32_t pointerId, float x, float y) {
    if (pointerId != m_activePointerId) return;
    if (m_pressed && HitTest(x, y) && IsEnabled()) {
        m_clicked = true;
        if (m_onClick) m_onClick();
    }
    m_pressed = false;
    m_activePointerId = -1;
}

void MobileButton::Update(float deltaTime) {
    m_clicked = false;
    if (m_cooldownRemaining > 0.0f) {
        m_cooldownRemaining = std::max(0.0f, m_cooldownRemaining - deltaTime);
    }
}

void MobileButton::Draw() {
    // Rendered by MobileRenderer UI pass
}
