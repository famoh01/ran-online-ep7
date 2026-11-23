#include "MobileCamera.h"
#include <algorithm>

namespace RANMobile {

void MobileCamera::OnDrag(float deltaX, float deltaY) {
    m_targetYaw += deltaX * m_sensitivity;
    m_targetPitch = std::clamp(m_targetPitch - deltaY * m_sensitivity, -15.0f, 75.0f);
}

void MobileCamera::OnPinch(float scaleDelta) {
    m_distance = std::clamp(m_distance - scaleDelta * 2.0f, m_minDistance, m_maxDistance);
}

void MobileCamera::SetTarget(float x, float y, float z) {
    m_targetX = x;
    m_targetY = y;
    m_targetZ = z;
}

void MobileCamera::SetDistanceBounds(float minDist, float maxDist) {
    m_minDistance = minDist;
    m_maxDistance = maxDist;
    m_distance = std::clamp(m_distance, m_minDistance, m_maxDistance);
}

void MobileCamera::Update(float deltaTime) {
    float t = std::min(1.0f, m_smoothing * deltaTime);
    m_yaw += (m_targetYaw - m_yaw) * t;
    m_pitch += (m_targetPitch - m_pitch) * t;
}

} // namespace RANMobile
