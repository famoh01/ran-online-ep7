//
// EP7CharacterFile.h — Authentic EP7 character file (.chf) reader (Phase 3J)
//
// SOURCE OF TRUTH
// ---------------
// enginelib/Meshs/DxSkinCharData.h/.cpp
//   enum { VERSION = 0x0102, };     // DxSkinCharData::VERSION
//   DxSkinCharData::LoadFile  ->  LOAD_Ver101() for 0x0101 / 0x0102, LOAD_Ver100() for 0x0100
//
// EXACT LAYOUT (version 0x0101 and 0x0102 — identical payload on disk)
// ----------------------------------------------------------------
//   float  m_fScale
//   DWORD  pieceCount                     (must be <= PIECE_SIZE == 13, else the original
//                                          returns FALSE without clearing)
//   repeat pieceCount:
//       BOOL   present                    (WIN32 BOOL, 4 bytes)
//       if present: string pieceName      (DWORD len+1 + bytes)
//   string skeletonName                   (DWORD len+1 + bytes)
//   DWORD  animationCount
//   repeat animationCount: string animationName
//   BOOL   hasBoundingBox
//   if hasBoundingBox: D3DXVECTOR3 m_vMax; D3DXVECTOR3 m_vMin
//   BOOL   hasWorldObj;  if hasWorldObj: BOOL m_bWorldObj
//   BOOL   hasHeightRadius; if hasHeightRadius: float m_fHeight; float m_fRadius
//   BOOL   trailer flag                   (always written as FALSE)
//
// Version 0x0100 (LOAD_Ver100) differs:
//   * no leading scale,
//   * the skeleton name is a fixed MAX_PATH (260) byte buffer, not a length-prefixed string,
//   * each animation name is a fixed ACF_SZNAME (128) byte buffer,
//   * the payload ends after the bounding box (no world-obj / height-radius fields).
//
// The .chf is only an index: the mesh geometry lives in the referenced `.cps` pieces,
// each of which points at a `.x` skin mesh and a skeleton.  Animation clips are named by
// the .chf and loaded from the separate animation container (.bin) + .cfg pair.
//
// IMPORTANT PLATFORM NOTE (verified from the vendored D3DX header)
// --------------------------------------------------------------
//   LegacyDX_Include/d3dx9math.h:411  #define D3DX_ALIGN16 __declspec(align(16))
//   ...:416                           typedef D3DX_ALIGN16 _D3DXMATRIXA16 D3DXMATRIXA16;
//   Because D3DXMATRIXA16 is 16-byte aligned, sizeof(SMatrixKey) == 80 (not 68) and
//   sizeof(SQuatPosKey) == 36.  A host compiler where D3DX_ALIGN16 expands to nothing
//   would compute 68/... — so the sizes are asserted in tests rather than assumed.
//
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "EP7ByteFormat.h"

namespace RANMobile {

// EMPIECECHAR index count (enginelib/Meshs/DxPieceDefine.h)
inline constexpr uint32_t kEP7PieceSize = 13;
// MAX_PATH on the original Win32 build.
inline constexpr uint32_t kEP7MaxPath = 260;
// ACF_SZNAME (enginelib/Meshs/SAnimation.h)
inline constexpr uint32_t kEP7AnimNameSize = 128;

struct EP7CharacterFileInfo {
    static constexpr uint32_t kVersion102 = 0x0102;
    static constexpr uint32_t kVersion101 = 0x0101;
    static constexpr uint32_t kVersion100 = 0x0100;

    uint32_t version = 0;
    float scale = 1.0f;

    // Indexed by EMPIECECHAR; empty string means "this slot is not defined".
    // The original stores m_strPIECE[PIECE_SIZE] in the same slot order.
    std::string pieces[kEP7PieceSize];

    std::string skeleton;
    std::vector<std::string> animations;

    bool hasBoundingBox = false;
    float boundsMax[3] = {7.0f, 20.0f, 7.0f};    // DxSkinCharData defaults
    float boundsMin[3] = {-7.0f, 0.0f, -7.0f};
    bool hasWorldObj = false;
    bool worldObj = false;
    bool hasHeightRadius = false;
    float height = 20.0f;   // default m_fHeight
    float radius = 7.0f;    // default m_fRadius

    bool valid = false;
    std::string error;

    uint32_t PieceCount() const {
        uint32_t n = 0;
        for (uint32_t i = 0; i < kEP7PieceSize; ++i) {
            if (!pieces[i].empty()) ++n;
        }
        return n;
    }
};

class EP7CharacterFileReader {
public:
    // `bytes` = full .chf file.  The .chf has no basestream header: DxSkinCharData::LoadFile
    // reads the version with `SFile >> dwVer` as the first payload DWORD.
    static EP7CharacterFileInfo Parse(const std::vector<uint8_t>& bytes, bool compByteEncoded);
    static EP7CharacterFileInfo ParseStream(ep7byte::Stream& stream);
};

}  // namespace RANMobile
