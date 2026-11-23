#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace RANMobile {

struct EP7CharacterSlot {
    uint32_t charId = 0;
    std::string name;
    uint16_t level = 1;
    uint16_t charClass = 0; // Original EP7 EMCHARCLASS
};

class CharacterSelectManager {
public:
    void ClearSlots();
    void AddSlotFromServer(const EP7CharacterSlot& slot);
    const std::vector<EP7CharacterSlot>& GetSlots() const { return m_slots; }

    bool SelectSlot(size_t index);
    int GetSelectedIndex() const { return m_selectedIndex; }
    const EP7CharacterSlot* GetSelectedCharacter() const;

private:
    std::vector<EP7CharacterSlot> m_slots;
    int m_selectedIndex = -1;
};

} // namespace RANMobile
