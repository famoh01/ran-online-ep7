#include "CharacterSelectManager.h"

namespace RANMobile {

void CharacterSelectManager::ClearSlots() {
    m_slots.clear();
    m_selectedIndex = -1;
}

void CharacterSelectManager::AddSlotFromServer(const EP7CharacterSlot& slot) {
    m_slots.push_back(slot);
}

void CharacterSelectManager::ParseChaBbaInfo(const EP7NetChaBbaInfo& pkt) {
    ClearSlots();
    int32_t count = pkt.nChaSNum;
    if (count < 0) count = 0;
    if (count > static_cast<int32_t>(EP7_MAX_ONESERVERCHAR_NUM)) {
        count = static_cast<int32_t>(EP7_MAX_ONESERVERCHAR_NUM);
    }
    for (int32_t i = 0; i < count; ++i) {
        if (pkt.nChaNum[i] > 0) {
            EP7CharacterSlot slot;
            slot.charId = static_cast<uint32_t>(pkt.nChaNum[i]);
            m_slots.push_back(slot);
        }
    }
}

bool CharacterSelectManager::SelectSlot(size_t index) {
    if (index >= m_slots.size()) return false;
    m_selectedIndex = static_cast<int>(index);
    return true;
}

const EP7CharacterSlot* CharacterSelectManager::GetSelectedCharacter() const {
    if (m_selectedIndex < 0 || static_cast<size_t>(m_selectedIndex) >= m_slots.size()) {
        return nullptr;
    }
    return &m_slots[m_selectedIndex];
}

} // namespace RANMobile
