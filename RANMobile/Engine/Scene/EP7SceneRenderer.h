#pragma once

#include "../../Data/Importer/EP7AssetPipeline.h"
#include "../../Data/Importer/EP7SceneData.h"
#include "../../Data/Importer/EP7WorldParser.h"
#include "../Renderer/MobileRenderer.h"
#include <cstddef>
#include <string>
#include <vector>

namespace RANMobile {
    class MobileCamera;
    class ResourceManager;
}

namespace RANMobile {

// Integrates EP7 scene metadata, 2D loading-screen preview fallback, and full 3D
// world streaming (.wld + .wld0 + .pis/.pie + .Lev + textures) over MobileRenderer.
class EP7SceneRenderer {
public:
    bool Load(const std::string& root, RANMobile::MobileRenderer* renderer,
              RANMobile::ResourceManager* resources,
              EP7ValidationMode mode = EP7ValidationMode::StrictAuthentic);
    bool Load(const std::string& root, RANMobile::MobileRenderer* renderer,
              RANMobile::ResourceManager* resources,
              const EP7SceneBuildOptions& options);

    // Loads a specific .wld (plus companion .wld0/.Lev, pieces, and textures) directly from root.
    bool LoadStandaloneWorld(const std::string& root,
                             const std::string& wldFileName,
                             RANMobile::MobileRenderer* renderer,
                             RANMobile::ResourceManager* resources);

    bool SwitchScene(size_t sceneIndex,
                     RANMobile::MobileRenderer* renderer,
                     RANMobile::ResourceManager* resources);

    void ConfigureDefaultCamera(MobileCamera* camera) const;

    void Draw(RANMobile::MobileRenderer* renderer) const;
    void Draw(RANMobile::MobileRenderer* renderer, const MobileCamera* camera) const;
    void Unload(RANMobile::MobileRenderer* renderer);

    bool IsLoaded() const { return m_index.valid || m_hasWorld3D; }
    bool HasPreviewTexture() const { return m_hasPreview; }
    bool HasWorld3D() const { return m_hasWorld3D; }
    EP7ValidationMode GetValidationMode() const { return m_index.validationMode; }
    size_t GetResolvedLevelCount() const { return m_index.resolvedLevelHeaders; }
    size_t GetSceneEntryCount() const { return m_index.scenes.size(); }
    const std::string& GetStatus() const { return m_status; }
    const std::string& GetCurrentWorldFile() const { return m_currentWorldFile; }
    const EP7SceneIndex& GetIndex() const { return m_index; }
    const AssetPipelineReport& GetAssetReport() const { return m_assetReport; }
    const EP7WorldData& GetWorldData() const { return m_world; }
    const EP7LevelFullData& GetLevelFullData() const { return m_levelFull; }

    size_t GetLoadedTextureCount() const { return m_loadedTextureCount; }
    size_t GetMissingTextureCount() const { return m_missingTextureCount; }
    size_t GetRegisteredMeshCount() const { return m_registeredMeshes.size(); }

private:
    struct BatchDrawBinding {
        uint32_t meshHandle = 0;
        uint32_t texId = 0;
        EP7AlphaBucket bucket = EP7AlphaBucket::Opaque;
        EP7AABB bounds;
        EP7Color4 tint{1.0f, 1.0f, 1.0f, 1.0f};
        bool twoSided = false;
    };

    struct PieceAssetGpu {
        std::vector<BatchDrawBinding> batches;
    };

    bool LoadWorldInternal(const std::string& root,
                           const std::string& wldFileName,
                           const std::string& levFileName,
                           MobileRenderer* renderer,
                           EP7ValidationMode mode);
    void UnloadWorldResources(MobileRenderer* renderer);
    void BuildLightingBaseline();
    WorldLightingState BuildFrameLighting(float focusX, float focusY, float focusZ) const;

    EP7SceneIndex m_index;
    AssetPipelineReport m_assetReport;
    std::string m_root;
    std::string m_previewTextureKey;
    std::string m_status = "NO SCENE DATA LOADED";
    bool m_hasPreview = false;
    float m_alphaCutoff = 0.0f;

    EP7WorldData m_world;
    EP7LevelFullData m_levelFull;
    std::string m_currentWorldFile;
    bool m_hasWorld3D = false;

    std::vector<uint32_t> m_registeredMeshes;
    std::vector<std::string> m_loadedWorldTextureKeys;
    std::vector<BatchDrawBinding> m_terrainDraws;
    std::vector<PieceAssetGpu> m_pieceAssetGpus;
    uint32_t m_skyMeshHandle = 0;
    uint32_t m_skyTexId = 0;

    WorldLightingState m_baseLighting;
    size_t m_loadedTextureCount = 0;
    size_t m_missingTextureCount = 0;
};

} // namespace RANMobile
