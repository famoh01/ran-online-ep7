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
// Version 0x0103+ (mobile-exported SkinCharData, Phase 3J B20)
// ------------------------------------------------------------
// The authentic EP7 character pack (skinobject.rcc) ships no 0x0100..0x0102
// .chf at all.  CI census of its 243 .chf (run 46131351370): 0x0104 x33,
// 0x0106 x83, 0x0107 x81, 0x0108 x3, 0x0200 x43.  Payload-head dumps prove the
// newer files carry NO leading scale (the version DWORD is followed directly
// by the piece count), then the same length-prefixed piece slots.  Tail dumps
// of the four smallest examples (NPC_ark 0x0104/309 B, event_Athena
// 0x0106/311 B, npc_police_1 0x0107/307 B, NPC_hangar 0x0108/319 B) were
// reconstructed byte-exactly against this layout:
//
//   DWORD  pieceCount                 (per-file, observed 10..13, <= PIECE_SIZE)
//   repeat pieceCount:
//       BOOL   present
//       if present: string pieceName (DWORD len+1 + bytes)   [unchanged encoding]
//   string skeletonName                  (DWORD len+1 + bytes)
//   DWORD  animationCount
//   repeat animationCount: string animationName
//   trailer block, total size selects the variant (all values LE):
//       60 B: float scale;      DWORD fA=1;                     // 0x0104 sample
//             float height; float radius; DWORD 0; DWORD 1;
//             float vMax[3]; float vMin[3]; DWORD 1; DWORD 0; DWORD 0;
//   The block shape is VALUE-ADAPTIVE (runs 46133690603..46147778604; both
//   shapes attest under 0x0104 AND 0x0106+ — m_cos_wing04 vs NPC_ark):
//     15 dwords (60 B): scale f32; DWORD Y; height f32; radius f32;
//                       DWORD 0; DWORD 1; vMax[3]; vMin[3];
//                       DWORD t0=1; DWORD t1=0; DWORD more=0
//     16 dwords (64 B): as 60 B plus DWORD X immediately after Y
//     0x0108: two extra flag DWORDs (1, 0) between vMin and the final run
//   (72 B).  Y/X are raw (the TrickBoomLow class carries an arbitrary float
//   12.5f in Y); the shape is picked by value: X-slot dword 0/1 selects the
//   16-dword shape (a real height float never has bit pattern 0 or 1), an
//   exact 60-B block forces the 15-dword shape.
//   The trailing [t0 t1 more] run maps naturally to hasWorldObj=1 /
//   worldObj=0 in the 0x0102 field order, and more switches an opaque
//   extension section on that runs to EOF (attested sizes: 8 B and 24 B
//   minimal (0,0) markers, 120 B prop-geometry block in GBOX02, 200 B = 5 x
//   40 B bone-attach records [3 floats][0][16-byte bone name][strlen][15] in
//   m_cos_wing04).  The parser skips it positionally and records its size.
//
//   In every authentic sample height/vMax/vMin satisfy vMax == (radius, height,
//   radius) and vMin == (-radius, 0, -radius) (the mobile exporter writes the
//   SetRadiusHeight-derived box).  The parser fills these fields positionally
//   and only sanity-checks the flag DWORDs as booleans (0/1); it does not
//   hard-assert the geometry relation.
//
//   0x0105 is shipped by NO authentic pack and has no known layout -> still
//   unsupported.  0x0103 is admitted with the same value-adaptive block:
//   no authentic 0x0103 sample exists (the sibling DxSkinPieceContainer
//   evolution 0x0102->0x0103 only made fixed arrays count-prefixed, which
//   SkinCharData already is at 0x0102), so 0x0103 files are parsed with the
//   0x0104-observed shape; a real 0x0103 file that diverges fails cleanly and
//   surfaces in the verify-tool census.
//
//   0x0200 (43 files in skinobject.rcc, type block "default", high-entropy
//   payload) is a different, still-undeciphered container -> unsupported.
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
    static constexpr uint32_t kVersion100 = 0x0100;
    static constexpr uint32_t kVersion101 = 0x0101;
    static constexpr uint32_t kVersion102 = 0x0102;
    // Mobile-exported SkinCharData versions (Phase 3J B20).  0x0105 has no
    // authentic sample and no known layout -> deliberately no constant.
    static constexpr uint32_t kVersion103 = 0x0103;
    static constexpr uint32_t kVersion104 = 0x0104;
    static constexpr uint32_t kVersion106 = 0x0106;
    static constexpr uint32_t kVersion107 = 0x0107;
    static constexpr uint32_t kVersion108 = 0x0108;

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
    bool hasExtensionSections = false;  // 0x0103+: opaque tail after more==1
    uint32_t extensionBytes = 0;        // size of that tail (skipped, not interpreted)
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
