//
// EP7SkinPiece.h — Authentic EP7 skin-piece (.cps) container reader (Phase 3J)
//
// SOURCE OF TRUTH
// ---------------
// enginelib/Meshs/DxSkinPieceContainer.h/.cpp
//   const DWORD DxSkinPiece::VERSION = 0x00000107;
//   HRESULT DxSkinPiece::LoadPiece(...)
//
// `LoadPiece` reads from a `basestream` (CSerialFile for loose files, CSerialMemory
// inside the .rcc pack), version 0x0107:
//
//   DWORD  dwVer                       (== VERSION, 0x0107)
//   DWORD  m_dwRef
//   DWORD  dwSize; char szXFileName[dwSize]     // "*.x"         -> skin mesh
//   DWORD  dwSize; char szSkeleton[dwSize]      // "*.skt/.x"    -> skeleton
//   DWORD  type        (EMPIECECHAR: head/upbody/lobody/glove/rhand/lhand/foot/hair/...)
//   DWORD  weaponWhereBack (EMPEACEZONEWEAPON)  [0x0106/0x0107 only]
//   DWORD  dwSize; char szMeshName[dwSize]      // which mesh container in the .x
//   DWORD  m_dwMaterialNum
//   for each material:  SMATERIAL_PIECE::LoadFile
//                       DWORD dwVERSION (0x0100), DWORD dwBLOCK,
//                       DWORD dwLength; char texture[dwLength],
//                       DWORD m_dwFlags, DWORD m_dwZBias
//   DWORD  traceCount
//   for each trace:     string name, then SVERTEXINFLU::LoadFile
//   DWORD  effCount
//   for each effect:    DWORD TypeID + effect payload (NOT parsed here)
//   DWORD  m_dwFlag
//   // 0x0107 adds: m_emWeaponWhereBack immediately after m_emType (already above)
//
// IMPORTANT: the vertex geometry, bone indices and per-vertex weights are NOT in the
// .cps.  They live in the referenced `.x` skin mesh, loaded by
// D3DXLoadSkinMeshFromXof inside DxSkinMesh::LoadMesh.  The .cps only records which
// mesh container to use, plus material/texture and attachment ("trace") data.
// This reader therefore returns the container metadata; the mesh is loaded separately
// by EP7SkinnedMesh.
//
// "Trace" vertices are attachment points: a piece names a vertex by SVERTEXINFLU so
// effects and held items can be pinned to it (DxSkinPiece::GetTracePos /
// CalculateVertexInflu).
//
// Not parsed (documented, not invented): DxEffChar effect payloads.  They are skipped
// using the enclosing D3DX effect block size only when that is derivable; otherwise the
// parse stops before the effect list and flags PartialEffectSkipped.
//
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "EP7ByteFormat.h"

namespace RANMobile {

// EMPIECECHAR (enginelib/Meshs/DxPieceDefine.h) — values must stay identical.
enum class EP7PieceType : int32_t {
    Head = 0,
    UpBody = 1,
    LoBody = 2,
    Glove = 3,
    RHand = 4,
    LHand = 5,
    Foot = 6,
    Hair = 7,
    HeadGear = 8,
    Vehicle = 9,
    Rev01 = 10,
    Rev02 = 11,
    Rev03 = 12,
    Size = 13,
};

// EMPEACEZONEWEAPON (enginelib/Meshs/DxPieceDefine.h)
enum class EP7WeaponBackSlot : int32_t {
    RSlot = 0,
    WhalSlot = 1,
    StickRSlot = 2,
};

// SVERTEXINFLU (enginelib/Meshs/DxSkinMeshContainer9.h) — MAX_BONE = 6.
struct EP7VertexInfluence {
    static constexpr uint32_t kMaxBone = 6;
    static constexpr uint32_t kColErr = 0xFFFFFFFFu;  // (DWORD)-1
    static constexpr uint32_t kVersion = 0x0100;      // SVERTEXINFLU::VERSION (see cpp)

    uint32_t index = kColErr;
    float position[3] = {0.0f, 0.0f, 0.0f};
    float normal[3] = {0.0f, 0.0f, 0.0f};
    uint32_t boneCount = kColErr;
    uint32_t bones[kMaxBone] = {0};
    float weights[kMaxBone] = {0.0f};

    bool valid = false;
};

struct EP7MaterialPiece {
    static constexpr uint32_t kVersion = 0x0100;  // SMATERIAL_PIECE::VERSION

    std::string texture;
    uint32_t flags = 0;   // FLAG_ALPHATEXTURE = 0x0001
    uint32_t zBias = 0;
    float zBiasScaled = 0.0f;  // m_fZBias = m_dwZBias * 0.00001f (SMATERIAL_PIECE::LoadFile)
};

struct EP7TracePoint {
    std::string name;
    EP7VertexInfluence influence;
};

struct EP7SkinPieceInfo {
    uint32_t fileVersion = 0;
    uint32_t refCount = 0;
    std::string xFileName;       // skin mesh .x
    std::string skeletonFile;    // skeleton .x
    EP7PieceType pieceType = EP7PieceType::Head;
    EP7WeaponBackSlot weaponWhereBack = EP7WeaponBackSlot::RSlot;
    bool hasWeaponWhereBack = false;  // only 0x0106 / 0x0107 store it

    std::string meshName;  // mesh container selected inside the .x
    std::vector<EP7MaterialPiece> materials;
    std::vector<EP7TracePoint> traces;
    uint32_t flag = 0;

    uint32_t effectCount = 0;         // declared in the file
    uint32_t effectsParsed = 0;       // always 0: effect payloads are not parsed
    bool effectsPresent = false;      // effect list non-empty
    bool valid = false;
    std::string error;
};

class EP7SkinPieceReader {
public:
    static constexpr uint32_t kVersion107 = 0x00000107;
    static constexpr uint32_t kVersion106 = 0x00000106;
    static constexpr uint32_t kVersion105 = 0x00000105;
    static constexpr uint32_t kVersion104 = 0x00000104;

    // `stream` must already be attached and positioned at payload offset 0 with
    // ReadFileType() consumed.  (The original reads the version through operator>>,
    // i.e. as the first payload DWORD, *not* through the 128-byte header.)
    static EP7SkinPieceInfo Parse(ep7byte::Stream& stream);
    static EP7SkinPieceInfo ParseFromBytes(const std::vector<uint8_t>& bytes, bool compByteEncoded);
};

}  // namespace RANMobile
