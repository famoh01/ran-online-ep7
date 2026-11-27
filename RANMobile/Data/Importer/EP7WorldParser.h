#pragma once

#include "EP7SceneData.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace RANMobile {

struct EP7Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct EP7Color4 {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct EP7Mat4 {
    std::array<float, 16> m = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    static EP7Mat4 Identity();
    static EP7Mat4 Multiply(const EP7Mat4& a, const EP7Mat4& b);
};

struct EP7AABB {
    EP7Vec3 min{0.0f, 0.0f, 0.0f};
    EP7Vec3 max{0.0f, 0.0f, 0.0f};
    bool valid = false;

    void Expand(const EP7Vec3& p);
    void ExpandAABB(const EP7AABB& other);
    EP7Vec3 Center() const;
    float BoundingRadius() const;
    static EP7AABB Transform(const EP7Mat4& mat, const EP7AABB& local);
};

// Unified interleaved 3D vertex format (36 bytes):
//   - OCTREEVERTEX (0x112): pos, normal, uv, color=0xFFFFFFFF
//   - VERTEXCOLORTEX (0x142): pos, normal=(0,1,0), uv, D3DCOLOR swizzled BGRA->RGBA
//   - VERTEXNORCOLORTEX (0x152): pos, normal, uv, D3DCOLOR swizzled BGRA->RGBA
struct EP7WorldVertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float nx = 0.0f;
    float ny = 1.0f;
    float nz = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    uint32_t rgba = 0xFFFFFFFFu; // little-endian R, G, B, A bytes
};

enum class EP7AlphaBucket : uint8_t {
    Opaque = 0,       // m_mapMesh / MATERIAL_NORMAL
    HardAlpha = 1,    // m_mapMeshALPHA / MATERIAL_ALPHA_HARD
    SoftAlpha = 2,    // m_mapMeshSOFTALPHA / MATERIAL_ALPHA_SOFT
    SoftAlpha01 = 3,  // m_mapMeshSOFTALPHA01
    SoftAlpha02 = 4   // m_mapMeshSOFTALPHA02
};

struct EP7MeshBatch {
    std::string textureName;
    EP7AlphaBucket bucket = EP7AlphaBucket::Opaque;
    EP7AABB bounds;
    EP7Color4 materialDiffuse{1.0f, 1.0f, 1.0f, 1.0f};
    EP7Color4 materialAmbient{1.0f, 1.0f, 1.0f, 1.0f};
    std::vector<EP7WorldVertex> vertices;
    std::vector<uint16_t> indices;
    bool twoSided = false;
};

struct EP7OctreeNode {
    EP7AABB bounds;
    std::vector<uint32_t> batchIndices;
    int32_t leftChild = -1;
    int32_t rightChild = -1;
};

struct EP7PieceAsset {
    std::string filename;
    bool isPie = false;
    bool valid = false;
    EP7AABB localBounds;
    std::vector<EP7MeshBatch> batches;
    std::vector<std::string> landEffects;
    size_t bytesConsumed = 0;
    std::string error;
};

struct EP7PieceInstance {
    std::string pieceFile;
    EP7Mat4 worldMatrix = EP7Mat4::Identity();
    EP7AABB worldBounds;
    EP7Vec3 center{0.0f, 0.0f, 0.0f};
    float boundingRadius = 0.0f;
    int32_t assetIndex = -1; // -1 if .pis/.pie asset not loaded
    bool fromPieManager = false;
};

enum class EP7LightType : uint32_t {
    Point = 1,
    Spot = 2,
    Directional = 3
};

struct EP7LightInfo {
    std::string name;
    EP7LightType type = EP7LightType::Point;
    bool enabled = true;
    EP7Color4 diffuse{1.0f, 1.0f, 1.0f, 1.0f};
    EP7Color4 ambient{0.2f, 0.2f, 0.2f, 1.0f};
    EP7Vec3 position{0.0f, 0.0f, 0.0f};
    EP7Vec3 direction{0.0f, -1.0f, 0.0f};
    float range = 100.0f;
    float attenuation0 = 1.0f;
    float attenuation1 = 0.01f;
    float attenuation2 = 0.0f;
};

struct EP7FogInfo {
    bool enabled = true;
    bool skyFogEnable = false;
    bool staticRange = false;
    float fogStart = 450.0f;
    float fogEnd = 1200.0f;
    EP7Color4 fogColor{0.55f, 0.65f, 0.78f, 1.0f};
    EP7Color4 fogColorDay{0.55f, 0.65f, 0.78f, 1.0f};
    EP7Color4 fogColorNight{0.10f, 0.14f, 0.24f, 1.0f};
};

struct EP7SkyInfo {
    bool skyEnable = false;
    bool cloudEnable = false;
    int32_t radioAxis = 0;
    float axisValue = 0.0f;
};

struct EP7PortalGate {
    std::string name;
    uint32_t gateId = 0;
    uint32_t gateFlags = 0;
    uint16_t toMapMainId = 0;
    uint16_t toMapSubId = 0;
    uint32_t toGateId = 0;
    EP7AABB bounds;
};

struct EP7MobSpawn {
    std::string name;
    uint16_t crowMainId = 0;
    uint16_t crowSubId = 0;
    EP7Vec3 position{0.0f, 0.0f, 0.0f};
    float regenTime = 0.0f;
};

struct EP7LevelFullData {
    EP7LevelHeader head;
    std::vector<EP7MobSpawn> mobSpawns;
    std::vector<EP7PortalGate> gates;
    std::vector<std::string> landEffects;
    uint32_t landMarkCount = 0;
    bool valid = false;
    size_t bytesConsumed = 0;
    std::string error;
};

struct EP7WorldData {
    uint32_t version = 0;
    uint16_t mapMainId = 0;
    uint16_t mapSubId = 0;
    std::string mapName;
    EP7AABB worldBounds;

    std::vector<EP7MeshBatch> terrainBatches;
    std::vector<EP7OctreeNode> octreeNodes;
    std::vector<EP7PieceAsset> pieceAssets;
    std::vector<EP7PieceInstance> pieceInstances;

    std::vector<EP7LightInfo> lights;
    EP7FogInfo fog;
    EP7SkyInfo sky;
    std::string bgmFile;
    std::vector<EP7PortalGate> gates;
    std::vector<std::string> landEffects;
    std::vector<std::string> skinObjects;

    uint32_t navVertexCount = 0;
    uint32_t navCellCount = 0;
    std::vector<EP7Vec3> navSamplePoints;
    EP7Vec3 recommendedSpawn{0.0f, 0.0f, 0.0f};
    bool hasRecommendedSpawn = false;

    std::vector<std::string> textureRefs;
    std::vector<std::string> missingPieceFiles;

    size_t wldBytesConsumed = 0;
    size_t wldFileBytes = 0;
    size_t wld0BytesConsumed = 0;
    size_t wld0FileBytes = 0;
    bool hasWld0 = false;
    bool valid = false;
    std::string error;

    size_t TotalTerrainVertices() const;
    size_t TotalTerrainTriangles() const;
};

class EP7WorldParser {
public:
    static constexpr uint32_t kMinWldVersion = 0x0108;
    static constexpr uint32_t kMaxWldVersion = 0x0114;

    // Parses an authentic LAND.MAN .wld file and optional companion .wld0 static mesh file.
    static EP7WorldData ParseWld(const std::vector<uint8_t>& wldBytes,
                                 const std::vector<uint8_t>& wld0Bytes = {});

    // Parses an authentic .pie (DxPieceEdit) or .pis (DXREPLACEPIECE) file.
    static bool ParsePieceFile(const std::string& filename,
                               const std::vector<uint8_t>& bytes,
                               EP7PieceAsset& outPiece,
                               std::string* errorOut = nullptr);

    // Parses an authentic encrypted .Lev file (glmap 0x0200 LOAD_200) to exact EOF.
    static EP7LevelFullData ParseFullLevel(
        const std::vector<uint8_t>& levBytes,
        EP7ValidationMode mode = EP7ValidationMode::StrictAuthentic);

    // Resolves all .pis/.pie files referenced by world.pieceInstances inside assetRoot,
    // deduplicating EP7PieceAsset records and populating world-space bounds & textureRefs.
    static void ResolveAndAttachPieces(EP7WorldData& world, const std::string& assetRoot);

    // Collects unique texture filenames referenced across terrain, loaded pieces, and sky.
    static void RebuildTextureRefs(EP7WorldData& world);
};

} // namespace RANMobile
