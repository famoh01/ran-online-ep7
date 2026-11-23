#pragma once

#include <string>
#include <cstdint>

namespace RANMobile {

class AnimationManager {
public:
    void PlayAnimation(uint32_t entityId, const std::string& animName, bool loop = true);
    void Update(float deltaTime);
    const std::string& GetCurrentAnimation() const { return m_currentAnim; }

private:
    std::string m_currentAnim;
    float m_animTime = 0.0f;
};

} // namespace RANMobile
