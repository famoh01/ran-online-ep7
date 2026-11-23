#pragma once

#include "../../Client/Quest/QuestManager.h"
#include <cstdint>
#include <string>

namespace RANMobile {

class QuestUI {
public:
    void Initialize(QuestManager* questMgr);
    void SelectQuest(uint32_t questID);
    void ToggleTrackQuest(uint32_t questID);

    int64_t GetSelectedQuestID() const { return m_selectedQuestID; }
    int64_t GetTrackedQuestID() const { return m_trackedQuestID; }
    size_t GetVisibleQuestCount() const;

private:
    QuestManager* m_questManager = nullptr;
    int64_t m_selectedQuestID = -1;
    int64_t m_trackedQuestID = -1;
};

} // namespace RANMobile
