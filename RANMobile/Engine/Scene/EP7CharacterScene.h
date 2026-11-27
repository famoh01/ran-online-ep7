//
// EP7CharacterScene.h — Authentic EP7 character instances for the mobile scene (Phase 3J)
//
// This is the integration layer the phase asks for: it owns
//   * character instances (one per .chf, sharing cached piece/skeleton/animation data),
//   * per-instance transform + animation playback,
//   * the CPU pose evaluation that feeds the renderer,
//   * verification diagnostics that state honestly whether authentic data was used.
//
// WHAT IT DELIBERATELY DOES NOT DO
// -------------------------------
//  * It does not create placeholder/fake characters.  A character only becomes
//    renderable after authentic .chf + .cps + .x assets were parsed successfully.
//  * It does not invent animation states: playback is limited to the clips named in the
//    .chf and the flags stored in the authentic .cfg.
//  * It does not claim GPU skinning.  The pose evaluation implemented here is the CPU
//    reference path (DxSkinMesh9_CPU equivalent); the GLES3 vertex-shader binding for the
//    palette is recorded as pending in PHASE3J_CHARACTER_ANIMATION.md because the mobile
//    renderer on this branch has no 3D world pass yet (that lives in the open PR #4).
//
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../Data/Importer/EP7Animation.h"
#include "../../Data/Importer/EP7CharacterFile.h"
#include "../../Data/Importer/EP7Skeleton.h"
#include "../../Data/Importer/EP7SkinPiece.h"
#include "../../Data/Importer/EP7SkinnedMesh.h"
#include "../Animation/EP7SkinAniControl.h"
#include "../Camera/MobileCamera.h"
#include "../Renderer/MobileRenderer.h"

namespace RANMobile {

// One loaded piece: .cps metadata + the .x skinned mesh it points at.
struct EP7CharacterPiece {
    EP7SkinPieceInfo info;
    EP7SkinnedMesh mesh;
    bool loaded = false;
    std::string error;

    // B18: per-vertex GPU skinning streams (DxSkinMesh9_HLSL.cpp:95-106 parity),
    // built once at load from the SkinWeights lists — renderer independent.
    EP7GpuSkinStreams gpuStreams;
    // Renderer handle of the uploaded static GPU mesh (0 = not uploaded).  Owned by
    // the scene session: EP7CharacterScene::Clear releases every handle it created.
    uint32_t gpuMeshHandle = 0;
};

// Everything shared by every instance of one .chf.
struct EP7CharacterAsset {
    EP7CharacterFileInfo file;
    std::shared_ptr<EP7Skeleton> skeleton;
    std::vector<EP7CharacterPiece> pieces;               // indexed by EP7PieceType
    std::vector<EP7AnimClip> clips;                      // parallel to file.animations
    std::vector<EP7AnimInfo> clipInfos;

    bool loaded = false;
    std::string error;

    // Diagnostics
    uint32_t boneCount = 0;
    uint32_t pieceCount = 0;
    uint32_t meshVertexCount = 0;
    uint32_t meshTriangleCount = 0;
    uint32_t skinBoneCount = 0;
    uint32_t decodedGeometryPieces = 0;
    uint32_t animationCount = 0;
};

class EP7CharacterLoader {
public:
    // Loads a .chf and everything it references.
    //   `fileBytes`   full .chf bytes
    //   `resolve`     maps an asset name (e.g. "o_m.cps", "o_m.skt") to its bytes; return
    //                 an empty vector when the asset is unavailable.
    // Missing optional assets are reported in EP7CharacterAsset::error but do not abort
    // the whole character: a piece may legitimately be absent (e.g. no head gear).
    EP7CharacterAsset LoadCharacter(const std::vector<uint8_t>& fileBytes, bool compByteEncoded);

    // Callback based resolver so the loader stays free of file-system policy
    // (Android asset manager / pack archive / test fixtures all plug in here).
    using AssetResolver = bool (*)(void* userData, const std::string& name, std::vector<uint8_t>& outBytes);
    void SetResolver(AssetResolver resolver, void* userData) {
        m_resolver = resolver;
        m_userData = userData;
    }

private:
    bool Resolve(const std::string& name, std::vector<uint8_t>& out) const;

    AssetResolver m_resolver = nullptr;
    void* m_userData = nullptr;
};

struct EP7CharacterInstance {
    uint32_t id = 0;
    std::shared_ptr<EP7CharacterAsset> asset;
    std::unique_ptr<EP7Skeleton> poseSkeleton;  // per-instance copy of the bind pose

    // World placement.
    float position[3] = {0.0f, 0.0f, 0.0f};
    float yawDegrees = 0.0f;
    float scale = 1.0f;

    // Playback: the authentic DxSkinAniControl state machine (B17). Spawn registers
    // every valid clip and plays DEFAULTANI (registry entry 0) exactly like the
    // original does when it has no explicit selection yet.
    EP7SkinAniControl aniControl;
    int32_t currentClip = -1;

    // Render state produced by EvaluatePose().
    std::vector<float> skinnedPositions;   // per piece, concatenated
    std::vector<float> skinnedNormals;
    std::vector<uint32_t> pieceVertexOffsets;

    // B17: persistent scratch buffers — the per-frame pose path must not allocate
    // (engine-quality requirement); capacity is reused across frames.
    std::vector<ep7math::Mat4> skinPaletteScratch;
    std::vector<float> skinPosScratch;
    std::vector<float> skinNormScratch;

    // B18: per-piece palette snapshots (offset*combined, DxSkinMesh9_HLSL.cpp:220
    // "mWorldMatrixArray" parity) captured during EvaluatePose and consumed by the
    // GPU skinned draw. Capacity is reused across frames.
    std::vector<std::vector<ep7math::Mat4>> piecePalettes;

    bool visible = true;
    bool poseValid = false;
    // TRUE while a non-looping clip is playing and has reported its end
    // (ANIMCONTNODE::FrameMove bEndAnim) — the gameplay layer re-selects on this.
    bool animationEnded = false;
    // World-space AABB, from the authentic .chf bounding box when present.
    float boundsMin[3] = {0.0f, 0.0f, 0.0f};
    float boundsMax[3] = {0.0f, 0.0f, 0.0f};
    bool inFrustum = true;
    float lastSkinMs = 0.0f;
    uint32_t drawCalls = 0;
    uint32_t triangles = 0;
};

// Diagnostics shown by the mobile overlay. Counts are measured, never assumed.
struct EP7CharacterDiagnostics {
    uint32_t characters = 0;
    uint32_t visibleCharacters = 0;
    uint32_t totalBones = 0;
    uint32_t animatedBones = 0;
    uint32_t totalVertices = 0;
    uint32_t totalTriangles = 0;
    uint32_t drawCalls = 0;
    uint32_t animationCount = 0;
    uint32_t decodedGeometryPieces = 0;
    float cpuSkinMs = 0.0f;
    float fps = 0.0f;
    bool authenticAssetsOnly = true;
    std::string lastError;
};

class EP7CharacterScene {
public:
    using AssetPtr = std::shared_ptr<EP7CharacterAsset>;

    // Registers a loaded authentic character asset. Returns false for invalid assets
    // (never registers placeholders).
    bool RegisterAsset(const AssetPtr& asset);

    // Creates an instance of `asset`. Returns 0 on failure.
    uint32_t Spawn(const AssetPtr& asset, float x, float y, float z, float yawDegrees);

    bool Destroy(uint32_t instanceId);
    void Clear();

    EP7CharacterInstance* Find(uint32_t instanceId);
    const std::vector<std::unique_ptr<EP7CharacterInstance>>& Instances() const { return m_instances; }

    // Plays a clip by its authentic name (as listed in the .chf). Returns false when the
    // name is not part of the character.
    bool PlayAnimation(uint32_t instanceId, const std::string& clipName);
    bool PlayAnimationIndex(uint32_t instanceId, int32_t clipIndex);

    // B17: authentic type-based selection (DxSkinAniControl::SELECTANI(MType, SType,
    // dwFlags, dwSelect)) and the DEFAULTANI / end-state helpers for the gameplay layer.
    bool SelectAnimationByType(uint32_t instanceId, int32_t mainType, int32_t subType = 0, uint32_t select = 0);
    void DefaultAnimation(uint32_t instanceId);
    void DoShockMix(uint32_t instanceId);
    // TRUE while a non-looping clip is playing and has reached its end window.
    bool IsAnimationEnded(uint32_t instanceId) const;

    // Advances animation clocks and re-evaluates poses.
    void Update(float deltaSeconds);

    // Camera-facing helpers (mobile UX): orbit/zoom drive the *existing* MobileCamera,
    // these only adapt character selection to it.
    void SetPreviewTarget(uint32_t instanceId) { m_previewTarget = instanceId; }
    uint32_t PreviewTarget() const { return m_previewTarget; }

    // Camera-facing culling. The frustum test itself stays in MobileRenderer
    // (MobileRenderer::IsBoxInFrustum) so there is exactly one implementation; this only
    // feeds it each instance's world-space AABB (derived from the authentic .chf bounds,
    // scaled by the character scale and offset by the instance position).
    uint32_t CullInstances(const MobileCamera& camera, const MobileRenderer* renderer);

    // World-space AABB of an instance as written by CullInstances()/UpdateBounds().
    void UpdateBounds(EP7CharacterInstance& instance) const;

    EP7CharacterDiagnostics GetDiagnostics() const;
    void SetMeasuredFrameTime(float milliseconds, float fps) {
        m_lastFrameMs = milliseconds;
        m_lastFps = fps;
    }

    // CPU pose evaluation for one instance.  Exposed for tests.
    // `outSkinMs` receives the measured skinning time for this character.
    static bool EvaluatePose(EP7CharacterInstance& instance, float* outSkinMs);

    // ------------------------------------------------------------------
    // B18: GPU skinning + texture binding integration.
    //
    // The renderer pointer must be set before GPU resources are requested
    // (RANMobileEngine wires it at Initialize).  Uploaded mesh handles and
    // decoded textures are scene-scoped caches; Clear() releases them.
    //
    // Authentic texture association (DxSkinPieceContainer.cpp:82
    // SMATERIAL_PIECE::LoadTexture): the .cps material texture name is the
    // binding key — TextureManager::LoadTexture(name) resolves it (with
    // IsEncrypt: a ".mtf" name decrypts through CMemoryTexture) and caches by
    // lowercase name.  The mobile resolver receives candidate names; the
    // resolution ladder is: exact name -> stem + ".mtf" -> name + ".mtf".
    // ------------------------------------------------------------------
    using AssetResolver = bool (*)(void* userData, const std::string& name, std::vector<uint8_t>& outBytes);
    void SetRenderer(MobileRenderer* renderer) { m_renderer = renderer; }
    void SetTextureResolver(AssetResolver resolver, void* userData) {
        m_textureResolver = resolver;
        m_textureResolverData = userData;
    }

    // Returns the renderer handle for the piece's static GPU mesh (0 = unsupported
    // piece; the caller falls back to the B17 CPU path).  Uploads once per piece.
    uint32_t EnsurePieceGpuHandle(const AssetPtr& asset, uint32_t slot);

    // Returns the GL texture id for the piece's subset material (0 = untextured:
    // missing/unresolvable texture names are never fabricated).
    uint32_t GetPieceTextureId(const AssetPtr& asset, uint32_t slot, uint32_t subsetIndex);

    // Palette snapshot (offset*combined, row-major floats) for the piece's GPU draw.
    bool GetPiecePalette(const EP7CharacterInstance& instance, uint32_t slot,
                         const float** outPtr, uint32_t* outCount) const;

private:

    std::vector<std::unique_ptr<EP7CharacterInstance>> m_instances;
    uint32_t m_nextId = 1;
    uint32_t m_previewTarget = 0;
    float m_lastFrameMs = 0.0f;
    float m_lastFps = 0.0f;
    std::string m_lastError;

    MobileRenderer* m_renderer = nullptr;
    AssetResolver m_textureResolver = nullptr;
    void* m_textureResolverData = nullptr;
    std::unordered_map<std::string, uint32_t> m_textureCache;  // lowercase name -> GL id
    std::vector<uint32_t> m_uploadedGpuMeshes;                 // handles this scene created

    void ReleaseGpuResources();
    uint32_t ResolveTextureLocked(const std::string& textureName);
};

}  // namespace RANMobile
