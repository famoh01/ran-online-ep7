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
    if (m_cachedTexId == 0 && !ddsBytes.empty()) {
        auto tex = EP7TextureDecoder::DecodeDDS(ddsBytes);
        if (!tex.valid) return false;
        m_cachedTexId = renderer->UploadRGBA8Texture(tex.width, tex.height, tex.rgba8.data());
    }
    if (m_cachedTexId == 0) return false;
    auto geom = EP7WorldCharParser::ExtractWldOctreeMesh(wldBytes);
    if (geom.valid) {
        renderer->DrawMesh3D(geom.vertices.data(), geom.vertices.size() / 3,
                             geom.indices.data(), geom.indices.size(),
                             0.25f, 0.45f, 0.30f);
    }
    renderer->DrawTexturedQuad(m_cachedTexId, 0.0f, 0.0f, 128.0f, 128.0f, 1.0f);
    return hdr.valid;
}

void MapManager::ReleaseCachedTextures(MobileRenderer* renderer) {
    if (renderer && m_cachedTexId != 0) {
        renderer->DeleteTexture(m_cachedTexId);
        m_cachedTexId = 0;
    }
}

} // namespace RANMobile
