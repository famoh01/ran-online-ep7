#include "TouchSystem.h"

namespace RANMobile {

void TouchSystem::OnTouchDown(int32_t pointerId, float x, float y) {
    TouchPoint pt;
    pt.pointerId = pointerId;
    pt.x = x;
    pt.y = y;
    pt.startX = x;
    pt.startY = y;
    pt.active = true;
    m_touches[pointerId] = pt;
}

void TouchSystem::OnTouchMove(int32_t pointerId, float x, float y) {
    auto it = m_touches.find(pointerId);
    if (it != m_touches.end()) {
        it->second.x = x;
        it->second.y = y;
    }
}

void TouchSystem::OnTouchUp(int32_t pointerId, float x, float y) {
    (void)x;
    (void)y;
    m_touches.erase(pointerId);
}

const TouchPoint* TouchSystem::GetTouch(int32_t pointerId) const {
    auto it = m_touches.find(pointerId);
    if (it == m_touches.end()) return nullptr;
    return &it->second;
}

void TouchSystem::Clear() {
    m_touches.clear();
}

} // namespace RANMobile
