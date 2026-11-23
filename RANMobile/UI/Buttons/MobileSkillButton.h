#pragma once

#include "MobileButton.h"

class MobileSkillButton {
public:
    void SetSkillID(int skillID);
    int GetSkillID() const { return m_skillID; }

    void SetCooldown(float seconds);
    void Update(float deltaTime);

    bool CanUse() const;
    void OnPressed();

    MobileButton& GetButton() { return m_button; }

private:
    int m_skillID = -1;
    float m_cooldown = 0.0f;
    float m_maxCooldown = 0.0f;
    MobileButton m_button{MobileButtonType::Skill1};
};
