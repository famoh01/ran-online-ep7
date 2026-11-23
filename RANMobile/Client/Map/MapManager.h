#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace RANMobile {

struct MapMarker {
    uint32_t entityId = 0;
    std::string label;
    float x = 0.0f;
    float z = 0.0f;
    bool isNPC = false;
};

class MapManager {
public:
    void SetCurrentMap(uint16_t mainID, uint16_t subID, const std::string& mapName);
    void SetPlayerPosition(float x, float z, float yawDeg);
    void UpdateMarkersFromServer(const std::vector<MapMarker>& markers);

    uint16_t GetMainID() const { return m_mainID; }
    uint16_t GetSubID() const { return m_subID; }
    const std::string& GetMapName() const { return m_mapName; }
    float GetPlayerX() const { return m_playerX; }
    float GetPlayerZ() const { return m_playerZ; }
    float GetPlayerYaw() const { return m_playerYaw; }
    const std::vector<MapMarker>& GetMarkers() const { return m_markers; }

private:
    uint16_t m_mainID = 0;
    uint16_t m_subID = 0;
    std::string m_mapName;
    float m_playerX = 0.0f;
    float m_playerZ = 0.0f;
    float m_playerYaw = 0.0f;
    std::vector<MapMarker> m_markers;
};

} // namespace RANMobile
