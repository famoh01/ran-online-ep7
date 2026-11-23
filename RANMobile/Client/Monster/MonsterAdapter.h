#pragma once

#include <cstdint>
#include <string>

namespace RANMobile {

// Client-side display/state adapter for existing EP7 Monsters (NO AI logic fabricated)
struct MonsterClientState {
    uint32_t globId = 0;
    uint16_t mainID = 0;
    uint16_t subID = 0;
    std::string name;
    uint32_t hp = 0;
    uint32_t maxHp = 0;
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;
    uint8_t actionState = 0; // Server-authoritative state
};

class MonsterAdapter {
public:
    void UpdateFromServer(const MonsterClientState& state) { m_state = state; }
    const MonsterClientState& GetState() const { return m_state; }

private:
    MonsterClientState m_state;
};

} // namespace RANMobile
