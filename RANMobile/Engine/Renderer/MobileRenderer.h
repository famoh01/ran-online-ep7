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

// ---------------------------------------------------------------------------
// B18: GPU skinning path (authentic D3DINDEXEDHLSLVS convention, DxSkinMesh9.cpp:18).
//
// UploadGpuSkinnedMesh stores a static per-piece vertex/index buffer once; the
// per-frame data is ONLY the bone palette (offset*combined matrices, DxSkinMesh9_
// HLSL.cpp:220-227 "mWorldMatrixArray" parity) written into a shared uniform block.
// Vertex streams follow the authentic HLSL declaration (DxSkinMesh9_HLSL.cpp:95-106):
// position + normal + uv + 4 float blend weights + 4 float blend indices, interleaved
// in one 64-byte stride.  The shader reproduces SkinVerticesCPU semantics exactly
// (per-influence row-vector transform with w-divide, translation-dropped normals,
// |sum-1|>1e-4 renormalization, zero-sum -> bind passthrough).
// ---------------------------------------------------------------------------
struct GpuSkinnedMeshData {
    const float* interleaved = nullptr;  // 16 floats per vertex: pos3, normal3, uv2, boneIdx4, boneWt4
    uint32_t vertexCount = 0;
    const uint32_t* indices = nullptr;   // triangle list
    uint32_t indexCount = 0;             // multiple of 3
};

// Maximum palette entries the GLES3 program declares (256 mat4 = 16 KB, the ES 3.0
// minimum guaranteed MAX_UNIFORM_BLOCK_SIZE).  Pieces whose palette exceeds this are
// skipped with a logged error — never approximated.
static constexpr uint32_t kMaxGpuBones = 256;

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

    // ------------------------------------------------------------------
    // B18: GPU skinned path.  Handles are 0 on failure and must be released
    // (ReleaseGpuSkinnedMesh) before the owning asset is destroyed.
    // ------------------------------------------------------------------
    uint32_t UploadGpuSkinnedMesh(const GpuSkinnedMeshData& data);
    void ReleaseGpuSkinnedMesh(uint32_t handle);
    // Draws one contiguous face range [faceStart, faceStart+faceCount) of the uploaded
    // mesh (the .x attribute-group/subset layout).  `paletteRowMajor` holds
    // paletteCount * 16 row-major floats (offset*combined, DxSkinMesh9_HLSL.cpp:220
    // parity — row-major storage fed into a column-major mat4 keeps the original
    // v*M row-vector semantics for M*v in GLSL).  Returns false (with a logged
    // error) on invalid handles, palette overflow, or out-of-range face ranges —
    // never on a missing texture (textureId 0 keeps the flat material colour).
    bool DrawGpuSkinnedMesh(uint32_t handle,
                            const float* paletteRowMajor,
                            uint32_t paletteCount,
                            uint32_t textureId,
                            const float color[4],
                            bool alphaBlend,
                            uint32_t faceStart,
                            uint32_t faceCount);
    uint32_t GpuMeshCount() const { return m_gpuMeshCount; }
    uint32_t GpuDrawCount() const { return m_gpuDrawCount; }
    uint32_t GpuDrawFailures() const { return m_gpuDrawFailures; }

    // B19 diagnostics (live-GLES verification): whether the GPU skin program is
    // compiled and linked, and the shader sources used for the skinning program
    // (the GpuSelfTest harness inspects the exact source text shipped to the GL
    // driver).  Headless builds have no GL program — Ready() is false and the
    // source accessors return nullptr.
    bool GpuSkinProgramReady() const {
#ifdef __ANDROID__
        return m_charGpuProgram != 0;
#else
        return false;
#endif
    }
    // B19: driver info log of the last GPU skin program compile/link attempt
    // ("" when none).  Embedded in the self-test JSON for CI evidence.
    static const std::string& GpuSkinProgramLog();
    static const char* CharGpuVertexShaderSource();
    static const char* CharGpuFragmentShaderSource();

    // B19 GPU-resource lifecycle: releases every GL object the renderer owns
    // (programs, VAO/VBO/EBO, palette UBO, per-mesh buffers, font atlas) while
    // keeping the renderer object usable.  Called by the app when the EGL
    // context is destroyed (window loss/rotation) so the next context rebuilds
    // instead of drawing with stale handles; Shutdown() forwards here.
    void ReleaseGlResources();

    uint32_t UploadRGBA8Texture(uint32_t width, uint32_t height, const uint8_t* rgba8);
    void DeleteTexture(uint32_t texId);

    // B19 diagnostic: overrides the view-projection matrix DrawGpuSkinnedMesh
    // uploads until the next BeginScene3D recomputes it.  Lets the GpuSelfTest
    // render NDC-space fixtures deterministically (pixel-level verification).
    void SetViewProjectionOverride(const float colMajor[16]);

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

    // B18 GPU skinned mesh registry — kept outside __ANDROID__ so headless builds
    // exercise the same upload/draw validation path (GL objects live only on Android).
    std::unordered_map<uint32_t, void*> m_gpuMeshes;  // handle -> GpuMeshGlState*
    uint32_t m_nextGpuMeshHandle = 1;
    uint32_t m_gpuMeshCount = 0;
    uint32_t m_gpuDrawCount = 0;
    uint32_t m_gpuDrawFailures = 0;

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

    // B18 GPU skinned pass state.
    uint32_t m_charGpuProgram = 0;
    uint32_t m_gpuPaletteUBO = 0;   // shared std140 mat4[kMaxGpuBones] palette block
    size_t m_gpuPaletteUBOBytes = 0;
#endif
};

} // namespace RANMobile
