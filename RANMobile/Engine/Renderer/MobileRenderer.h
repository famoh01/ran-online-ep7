#pragma once

#include "GraphicsSettings.h"
#include "RenderLayout.h"
#include "../Camera/MobileCamera.h"
#include "../../Data/Importer/EP7WorldParser.h"
#include <array>
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

struct CachedAssetTexture {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> rgba8; // retained for re-upload after Android surface recreation
    uint32_t gpuId = 0;
    bool repeatWrap = false;
    bool generateMips = false;
};

struct CachedMesh3D {
    uint32_t handle = 0;
    std::vector<EP7WorldVertex> vertices;
    std::vector<uint16_t> indices;
    uint32_t vao = 0;
    uint32_t vbo = 0;
    uint32_t ibo = 0;
};

struct WorldPointLightGpu {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float range = 100.0f;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

struct WorldLightingState {
    EP7Color4 ambientColor{0.42f, 0.42f, 0.45f, 1.0f};
    EP7Vec3 dirLightDir{-0.35f, -0.75f, 0.55f};
    EP7Color4 dirLightColor{0.85f, 0.85f, 0.80f, 1.0f};
    std::array<WorldPointLightGpu, 4> pointLights{};
    uint32_t pointLightCount = 0;
    EP7FogInfo fog;
};

struct GlyphInfo {
    float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
    float w = 0.0f, h = 0.0f;
    float xoff = 0.0f, yoff = 0.0f;
    float advance = 0.0f;
    bool valid = false;
};

// GLES3 2D UI + 3D EP7 World renderer. 2D UI draw calls use virtual design coordinates;
// 3D world passes use LH D3D9 coordinates and survive EGL surface recreation.
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
    void SetClearColor(float r, float g, float b, float a);
    void DrawUIQuad(float x, float y, float width, float height, float r, float g, float b, float a);
    void DrawUIText(float x, float y, const std::string& text, float scale, float r, float g, float b, float a);
    float MeasureTextWidth(const std::string& text, float scale);

    uint32_t UploadRGBA8Texture(uint32_t width, uint32_t height, const uint8_t* rgba8,
                                bool repeatWrap = false, bool generateMips = false);
    void DeleteTexture(uint32_t texId);

    // Asset-backed texture cache: one CPU copy per key, re-uploaded on surface recreation.
    bool LoadAssetTexture(const std::string& key, uint32_t width, uint32_t height,
                          const std::vector<uint8_t>& rgba8,
                          bool repeatWrap = false, bool generateMips = false);
    bool LoadWorldTexture(const std::string& key, uint32_t width, uint32_t height,
                          const std::vector<uint8_t>& rgba8);
    uint32_t GetAssetTexture(const std::string& key) const;
    bool HasAssetTexture(const std::string& key) const;
    void UnloadAssetTexture(const std::string& key);
    size_t GetAssetTextureCount() const { return m_assetTextures.size(); }
    void DrawTexturedQuad(float x, float y, float width, float height, uint32_t texId,
                          float r, float g, float b, float a, float alphaCutoff = 0.0f);

    // 3D mesh registry (retains CPU vertices/indices for EGL surface recreation).
    uint32_t RegisterMesh3D(const std::vector<EP7WorldVertex>& vertices,
                            const std::vector<uint16_t>& indices);
    void UnregisterMesh3D(uint32_t meshHandle);
    void ClearMeshes3D();
    size_t GetMesh3DCount() const { return m_meshes3D.size(); }

    // 3D world render pass API
    void SetCameraMatrices(const MobileCamera& camera);
    void ClearCamera3D() { m_hasCamera3D = false; }
    bool BeginWorldPass(const MobileCamera& camera, const WorldLightingState& lighting);
    void DrawMesh3D(uint32_t meshHandle,
                    uint32_t texId,
                    const EP7Mat4& modelMatrix,
                    const EP7AABB& worldBounds,
                    EP7AlphaBucket bucket = EP7AlphaBucket::Opaque,
                    bool twoSided = false,
                    float alphaCutoff = 0.0f,
                    const EP7Color4& tint = {1.0f, 1.0f, 1.0f, 1.0f},
                    bool depthWrite = true,
                    bool unlit = false,
                    bool skipCulling = false);
    void EndWorldPass();

    static uint32_t DecodeUTF8(const std::string& text, size_t& index);

    void SetSafeAreaInsets(float left, float top, float right, float bottom) {
        m_layout.SetSafeAreaInsets(left, top, right, bottom);
    }

    void ApplySettings(const GraphicsSettings& settings);
    const GraphicsSettings& GetSettings() const { return m_settings; }
    const RenderStats& GetStats() const { return m_stats; }
    void RecordCulledObjects(uint32_t count = 1) { m_stats.culledObjects += count; }
    void SetWorldViewScale(float scale) { if (scale > 0.1f) m_worldViewScale = scale; }
    float GetWorldViewScale() const { return m_worldViewScale; }
    float GetEffectiveWorldViewDistance() const { return m_settings.viewDistance * m_worldViewScale; }

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
    bool UploadMesh3DGPU(CachedMesh3D& mesh);

    bool m_initialized = false;
    bool m_gpuReady = false;
    RenderLayout m_layout;
    float m_projection[16] = {0.0f};
    GraphicsSettings m_settings;
    RenderStats m_stats;

    bool m_hasCamera3D = false;
    bool m_worldPassActive = false;
    float m_worldViewScale = 8.0f;
    float m_cameraPos[3] = {0.0f, 0.0f, 0.0f};
    float m_viewMatrix[16] = {0.0f};
    float m_projMatrix[16] = {0.0f};
    float m_viewProjMatrix[16] = {0.0f};
    std::array<FrustumPlane, 6> m_frustumPlanes{};
    float m_clearColor[4] = {0.03f, 0.06f, 0.13f, 1.0f};

    std::vector<uint8_t> m_primaryFontData;
    std::vector<uint8_t> m_thaiFontData;
    std::vector<uint8_t> m_glyphScratch;
    bool m_hasPrimaryFont = false;
    bool m_hasThaiFont = false;
    std::unordered_map<uint32_t, GlyphInfo> m_glyphCache;
    std::vector<uint32_t> m_ownedTextures;
    std::unordered_map<std::string, CachedAssetTexture> m_assetTextures;
    std::unordered_map<uint32_t, CachedMesh3D> m_meshes3D;
    uint32_t m_nextMeshHandle = 1;
    uint32_t m_lastBoundWorldTex = 0xFFFFFFFFu;
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
    int32_t m_uAlphaCutoffLoc = -1;

    uint32_t m_worldProgram = 0;
    int32_t m_wViewProjLoc = -1;
    int32_t m_wModelLoc = -1;
    int32_t m_wCameraPosLoc = -1;
    int32_t m_wTexLoc = -1;
    int32_t m_wHasTexLoc = -1;
    int32_t m_wAlphaCutoffLoc = -1;
    int32_t m_wTintLoc = -1;
    int32_t m_wUnlitLoc = -1;
    int32_t m_wAmbientLoc = -1;
    int32_t m_wDirDirLoc = -1;
    int32_t m_wDirColorLoc = -1;
    int32_t m_wPointCountLoc = -1;
    int32_t m_wPointPosInvRangeLoc = -1;
    int32_t m_wPointColorLoc = -1;
    int32_t m_wFogEnabledLoc = -1;
    int32_t m_wFogStartLoc = -1;
    int32_t m_wFogEndLoc = -1;
    int32_t m_wFogColorLoc = -1;
#endif
};

} // namespace RANMobile
