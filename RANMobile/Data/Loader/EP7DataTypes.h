#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace RANMobile {

// Mirrors EP7 GLMapList / SMAPNODE
struct EP7MapData {
    uint16_t mainID = 0;
    uint16_t subID = 0;
    std::string mapName;
    std::string fileName;
    bool loaded = false;
};

// Mirrors EP7 GLCrowData (Monsters & NPCs)
enum class EP7CrowType : uint8_t {
    Monster = 0,
    NPC = 1,
    Material = 2
};

struct EP7CrowData {
    uint16_t mainID = 0;
    uint16_t subID = 0;
    EP7CrowType type = EP7CrowType::Monster;
    std::string name;
    uint16_t level = 1;
    uint32_t hp = 0;
    std::string skinFile;
    bool loaded = false;
};

// Mirrors EP7 GLItem / SITEM
struct EP7ItemData {
    uint16_t mainID = 0;
    uint16_t subID = 0;
    std::string name;
    uint32_t itemType = 0;
    uint32_t price = 0;
    std::string iconFile;
    bool loaded = false;
};

// Mirrors EP7 GLSkill / GLSKILL
struct EP7SkillData {
    uint16_t mainID = 0;
    uint16_t subID = 0;
    std::string name;
    float cooldown = 0.0f;
    uint16_t spCost = 0;
    uint16_t mpCost = 0;
    float range = 0.0f;
    bool loaded = false;
};

// Mirrors EP7 GLQuest / GLQUEST
struct EP7QuestData {
    uint32_t questID = 0;
    std::string title;
    std::string description;
    uint32_t npcID = 0;
    bool loaded = false;
};

} // namespace RANMobile
