#include "TouchSystem.h"

namespace RANMobile {

void TouchSystem::OnTouchDown(int32_t pointerId, float x, float y) {
    TouchPoint pt;
    pt.pointerId = pointerId;
    pt.x = x;
    pt.y = y;
    pt.designX = ToDesignX(x);
    pt.designY = ToDesignY(y);
    pt.startX = x;
    pt.startY = y;
    pt.startDesignX = pt.designX;
    pt.startDesignY = pt.designY;
    pt.active = true;
    pt.phase = TouchPhase::Down;
    m_touches[pointerId] = pt;
    m_lastPhases[pointerId] = TouchPhase::Down;
}

void TouchSystem::OnTouchMove(int32_t pointerId, float x, float y) {
    auto it = m_touches.find(pointerId);
    if (it == m_touches.end()) return; // unknown pointer: never synthesize a touch
    TouchPoint& pt = it->second;
    pt.deltaX = x - pt.x;
    pt.deltaY = y - pt.y;
    pt.x = x;
    pt.y = y;
    pt.designX = ToDesignX(x);
    pt.designY = ToDesignY(y);
    pt.phase = TouchPhase::Move;
    m_lastPhases[pointerId] = TouchPhase::Move;
}

void TouchSystem::OnTouchUp(int32_t pointerId, float x, float y) {
    m_lastPhases[pointerId] = TouchPhase::Up;
    m_touches.erase(pointerId);
    (void)x;
    (void)y;
}

void TouchSystem::OnTouchCancel(int32_t pointerId, float x, float y) {
    m_lastPhases[pointerId] = TouchPhase::Cancel;
    m_touches.erase(pointerId);
    (void)x;
    (void)y;
}

const TouchPoint* TouchSystem::GetTouch(int32_t pointerId) const {
    auto it = m_touches.find(pointerId);
    if (it == m_touches.end()) return nullptr;
    return &it->second;
}

std::vector<int32_t> TouchSystem::GetActivePointerIds() const {
    std::vector<int32_t> ids;
    ids.reserve(m_touches.size());
    for (const auto& entry : m_touches) ids.push_back(entry.first);
    return ids;
}

TouchPhase TouchSystem::GetLastPhase(int32_t pointerId) const {
    auto it = m_lastPhases.find(pointerId);
    if (it == m_lastPhases.end()) return TouchPhase::Cancel;
    return it->second;
}

void TouchSystem::Clear() {
    for (const auto& entry : m_touches) m_lastPhases[entry.first] = TouchPhase::Cancel;
    m_touches.clear();
}

} // namespace RANMobile
