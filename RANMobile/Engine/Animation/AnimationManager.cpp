#include "AnimationManager.h"

namespace RANMobile {

void AnimationManager::PlayAnimation(uint32_t entityId, const std::string& animName, bool loop) {
    (void)entityId;
    (void)loop;
    m_currentAnim = animName;
    m_animTime = 0.0f;
}

void AnimationManager::Update(float deltaTime) {
    m_animTime += deltaTime;
}

} // namespace RANMobile
