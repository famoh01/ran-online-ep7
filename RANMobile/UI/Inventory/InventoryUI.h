#pragma once

#include "../../Client/Inventory/InventoryManager.h"

namespace RANMobile {

class InventoryUI {
public:
    void Initialize(InventoryManager* invMgr) { m_inventory = invMgr; }
    void SelectSlot(int slotIndex) { m_selectedSlot = slotIndex; }
    int GetSelectedSlot() const { return m_selectedSlot; }
    size_t GetItemCount() const { return m_inventory ? m_inventory->GetItemCount() : 0; }

private:
    InventoryManager* m_inventory = nullptr;
    int m_selectedSlot = -1;
};

} // namespace RANMobile
