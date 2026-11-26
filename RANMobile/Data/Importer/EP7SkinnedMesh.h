//
// EP7SkinnedMesh.h — Authentic EP7 skinned-mesh (.x MeshData + SkinWeights) reader
//
// SOURCE OF TRUTH
// ---------------
// enginelib/Meshs/DxSkinMeshLoad.cpp
//     DxSkinMesh::LoadFile  -> DirectXFileCreate / RegisterTemplates(D3DRM_XTEMPLATES)
//                              CreateEnumObject -> LoadFrames -> FindBones
//     DxSkinMesh::LoadMesh  -> D3DXLoadSkinMeshFromXof(...)
// enginelib/Meshs/DxSkinMesh9.cpp
//     SetupBoneMatrixPointersOnMesh:  ppBoneMatrixPtrs[i] = &skeleton->FindBone(
//                                        pSkinInfo->GetBoneName(i))->matCombined
// enginelib/Meshs/DxSkinMesh9_CPU.cpp  (CPU skinning reference)
//     g_pBoneMatrices[palette] = pBoneOffsetMatrices[iMatrixIndex]
//                              * ppBoneMatrixPtrs[iMatrixIndex]
//     PhysiqueTransform_SSE transforms each vertex by the palette matrices,
//     weighted by the per-vertex blend weights.
// enginelib/Meshs/DxSkinMesh9_HLSL.cpp (GPU skinning reference)
//     vertex declaration: POSITION0 f3 @0, NORMAL0 f3 @12, TEXCOORD0 f2 @24,
//     TANGENT0 f3 @32, BLENDWEIGHT0 f4 @44, BLENDINDICES0 f4 @60
//     -> this proves the EP7 runtime vertex carries up to 4 bone influences,
//        i.e. D3DXSkinInfo::GetMaxVertexInfluences() <= 4 for these assets.
//
// DATA MODEL OF A DIRECTX 9 SKINNED .X FILE
// ----------------------------------------
//   SkinWeights {
//       "boneName";                 // string
//       <n>;                        // DWORD number of vertices
//       <i0>, <i1>, ...;            // n DWORD vertex indices
//       <w0>, <w1>, ...;            // n FLOAT weights (same vertices)
//       <16 floats>;                // offset matrix (D3DXMATRIX, row-major)
//   }
//   Mesh <name> {
//       <nFaces>;                   // DWORD
//       <faceCount*3> DWORD indices;
//       <nVertices>;                // DWORD
//       <nVertices * vertexData>;   // raw vertex bytes (declaration-dependent)
//       <nMaterials>; <MaterialList body>
//       [ MeshNormals / MeshTextureCoords / MeshMaterialList / Declarations / ... ]
//       ...
//   }
//
// D3DXLoadSkinMeshFromXof builds the vertex declaration from the vertex byte size
// recorded in the file (it infers position/normal/texcoord from the buffer length and
// the "MeshNormals"/"MeshTextureCoords" objects). Reproducing that inference exactly
// from source alone is NOT possible without D3DX; therefore this reader
//   (a) parses faces, SkinWeights (bone names, per-vertex indices, weights, offset
//       matrices) and material/texture names, which are unambiguous, and
//   (b) exposes the vertex stream size + the standard EP7 declaration candidates so a
//       caller can decode positions when the stream size matches a known layout, and
//   (c) refuses to guess otherwise (documented limitation, never fabricated data).
//
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "EP7Math.h"
#include "EP7Skeleton.h"

namespace RANMobile {

// One SkinWeights object (D3DXSKININFO bone).
struct EP7SkinBoneInfo {
    std::string name;
    int32_t skeletonBoneIndex = -1;   // resolved by BindToSkeleton
    std::vector<uint32_t> vertices;   // vertex indices influenced
    std::vector<float> weights;       // parallel to `vertices`
    ep7math::Mat4 offsetMatrix = ep7math::Mat4::Identity();  // pBoneOffsetMatrices[i]
};

// Standard EP7 runtime declaration (verified in DxSkinMesh9_HLSL.cpp:104-105).
enum class EP7VertexLayout {
    Unknown = 0,
    Pos32 = 12,        // D3DFVF_XYZ
    PosNormal32 = 24,  // D3DFVF_XYZ|D3DFVF_NORMAL
    PosNormalUV32 = 32,  // + D3DFVF_TEX1
};

struct EP7MeshSubset {
    std::string texture;     // from MeshMaterialList/Material (Material_DiffuseTexture)
    uint32_t materialIndex = 0;
    uint32_t attributeId = 0;
    uint32_t faceStart = 0;
    uint32_t faceCount = 0;
};

struct EP7SkinnedMesh {
    std::string name;
    std::vector<uint32_t> faces;     // 3 indices per triangle
    uint32_t vertexCount = 0;
    uint32_t vertexStride = 0;       // bytes per vertex as written in the file
    EP7VertexLayout layout = EP7VertexLayout::Unknown;

    std::vector<float> positions;    // xyz per vertex, filled only when the layout is known
    std::vector<float> normals;
    std::vector<float> texcoords;    // uv per vertex
    std::vector<float> tangents;

    std::vector<EP7SkinBoneInfo> bones;
    std::vector<EP7MeshSubset> subsets;
    std::vector<std::string> textures;

    bool valid = false;
    bool geometryDecoded = false;    // true only when positions were actually decoded
    std::string error;

    uint32_t MaxInfluencesPerVertex() const;
};

class EP7SkinnedMeshReader {
public:
    // Parses the *first* skinned Mesh object that matches `meshName` (the .cps
    // `m_szMeshName`), or the first Mesh when `meshName` is empty.
    // `skeleton` is optional; when supplied, bone names are resolved to bone indices
    // exactly like DxSkinMesh9::SetupBoneMatrixPointersOnMesh (a missing bone is an
    // error in the original: it returns E_FAIL).
    static EP7SkinnedMesh Parse(const std::vector<uint8_t>& data,
                                const std::string& meshName,
                                const EP7Skeleton* skeleton);

    static constexpr uint32_t kMaxVertices = 2000000;   // sanity bound
    static constexpr uint32_t kMaxFaces = 2000000;      // sanity bound
    static constexpr uint32_t kMaxBones = 512;          // sanity bound
};

// ---------------------------------------------------------------------------
// Skinning evaluation (CPU reference path, mirrors DxSkinMesh9_CPU)
// ---------------------------------------------------------------------------

// Builds palette matrices: paletteMats[b] = offsetMatrix(b) * bone.matCombined.
void BuildBonePalette(const EP7SkinnedMesh& mesh,
                      const EP7Skeleton& skeleton,
                      std::vector<ep7math::Mat4>& outPalette);

// Skins every vertex with up to 4 influences.  Returns false when the mesh has no
// decodable positions.  Normals are transformed by the same blended matrix.
bool SkinVerticesCPU(const EP7SkinnedMesh& mesh,
                     const std::vector<ep7math::Mat4>& palette,
                     std::vector<float>& outPositions,
                     std::vector<float>& outNormals);

}  // namespace RANMobile
