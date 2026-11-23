#include "QuestManager.h"

namespace RANMobile {

void QuestManager::Clear() {
    m_quests.clear();
}

void QuestManager::UpdateQuestFromServer(const ActiveQuestState& state) {
    for (auto& q : m_quests) {
        if (q.questID == state.questID) {
            q = state;
            return;
        }
    }
    m_quests.push_back(state);
}

} // namespace RANMobile
