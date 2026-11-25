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

EP7NetReqGateOut MapManager::BuildGateOutRequest(uint32_t gateId, uint32_t gaeaId) const {
    EP7NetReqGateOut req;
    req.dwGateID = gateId;
    req.dwGaeaID = gaeaId;
    return req;
}

bool MapManager::ApplyGateOutFeedback(const EP7NetReqGateOutFB& fb) {
    if (fb.emFB != 0) return false; // EMCHAR_GATEOUT_OK == 0
    m_mainID = fb.sMapID.wMainID;
    m_subID = fb.sMapID.wSubID;
    m_playerX = fb.vPos.x;
    m_playerZ = fb.vPos.z;
    return true;
}

EP7WldHeader MapManager::LoadWldHeader(const std::vector<uint8_t>& wldBytes) const {
    return EP7WorldCharParser::ParseWldHeader(wldBytes);
}

} // namespace RANMobile
