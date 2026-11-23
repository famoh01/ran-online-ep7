#include "InventoryManager.h"

namespace RANMobile {

void InventoryManager::Clear() {
    m_slots.clear();
}

void InventoryManager::SetSlotFromServer(const InventorySlot& slot) {
    m_slots.push_back(slot);
}

const InventorySlot* InventoryManager::FindItem(uint16_t mainID, uint16_t subID) const {
    for (const auto& s : m_slots) {
        if (s.mainID == mainID && s.subID == subID && s.count > 0) {
            return &s;
        }
    }
    return nullptr;
}

} // namespace RANMobile
