#include "GameDataLoader.h"
#include "../../Engine/Core/Logger.h"

namespace RANMobile {

bool GameDataLoader::LoadFromDirectory(const std::string& dataDir) {
    m_lastReport = DataValidator::ValidateDirectory(dataDir);
    Logger::Info("GameDataLoader", m_lastReport.ToString());

    // Strictly do NOT fabricate mock data if files are missing.
    return m_lastReport.errors.empty();
}

} // namespace RANMobile
