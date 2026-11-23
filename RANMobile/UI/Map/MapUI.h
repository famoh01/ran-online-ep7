#pragma once

#include "../../Client/Map/MapManager.h"
#include <algorithm>

namespace RANMobile {

class MapUI {
public:
    void Initialize(MapManager* mapMgr) { m_mapManager = mapMgr; }
    void SetFullMapOpen(bool open) { m_fullMapOpen = open; }
    bool IsFullMapOpen() const { return m_fullMapOpen; }
    void SetZoom(float zoom) { m_zoom = std::clamp(zoom, 0.5f, 4.0f); }
    float GetZoom() const { return m_zoom; }

private:
    MapManager* m_mapManager = nullptr;
    bool m_fullMapOpen = false;
    float m_zoom = 1.0f;
};

} // namespace RANMobile
