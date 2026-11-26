#pragma once

#include "GraphicsSettings.h"
#include "RenderLayout.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace RANMobile {

struct RenderStats {
    uint32_t drawCalls = 0;
    uint32_t triangles = 0;
    uint32_t culledObjects = 0;
    uint32_t instancedBatches = 0;
};

struct GlyphInfo {
    float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
    float w = 0.0f, h = 0.0f;
    float xoff = 0.0f, yoff = 0.0f;
    float advance = 0.0f;
    bool valid = false;
};

// UI-only GLES3 renderer. Draw calls use virtual design coordinates; the
// projection and touch transform are derived from one shared RenderLayout.
class MobileRenderer {
public:
    bool Initialize(int width, int height);

    // Android surface lifecycle. Context-specific GPU objects are recreated on
    // every surface creation and never used after the surface is lost.
    void OnSurfaceCreated(int width, int height);
    void OnSurfaceLost();
    void Resize(int width, int height);
    void Shutdown();

    bool BeginFrame();
    void EndFrame();
    void DrawUIQuad(float x, float y, float width, float height, float r, float g, float b, float a);
    void DrawUIText(float x, float y, const std::string& text, float scale, float r, float g, float b, float a);
    float MeasureTextWidth(const std::string& text, float scale);

    uint32_t UploadRGBA8Texture(uint32_t width, uint32_t height, const uint8_t* rgba8);
    void DeleteTexture(uint32_t texId);

    static uint32_t DecodeUTF8(const std::string& text, size_t& index);

    void ApplySettings(const GraphicsSettings& settings);
    const GraphicsSettings& GetSettings() const { return m_settings; }
    const RenderStats& GetStats() const { return m_stats; }

    int GetWidth() const { return m_layout.GetPhysicalWidth(); }
    int GetHeight() const { return m_layout.GetPhysicalHeight(); }
    float GetDesignWidth() const { return m_layout.GetDesignWidth(); }
    float GetDesignHeight() const { return m_layout.GetDesignHeight(); }
    const RenderLayout& GetLayout() const { return m_layout; }
    bool IsInitialized() const { return m_initialized; }
    bool HasGPUContext() const;

    bool IsBoxInFrustum(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) const;

private:
    const GlyphInfo* GetOrLoadGlyph(uint32_t codepoint);
    bool LoadFontFile(const std::string& path, std::vector<uint8_t>& outBuffer);
    void UpdateProjection();
    void ResetGlyphAtlas();
    bool CreateGPUResources();
    void DestroyGPUResources(bool deleteObjects);

    bool m_initialized = false;
    bool m_gpuReady = false;
    RenderLayout m_layout;
    float m_projection[16] = {0.0f};
    GraphicsSettings m_settings;
    RenderStats m_stats;

    std::vector<uint8_t> m_primaryFontData;
    std::vector<uint8_t> m_thaiFontData;
    std::vector<uint8_t> m_glyphScratch;
    bool m_hasPrimaryFont = false;
    bool m_hasThaiFont = false;
    std::unordered_map<uint32_t, GlyphInfo> m_glyphCache;
    std::vector<uint32_t> m_ownedTextures;
    int m_atlasX = 1;
    int m_atlasY = 1;
    int m_atlasRowH = 0;

#ifdef __ANDROID__
    uint32_t m_uiProgram = 0;
    uint32_t m_uiVAO = 0;
    uint32_t m_uiVBO = 0;
    uint32_t m_fontAtlasTex = 0;
    int32_t m_uProjLoc = -1;
    int32_t m_uColorLoc = -1;
    int32_t m_uUseTexLoc = -1;
#endif
};

} // namespace RANMobile
