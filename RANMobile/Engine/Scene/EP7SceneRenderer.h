#pragma once

#include "../../Data/Importer/EP7AssetPipeline.h"
#include "../../Data/Importer/EP7SceneData.h"
#include <string>

namespace RANMobile {
    class MobileRenderer;
    class ResourceManager;
}

namespace RANMobile {

// Render-only integration of original EP7 scene metadata and loading textures.
// It does NOT parse .wld geometry, place entities, or implement gameplay.
class EP7SceneRenderer {
public:
    bool Load(const std::string& root, RANMobile::MobileRenderer* renderer,
              RANMobile::ResourceManager* resources);
    void Draw(RANMobile::MobileRenderer* renderer) const;
    void Unload(RANMobile::MobileRenderer* renderer);

    bool IsLoaded() const { return m_index.valid; }
    bool HasPreviewTexture() const { return m_hasPreview; }
    const std::string& GetStatus() const { return m_status; }
    const EP7SceneIndex& GetIndex() const { return m_index; }
    const AssetPipelineReport& GetAssetReport() const { return m_assetReport; }

private:
    EP7SceneIndex m_index;
    AssetPipelineReport m_assetReport;
    std::string m_previewTextureKey;
    std::string m_status = "NO SCENE DATA LOADED";
    bool m_hasPreview = false;
    float m_alphaCutoff = 0.0f;
};

} // namespace RANMobile
