#include "CharacterSelectManager.h"

namespace RANMobile {

void CharacterSelectManager::ClearSlots() {
    m_slots.clear();
    m_selectedIndex = -1;
}

void CharacterSelectManager::AddSlotFromServer(const EP7CharacterSlot& slot) {
    m_slots.push_back(slot);
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
