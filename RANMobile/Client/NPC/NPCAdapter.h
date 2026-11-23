#pragma once

#include <cstdint>
#include <string>

namespace RANMobile {

// Client-side display/interaction adapter for existing EP7 NPCs (NO NPCs fabricated)
struct NPCClientState {
    uint32_t globId = 0;
    uint16_t mainID = 0;
    uint16_t subID = 0;
    std::string name;
    float posX = 0.0f;
    float posY = 0.0f;
    float posZ = 0.0f;
};

class NPCAdapter {
public:
    void UpdateFromServer(const NPCClientState& state) { m_state = state; }
    const NPCClientState& GetState() const { return m_state; }

private:
    NPCClientState m_state;
};

} // namespace RANMobile
