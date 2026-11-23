#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace RANMobile {

struct ActiveQuestState {
    uint32_t questID = 0;
    uint32_t step = 0;
    uint32_t progress = 0;
    bool completed = false;
};

class QuestManager {
public:
    void Clear();
    void UpdateQuestFromServer(const ActiveQuestState& state);
    const std::vector<ActiveQuestState>& GetActiveQuests() const { return m_quests; }

private:
    std::vector<ActiveQuestState> m_quests;
};

} // namespace RANMobile
