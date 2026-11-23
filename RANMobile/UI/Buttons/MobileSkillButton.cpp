#include "MobileSkillButton.h"
#include <algorithm>

void MobileSkillButton::SetSkillID(int skillID) {
    m_skillID = skillID;
    m_button.SetEnabled(m_skillID >= 0);
}

void MobileSkillButton::SetCooldown(float seconds) {
    m_maxCooldown = std::max(0.0f, seconds);
    m_cooldown = m_maxCooldown;
    m_button.SetCooldown(m_cooldown);
}

void MobileSkillButton::Update(float deltaTime) {
    if (m_cooldown > 0.0f) {
        m_cooldown = std::max(0.0f, m_cooldown - deltaTime);
    }
    m_button.Update(deltaTime);
}

bool MobileSkillButton::CanUse() const {
    return m_skillID >= 0 && m_cooldown <= 0.0f;
}

void MobileSkillButton::OnPressed() {
    if (!CanUse()) return;
    // Triggered by MobileHUD -> CombatController -> EP7 Skill Packet
}
