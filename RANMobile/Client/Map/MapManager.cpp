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

void MapManager::ApplyDropCrow(const EP7DropCrow& crow) {
    MapMarker m;
    m.entityId = crow.dwGlobID;
    m.x = crow.vPos.x;
    m.z = crow.vPos.z;
    m.isNPC = (crow.dwFLAGS != 0);
    m_markers.push_back(m);
}

bool MapManager::RenderScene(MobileRenderer* renderer, const std::vector<uint8_t>& wldBytes, const std::vector<uint8_t>& ddsBytes) const {
    if (!renderer || !renderer->IsInitialized()) return false;
    auto hdr = LoadWldHeader(wldBytes);
    auto tex = EP7TextureDecoder::DecodeDDS(ddsBytes);
    if (!tex.valid) return false;
    uint32_t texId = renderer->UploadRGBA8Texture(tex.width, tex.height, tex.rgba8.data());
    if (texId == 0) return false;
    renderer->DrawTexturedQuad(texId, 0.0f, 0.0f, 128.0f, 128.0f, 1.0f);
    renderer->DeleteTexture(texId);
    return hdr.valid;
}

} // namespace RANMobile
