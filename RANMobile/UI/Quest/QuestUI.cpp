#include "QuestUI.h"

namespace RANMobile {

void QuestUI::Initialize(QuestManager* questMgr) {
    m_questManager = questMgr;
}

void QuestUI::SelectQuest(uint32_t questID) {
    m_selectedQuestID = static_cast<int64_t>(questID);
}

void QuestUI::ToggleTrackQuest(uint32_t questID) {
    if (m_trackedQuestID == static_cast<int64_t>(questID)) {
        m_trackedQuestID = -1;
    } else {
        m_trackedQuestID = static_cast<int64_t>(questID);
    }
}

size_t QuestUI::GetVisibleQuestCount() const {
    return m_questManager ? m_questManager->GetActiveQuests().size() : 0;
}

} // namespace RANMobile
