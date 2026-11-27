#include "EP7CharacterScene.h"
#include "../../Data/Importer/EP7TextureDecoder.h"

#include <chrono>
#include <cmath>
#include <cctype>
#include <cstring>

namespace RANMobile {
namespace {

// Character pieces are indexed by EPIECECHAR; the .chf order and the piece order agree.
inline size_t PieceIndex(EP7PieceType type) {
    const int32_t index = static_cast<int32_t>(type);
    if (index < 0 || index >= static_cast<int32_t>(kEP7PieceSize)) return 0;
    return static_cast<size_t>(index);
}

std::string ToLower(std::string value) {
    for (auto& ch : value) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    }
    return value;
}

}  // namespace

// ---------------------------------------------------------------------------
// EP7CharacterLoader
// ---------------------------------------------------------------------------

bool EP7CharacterLoader::Resolve(const std::string& name, std::vector<uint8_t>& out) const {
    out.clear();
    if (!m_resolver) return false;
    return m_resolver(m_userData, name, out);
}

EP7CharacterAsset EP7CharacterLoader::LoadCharacter(const std::vector<uint8_t>& fileBytes, bool compByteEncoded) {
    auto asset = std::make_shared<EP7CharacterAsset>();

    asset->file = EP7CharacterFileReader::Parse(fileBytes, compByteEncoded);
    if (!asset->file.valid) {
        asset->error = "chf: " + asset->file.error;
        return *asset;
    }

    // --- skeleton -----------------------------------------------------------
    if (asset->file.skeleton.empty()) {
        asset->error = "chf names no skeleton file";
        return *asset;
    }

    std::vector<uint8_t> skeletonBytes;
    if (!Resolve(asset->file.skeleton, skeletonBytes) || skeletonBytes.empty()) {
        asset->error = "skeleton asset unavailable: " + asset->file.skeleton;
        return *asset;
    }

    auto skeleton = std::make_shared<EP7Skeleton>();
    if (!skeleton->LoadFromXFile(skeletonBytes)) {
        asset->error = "skeleton parse failed (" + asset->file.skeleton + "): " + skeleton->Error();
        return *asset;
    }
    asset->skeleton = skeleton;
    asset->boneCount = static_cast<uint32_t>(skeleton->BoneCount());

    // --- pieces -------------------------------------------------------------
    asset->pieces.resize(kEP7PieceSize);
    for (uint32_t slot = 0; slot < kEP7PieceSize; ++slot) {
        const std::string& pieceName = asset->file.pieces[slot];
        EP7CharacterPiece& piece = asset->pieces[slot];
        if (pieceName.empty()) continue;

        std::vector<uint8_t> pieceBytes;
        if (!Resolve(pieceName, pieceBytes) || pieceBytes.empty()) {
            piece.error = "piece asset unavailable: " + pieceName;
            continue;
        }

        // ParseFromBytes consumes the basestream header (ReadFileType) first, which is
        // what CSerialFile/CSerialMemory do when the engine opens the .cps.
        piece.info = EP7SkinPieceReader::ParseFromBytes(pieceBytes, compByteEncoded);
        if (!piece.info.valid) {
            piece.error = "cps parse failed (" + pieceName + "): " + piece.info.error;
            continue;
        }

        // Piece type must agree with the slot it was named in, otherwise the .chf is
        // inconsistent and we refuse to silently rename it.
        if (PieceIndex(piece.info.pieceType) != slot) {
            piece.error = "cps piece type disagrees with .chf slot: " + pieceName;
            continue;
        }

        std::vector<uint8_t> meshBytes;
        if (!Resolve(piece.info.xFileName, meshBytes) || meshBytes.empty()) {
            piece.error = "mesh asset unavailable: " + piece.info.xFileName;
            continue;
        }

        // The .cps skeleton reference must match the character skeleton (FG_BONEFILE_CHECK
        // in DxSkinCharData::SetPiece compares the two names case-insensitively).
        if (ToLower(piece.info.skeletonFile) != ToLower(asset->file.skeleton)) {
            piece.error = "cps skeleton mismatch: " + piece.info.skeletonFile + " vs " + asset->file.skeleton;
            continue;
        }

        piece.mesh = EP7SkinnedMeshReader::Parse(meshBytes, piece.info.meshName, skeleton.get());
        if (!piece.mesh.valid) {
            piece.error = "mesh parse failed (" + piece.info.xFileName + "): " + piece.mesh.error;
            continue;
        }

        piece.loaded = true;
        // B18: build the per-vertex GPU skinning streams once at load (DxSkinMesh9_HLSL
        // BLENDWEIGHT/BLENDINDICES FLOAT4 parity; same influence rules as SkinVerticesCPU).
        piece.gpuStreams = BuildGpuSkinStreams(piece.mesh);
        ++asset->pieceCount;
        asset->meshVertexCount += piece.mesh.vertexCount;
        asset->meshTriangleCount += static_cast<uint32_t>(piece.mesh.faces.size() / 3);
        asset->skinBoneCount += static_cast<uint32_t>(piece.mesh.bones.size());
        if (piece.mesh.geometryDecoded) ++asset->decodedGeometryPieces;
    }

    // --- animations ---------------------------------------------------------
    asset->clips.resize(asset->file.animations.size());
    asset->clipInfos.resize(asset->file.animations.size());
    for (size_t i = 0; i < asset->file.animations.size(); ++i) {
        const std::string& name = asset->file.animations[i];

        // DxSkinAniMan::CreateAnimationData swaps ".x" for ".bin" (and the .cfg next to it).
        std::string base = name;
        const size_t dot = base.find_last_of('.');
        if (dot != std::string::npos) base = base.substr(0, dot);

        std::vector<uint8_t> binBytes;
        if (!Resolve(base + ".bin", binBytes) || binBytes.empty()) continue;

        asset->clips[i] = EP7AnimationReader::ParseContainer(binBytes, true, compByteEncoded);
        if (!asset->clips[i].valid) continue;
        EP7AnimationReader::BindToSkeleton(asset->clips[i], *skeleton);
        // SAnimation::Load_* variants run ConvertMatToQuat right after reading a track
        // (USE_ANI_QUATERNION): matrix-only tracks become quatPos tracks, tracks with
        // quatPos keys keep their matrix keys. The byte-level reader stays untouched —
        // this is the runtime load transform, not a parser change.
        EP7AnimationReader::ConvertMatToQuat(asset->clips[i]);

        std::vector<uint8_t> cfgBytes;
        if (Resolve(base + ".cfg", cfgBytes) && !cfgBytes.empty()) {
            asset->clipInfos[i] = EP7AnimationReader::ParseInfo(cfgBytes, true, compByteEncoded);
        }
        ++asset->animationCount;
    }

    asset->loaded = (asset->pieceCount > 0);
    if (!asset->loaded && asset->error.empty()) {
        asset->error = "no piece could be loaded";
    }
    return *asset;
}

// ---------------------------------------------------------------------------
// EP7CharacterScene
// ---------------------------------------------------------------------------

bool EP7CharacterScene::RegisterAsset(const AssetPtr& asset) {
    if (!asset) {
        m_lastError = "null character asset";
        return false;
    }
    if (!asset->loaded || !asset->skeleton || !asset->skeleton->IsValid()) {
        m_lastError = "character asset is not loaded: " + asset->error;
        return false;
    }
    return true;
}

uint32_t EP7CharacterScene::Spawn(const AssetPtr& asset, float x, float y, float z, float yawDegrees) {
    if (!RegisterAsset(asset)) return 0;

    auto instance = std::make_unique<EP7CharacterInstance>();
    instance->asset = asset;
    instance->poseSkeleton = std::make_unique<EP7Skeleton>(*asset->skeleton);
    instance->piecePalettes.resize(kEP7PieceSize);
    instance->position[0] = x;
    instance->position[1] = y;
    instance->position[2] = z;
    instance->yawDegrees = yawDegrees;
    instance->scale = asset->file.scale > 0.0f ? asset->file.scale : 1.0f;
    instance->id = m_nextId++;

    // B17: register every valid clip in the instance's animation registry
    // (DxSkinAniControl::LoadAnimation + ClassifyAnimation) and play the default
    // clip (DEFAULTANI: m_vecAnim[0]) so the character is posed like the original.
    for (size_t i = 0; i < asset->clips.size(); ++i) {
        const EP7AnimClip& clip = asset->clips[i];
        if (!clip.valid) continue;
        const EP7AnimInfo* info = asset->clipInfos[i].valid ? &asset->clipInfos[i] : nullptr;
        instance->aniControl.AddAnimation(&clip, info, asset->file.animations[i]);
    }
    instance->aniControl.DefaultAnimation();
    instance->currentClip = instance->aniControl.CurrentClipIndex();

    m_instances.push_back(std::move(instance));
    return m_instances.back()->id;
}

bool EP7CharacterScene::Destroy(uint32_t instanceId) {
    for (auto it = m_instances.begin(); it != m_instances.end(); ++it) {
        if ((*it)->id == instanceId) {
            m_instances.erase(it);
            if (m_previewTarget == instanceId) m_previewTarget = 0;
            return true;
        }
    }
    return false;
}

void EP7CharacterScene::Clear() {
    ReleaseGpuResources();
    m_instances.clear();
    m_previewTarget = 0;
    m_nextId = 1;
}

EP7CharacterInstance* EP7CharacterScene::Find(uint32_t instanceId) {
    for (auto& instance : m_instances) {
        if (instance->id == instanceId) return instance.get();
    }
    return nullptr;
}

bool EP7CharacterScene::PlayAnimation(uint32_t instanceId, const std::string& clipName) {
    EP7CharacterInstance* instance = Find(instanceId);
    if (!instance || !instance->asset) return false;

    const auto& names = instance->asset->file.animations;
    const std::string wanted = ToLower(clipName);
    for (size_t i = 0; i < names.size(); ++i) {
        if (ToLower(names[i]) == wanted) return PlayAnimationIndex(instanceId, static_cast<int32_t>(i));
    }
    return false;
}

bool EP7CharacterScene::PlayAnimationIndex(uint32_t instanceId, int32_t clipIndex) {
    EP7CharacterInstance* instance = Find(instanceId);
    if (!instance || !instance->asset) return false;
    if (clipIndex < 0 || static_cast<size_t>(clipIndex) >= instance->asset->clips.size()) return false;

    const EP7AnimClip& clip = instance->asset->clips[static_cast<size_t>(clipIndex)];
    if (!clip.valid) return false;

    // B17: route through the DxSkinAniControl state machine (restart/blend semantics).
    if (!instance->aniControl.SelectAnimationIndex(clipIndex)) return false;
    instance->currentClip = instance->aniControl.CurrentClipIndex();
    return true;
}

bool EP7CharacterScene::SelectAnimationByType(uint32_t instanceId, int32_t mainType, int32_t subType,
                                              uint32_t select) {
    // DxSkinAniControl::SELECTANI(MType, SType, dwFlags, dwSelect).
    EP7CharacterInstance* instance = Find(instanceId);
    if (!instance) return false;
    const bool ok = instance->aniControl.SelectAnimation(mainType, subType, select);
    instance->currentClip = instance->aniControl.CurrentClipIndex();
    return ok;
}

void EP7CharacterScene::DefaultAnimation(uint32_t instanceId) {
    EP7CharacterInstance* instance = Find(instanceId);
    if (!instance) return;
    instance->aniControl.DefaultAnimation();
    instance->currentClip = instance->aniControl.CurrentClipIndex();
}

void EP7CharacterScene::DoShockMix(uint32_t instanceId) {
    EP7CharacterInstance* instance = Find(instanceId);
    if (!instance) return;
    instance->aniControl.DoShockMix();
}

bool EP7CharacterScene::IsAnimationEnded(uint32_t instanceId) const {
    for (const auto& instance : m_instances) {
        if (instance->id == instanceId) return instance->aniControl.CurrentEnded();
    }
    return false;
}

void EP7CharacterScene::Update(float deltaSeconds) {
    for (auto& instance : m_instances) {
        if (!instance->visible) continue;
        // B17: the authentic DxSkinAniControl::FrameMove drives all controls
        // (clocks, end flags, blend layers, mix overlay clock).
        instance->aniControl.FrameMove(deltaSeconds);
        instance->animationEnded = instance->aniControl.CurrentEnded();
        float skinMs = 0.0f;
        EvaluatePose(*instance, &skinMs);
        instance->lastSkinMs = skinMs;
    }
}

bool EP7CharacterScene::EvaluatePose(EP7CharacterInstance& instance, float* outSkinMs) {
    if (!instance.asset || !instance.poseSkeleton) return false;

    const auto t0 = std::chrono::high_resolution_clock::now();

    EP7Skeleton& skeleton = *instance.poseSkeleton;

    // 1. DxSkinAniControl::Render pose pipeline (DxSkinAniControl.cpp:814-860):
    //    ResetBone -> UpdateTime(weight) [bone scale applied at its tail] ->
    //    UpdateMix -> skeleton UpdateBones(matRot) with g_fWeight.
    skeleton.ResetToRestPose();
    instance.aniControl.UpdateTime(skeleton);
    instance.aniControl.UpdateMix(skeleton);

    // 2. Accumulate world transforms (DxSkeleton::UpdateBones), passing the blend
    //    weight (DxSkeleton::g_fWeight) for USE_ANI_BLENDING bones.
    ep7math::Mat4 worldRoot;
    const float yawRad = instance.yawDegrees * 3.14159265358979323846f / 180.0f;
    ep7math::Mat4 rot = ep7math::RotationQuaternion(
        ep7math::Quat(0.0f, std::sin(yawRad * 0.5f), 0.0f, std::cos(yawRad * 0.5f)));
    ep7math::Mat4 scale = ep7math::Scaling(instance.scale, instance.scale, instance.scale);
    ep7math::Mat4 translate = ep7math::Translation(instance.position[0], instance.position[1], instance.position[2]);
    worldRoot = ep7math::Multiply(ep7math::Multiply(scale, rot), translate);

    skeleton.UpdateBones(worldRoot, instance.aniControl.Weight());

    // 3. Skin every loaded piece with the posed skeleton (CPU reference path).
    instance.skinnedPositions.clear();
    instance.skinnedNormals.clear();
    instance.pieceVertexOffsets.assign(kEP7PieceSize, 0);
    instance.drawCalls = 0;
    instance.triangles = 0;
    instance.poseValid = false;

    // B17: scratch buffers are instance-owned; std::vector::resize keeps their
    // capacity, so the per-frame pose path performs no heap allocations.
    std::vector<ep7math::Mat4>& palette = instance.skinPaletteScratch;
    for (uint32_t slot = 0; slot < kEP7PieceSize; ++slot) {
        EP7CharacterPiece& piece = instance.asset->pieces[slot];
        if (!piece.loaded) continue;

        BuildBonePalette(piece.mesh, skeleton, palette);

        std::vector<float>& positions = instance.skinPosScratch;
        std::vector<float>& normals = instance.skinNormScratch;
        if (!SkinVerticesCPU(piece.mesh, palette, positions, normals)) continue;

        instance.pieceVertexOffsets[slot] = static_cast<uint32_t>(instance.skinnedPositions.size() / 3);
        instance.skinnedPositions.insert(instance.skinnedPositions.end(), positions.begin(), positions.end());
        instance.skinnedNormals.insert(instance.skinnedNormals.end(), normals.begin(), normals.end());

        // B18: stash the palette for the GPU skinned draw (offset*combined per piece
        // bone; consumed by DrawGpuSkinnedMesh through the shared uniform block).
        instance.piecePalettes[slot].assign(palette.begin(), palette.end());

        // One draw call per material subset; the EP7 piece always has >= 1 material.
        instance.drawCalls += static_cast<uint32_t>(piece.mesh.subsets.empty() ? 1 : piece.mesh.subsets.size());
        instance.triangles += static_cast<uint32_t>(piece.mesh.faces.size() / 3);
        instance.poseValid = true;
    }

    const auto t1 = std::chrono::high_resolution_clock::now();
    if (outSkinMs) {
        *outSkinMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
    }
    return instance.poseValid;
}

void EP7CharacterScene::UpdateBounds(EP7CharacterInstance& instance) const {
    // DxSkinCharData stores m_vBoundBoxMax / m_vBoundBoxMin (defaults +-(7,20,7) with the
    // min y at 0 and height 20). Scale them by the character scale and translate to the
    // instance position; the animation is allowed to move a few centimetres outside the
    // bind-pose box, so the box is inflated slightly (documented conservative padding).
    float minX = -7.0f, minY = 0.0f, minZ = -7.0f;
    float maxX = 7.0f, maxY = 20.0f, maxZ = 7.0f;
    if (instance.asset && instance.asset->file.hasBoundingBox) {
        minX = instance.asset->file.boundsMin[0];
        minY = instance.asset->file.boundsMin[1];
        minZ = instance.asset->file.boundsMin[2];
        maxX = instance.asset->file.boundsMax[0];
        maxY = instance.asset->file.boundsMax[1];
        maxZ = instance.asset->file.boundsMax[2];
    }
    if (instance.asset && instance.asset->file.hasHeightRadius) {
        maxY = instance.asset->file.height;
    }

    const float pad = 0.5f;
    instance.boundsMin[0] = instance.position[0] + minX * instance.scale - pad;
    instance.boundsMin[1] = instance.position[1] + minY * instance.scale - pad;
    instance.boundsMin[2] = instance.position[2] + minZ * instance.scale - pad;
    instance.boundsMax[0] = instance.position[0] + maxX * instance.scale + pad;
    instance.boundsMax[1] = instance.position[1] + maxY * instance.scale + pad;
    instance.boundsMax[2] = instance.position[2] + maxZ * instance.scale + pad;
}

uint32_t EP7CharacterScene::CullInstances(const MobileCamera& camera, const MobileRenderer* renderer) {
    (void)camera;  // the renderer already owns the view distance used by the frustum test

    uint32_t visible = 0;
    for (auto& instance : m_instances) {
        if (!instance->poseValid) {
            instance->inFrustum = false;
            continue;
        }
        UpdateBounds(*instance);

        if (renderer) {
            instance->inFrustum = renderer->IsBoxInFrustum(
                instance->boundsMin[0], instance->boundsMin[1], instance->boundsMin[2],
                instance->boundsMax[0], instance->boundsMax[1], instance->boundsMax[2]);
        } else {
            instance->inFrustum = true;
        }

        if (instance->inFrustum) ++visible;
    }
    return visible;
}

EP7CharacterDiagnostics EP7CharacterScene::GetDiagnostics() const {
    EP7CharacterDiagnostics diag;
    diag.characters = static_cast<uint32_t>(m_instances.size());
    diag.cpuSkinMs = 0.0f;
    diag.fps = m_lastFps;
    (void)m_lastFrameMs;

    for (const auto& instance : m_instances) {
        if (!instance->asset) {
            diag.authenticAssetsOnly = false;
            continue;
        }
        if (instance->visible) ++diag.visibleCharacters;
        diag.totalBones += instance->asset->boneCount;
        diag.totalVertices += instance->asset->meshVertexCount;
        diag.totalTriangles += instance->asset->meshTriangleCount;
        diag.animationCount += instance->asset->animationCount;
        diag.decodedGeometryPieces += instance->asset->decodedGeometryPieces;
        diag.drawCalls += instance->drawCalls;
        diag.cpuSkinMs += instance->lastSkinMs;

        if (!instance->asset->loaded || !instance->asset->skeleton) diag.authenticAssetsOnly = false;

        if (instance->aniControl.UpBodyActive() || instance->currentClip >= 0) {
            const EP7AnimClip* clip = instance->aniControl.CurrentClip();
            const size_t activeIndex =
                (instance->currentClip >= 0) ? static_cast<size_t>(instance->currentClip)
                                             : (instance->asset ? instance->asset->clips.size() : 0);
            if (!clip && instance->asset && activeIndex < instance->asset->clips.size()) {
                clip = &instance->asset->clips[activeIndex];
            }
            if (clip) {
                auto countAnimated = [](const std::vector<EP7BoneTrack>& tracks) {
                    uint32_t n = 0;
                    for (const auto& track : tracks) {
                        if (track.boneIndex >= 0) ++n;
                    }
                    return n;
                };
                diag.animatedBones += countAnimated(clip->mainTracks) + countAnimated(clip->upBodyTracks);
            }
        }

        if (diag.lastError.empty() && !instance->asset->error.empty()) diag.lastError = instance->asset->error;
    }

    if (diag.lastError.empty()) diag.lastError = m_lastError;
    return diag;
}

// ---------------------------------------------------------------------------
// B18: GPU skinning + texture binding integration.
// ---------------------------------------------------------------------------

void EP7CharacterScene::ReleaseGpuResources() {
    if (m_renderer) {
        for (uint32_t handle : m_uploadedGpuMeshes) {
            m_renderer->ReleaseGpuSkinnedMesh(handle);
        }
        for (const auto& kv : m_textureCache) {
            m_renderer->DeleteTexture(kv.second);
        }
    }
    m_uploadedGpuMeshes.clear();
    m_textureCache.clear();
}

uint32_t EP7CharacterScene::EnsurePieceGpuHandle(const AssetPtr& asset, uint32_t slot) {
    if (!asset || slot >= kEP7PieceSize || !m_renderer) return 0;
    EP7CharacterPiece& piece = asset->pieces[slot];
    if (!piece.loaded || !piece.gpuStreams.valid || piece.mesh.faces.empty()) return 0;
    if (piece.gpuMeshHandle != 0) return piece.gpuMeshHandle;

    // Interleave pos(3) + normal(3) + uv(2) + boneIdx(4) + boneWt(4) = 16 floats per
    // vertex — the authentic DxSkinMesh9_HLSL.cpp:95-106 declaration (without the
    // tangent stream, which no B18 draw consumes).
    const size_t vertexFloats = static_cast<size_t>(piece.mesh.vertexCount) * 16;
    std::vector<float> interleaved(vertexFloats);
    const bool hasNormals = piece.mesh.normals.size() >= static_cast<size_t>(piece.mesh.vertexCount) * 3;
    const bool hasUV = piece.mesh.texcoords.size() >= static_cast<size_t>(piece.mesh.vertexCount) * 2;
    for (uint32_t v = 0; v < piece.mesh.vertexCount; ++v) {
        float* d = interleaved.data() + static_cast<size_t>(v) * 16;
        d[0] = piece.mesh.positions[v * 3 + 0];
        d[1] = piece.mesh.positions[v * 3 + 1];
        d[2] = piece.mesh.positions[v * 3 + 2];
        d[3] = hasNormals ? piece.mesh.normals[v * 3 + 0] : 0.0f;
        d[4] = hasNormals ? piece.mesh.normals[v * 3 + 1] : 1.0f;
        d[5] = hasNormals ? piece.mesh.normals[v * 3 + 2] : 0.0f;
        d[6] = hasUV ? piece.mesh.texcoords[v * 2 + 0] : 0.0f;
        d[7] = hasUV ? piece.mesh.texcoords[v * 2 + 1] : 0.0f;
        for (int k = 0; k < 4; ++k) {
            d[8 + k] = piece.gpuStreams.boneIndices[static_cast<size_t>(v) * 4 + k];
            d[12 + k] = piece.gpuStreams.boneWeights[static_cast<size_t>(v) * 4 + k];
        }
    }

    RANMobile::GpuSkinnedMeshData data;
    data.interleaved = interleaved.data();
    data.vertexCount = piece.mesh.vertexCount;
    data.indices = piece.mesh.faces.data();
    data.indexCount = static_cast<uint32_t>(piece.mesh.faces.size());
    const uint32_t handle = m_renderer->UploadGpuSkinnedMesh(data);
    if (handle != 0) m_uploadedGpuMeshes.push_back(handle);
    piece.gpuMeshHandle = handle;
    return handle;
}

uint32_t EP7CharacterScene::ResolveTextureLocked(const std::string& textureName) {
    if (!m_renderer || textureName.empty()) return 0;

    // Resolution ladder (TextureManager::LoadTexture + IsEncrypt parity): exact name,
    // stem + ".mtf", name + ".mtf".  Cached by lowercase key like the original manager.
    std::vector<std::string> candidates;
    candidates.push_back(textureName);
    const size_t dot = textureName.find_last_of('.');
    if (dot != std::string::npos) {
        candidates.push_back(textureName.substr(0, dot) + ".mtf");
    }
    candidates.push_back(textureName + ".mtf");

    for (const std::string& candidate : candidates) {
        std::string lower;
        lower.reserve(candidate.size());
        for (char c : candidate) {
            lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        const auto cached = m_textureCache.find(lower);
        if (cached != m_textureCache.end()) return cached->second;
        if (!m_textureResolver) continue;

        std::vector<uint8_t> bytes;
        if (!m_textureResolver(m_textureResolverData, candidate, bytes) || bytes.empty()) continue;

        // CMemoryTexture parity: a ".mtf" name decrypts before decoding
        // (MemoryTexture.cpp IsEncrypt / DecryptTexture: (b + 3) ^ 0x47 body after
        // the 12-byte {version,size,fileType} header).
        const bool encrypted = lower.find(".mtf") != std::string::npos;
        std::vector<uint8_t> raw;
        int fileType = -1;
        if (encrypted && EP7TextureDecoder::DecryptMTF(bytes, raw, fileType)) {
            bytes.swap(raw);
        } else if (encrypted) {
            continue;  // undecryptable .mtf: not fabricated
        }

        DecodedTexture tex;
        if (bytes.size() >= 4 && std::memcmp(bytes.data(), "DDS ", 4) == 0) {
            tex = EP7TextureDecoder::DecodeDDS(bytes);
        } else if (fileType == 1) {
            // TEXTURE_TGA branch of the .mtf container (MemoryTexture.cpp EncryptTexture).
            tex = EP7TextureDecoder::DecodeTGA(bytes);
        } else {
            tex = EP7TextureDecoder::DecodeTGA(bytes);  // plain .tga by content
            if (!tex.valid) tex = EP7TextureDecoder::DecodeDDS(bytes);
        }
        if (!tex.valid) continue;

        const uint32_t texId = m_renderer->UploadRGBA8Texture(tex.width, tex.height, tex.rgba8.data());
        if (texId != 0) {
            m_textureCache[lower] = texId;
        }
        return texId;
    }
    return 0;
}

uint32_t EP7CharacterScene::GetPieceTextureId(const AssetPtr& asset, uint32_t slot, uint32_t subsetIndex) {
    if (!asset || slot >= kEP7PieceSize) return 0;
    const EP7CharacterPiece& piece = asset->pieces[slot];
    if (!piece.loaded) return 0;

    // Authentic association (DxSkinPieceContainer.cpp:82 SMATERIAL_PIECE::LoadTexture):
    // the .cps material texture name is the binding key; the .x MeshMaterialList name is
    // the fallback (the .cps materials array corresponds 1:1 with the mesh's attribute
    // groups — DxCharPart::SetPart copies SMATERIAL_PIECE[m_dwMaterialNum] verbatim).
    std::string name;
    if (subsetIndex < piece.info.materials.size() && !piece.info.materials[subsetIndex].texture.empty()) {
        name = piece.info.materials[subsetIndex].texture;
    } else if (subsetIndex < piece.mesh.subsets.size()) {
        name = piece.mesh.subsets[subsetIndex].texture;
    }
    if (name.empty() || name.rfind("material_", 0) == 0) return 0;  // synthetic fallback name
    return ResolveTextureLocked(name);
}

bool EP7CharacterScene::GetPiecePalette(const EP7CharacterInstance& instance, uint32_t slot,
                                        const float** outPtr, uint32_t* outCount) const {
    if (slot >= instance.piecePalettes.size()) return false;
    const auto& palette = instance.piecePalettes[slot];
    if (palette.empty()) return false;
    *outPtr = reinterpret_cast<const float*>(palette.data());
    *outCount = static_cast<uint32_t>(palette.size());
    return true;
}

}  // namespace RANMobile
