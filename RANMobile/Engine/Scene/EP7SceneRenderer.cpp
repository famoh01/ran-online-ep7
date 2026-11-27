#include "EP7SceneRenderer.h"
#include "../Camera/MobileCamera.h"
#include "../Renderer/MobileRenderer.h"
#include "../Resource/ResourceManager.h"
#include "../Core/Logger.h"
#include "../../Data/Importer/EP7HashVerify.h"
#include "../../Data/Importer/EP7TextureDecoder.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <unordered_map>

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

void BuildLowerFileIndex(const fs::path& root, std::unordered_map<std::string, fs::path>& outMap) {
    outMap.clear();
    std::error_code ec;
    for (fs::recursive_directory_iterator it(root, ec), end; it != end && !ec; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        const std::string lower = EP7HashVerify::LowerAscii(it->path().filename().string());
        if (outMap.find(lower) == outMap.end()) {
            outMap.emplace(lower, it->path());
        }
    }
}

fs::path LookupIndexedAsset(const std::unordered_map<std::string, fs::path>& index,
                            const std::string& filename) {
    if (filename.empty() ||
        filename.find('/') != std::string::npos ||
        filename.find('\\') != std::string::npos ||
        filename.find("..") != std::string::npos) {
        return {};
    }
    const std::string lower = EP7HashVerify::LowerAscii(filename);
    auto it = index.find(lower);
    if (it != index.end()) return it->second;

    // EP7 clients also package .dds/.tga inside .mtf containers under the same stem.
    const size_t dot = lower.find_last_of('.');
    if (dot != std::string::npos) {
        const std::string stem = lower.substr(0, dot);
        auto mtfIt = index.find(stem + ".mtf");
        if (mtfIt != index.end()) return mtfIt->second;
    }
    return {};
}

// Generates a simple unit dome mesh (inward-facing CW) for EP7 sky rendering.
void BuildSkyDomeMesh(std::vector<EP7WorldVertex>& outVerts, std::vector<uint16_t>& outIndices) {
    outVerts.clear();
    outIndices.clear();
    const int stacks = 10;
    const int slices = 20;
    const float kPi = 3.14159265358979323846f;
    for (int st = 0; st <= stacks; ++st) {
        const float v = static_cast<float>(st) / static_cast<float>(stacks);
        const float phi = v * (kPi * 0.6f); // upper dome slightly below horizon
        const float y = std::cos(phi);
        const float r = std::sin(phi);
        for (int sl = 0; sl <= slices; ++sl) {
            const float u = static_cast<float>(sl) / static_cast<float>(slices);
            const float theta = u * 2.0f * kPi;
            EP7WorldVertex vert;
            vert.x = r * std::cos(theta);
            vert.y = y;
            vert.z = r * std::sin(theta);
            vert.nx = 0.0f;
            vert.ny = 1.0f;
            vert.nz = 0.0f;
            vert.u = u * 2.0f;
            vert.v = v;
            vert.rgba = 0xFFFFFFFFu;
            outVerts.push_back(vert);
        }
    }
    const int ringStride = slices + 1;
    for (int st = 0; st < stacks; ++st) {
        for (int sl = 0; sl < slices; ++sl) {
            const uint16_t a = static_cast<uint16_t>(st * ringStride + sl);
            const uint16_t b = static_cast<uint16_t>(a + 1);
            const uint16_t c = static_cast<uint16_t>((st + 1) * ringStride + sl);
            const uint16_t d = static_cast<uint16_t>(c + 1);
            outIndices.push_back(a); outIndices.push_back(b); outIndices.push_back(c);
            outIndices.push_back(b); outIndices.push_back(d); outIndices.push_back(c);
        }
    }
}

bool ColorIsNearlyBlack(const EP7Color4& c) {
    return (c.r + c.g + c.b) < 0.02f;
}

float AlphaCutoffForBucket(EP7AlphaBucket bucket) {
    return (bucket == EP7AlphaBucket::HardAlpha) ? 0.45f
         : (bucket == EP7AlphaBucket::SoftAlpha ||
            bucket == EP7AlphaBucket::SoftAlpha01 ||
            bucket == EP7AlphaBucket::SoftAlpha02) ? 0.01f
         : 0.0f;
}

bool IsTwoSidedBucket(EP7AlphaBucket bucket, bool batchTwoSided) {
    return batchTwoSided || bucket != EP7AlphaBucket::Opaque;
}

bool IsSoftAlphaBucket(EP7AlphaBucket bucket) {
    return bucket == EP7AlphaBucket::SoftAlpha ||
           bucket == EP7AlphaBucket::SoftAlpha01 ||
           bucket == EP7AlphaBucket::SoftAlpha02;
}

} // namespace

bool EP7SceneRenderer::Load(const std::string& root, MobileRenderer* renderer,
                            ResourceManager* resources, EP7ValidationMode mode) {
    EP7SceneBuildOptions options;
    options.validationMode = mode;
    return Load(root, renderer, resources, options);
}

bool EP7SceneRenderer::Load(const std::string& root, MobileRenderer* renderer,
                            ResourceManager* resources, const EP7SceneBuildOptions& options) {
    Unload(renderer);
    m_root = root;
    m_assetReport = EP7AssetPipeline::ScanDirectory(root, resources);
    m_index = EP7SceneIndexBuilder::Build(root, options);
    if (!m_index.valid) {
        if (!m_index.diagnostics.empty()) {
            m_status = "NO ORIGINAL SCENE METADATA RESOLVED: " + m_index.diagnostics.front();
        } else {
            m_status = "NO ORIGINAL SCENE METADATA RESOLVED";
        }
        Logger::Warn("EP7SceneRenderer", m_status);
        return false;
    }

    std::unordered_map<std::string, fs::path> fileIndex;
    BuildLowerFileIndex(root, fileIndex);

    // Load the first resolvable loading image for 2D preview / splash fallback.
    for (const EP7SceneEntry& scene : m_index.scenes) {
        if (!scene.glmapValid || scene.loadingImage.empty()) continue;
        const fs::path path = LookupIndexedAsset(fileIndex, scene.loadingImage);
        if (path.empty()) continue;
        std::vector<uint8_t> bytes = ReadBytes(path);
        if (bytes.empty()) continue;

        const DecodedTexture decoded = EP7TextureDecoder::DecodeTextureFile(bytes, path.filename().string());
        if (!decoded.valid || !renderer) continue;
        m_previewTextureKey = path.generic_string();
        if (!renderer->LoadAssetTexture(m_previewTextureKey, decoded.width, decoded.height,
                                        decoded.rgba8)) {
            m_status = "TEXTURE UPLOAD FAILED: " + m_previewTextureKey;
            Logger::Error("EP7SceneRenderer", m_status);
            continue;
        }
        m_hasPreview = true;
        m_alphaCutoff = decoded.alphaMode == EP7AlphaMode::Hard ? 0.5f : 0.0f;
        m_status = "ORIGINAL ASSET TEXTURE READY: " + path.filename().string() +
                   " (" + std::to_string(decoded.width) + "x" + std::to_string(decoded.height) + ")";
        Logger::Info("EP7SceneRenderer", m_status);
        break;
    }

    // Stream full 3D geometry for the first scene whose .wld exists on disk.
    for (const EP7SceneEntry& scene : m_index.scenes) {
        if (!scene.glmapValid || !scene.wldFound || scene.wldFile.empty()) continue;
        if (LoadWorldInternal(root, scene.wldFile, scene.glmapFile, renderer, options.validationMode)) {
            break;
        }
    }

    if (!m_hasWorld3D && !m_hasPreview) {
        m_status = "SCENE METADATA LOADED; NO COMPATIBLE ORIGINAL TEXTURE FOUND";
    }
    return true;
}

bool EP7SceneRenderer::LoadStandaloneWorld(const std::string& root,
                                           const std::string& wldFileName,
                                           MobileRenderer* renderer,
                                           ResourceManager* resources) {
    Unload(renderer);
    m_root = root;
    m_assetReport = EP7AssetPipeline::ScanDirectory(root, resources);
    return LoadWorldInternal(root, wldFileName, "", renderer, EP7ValidationMode::StrictAuthentic);
}

bool EP7SceneRenderer::SwitchScene(size_t sceneIndex,
                                   MobileRenderer* renderer,
                                   ResourceManager* resources) {
    (void)resources;
    if (!m_index.valid || sceneIndex >= m_index.scenes.size()) return false;
    const EP7SceneEntry& entry = m_index.scenes[sceneIndex];
    if (!entry.glmapValid || entry.wldFile.empty()) return false;
    UnloadWorldResources(renderer);
    return LoadWorldInternal(m_root, entry.wldFile, entry.glmapFile, renderer, m_index.validationMode);
}

bool EP7SceneRenderer::LoadWorldInternal(const std::string& root,
                                         const std::string& wldFileName,
                                         const std::string& levFileName,
                                         MobileRenderer* renderer,
                                         EP7ValidationMode mode) {
    if (root.empty() || wldFileName.empty() || !renderer) return false;

    std::unordered_map<std::string, fs::path> fileIndex;
    BuildLowerFileIndex(root, fileIndex);

    const fs::path wldPath = LookupIndexedAsset(fileIndex, wldFileName);
    if (wldPath.empty()) return false;
    const std::vector<uint8_t> wldBytes = ReadBytes(wldPath);
    if (wldBytes.empty()) return false;

    std::vector<uint8_t> wld0Bytes;
    const fs::path wld0Path = LookupIndexedAsset(fileIndex, wldFileName + "0");
    if (!wld0Path.empty()) {
        wld0Bytes = ReadBytes(wld0Path);
    }

    EP7WorldData parsed = EP7WorldParser::ParseWld(wldBytes, wld0Bytes);
    if (!parsed.valid) {
        Logger::Warn("EP7SceneRenderer", "Failed to parse 3D world " + wldFileName + ": " + parsed.error);
        return false;
    }

    EP7WorldParser::ResolveAndAttachPieces(parsed, root);

    m_levelFull = EP7LevelFullData{};
    if (!levFileName.empty()) {
        const fs::path levPath = LookupIndexedAsset(fileIndex, levFileName);
        if (!levPath.empty()) {
            const auto levBytes = ReadBytes(levPath);
            if (!levBytes.empty()) {
                m_levelFull = EP7WorldParser::ParseFullLevel(levBytes, mode);
            }
        }
    }

    // Decode & upload all referenced world/piece/sky textures
    std::unordered_map<std::string, uint32_t> lowerTexToGpuId;
    m_loadedTextureCount = 0;
    m_missingTextureCount = 0;

    auto ensureTextureLoaded = [&](const std::string& texName) -> uint32_t {
        if (texName.empty()) return 0;
        const std::string lower = EP7HashVerify::LowerAscii(texName);
        auto cached = lowerTexToGpuId.find(lower);
        if (cached != lowerTexToGpuId.end()) return cached->second;

        const fs::path texPath = LookupIndexedAsset(fileIndex, texName);
        if (texPath.empty()) {
            lowerTexToGpuId[lower] = 0;
            ++m_missingTextureCount;
            return 0;
        }
        const auto bytes = ReadBytes(texPath);
        const DecodedTexture decoded = EP7TextureDecoder::DecodeTextureFile(bytes, texPath.filename().string());
        if (!decoded.valid) {
            lowerTexToGpuId[lower] = 0;
            ++m_missingTextureCount;
            return 0;
        }
        const std::string key = "wldtex:" + lower;
        if (!renderer->LoadWorldTexture(key, decoded.width, decoded.height, decoded.rgba8)) {
            lowerTexToGpuId[lower] = 0;
            ++m_missingTextureCount;
            return 0;
        }
        const uint32_t gpuId = renderer->GetAssetTexture(key);
        lowerTexToGpuId[lower] = gpuId;
        m_loadedWorldTextureKeys.push_back(key);
        ++m_loadedTextureCount;
        return gpuId;
    };

    for (const std::string& texRef : parsed.textureRefs) {
        ensureTextureLoaded(texRef);
    }
    // Probe common EP7 sky textures if sky is enabled or present in asset root
    const char* skyCandidates[] = {"sky_001.dds", "dark_sky3.tga"};
    for (const char* skyName : skyCandidates) {
        const fs::path p = LookupIndexedAsset(fileIndex, skyName);
        if (!p.empty()) {
            const uint32_t id = ensureTextureLoaded(skyName);
            if (id != 0 && m_skyTexId == 0) m_skyTexId = id;
        }
    }

    // Register terrain batches with renderer
    m_terrainDraws.clear();
    m_terrainDraws.reserve(parsed.terrainBatches.size());
    for (const EP7MeshBatch& batch : parsed.terrainBatches) {
        const uint32_t meshId = renderer->RegisterMesh3D(batch.vertices, batch.indices);
        if (meshId == 0) continue;
        m_registeredMeshes.push_back(meshId);
        BatchDrawBinding b;
        b.meshHandle = meshId;
        b.texId = batch.textureName.empty() ? 0 : lowerTexToGpuId[EP7HashVerify::LowerAscii(batch.textureName)];
        b.bucket = batch.bucket;
        b.bounds = batch.bounds;
        b.tint = batch.materialDiffuse;
        b.twoSided = batch.twoSided;
        m_terrainDraws.push_back(b);
    }

    // Register deduplicated piece asset batches
    m_pieceAssetGpus.resize(parsed.pieceAssets.size());
    for (size_t ai = 0; ai < parsed.pieceAssets.size(); ++ai) {
        const EP7PieceAsset& asset = parsed.pieceAssets[ai];
        auto& gpuAsset = m_pieceAssetGpus[ai];
        gpuAsset.batches.reserve(asset.batches.size());
        for (const EP7MeshBatch& batch : asset.batches) {
            const uint32_t meshId = renderer->RegisterMesh3D(batch.vertices, batch.indices);
            if (meshId == 0) continue;
            m_registeredMeshes.push_back(meshId);
            BatchDrawBinding b;
            b.meshHandle = meshId;
            b.texId = batch.textureName.empty() ? 0 : lowerTexToGpuId[EP7HashVerify::LowerAscii(batch.textureName)];
            b.bucket = batch.bucket;
            b.bounds = batch.bounds;
            b.tint = batch.materialDiffuse;
            b.twoSided = batch.twoSided;
            gpuAsset.batches.push_back(b);
        }
    }

    // Register procedural unit sky dome mesh when sky texture is available
    if (m_skyTexId != 0) {
        std::vector<EP7WorldVertex> skyVerts;
        std::vector<uint16_t> skyIndices;
        BuildSkyDomeMesh(skyVerts, skyIndices);
        m_skyMeshHandle = renderer->RegisterMesh3D(skyVerts, skyIndices);
        if (m_skyMeshHandle != 0) m_registeredMeshes.push_back(m_skyMeshHandle);
    }

    m_world = std::move(parsed);
    m_currentWorldFile = wldFileName;
    m_hasWorld3D = true;
    BuildLightingBaseline();

    m_status = "EP7 3D WORLD READY: " + wldFileName +
               " | batches=" + std::to_string(m_terrainDraws.size()) +
               " pieces=" + std::to_string(m_world.pieceInstances.size()) +
               " tex=" + std::to_string(m_loadedTextureCount);
    Logger::Info("EP7SceneRenderer", m_status);
    return true;
}

void EP7SceneRenderer::BuildLightingBaseline() {
    m_baseLighting = WorldLightingState{};
    m_baseLighting.fog = m_world.fog;
    if (ColorIsNearlyBlack(m_baseLighting.fog.fogColor) &&
        !ColorIsNearlyBlack(m_baseLighting.fog.fogColorDay)) {
        m_baseLighting.fog.fogColor = m_baseLighting.fog.fogColorDay;
    }

    bool foundDir = false;
    for (const EP7LightInfo& li : m_world.lights) {
        if (!li.enabled) continue;
        if (li.type == EP7LightType::Directional && !foundDir) {
            m_baseLighting.dirLightDir = li.direction;
            m_baseLighting.dirLightColor = li.diffuse;
            if (!ColorIsNearlyBlack(li.ambient)) {
                m_baseLighting.ambientColor = li.ambient;
            }
            foundDir = true;
        }
    }
    if (!foundDir) {
        m_baseLighting.dirLightDir = {-0.35f, -0.75f, 0.55f};
        m_baseLighting.dirLightColor = {0.85f, 0.85f, 0.80f, 1.0f};
        m_baseLighting.ambientColor = {0.50f, 0.50f, 0.54f, 1.0f};
    }
}

WorldLightingState EP7SceneRenderer::BuildFrameLighting(float focusX, float focusY, float focusZ) const {
    WorldLightingState st = m_baseLighting;
    struct CandidateLight {
        float distSq;
        const EP7LightInfo* li;
    };
    std::vector<CandidateLight> nearby;
    nearby.reserve(m_world.lights.size());
    for (const EP7LightInfo& li : m_world.lights) {
        if (!li.enabled || li.type == EP7LightType::Directional || li.range <= 0.1f) continue;
        const float dx = li.position.x - focusX;
        const float dy = li.position.y - focusY;
        const float dz = li.position.z - focusZ;
        nearby.push_back({dx * dx + dy * dy + dz * dz, &li});
    }
    std::sort(nearby.begin(), nearby.end(), [](const CandidateLight& a, const CandidateLight& b) {
        return a.distSq < b.distSq;
    });
    const size_t count = std::min<size_t>(4u, nearby.size());
    st.pointLightCount = static_cast<uint32_t>(count);
    for (size_t i = 0; i < count; ++i) {
        const EP7LightInfo* li = nearby[i].li;
        st.pointLights[i] = {
            li->position.x, li->position.y, li->position.z, li->range,
            li->diffuse.r, li->diffuse.g, li->diffuse.b
        };
    }
    return st;
}

void EP7SceneRenderer::ConfigureDefaultCamera(MobileCamera* camera) const {
    if (!camera || !m_hasWorld3D) return;
    const EP7Vec3 spawn = m_world.hasRecommendedSpawn
        ? m_world.recommendedSpawn
        : m_world.worldBounds.Center();
    const float radius = std::max(40.0f, m_world.worldBounds.BoundingRadius());
    const float initDist = std::clamp(radius * 0.14f, 35.0f, 180.0f);
    const float farPlane = std::max(1800.0f, m_world.fog.fogEnd * 1.6f);
    camera->SetDistanceBounds(6.0f, std::max(280.0f, radius * 1.2f));
    camera->SetProjectionParams(55.0f, camera->GetAspect(), 0.5f, farPlane);
    camera->Reset(25.0f, 22.0f, initDist, spawn.x, spawn.y + 8.0f, spawn.z);
}

void EP7SceneRenderer::Draw(MobileRenderer* renderer) const {
    Draw(renderer, nullptr);
}

void EP7SceneRenderer::Draw(MobileRenderer* renderer, const MobileCamera* camera) const {
    if (!renderer || !IsLoaded()) return;

    if (!m_hasWorld3D) {
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
        return;
    }

    MobileCamera fallbackCam;
    const MobileCamera* activeCam = camera;
    if (!activeCam) {
        ConfigureDefaultCamera(&fallbackCam);
        fallbackCam.SetViewportAspect(renderer->GetDesignWidth() /
                                      std::max(1.0f, renderer->GetDesignHeight()));
        activeCam = &fallbackCam;
    }

    const EP7Color4& bg = ColorIsNearlyBlack(m_baseLighting.fog.fogColor)
        ? m_baseLighting.fog.fogColorDay
        : m_baseLighting.fog.fogColor;
    renderer->SetClearColor(bg.r, bg.g, bg.b, 1.0f);

    const WorldLightingState frameLighting = BuildFrameLighting(
        activeCam->GetTargetX(), activeCam->GetTargetY(), activeCam->GetTargetZ());

    if (!renderer->BeginWorldPass(*activeCam, frameLighting)) {
        return;
    }

    float eyeX = 0.0f, eyeY = 0.0f, eyeZ = 0.0f;
    activeCam->GetEyePosition(eyeX, eyeY, eyeZ);

    // Pass 1: Sky dome centered on camera eye (when enabled for outdoor maps)
    if (m_skyMeshHandle != 0 && m_skyTexId != 0 && m_world.sky.skyEnable) {
        EP7Mat4 skyMat = EP7Mat4::Identity();
        const float skyRadius = activeCam->GetFarZ() * 0.7f;
        skyMat.m[0] = skyRadius;
        skyMat.m[5] = skyRadius;
        skyMat.m[10] = skyRadius;
        skyMat.m[12] = eyeX;
        skyMat.m[13] = eyeY - skyRadius * 0.12f;
        skyMat.m[14] = eyeZ;
        renderer->DrawMesh3D(m_skyMeshHandle, m_skyTexId, skyMat, EP7AABB{},
                             EP7AlphaBucket::Opaque, true, 0.0f,
                             {1.0f, 1.0f, 1.0f, 1.0f}, false, true, true);
    }

    struct PendingSoftDraw {
        float distSq = 0.0f;
        const BatchDrawBinding* batch = nullptr;
        const EP7Mat4* model = nullptr;
        EP7AABB worldBounds;
    };
    std::vector<PendingSoftDraw> softDraws;
    const EP7Mat4 identity = EP7Mat4::Identity();

    auto drawOrQueueBatch = [&](const BatchDrawBinding& b, const EP7Mat4& model, const EP7AABB& wb) {
        if (IsSoftAlphaBucket(b.bucket)) {
            if (wb.valid && !renderer->IsBoxInFrustum(wb.min.x, wb.min.y, wb.min.z,
                                                      wb.max.x, wb.max.y, wb.max.z)) {
                return;
            }
            const EP7Vec3 c = wb.Center();
            const float dx = c.x - eyeX;
            const float dy = c.y - eyeY;
            const float dz = c.z - eyeZ;
            softDraws.push_back({dx * dx + dy * dy + dz * dz, &b, &model, wb});
            return;
        }
        renderer->DrawMesh3D(b.meshHandle, b.texId, model, wb, b.bucket,
                             IsTwoSidedBucket(b.bucket, b.twoSided),
                             AlphaCutoffForBucket(b.bucket), b.tint, true, false, false);
    };

    // Pass 2 & 3: Opaque + HardAlpha terrain batches
    for (const BatchDrawBinding& b : m_terrainDraws) {
        if (!IsSoftAlphaBucket(b.bucket) && b.bucket == EP7AlphaBucket::Opaque) {
            drawOrQueueBatch(b, identity, b.bounds);
        }
    }
    for (const EP7PieceInstance& inst : m_world.pieceInstances) {
        if (inst.assetIndex < 0 || static_cast<size_t>(inst.assetIndex) >= m_pieceAssetGpus.size()) continue;
        const PieceAssetGpu& gpuAsset = m_pieceAssetGpus[static_cast<size_t>(inst.assetIndex)];
        if (inst.worldBounds.valid &&
            !renderer->IsBoxInFrustum(inst.worldBounds.min.x, inst.worldBounds.min.y, inst.worldBounds.min.z,
                                      inst.worldBounds.max.x, inst.worldBounds.max.y, inst.worldBounds.max.z)) {
            renderer->RecordCulledObjects(static_cast<uint32_t>(gpuAsset.batches.size()));
            continue;
        }
        for (const BatchDrawBinding& b : gpuAsset.batches) {
            if (b.bucket == EP7AlphaBucket::Opaque) {
                drawOrQueueBatch(b, inst.worldMatrix, inst.worldBounds);
            }
        }
    }

    // Hard-alpha cutout pass
    for (const BatchDrawBinding& b : m_terrainDraws) {
        if (b.bucket == EP7AlphaBucket::HardAlpha) {
            drawOrQueueBatch(b, identity, b.bounds);
        } else if (IsSoftAlphaBucket(b.bucket)) {
            drawOrQueueBatch(b, identity, b.bounds);
        }
    }
    for (const EP7PieceInstance& inst : m_world.pieceInstances) {
        if (inst.assetIndex < 0 || static_cast<size_t>(inst.assetIndex) >= m_pieceAssetGpus.size()) continue;
        if (inst.worldBounds.valid &&
            !renderer->IsBoxInFrustum(inst.worldBounds.min.x, inst.worldBounds.min.y, inst.worldBounds.min.z,
                                      inst.worldBounds.max.x, inst.worldBounds.max.y, inst.worldBounds.max.z)) {
            continue;
        }
        const PieceAssetGpu& gpuAsset = m_pieceAssetGpus[static_cast<size_t>(inst.assetIndex)];
        for (const BatchDrawBinding& b : gpuAsset.batches) {
            if (b.bucket == EP7AlphaBucket::HardAlpha || IsSoftAlphaBucket(b.bucket)) {
                drawOrQueueBatch(b, inst.worldMatrix, inst.worldBounds);
            }
        }
    }

    // Pass 4: Soft-alpha transparent draws sorted back-to-front
    std::sort(softDraws.begin(), softDraws.end(), [](const PendingSoftDraw& a, const PendingSoftDraw& b) {
        return a.distSq > b.distSq;
    });
    for (const PendingSoftDraw& sd : softDraws) {
        renderer->DrawMesh3D(sd.batch->meshHandle, sd.batch->texId, *sd.model, sd.worldBounds,
                             sd.batch->bucket, true, AlphaCutoffForBucket(sd.batch->bucket),
                             sd.batch->tint, false, false, true);
    }

    renderer->EndWorldPass();
}

void EP7SceneRenderer::UnloadWorldResources(MobileRenderer* renderer) {
    if (renderer) {
        for (uint32_t meshId : m_registeredMeshes) {
            renderer->UnregisterMesh3D(meshId);
        }
        for (const std::string& key : m_loadedWorldTextureKeys) {
            renderer->UnloadAssetTexture(key);
        }
    }
    m_registeredMeshes.clear();
    m_loadedWorldTextureKeys.clear();
    m_terrainDraws.clear();
    m_pieceAssetGpus.clear();
    m_skyMeshHandle = 0;
    m_skyTexId = 0;
    m_world = EP7WorldData{};
    m_levelFull = EP7LevelFullData{};
    m_currentWorldFile.clear();
    m_baseLighting = WorldLightingState{};
    m_loadedTextureCount = 0;
    m_missingTextureCount = 0;
    m_hasWorld3D = false;
}

void EP7SceneRenderer::Unload(MobileRenderer* renderer) {
    UnloadWorldResources(renderer);
    if (renderer && !m_previewTextureKey.empty()) renderer->UnloadAssetTexture(m_previewTextureKey);
    m_index = EP7SceneIndex{};
    m_assetReport = AssetPipelineReport{};
    m_root.clear();
    m_previewTextureKey.clear();
    m_hasPreview = false;
    m_alphaCutoff = 0.0f;
    m_status = "NO SCENE DATA LOADED";
}

} // namespace RANMobile
