#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace RANMobile {

struct InventorySlot {
    uint16_t mainID = 0;
    uint16_t subID = 0;
    uint16_t count = 0;
    uint8_t posX = 0;
    uint8_t posY = 0;
};

class InventoryManager {
public:
    void Clear();
    void SetSlotFromServer(const InventorySlot& slot);
    const InventorySlot* FindItem(uint16_t mainID, uint16_t subID) const;
    size_t GetItemCount() const { return m_slots.size(); }

private:
    std::vector<InventorySlot> m_slots;
};

} // namespace RANMobile
