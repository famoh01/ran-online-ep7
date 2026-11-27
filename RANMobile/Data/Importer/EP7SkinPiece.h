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
// LEGACY VERSIONS (the layout above is 0x0105-0x0107; the older branches differ):
//
//   0x0104  identical to 0x0105 but the file ENDS after the effect list -- there is no
//           trailing m_dwFlag (DxSkinPieceContainer.cpp:629-711).
//   0x0103  no material count at all; header is ref/x/skel/type/mesh, then the trace
//           count and the effect count.  No trailing flag (:712-782).
//   0x0102  no material count; after the mesh name come 10 SVERTEXINFLU records with no
//           names and no count, then the effect count.  Names are the hard-coded
//           STRACE_IMAGE01/02/11/12 macros for slots 0..3, and only slots whose
//           m_dwIndex != COL_ERR are kept (:783-858).
//   0x0101  as 0x0102 but with 8 slot records instead of 10 (:859-933).
//   Every legacy branch ends after the effect list; none of them reads m_dwFlag, and the
//   constructor initialises it to 0 (DxSkinPieceContainer.cpp:192).
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
    // SVERTEXINFLU::VERSION — defined as 0x0101 in DxSkinMeshContainer9.cpp:36 and
    // SMeshContainer.cpp:29, written by SaveFile() and compared by LoadFile().
    static constexpr uint32_t kVersion = 0x0101;

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

    // 0x0101/0x0102 store no dependency name: `LoadPiece` fills STRACOR's m_szName from the
    // hard-coded STRACE_IMAGE0x/1x macros (DxSkinPieceContainer.h:22-25) and only keeps the
    // four slots whose m_dwIndex != COL_ERR.  `syntheticName` records that the name came
    // from the engine's table rather than from the file.
    bool syntheticName = false;
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
    static constexpr uint32_t kVersion103 = 0x00000103;
    static constexpr uint32_t kVersion102 = 0x00000102;
    static constexpr uint32_t kVersion101 = 0x00000101;

    // SVERTEXINFLU slots read by 0x0101 (8 slots) and 0x0102 (10 slots).  Both versions
    // read a FIXED array with no count and no names; only the first four entries can ever
    // reach m_vecTrace (DxSkinPieceContainer.cpp:815-830 / 891-904).
    static constexpr uint32_t kLegacyTraceSlots101 = 8;
    static constexpr uint32_t kLegacyTraceSlots102 = 10;
    static constexpr uint32_t kLegacyTraceNames = 4;

    // `stream` must already be attached and positioned at payload offset 0 with
    // ReadFileType() consumed.  (The original reads the version through operator>>,
    // i.e. as the first payload DWORD, *not* through the 128-byte header.)
    static EP7SkinPieceInfo Parse(ep7byte::Stream& stream);
    static EP7SkinPieceInfo ParseFromBytes(const std::vector<uint8_t>& bytes, bool compByteEncoded);
};

}  // namespace RANMobile
