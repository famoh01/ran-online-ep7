#pragma once

#include "../Buttons/MobileSkillButton.h"
#include <array>

namespace RANMobile {

class SkillUI {
public:
    void BindSkillSlot(size_t slotIndex, int skillID) {
        if (slotIndex < m_slots.size()) {
            m_slots[slotIndex].SetSkillID(skillID);
        }
    }

    MobileSkillButton* GetSkillSlot(size_t slotIndex) {
        if (slotIndex >= m_slots.size()) return nullptr;
        return &m_slots[slotIndex];
    }

private:
    std::array<MobileSkillButton, 9> m_slots;
};

} // namespace RANMobile
