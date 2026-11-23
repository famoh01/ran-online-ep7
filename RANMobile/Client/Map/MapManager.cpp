#include "MapManager.h"

namespace RANMobile {

void MapManager::SetCurrentMap(uint16_t mainID, uint16_t subID, const std::string& mapName) {
    m_mainID = mainID;
    m_subID = subID;
    m_mapName = mapName;
}

void MapManager::SetPlayerPosition(float x, float z, float yawDeg) {
    m_playerX = x;
    m_playerZ = z;
    m_playerYaw = yawDeg;
}

void MapManager::UpdateMarkersFromServer(const std::vector<MapMarker>& markers) {
    m_markers = markers;
}

} // namespace RANMobile
