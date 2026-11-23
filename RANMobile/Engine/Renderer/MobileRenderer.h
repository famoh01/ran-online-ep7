#pragma once

#include "GraphicsSettings.h"
#include <cstdint>
#include <string>

namespace RANMobile {

struct RenderStats {
    uint32_t drawCalls = 0;
    uint32_t triangles = 0;
    uint32_t culledObjects = 0;
    uint32_t instancedBatches = 0;
};

class MobileRenderer {
public:
    bool Initialize(int width, int height);
    void Resize(int width, int height);
    void Shutdown();

    void BeginFrame();
    void EndFrame();

    void ApplySettings(const GraphicsSettings& settings);
    const GraphicsSettings& GetSettings() const { return m_settings; }
    const RenderStats& GetStats() const { return m_stats; }

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    // Frustum culling helper (AABB)
    bool IsBoxInFrustum(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) const;

private:
    bool m_initialized = false;
    int m_width = 1280;
    int m_height = 720;
    GraphicsSettings m_settings;
    RenderStats m_stats;
};

} // namespace RANMobile
