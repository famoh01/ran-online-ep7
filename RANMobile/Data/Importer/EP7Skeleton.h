//
// EP7Skeleton.h — Authentic EP7 skeleton (.x frame hierarchy) reader (Phase 3J)
//
// WHAT THE ORIGINAL CLIENT DOES
// ----------------------------
// A `.chf` record names a skeleton file (e.g. "o_m.skt" / a `.x`); the loader is
// DxSkeleton::LoadFile (enginelib/Meshs/DxBoneCollector.cpp):
//
//   1. DirectXFileCreate(&pxofapi); pxofapi->RegisterTemplates(D3DRM_XTEMPLATES)
//   2. CreateEnumObject(path, DXFILELOAD_FROMFILE, &pxofenum)
//   3. pBoneRoot = new DxBoneTrans named "__ROOT"
//   4. while (pxofenum->GetNextDataObject(&pxofobjCur)) {
//          if (*type == TID_D3DRMFrame)  LoadBoneTrans(pBoneRoot, pxofobjCur, EMBODY_DEFAULT);
//      }
//   5. LoadBoneTrans(...) walks each Frame's child objects:
//          TID_D3DRMFrameTransformMatrix -> GetData(&pmatNew); bone->matRot = matRotOrig = *pmatNew
//          TID_D3DRMFrame(name)          -> recursive child bone, inserted in BoneMap by name,
//                                           linked via pBoneTransFirstChild / pBoneTransSibling.
//      A child named exactly "Bip01_Spine1" is tagged EMBODY_UPBODY (upper/lower body split).
//
// Parent/sibling ordering is *reversed* relative to file order because
// DxBoneTrans::AddBoneTrans() pushes each new child at the head of the sibling list,
// and recursion happens in the original loop order.  We reproduce this ordering
// because DxSkeleton::UpdateBones recurses first-child then sibling; any other
// ordering changes only traversal order, so the transforms are identical, but the
// flattened bone indices (used by skin palettes) must be stable and documented.
//
// TRANSFORMS
// ----------
// TID_D3DRMFrameTransformMatrix is a standard DirectX 9 matrix template: 16 floats.
// D3DX writes matrices row-major in .x files, so element order is _11,_12,_13,_14,
// _21,... and translation lands in _41.._43 -- the same layout our Mat4 uses.
//
// WHAT IS *NOT* IMPLEMENTED HERE
// ------------------------------
// Only the Frame / FrameTransformMatrix subset is implemented: that is exactly the
// subset DxBoneCollector consumes.  MeshData, MeshMaterialList, SkinWeights and
// AnimationSet objects are handled elsewhere (EP7SkinnedMesh / the .cps reader is
// the authoritative path for skin data in EP7; .x skinning is only a fallback).
//
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "EP7Math.h"

namespace RANMobile {

enum class EP7BodyPart : uint8_t {
    Default = 0,   // EMBODY_DEFAULT
    UpBody = 1,    // EMBODY_UPBODY   ("Bip01_Spine1" and its descendants)
};

struct EP7Bone {
    std::string name;
    int32_t parent = -1;
    int32_t firstChild = -1;
    int32_t nextSibling = -1;
    EP7BodyPart body = EP7BodyPart::Default;

    // Local rest transform from FrameTransformMatrix (identity if the frame has none,
    // which is what Direct3D leaves in D3DXFRAME::TransformationMatrix).
    ep7math::Mat4 localRest = ep7math::Mat4::Identity();
    // Current local transform (rest pose until an animation drives it).
    ep7math::Mat4 local = ep7math::Mat4::Identity();
    // True when `local` must be rebuilt from the SQT fields (m_pQuatPosCUR) instead of
    // being used directly, mirroring the USE_ANI_QUATERNION branch of UpdateBones.
    bool localIsFromSqt = false;
    // Accumulated world transform (matCombined).
    ep7math::Mat4 combined = ep7math::Mat4::Identity();

    // ---------------------------------------------------------------------
    // B17: DxBoneTrans runtime pose state (enginelib/Meshs/DxBoneCollector.h/.cpp).
    // The shipped client defines USE_ANI_QUATERNION (DxSkinDefine.h:3), so the
    // per-bone pose lives in an SQuatPos triple plus flags, not in matRot:
    //   struct DxBoneTrans {
    //       SQuatPos m_pQuatPosORIG;  // bind pose SQT (D3DXMatrixToSQT of matRotOrig)
    //       SQuatPos m_pQuatPosCUR;   // pose written by SetPosQuat(bFirst=TRUE)
    //       SQuatPos m_pQuatPosPREV;  // pose written by the blend layer (bFirst=FALSE)
    //       D3DXQUATERNION m_vQuatMIX;    // USE_ANI_ADD overlay (shock mix)
    //       float m_fScale;               // DxAniScale uniform bone scale
    //       DWORD m_dwFlag;               // USE_ANI_ADD / USE_ANI_BLENDING
    //   };
    // ResetBone(): flags=0, m_fScale=1, m_vQuatMIX=identity, CUR=PREV=ORIG.
    // ---------------------------------------------------------------------
    ep7math::Vec3 restScale{1.0f, 1.0f, 1.0f};
    ep7math::Vec3 restPosition{0.0f, 0.0f, 0.0f};
    ep7math::Quat restRotation{0.0f, 0.0f, 0.0f, 1.0f};

    ep7math::Vec3 prevScale{1.0f, 1.0f, 1.0f};
    ep7math::Vec3 prevPosition{0.0f, 0.0f, 0.0f};
    ep7math::Quat prevRotation{0.0f, 0.0f, 0.0f, 1.0f};

    bool useBlending = false;   // DxBoneTrans::m_dwFlag & USE_ANI_BLENDING
    bool useAniAdd = false;     // DxBoneTrans::m_dwFlag & USE_ANI_ADD
    ep7math::Quat mixRotation{0.0f, 0.0f, 0.0f, 1.0f};  // m_vQuatMIX
    float aniScale = 1.0f;      // m_fScale (DxAniScale result)

    // Current local SQT (m_pQuatPosCUR).
    ep7math::Vec3 scale{1.0f, 1.0f, 1.0f};
    ep7math::Vec3 position{0.0f, 0.0f, 0.0f};
    ep7math::Quat rotation{0.0f, 0.0f, 0.0f, 1.0f};
};

class EP7Skeleton {
public:
    // Parses an authentic DirectX .x file containing Frame/FrameTransformMatrix data.
    // `data` is the raw (already container-decompressed) file bytes.
    // Returns false and sets Error() on malformed input; never partially applies state.
    bool LoadFromXFile(const std::vector<uint8_t>& data);
    bool LoadFromXFile(const uint8_t* data, size_t size);

    void Clear();
    bool IsValid() const { return m_valid; }
    const std::string& Error() const { return m_error; }

    size_t BoneCount() const { return m_bones.size(); }
    const EP7Bone& GetBone(size_t index) const { return m_bones[index]; }
    EP7Bone* FindBone(const std::string& name);
    int32_t FindBoneIndex(const std::string& name) const;

    // Bones whose parent is -1 (normally exactly one "__ROOT").
    const std::vector<int32_t>& RootBones() const { return m_roots; }

    // Bone indices ordered by the authentic depth-first traversal used by
    // DxSkeleton::UpdateBones (firstChild, then nextSibling). Useful as a stable
    // skinning palette order.
    const std::vector<int32_t>& PaletteOrder() const { return m_paletteOrder; }

    // DxSkeleton::UpdateBones — recompute `combined` for the whole hierarchy from
    // the current local transforms.  matRoot is the entity world transform (identity
    // by default), mirroring UpdateBones(pBoneRoot, matCur).
    // `blendWeight` is DxSkeleton::g_fWeight: bones flagged USE_ANI_BLENDING lerp
    // PREV->CUR by it before the local matrix is built (0 = previous pose, 1 = new pose).
    void UpdateBones(const ep7math::Mat4& matRoot = ep7math::Mat4::Identity(), float blendWeight = 1.0f);

    // Reset every bone to its rest transform (DxBoneTrans::ResetBone: flags cleared,
    // m_fScale = 1, m_vQuatMIX = identity, CUR = PREV = ORIG).
    void ResetToRestPose();

    // Synthetic root the original always inserts ("__ROOT").
    static constexpr const char* kRootName = "__ROOT";
    static constexpr const char* kUpBodyBoneName = "Bip01_Spine1";

private:
    int32_t AddBone(const std::string& name, int32_t parent, EP7BodyPart body);
    void RebuildLinks();
    void UpdateBonesRecursive(int32_t index, const ep7math::Mat4& parentCombined, float blendWeight);

    bool m_valid = false;
    std::string m_error;
    std::vector<EP7Bone> m_bones;
    std::vector<int32_t> m_roots;
    std::vector<int32_t> m_paletteOrder;
    std::unordered_map<std::string, int32_t> m_nameToIndex;
};

}  // namespace RANMobile
