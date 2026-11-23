#pragma once

#include "EP7DataTypes.h"
#include "../Validator/DataValidator.h"
#include <string>
#include <vector>

namespace RANMobile {

class GameDataLoader {
public:
    bool LoadFromDirectory(const std::string& dataDir);

    const DataValidationReport& GetLastReport() const { return m_lastReport; }
    const std::vector<EP7MapData>& GetMaps() const { return m_maps; }
    const std::vector<EP7CrowData>& GetCrows() const { return m_crows; }
    const std::vector<EP7ItemData>& GetItems() const { return m_items; }
    const std::vector<EP7SkillData>& GetSkills() const { return m_skills; }
    const std::vector<EP7QuestData>& GetQuests() const { return m_quests; }

private:
    DataValidationReport m_lastReport;
    std::vector<EP7MapData> m_maps;
    std::vector<EP7CrowData> m_crows;
    std::vector<EP7ItemData> m_items;
    std::vector<EP7SkillData> m_skills;
    std::vector<EP7QuestData> m_quests;
};

} // namespace RANMobile
