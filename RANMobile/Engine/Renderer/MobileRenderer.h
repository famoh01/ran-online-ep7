#pragma once

#include "GraphicsSettings.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace RANMobile {

class MobileCamera;

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

// B17: one skinned-mesh draw submission for the 3D character pass.  All arrays are
// in world space (the CPU pose path already applied the entity transform), so the
// renderer only multiplies by the camera view-projection.
struct SkinnedDrawInput {
    const float* positions = nullptr;   // xyz per vertex (world space)
    const float* normals = nullptr;     // xyz per vertex (world space)
    const float* texcoords = nullptr;   // uv per vertex (may be null -> untextured)
    uint32_t vertexCount = 0;
    const uint32_t* indices = nullptr;  // triangle list
    uint32_t indexCount = 0;            // multiple of 3
    uint32_t textureId = 0;             // 0 -> flat material colour
    float colorR = 0.75f, colorG = 0.75f, colorB = 0.75f, colorA = 1.0f;
};

class MobileRenderer {
public:
    bool Initialize(int width, int height);
    void Resize(int width, int height);
    void Shutdown();

    void BeginFrame();
    void EndFrame();
    void DrawUIQuad(float x, float y, float width, float height, float r, float g, float b, float a);
    void DrawUIText(float x, float y, const std::string& text, float scale, float r, float g, float b, float a);
    float MeasureTextWidth(const std::string& text, float scale);

    // ------------------------------------------------------------------
    // B17: 3D character pass.  BeginScene3D computes the camera view-projection
    // from the existing MobileCamera orbit state (yaw/pitch/distance/target) and
    // switches OpenGL to depth-tested 3D rendering; EndScene3D restores the UI
    // state.  DrawSkinnedMesh uploads the CPU-skinned vertices into a dynamic
    // buffer and submits one draw call per material subset — the mobile
    // equivalent of the DxSkinMesh9_CPU dynamic-VB path, fed by EP7CharacterScene's
    // pose evaluation instead of D3DX palette shader skinning.
    // ------------------------------------------------------------------
    bool BeginScene3D(const MobileCamera& camera);
    void EndScene3D();
    bool DrawSkinnedMesh(const SkinnedDrawInput& input);
    bool Scene3DActive() const { return m_scene3DActive; }

    uint32_t UploadRGBA8Texture(uint32_t width, uint32_t height, const uint8_t* rgba8);
    void DeleteTexture(uint32_t texId);

    static uint32_t DecodeUTF8(const std::string& text, size_t& index);

    void ApplySettings(const GraphicsSettings& settings);
    const GraphicsSettings& GetSettings() const { return m_settings; }
    const RenderStats& GetStats() const { return m_stats; }

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    bool IsInitialized() const { return m_initialized; }

    bool IsBoxInFrustum(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) const;

private:
    const GlyphInfo* GetOrLoadGlyph(uint32_t codepoint);
    bool LoadFontFile(const std::string& path, std::vector<uint8_t>& outBuffer);

    bool m_initialized = false;
    int m_width = 1280;
    int m_height = 720;
    GraphicsSettings m_settings;
    RenderStats m_stats;

    bool m_scene3DActive = false;

    std::vector<uint8_t> m_primaryFontData;
    std::vector<uint8_t> m_thaiFontData;
    bool m_hasPrimaryFont = false;
    bool m_hasThaiFont = false;
    std::unordered_map<uint32_t, GlyphInfo> m_glyphCache;
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

    // B17 3D pass state.
    uint32_t m_charProgram = 0;
    uint32_t m_charVAO = 0;
    uint32_t m_charVBO = 0;       // dynamic: position+normal+uv, grown on demand
    uint32_t m_charEBO = 0;       // dynamic: triangle indices, uploaded per draw
    size_t m_charVBOBytes = 0;
    size_t m_charEBOBytes = 0;
    int32_t m_uVPLoc = -1;
    int32_t m_uLightDirLoc = -1;
    int32_t m_uColorLoc3D = -1;
    int32_t m_uUseTexLoc3D = -1;
    int32_t m_uTexLoc3D = -1;
    float m_viewProj[16] = {0};
#endif
};

} // namespace RANMobile
