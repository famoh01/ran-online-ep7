#include "EP7SceneRenderer.h"
#include "../Renderer/MobileRenderer.h"
#include "../Resource/ResourceManager.h"
#include "../Core/Logger.h"
#include "../../Data/Importer/EP7TextureDecoder.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace RANMobile {

namespace {

std::vector<uint8_t> ReadBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return {};
    const std::streamsize size = in.tellg();
    if (size < 0) return {};
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    if (size > 0 && !in.read(reinterpret_cast<char*>(bytes.data()), size)) return {};
    return bytes;
}

fs::path FindFile(const fs::path& root, const std::string& name) {
    if (name.empty()) return {};
    std::error_code ec;
    for (fs::recursive_directory_iterator it(root, ec), end; it != end && !ec; it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().filename() == name) return it->path();
    }
    return {};
}

} // namespace

bool EP7SceneRenderer::Load(const std::string& root, MobileRenderer* renderer,
                            ResourceManager* resources) {
    Unload(renderer);
    m_assetReport = EP7AssetPipeline::ScanDirectory(root, resources);
    m_index = EP7SceneIndexBuilder::Build(root);
    if (!m_index.valid) {
        m_status = "NO ORIGINAL SCENE METADATA RESOLVED";
        Logger::Warn("EP7SceneRenderer", m_status);
        return false;
    }

    // The first resolvable loading image is an original DDS/MTF texture referenced
    // by mapslist.mst. It is shown as a texture preview, never as a game scene.
    for (const EP7SceneEntry& scene : m_index.scenes) {
        if (scene.loadingImage.empty()) continue;
        const fs::path path = FindFile(root, scene.loadingImage);
        if (path.empty()) continue;
        std::vector<uint8_t> bytes = ReadBytes(path);
        if (bytes.empty()) continue;

        std::vector<uint8_t> ddsBytes;
        const std::string ext = path.extension().string();
        if (ext == ".mtf") {
            int fileType = -1;
            if (!EP7TextureDecoder::DecryptMTF(bytes, ddsBytes, fileType)) continue;
            if (fileType != 0) continue; // TEXTURE_DDS per EncryptTextureDef.h
        } else if (ext == ".dds") {
            ddsBytes = std::move(bytes);
        } else {
            continue;
        }

        const DecodedTexture decoded = EP7TextureDecoder::DecodeDDS(ddsBytes, path.filename().string());
        if (!decoded.valid || !renderer) continue;
        m_previewTextureKey = path.generic_string();
        if (!renderer->LoadAssetTexture(m_previewTextureKey, decoded.width, decoded.height,
                                        decoded.rgba8)) {
            m_status = "TEXTURE UPLOAD FAILED: " + m_previewTextureKey;
            Logger::Error("EP7SceneRenderer", m_status);
            continue;
        }
        m_hasPreview = true;
        // Hard/cutout textures use alpha test; soft textures use blending only.
        m_alphaCutoff = decoded.alphaMode == EP7AlphaMode::Hard ? 0.5f : 0.0f;
        m_status = "ORIGINAL ASSET TEXTURE READY: " + path.filename().string() +
                   " (" + std::to_string(decoded.width) + "x" + std::to_string(decoded.height) + ")";
        Logger::Info("EP7SceneRenderer", m_status);
        break;
    }

    if (!m_hasPreview) {
        m_status = "SCENE METADATA LOADED; NO COMPATIBLE ORIGINAL TEXTURE FOUND";
    }
    return true;
}

void EP7SceneRenderer::Draw(MobileRenderer* renderer) const {
    if (!renderer || !m_index.valid) return;
    const float designW = renderer->GetDesignWidth();
    const float designH = renderer->GetDesignHeight();
    renderer->DrawUIQuad(0.0f, 0.0f, designW, designH, 0.02f, 0.04f, 0.09f, 1.0f);

    if (m_hasPreview) {
        const uint32_t tex = renderer->GetAssetTexture(m_previewTextureKey);
        if (tex != 0) {
            const float availableW = designW - 96.0f;
            const float availableH = designH - 180.0f;
            const float scale = std::min(availableW / designW, availableH / designH);
            const float w = designW * scale;
            const float h = designH * scale;
            renderer->DrawTexturedQuad((designW - w) * 0.5f, 72.0f, w, h, tex, 1.0f, 1.0f, 1.0f, 1.0f,
                                       m_alphaCutoff);
        }
    }
    renderer->DrawUIText(28.0f, 20.0f, "ORIGINAL EP7 ASSET PREVIEW - NOT A PLAYABLE SCENE", 0.78f,
                         0.94f, 0.76f, 0.28f, 1.0f);
    renderer->DrawUIText(28.0f, designH - 46.0f, m_status, 0.72f, 0.82f, 0.88f, 1.0f, 1.0f);
}

void EP7SceneRenderer::Unload(MobileRenderer* renderer) {
    if (renderer && !m_previewTextureKey.empty()) renderer->UnloadAssetTexture(m_previewTextureKey);
    m_previewTextureKey.clear();
    m_hasPreview = false;
    m_alphaCutoff = 0.0f;
    m_status = "NO SCENE DATA LOADED";
}

} // namespace RANMobile
