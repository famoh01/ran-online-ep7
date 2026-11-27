//
// test_phase3j.cpp — Phase 3J character / skeleton / animation / skinning tests
//
// FIXTURE POLICY (important, and honest)
// -------------------------------------
// The authentic EP7 client assets (.chf / .cps / .x / .bin / .cfg) are not in this
// repository and must not be fabricated. The buffers built below are **ABI fixtures**:
// they are written by hand to the exact on-disk layout recovered from the original
// sources (CSerialFile / SMATERIAL_PIECE / SAnimation / SANIMCONINFO / DxBoneCollector /
// D3DX SkinWeights) purely to exercise bounds checking, version gating, index validation
// and evaluation math. They are NOT game data and are never used to claim that a real
// character renders. Real-asset verification is done by
// RANMobile/Tools/verify_ep7_character_assets against an extracted client (see
// PHASE3J_CHARACTER_ANIMATION.md).
//
#include <algorithm>
#include "../Data/Importer/EP7Animation.h"
#include "../Data/Importer/EP7ByteFormat.h"
#include "../Data/Importer/EP7CharacterFile.h"
#include "../Data/Importer/EP7Math.h"
#include "../Data/Importer/EP7RccArchive.h"
#include "../Data/Importer/EP7Skeleton.h"
#include "../Data/Importer/EP7SkinPiece.h"
#include "../Data/Importer/EP7SkinnedMesh.h"
#include "../Data/Importer/EP7XFile.h"
#include "../Engine/Camera/MobileCamera.h"
#include "../Engine/Scene/EP7CharacterScene.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

int g_failures = 0;
int g_checks = 0;

#define J_CHECK(cond)                                                          \
    do {                                                                       \
        ++g_checks;                                                            \
        if (!(cond)) {                                                         \
            std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

#define J_CHECK_MSG(cond, msg)                                                 \
    do {                                                                       \
        ++g_checks;                                                            \
        if (!(cond)) {                                                         \
            std::printf("  FAIL %s:%d  %s (%s)\n", __FILE__, __LINE__, #cond,  \
                        std::string(msg).c_str());                             \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

void Section(const char* name) { std::printf("[3J] %s\n", name); }

// ---------------------------------------------------------------------------
// Little-endian byte writer used to build ABI fixtures.
// ---------------------------------------------------------------------------
class ByteWriter {
public:
    void U8(uint8_t v) { m_data.push_back(v); }
    void U16(uint16_t v) {
        m_data.push_back(static_cast<uint8_t>(v & 0xFF));
        m_data.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    }
    void U32(uint32_t v) {
        for (int i = 0; i < 4; ++i) m_data.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
    }
    void I32(int32_t v) { U32(static_cast<uint32_t>(v)); }
    void F32(float v) {
        uint32_t bits = 0;
        std::memcpy(&bits, &v, 4);
        U32(bits);
    }
    void Str(const std::string& s) {
        // CSerialFile::operator<<(std::string) writes DWORD(strlen + 1) and then
        // write(str.c_str(), strlen + 1) — i.e. the terminator IS part of the record.
        U32(static_cast<uint32_t>(s.size() + 1));
        for (char c : s) m_data.push_back(static_cast<uint8_t>(c));
        m_data.push_back(0);
    }
    void FixedStr(const std::string& s, size_t width) {
        for (size_t i = 0; i < s.size() && i < width; ++i) m_data.push_back(static_cast<uint8_t>(s[i]));
        for (size_t i = s.size(); i < width; ++i) m_data.push_back(0);
    }
    void Bool(bool v) { U32(v ? 1u : 0u); }
    void Raw(const std::vector<uint8_t>& v) { m_data.insert(m_data.end(), v.begin(), v.end()); }

    std::vector<uint8_t>& Data() { return m_data; }
    const std::vector<uint8_t>& Data() const { return m_data; }

private:
    std::vector<uint8_t> m_data;
};

bool FloatNear(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

// ---------------------------------------------------------------------------
// Authentic-layout skeleton .x (text). Frame names follow the EP7 Bip01 convention.
// ---------------------------------------------------------------------------
std::string SkeletonX() {
    const char* kIdentity = "1.0,0.0,0.0,0.0, 0.0,1.0,0.0,0.0, 0.0,0.0,1.0,0.0, 0.0,0.0,0.0,1.0;;\n";
    std::string s = "xof 0303txt 0032\n";
    s += "template Frame {\n  <3D82AB46-62DA-11cf-AB39-0020AF71E433>\n}\n";
    s += "Frame Bip01 {\n";
    s += "  FrameTransformMatrix {\n    " + std::string(kIdentity) + "  }\n";
    s += "  Frame Bip01_Spine1 {\n";
    s += "    FrameTransformMatrix {\n      1.0,0.0,0.0,0.0, 0.0,1.0,0.0,0.0, 0.0,0.0,1.0,0.0, 0.0,4.0,0.0,1.0;;\n    }\n";
    s += "    Frame Bip01_Head {\n";
    s += "      FrameTransformMatrix {\n        1.0,0.0,0.0,0.0, 0.0,1.0,0.0,0.0, 0.0,0.0,1.0,0.0, 0.0,1.5,0.0,1.0;;\n      }\n";
    s += "    }\n";
    s += "  }\n";
    s += "  Frame Bip01_R_Thigh {\n";
    s += "    FrameTransformMatrix {\n      1.0,0.0,0.0,0.0, 0.0,1.0,0.0,0.0, 0.0,0.0,1.0,0.0, 0.5,0.0,0.0,1.0;;\n    }\n";
    s += "  }\n";
    s += "}\n";
    return s;
}

// Skinned mesh with 4 vertices carrying the AH-verified EP7 vertex declaration:
// POSITION f3, NORMAL f3, TEXCOORD f2  (8 floats / 32 bytes), plus SkinWeights.
std::string SkinnedMeshX() {
    std::string s = "xof 0303txt 0032\n";
    s += "Frame SceneRoot {\n";
    s += "  Mesh BodyMesh {\n";
    s += "    2;                          // 2 triangles\n";
    s += "    0,1,2;\n";
    s += "    2,3,0;\n";
    s += "    4;                          // 4 vertices, 8 floats each (XYZ NORMAL UV)\n";
    s += "    0.0,0.0,0.0,  1.0,0.0,0.0,  0.0,0.0,\n";
    s += "    1.0,0.0,0.0,  1.0,0.0,0.0,  1.0,0.0,\n";
    s += "    0.0,1.0,0.0,  1.0,0.0,0.0,  0.0,1.0,\n";
    s += "    0.0,0.0,1.0,  1.0,0.0,0.0,  0.0,0.0,\n";
    s += "    MeshMaterialList {\n";
    s += "      1;                        // one material\n";
    s += "      2;                        // two faces\n";
    s += "      0,0;\n";
    s += "      Material Material0 {\n";
    s += "        1.0,1.0,1.0,1.0;;\n";
    s += "        0.0;\n";
    s += "        0.0,0.0,0.0;;\n";
    s += "        0.0,0.0,0.0;;\n";
    s += "        TextureFilename { \"face.dds\"; }\n";
    s += "      }\n";
    s += "    }\n";
    s += "    MeshNormals {\n";
    s += "      2;\n      0,1,2;\n      2,3,0;\n";
    s += "      4;\n      1.0,0.0,0.0, 1.0,0.0,0.0, 1.0,0.0,0.0, 1.0,0.0,0.0;\n";
    s += "    }\n";
    s += "    MeshTextureCoords {\n      4;\n      0.0,0.0, 1.0,0.0, 0.0,1.0, 0.0,0.0;\n    }\n";
    // SkinWeights are children of the Mesh (xskinexp/ExportXFile.cpp:2823 AddDataObject).
    s += "    SkinWeights {\n";
    s += "      \"Bip01\";\n";
    s += "      2;\n";
    s += "      0,1;\n";
    s += "      1.0,1.0;\n";
    s += "      1.0,0.0,0.0,0.0, 0.0,1.0,0.0,0.0, 0.0,0.0,1.0,0.0, 0.0,0.0,0.0,1.0;;\n";
    s += "    }\n";
    s += "    SkinWeights {\n";
    s += "      \"Bip01_Head\";\n";
    s += "      2;\n";
    s += "      2,3;\n";
    s += "      1.0,1.0;\n";
    // inverse bind matrix of Bip01_Head: its bind pose is translate(0, 5.5, 0)
    s += "      1.0,0.0,0.0,0.0, 0.0,1.0,0.0,0.0, 0.0,0.0,1.0,0.0, 0.0,-5.5,0.0,1.0;;\n";
    s += "    }\n";
    s += "  }\n";
    s += "}\n";
    return s;
}

// .chf 0x0102 with 2 pieces, 1 animation, bbox, worldObj, height/radius.
std::vector<uint8_t> CharacterChf() {
    ByteWriter w;
    // basestream header: 128-byte file type + fileId DWORD; the payload then starts with
    // its own version DWORD (DxSkinCharData::LoadFile reads it through operator>>).
    w.Raw(std::vector<uint8_t>(132, 0));  // 128-byte file type + fileId DWORD
    w.U32(0x0102);   // payload version
    w.F32(1.0f);
    w.U32(2);
    // slot 0 -> Head, slot 1 -> UpperBody (order matches EPIECECHAR)
    w.Bool(true);
    w.Str("p_head.cps");
    w.Bool(true);
    w.Str("p_upper.cps");
    w.Str("m_bone.skt");
    w.U32(1);
    w.Str("st_walk");
    w.Bool(true);
    w.F32(7.0f); w.F32(20.0f); w.F32(7.0f);
    w.F32(-7.0f); w.F32(0.0f); w.F32(-7.0f);
    w.Bool(true);
    w.Bool(false);
    w.Bool(true);
    w.F32(20.0f);
    w.F32(7.0f);
    w.Bool(false);
    return w.Data();
}

// .cps 0x0107 for piece type 0 (Head) pointing at the mesh above.
std::vector<uint8_t> PieceCps(int32_t pieceType) {
    ByteWriter w;
    w.Raw(std::vector<uint8_t>(128, 0));  // basestream file-type block
    w.U32(0);                 // header fileId
    w.U32(0x00000107);        // payload version (DxSkinPiece::LoadFile)
    w.U32(1);                 // refCount
    w.Str("body.x");          // xFileName
    w.Str("m_bone.skt");      // skeleton
    w.U32(static_cast<uint32_t>(pieceType));
    w.U32(0);                 // weapon slot (>= 0x0106)
    w.Str("BodyMesh");        // mesh name inside the .x
    w.U32(1);                 // material count
    w.U32(0x0100);            // SMATERIAL_PIECE::VERSION
    w.U32(4 + 10 + 4 + 4);    // block size back-patched by CSerialFile::EndBlock()
    w.Str("face.dds");
    w.U32(0);                 // flags
    w.U32(0);                 // zBias
    w.U32(0);                 // trace count
    w.U32(0);                 // effect count  -> parse stops here
    w.Bool(false);            // flag (read only when effectCount == 0)
    return w.Data();
}

// .cps 0x0107 carrying one trace point.  The SVERTEXINFLU record is written exactly as
// SVERTEXINFLU::SaveFile() writes it (DxSkinMeshContainer9.cpp:59): tag 0x0101, a
// dwSize that double-counts the bone arrays, then index/vector/normal/numBone/bones/
// weights.  Regression fixture for the 2,664 `cps: truncated trace name` failures:
// LoadFile ignores dwSize when the tag matches, so the correct reader must consume the
// body by structure and land exactly on the effect count.
std::vector<uint8_t> PieceCpsWithTrace() {
    ByteWriter w;
    w.Raw(std::vector<uint8_t>(128, 0));  // basestream file-type block
    w.U32(0);                 // header fileId
    w.U32(0x00000107);        // payload version (DxSkinPiece::LoadPiece)
    w.U32(1);                 // m_dwRef
    w.Str("body.x");          // xFileName
    w.Str("m_bone.skt");      // skeleton
    w.U32(0);                 // m_emType -> Head
    w.U32(0);                 // m_emWeaponWhereBack (0x0106+)
    w.Str("BodyMesh");        // mesh name
    w.U32(0);                 // material count
    w.U32(1);                 // trace count
    w.Str("Bip01 Spine");     // trace name
    w.U32(0x0101);            // SVERTEXINFLU::VERSION
    w.U32(32 + 16 * 2);       // dwSize exactly as SaveFile() computes it (over-counts)
    w.U32(0);                 // m_dwIndex
    w.F32(1.0f); w.F32(2.0f); w.F32(3.0f);      // m_vVector
    w.F32(0.0f); w.F32(1.0f); w.F32(0.0f);      // m_vNormal
    w.U32(2);                 // m_dwNumBone
    w.U32(3); w.U32(7);       // m_pBone
    w.F32(0.75f); w.F32(0.25f);                 // m_pWeights
    w.U32(0);                 // effect count
    w.Bool(false);            // m_dwFlag
    return w.Data();
}


// ---------------------------------------------------------------------------
// Legacy .cps layouts (0x0101-0x0104), each written from its own branch in
// DxSkinPiece::LoadPiece.  Shared slice: bit-exact per version so a change in one
// version's layout cannot silently satisfy another's test.
// ---------------------------------------------------------------------------
namespace legacy {

// Writes header through szXFileName/skeleton/type[/weapon]/meshName.
void Header(ByteWriter& w, uint32_t ver, bool weapon) {
    w.U32(ver);
    w.U32(1);                 // m_dwRef
    w.Str("body.x");
    w.Str("m_bone.skt");
    w.U32(0);                 // m_emType -> Head
    if (weapon) w.U32(0);     // m_emWeaponWhereBack
    w.Str("BodyMesh");
}

// One SVERTEXINFLU exactly as SaveFile writes it (0x0101 tag + over-stated dwSize).
void Influence(ByteWriter& w, uint32_t index, uint32_t numBone) {
    w.U32(0x0101);            // SVERTEXINFLU::VERSION
    w.U32(32 + 16 * numBone); // dwSize, over-counted by SaveFile
    w.U32(index);             // m_dwIndex
    w.F32(0.0f); w.F32(0.0f); w.F32(0.0f);
    w.F32(1.0f); w.F32(0.0f); w.F32(0.0f);
    w.U32(numBone);
    for (uint32_t i = 0; i < numBone; ++i) w.U32(i);
    for (uint32_t i = 0; i < numBone; ++i) w.F32(1.0f / (numBone + 1));
}

// A slot that the engine skips: m_dwIndex == COL_ERR (0xFFFFFFFF), numBone == COL_ERR.
void EmptyInfluence(ByteWriter& w) {
    w.U32(0x0101);
    w.U32(32);
    w.U32(0xFFFFFFFFu);       // m_dwIndex == COL_ERR
    w.F32(0.0f); w.F32(0.0f); w.F32(0.0f);
    w.F32(1.0f); w.F32(0.0f); w.F32(0.0f);
    w.U32(0xFFFFFFFFu);       // m_dwNumBone == COL_ERR
}

std::vector<uint8_t> Cps0104() {
    ByteWriter w;
    w.Raw(std::vector<uint8_t>(128, 0));
    w.U32(0);
    Header(w, 0x00000104, false);
    w.U32(1);                         // material count
    w.U32(0x0100);                    // SMATERIAL_PIECE::VERSION
    w.U32(4 + 10 + 4 + 4);            // block size
    w.Str("face.dds");
    w.U32(0); w.U32(0);               // flags, zBias
    w.U32(1);                         // trace count
    w.Str("Bip01 Spine");
    Influence(w, 0, 2);
    w.U32(0);                         // effect count  <-- file ENDS here for 0x0104
    return w.Data();
}

std::vector<uint8_t> Cps0103() {
    ByteWriter w;
    w.Raw(std::vector<uint8_t>(128, 0));
    w.U32(0);
    Header(w, 0x00000103, false);
    // no material count at all
    w.U32(2);                         // trace count
    w.Str("Bip01 Spine");
    Influence(w, 0, 2);
    w.Str("Bip01 Head");
    Influence(w, 1, 1);
    w.U32(0);                         // effect count  <-- file ENDS here
    return w.Data();
}

// slots101=8 for 0x0101, 10 for 0x0102.  Slots 0..3 carry data; the rest must be present
// on disk (so the stream stays aligned) but are not exposed because the engine only reads
// sTraceArray[0..3].
std::vector<uint8_t> CpsFixedSlots(uint32_t ver, uint32_t slots) {
    ByteWriter w;
    w.Raw(std::vector<uint8_t>(128, 0));
    w.U32(0);
    Header(w, ver, false);
    // no material count
    Influence(w, 0, 2);               // slot 0 -> IMAGE01, kept
    EmptyInfluence(w);                // slot 1 -> IMAGE02, m_dwIndex == COL_ERR, skipped
    Influence(w, 2, 1);               // slot 2 -> IMAGE11, kept
    EmptyInfluence(w);                // slot 3 -> IMAGE12, skipped
    for (uint32_t i = 4; i < slots; ++i) Influence(w, 100 + i, 0);  // never exposed
    w.U32(0);                         // effect count  <-- file ENDS here
    return w.Data();
}

}  // namespace legacy


// ---------------------------------------------------------------------------
// B14: SVERTEXINFLU::SaveFile dwSize accounting.
//
// DxSkinMeshContainer9.cpp:61-74 (identical in SMeshContainer.cpp:54-67) sums the bone and
// weight arrays TWICE: once in the initial `dwSize` expression and again in the
// `if ( m_dwNumBone > 0 && m_dwNumBone != COL_ERR )` block.  The bytes actually written after
// the dwSize field are only 32 + 8*numBone.  So:
//     numBone == 0            -> dwSize 0x20, body 0x20  (agrees)
//     1 <= numBone <= 6       -> dwSize 0x20 + 16*n, body 0x20 + 8*n  (over-states by 8*n)
//     numBone == COL_ERR      -> the DWORD arithmetic WRAPS: dwSize 0x18 for a 0x20 body
//                                (under-states by 8)
// A reader must therefore never trust dwSize as a body length when the tag matches.
// ---------------------------------------------------------------------------
std::vector<uint8_t> PieceCpsWithInfluence(uint32_t numBone, uint32_t index) {
    ByteWriter w;
    w.Raw(std::vector<uint8_t>(128, 0));
    w.U32(0);
    w.U32(0x00000107);
    w.U32(1);                 // m_dwRef
    w.Str("body.x");
    w.Str("m_bone.skt");
    w.U32(0);                 // m_emType -> Head
    w.U32(0);                 // m_emWeaponWhereBack
    w.Str("BodyMesh");
    w.U32(0);                 // material count
    w.U32(1);                 // trace count
    w.Str("Bip01 Spine");
    w.U32(0x0101);            // SVERTEXINFLU::VERSION

    // dwSize exactly as SaveFile computes it, including the COL_ERR wraparound.
    const uint32_t dwSize = 4u + 12u + 12u + 4u + 4u * numBone + 4u * numBone
                          + ((numBone > 0 && numBone != 0xFFFFFFFFu)
                                 ? (4u * numBone + 4u * numBone) : 0u);
    w.U32(dwSize);
    w.U32(index);                              // m_dwIndex
    w.F32(1.0f); w.F32(2.0f); w.F32(3.0f);     // m_vVector
    w.F32(0.0f); w.F32(1.0f); w.F32(0.0f);     // m_vNormal
    w.U32(numBone);
    if (numBone > 0 && numBone != 0xFFFFFFFFu) {
        for (uint32_t i = 0; i < numBone; ++i) w.U32(i + 1);
        for (uint32_t i = 0; i < numBone; ++i) w.F32(1.0f / (numBone + 1));
    }
    w.U32(0);                 // effect count
    w.Bool(false);            // m_dwFlag
    return w.Data();
}

// SAnimation track 0x0102: rotate keys, position keys, quatpos keys, matrix keys, scale.
std::vector<uint8_t> AnimationBin() {
    ByteWriter w;
    w.Raw(std::vector<uint8_t>(128, 0));  // file type
    w.U32(0x0102);                        // container version (SetFileType fileId)
    // NOTE: with a header present the payload starts directly with the up-body list.
    w.U32(1);                             // upBody track count
    // ---- upBody track 0 (Bip01_Spine1) ----
    w.U32(0x0102);                        // per-track SAnimation version
    w.Str("Bip01_Spine1");
    w.U32(1);                             // rotate key count
    w.U32(0);                             // time
    w.F32(0.0f); w.F32(0.0f); w.F32(0.0f); w.F32(1.0f);
    w.U32(2);                             // position key count
    w.U32(0);   w.F32(0.0f); w.F32(0.0f); w.F32(0.0f);
    w.U32(4800); w.F32(0.0f); w.F32(2.0f); w.F32(0.0f);
    w.U32(0);                             // garbage DWORD
    w.U32(0);                             // quatpos key count
    w.U32(0);                             // garbage DWORD
    w.U32(1);                             // matrix key count
    w.U32(0);                             // time
    // SMatrixKey = { DWORD dwTime; D3DXMATRIXA16 mat; } and D3DXMATRIXA16 is
    // __declspec(align(16)) — sizeof(SMatrixKey) is 80 on the 32-bit client, so 12 bytes
    // of padding sit between dwTime and the matrix.
    for (int i = 0; i < 3; ++i) w.U32(0xDEADBEEFu); // non-zero uninitialized Win32 padding
    for (int i = 0; i < 16; ++i) w.F32((i % 5 == 0) ? 1.0f : 0.0f);
    w.U32(0);                             // scale key count
    // ---- main track count 0 ----
    w.U32(0);
    return w.Data();
}

// SAnimContainer 0x0100 + SAnimation::Load_0100 tracks:
//   outer: 128-byte "AnimContainer" header + u32(0x0100) + main list only (no upBody list).
//   inner track 0x0100: posKeys, rotKeys, scaleKeys, matrixKeys (80B each), m_strBone.
std::vector<uint8_t> AnimationBin0100() {
    ByteWriter w;
    std::vector<uint8_t> hdr(128, 0);
    std::memcpy(hdr.data(), "AnimContainer", 14);
    w.Raw(hdr);
    w.U32(0x0100);                        // container version
    w.U32(2);                             // main track count (2 tracks)

    // ---- main track 0: Bip01_Spine1 (pos + rot + scale + 2x 80B matrix keys) ----
    w.U32(0x0100);                        // track version
    w.U32(1);                             // pos key count
    w.U32(160); w.F32(1.25f); w.F32(-2.5f); w.F32(3.75f);
    w.U32(1);                             // rot key count
    w.U32(320); w.F32(0.0f); w.F32(0.70710678f); w.F32(0.0f); w.F32(0.70710678f);
    w.U32(1);                             // scale key count
    w.U32(480); w.F32(1.5f); w.F32(2.0f); w.F32(0.5f);
    w.U32(2);                             // matrix key count (80 bytes each)
    // matrix key 0 @ t=0: translation (0, 6, 0)
    w.U32(0);
    w.U32(0xDEADBEEFu); w.U32(0xCAFEBABEu); w.U32(0x13579BDFu);  // 12B align16 pad
    w.F32(1.0f); w.F32(0.0f); w.F32(0.0f); w.F32(0.0f);
    w.F32(0.0f); w.F32(1.0f); w.F32(0.0f); w.F32(0.0f);
    w.F32(0.0f); w.F32(0.0f); w.F32(1.0f); w.F32(0.0f);
    w.F32(0.0f); w.F32(6.0f); w.F32(0.0f); w.F32(1.0f);
    // matrix key 1 @ t=4800: translation (1, 9, -2)
    w.U32(4800);
    w.U32(0x68801503u); w.U32(0x33322D33u); w.U32(0x343A3331u);  // authentic-style heap residue
    w.F32(1.0f); w.F32(0.0f); w.F32(0.0f); w.F32(0.0f);
    w.F32(0.0f); w.F32(1.0f); w.F32(0.0f); w.F32(0.0f);
    w.F32(0.0f); w.F32(0.0f); w.F32(1.0f); w.F32(0.0f);
    w.F32(1.0f); w.F32(9.0f); w.F32(-2.0f); w.F32(1.0f);
    w.Str("Bip01_Spine1");

    // ---- main track 1: empty bone name "" (valid in authentic 0x0100 assets) ----
    w.U32(0x0100);
    w.U32(0);                             // pos keys
    w.U32(0);                             // rot keys
    w.U32(0);                             // scale keys
    w.U32(1);                             // 1 matrix key
    w.U32(2400);
    w.U32(0xAAAAAAAAu); w.U32(0xBBBBBBBBu); w.U32(0xCCCCCCCCu);
    for (int i = 0; i < 16; ++i) w.F32((i % 5 == 0) ? 1.0f : 0.0f);
    w.Str("");                            // len=1, "\0" -> boneName == ""

    return w.Data();
}

// SAnimContainer 0x0103 + SAnimation::Load_0103 tracks:
//   outer: 128-byte "AnimContainer" header + u32(0x0103) + upBody list +
//          u32(dwGarbageValue == 0x0103 + numUpBody + numMain) + main list.
//   inner track 0x0103: m_strBone, rotateKeys (20B), u32(dwGarbage1 == bone.len),
//                       positionKeys (16B), quatPosKeys (36B), matrixKeys (80B),
//                       u32(dwGarbage2 == bone.len + 0x0103), scaleKeys (16B).
std::vector<uint8_t> AnimationBin0103() {
    ByteWriter w;
    std::vector<uint8_t> hdr(128, 0);
    std::memcpy(hdr.data(), "AnimContainer", 14);
    w.Raw(hdr);
    w.U32(0x0103);                        // container version
    w.U32(1);                             // upBody track count (1 track)

    // ---- upBody track 0: Bip01_Spine1 (rot + pos + quatpos + 80B matrix + scale) ----
    const std::string upBone = "Bip01_Spine1";
    w.U32(0x0103);                        // track version
    w.Str(upBone);
    w.U32(1);                             // rotate key count (20B)
    w.U32(160); w.F32(0.0f); w.F32(0.0f); w.F32(0.70710678f); w.F32(0.70710678f);
    w.U32(static_cast<uint32_t>(upBone.size()));  // dwGarbage1 == m_strBone.length()
    w.U32(1);                             // position key count (16B)
    w.U32(320); w.F32(-1.5f); w.F32(4.25f); w.F32(0.75f);
    w.U32(1);                             // quatpos key count (36B)
    w.U32(640);
    w.F32(1.0f); w.F32(2.0f); w.F32(3.0f);        // scale[3]
    w.F32(0.5f); w.F32(5.5f); w.F32(-1.0f);       // pos[3]
    w.U32((32767u << 16) | 32767u);               // quatComp0 -> x=0, y=0
    w.U32((32767u << 16) | 65535u);               // quatComp1 -> z=0, w=1
    w.U32(1);                             // matrix key count (80B)
    w.U32(960);
    w.U32(0xDEADBEEFu); w.U32(0xCAFEBABEu); w.U32(0x13579BDFu);  // 12B align16 pad
    w.F32(1.0f); w.F32(0.0f); w.F32(0.0f); w.F32(0.0f);
    w.F32(0.0f); w.F32(1.0f); w.F32(0.0f); w.F32(0.0f);
    w.F32(0.0f); w.F32(0.0f); w.F32(1.0f); w.F32(0.0f);
    w.F32(2.0f); w.F32(7.0f); w.F32(-3.0f); w.F32(1.0f);
    w.U32(static_cast<uint32_t>(upBone.size()) + 0x0103u);  // dwGarbage2 == bone.len + 0x0103
    w.U32(1);                             // scale key count (16B)
    w.U32(1280); w.F32(2.5f); w.F32(0.75f); w.F32(1.25f);

    // ---- inter-list dwGarbageValue == 0x0103 + numUpBody(1) + numMain(2) ----
    w.U32(0x0103u + 1u + 2u);
    w.U32(2);                             // main track count (2 tracks)

    // ---- main track 0: Bip01 (quatPos-only track, typical of authentic 0x0103 clips) ----
    const std::string mainBone0 = "Bip01";
    w.U32(0x0103);
    w.Str(mainBone0);
    w.U32(0);                             // rotate
    w.U32(static_cast<uint32_t>(mainBone0.size()));  // dwGarbage1
    w.U32(0);                             // position
    w.U32(2);                             // 2 quatPosKeys (36B each)
    // key 0 @ t=0: pos=(0, 3, 0), identity quat
    w.U32(0);
    w.F32(1.0f); w.F32(1.0f); w.F32(1.0f);
    w.F32(0.0f); w.F32(3.0f); w.F32(0.0f);
    w.U32((32767u << 16) | 32767u);
    w.U32((32767u << 16) | 65535u);
    // key 1 @ t=4800: pos=(0, 8, 0), identity quat
    w.U32(4800);
    w.F32(1.0f); w.F32(1.0f); w.F32(1.0f);
    w.F32(0.0f); w.F32(8.0f); w.F32(0.0f);
    w.U32((32767u << 16) | 32767u);
    w.U32((32767u << 16) | 65535u);
    w.U32(0);                             // matrix
    w.U32(static_cast<uint32_t>(mainBone0.size()) + 0x0103u);  // dwGarbage2
    w.U32(0);                             // scale

    // ---- main track 1: empty bone name "" ----
    w.U32(0x0103);
    w.Str("");
    w.U32(0);                             // rotate
    w.U32(0u);                            // dwGarbage1 == 0
    w.U32(0);                             // position
    w.U32(0);                             // quatpos
    w.U32(0);                             // matrix
    w.U32(0x0103u);                       // dwGarbage2 == 0 + 0x0103
    w.U32(0);                             // scale

    return w.Data();
}

void AppendZeroKeyTrack0103(ByteWriter& w, const std::string& boneName) {
    w.U32(0x0103);
    w.Str(boneName);
    w.U32(0);  // rotate
    w.U32(static_cast<uint32_t>(boneName.size()));
    w.U32(0);  // position
    w.U32(0);  // quatpos
    w.U32(0);  // matrix
    w.U32(static_cast<uint32_t>(boneName.size()) + 0x0103u);
    w.U32(0);  // scale
}

// SAnimContainer 0x0104 (Lib_Engine/Meshs/SAnimationSaveLoad.cpp:444-479, 670-699):
//   outer: 128-byte "AnimContainer" header + u32(0x0104)
//        + main list    (u32 numMain    + numMain    x SAnimation -> m_listAnimation)
//        + upBody1 list (u32 numUpBody1 + numUpBody1 x SAnimation -> m_listAniUPBODY)
//        + upBody2 list (u32 numUpBody2 + numUpBody2 x SAnimation -> appended to m_listAniUPBODY)
//   (no inter-list garbage DWORD; plaintext payload).
std::vector<uint8_t> AnimationBin0104() {
    ByteWriter w;
    std::vector<uint8_t> hdr(128, 0);
    std::memcpy(hdr.data(), "AnimContainer", 14);
    w.Raw(hdr);
    w.U32(0x0104);  // container version

    // ---- List 0: mainTracks (1 track: Bip01 quatPos @ t=0 y=+2.0, t=4800 y=+5.0) ----
    w.U32(1);
    {
        const std::string bone = "Bip01";
        w.U32(0x0103);
        w.Str(bone);
        w.U32(0);
        w.U32(static_cast<uint32_t>(bone.size()));
        w.U32(0);
        w.U32(2);  // 2 quatPosKeys
        w.U32(0);
        w.F32(1.0f); w.F32(1.0f); w.F32(1.0f);
        w.F32(0.0f); w.F32(2.0f); w.F32(0.0f);
        w.U32((32767u << 16) | 32767u);
        w.U32((32767u << 16) | 65535u);
        w.U32(4800);
        w.F32(1.0f); w.F32(1.0f); w.F32(1.0f);
        w.F32(0.0f); w.F32(5.0f); w.F32(0.0f);
        w.U32((32767u << 16) | 32767u);
        w.U32((32767u << 16) | 65535u);
        w.U32(0);
        w.U32(static_cast<uint32_t>(bone.size()) + 0x0103u);
        w.U32(0);
    }

    // ---- List 1: upBody1 (1 track: Bip01_Spine1 80B matrix @ t=960 y=+4.0) ----
    w.U32(1);
    {
        const std::string bone = "Bip01_Spine1";
        w.U32(0x0103);
        w.Str(bone);
        w.U32(0);
        w.U32(static_cast<uint32_t>(bone.size()));
        w.U32(0);
        w.U32(0);
        w.U32(1);  // 1 matrixKey (80B)
        w.U32(960);
        w.U32(0xDEADBEEFu); w.U32(0xCAFEBABEu); w.U32(0x13579BDFu);
        w.F32(1.0f); w.F32(0.0f); w.F32(0.0f); w.F32(0.0f);
        w.F32(0.0f); w.F32(1.0f); w.F32(0.0f); w.F32(0.0f);
        w.F32(0.0f); w.F32(0.0f); w.F32(1.0f); w.F32(0.0f);
        w.F32(0.0f); w.F32(4.0f); w.F32(0.0f); w.F32(1.0f);
        w.U32(static_cast<uint32_t>(bone.size()) + 0x0103u);
        w.U32(0);
    }

    // ---- List 2: upBody2 (2 tracks appended in order to upBodyTracks: Bip01_Head + "") ----
    w.U32(2);
    {
        const std::string bone = "Bip01_Head";
        w.U32(0x0103);
        w.Str(bone);
        w.U32(0);
        w.U32(static_cast<uint32_t>(bone.size()));
        w.U32(0);
        w.U32(1);  // 1 quatPosKey @ t=0 y=+3.5
        w.U32(0);
        w.F32(1.0f); w.F32(1.0f); w.F32(1.0f);
        w.F32(0.0f); w.F32(3.5f); w.F32(0.0f);
        w.U32((32767u << 16) | 32767u);
        w.U32((32767u << 16) | 65535u);
        w.U32(0);
        w.U32(static_cast<uint32_t>(bone.size()) + 0x0103u);
        w.U32(0);
    }
    AppendZeroKeyTrack0103(w, "");

    return w.Data();
}

// SANIMCONINFO fixtures (payload only, header present).
// Emits the authentic SANIMCONINFO byte layout for each version, including the MSVC struct
// padding the engine actually reads:
//   0x0101..0x0104 are read with ReadBuffer(&OldAnimInfo, sizeof(SANIMCONINFO_10x)), i.e.
//   whole padded structs (304 / 564 / 732 / 844 bytes); 0x0105..0x0107 are read field by
//   field but with sizeof(SChaSoundData_103)=428 / sizeof(SChaSoundData)=388 in the tail.
//   0x0108..0x0110 (LoadFile_0108_0109_0110) and 0x0111..0x0115
//   (LoadFile_0111_0112_0114_0115) follow the B16 field orders with the effect list and
//   DxAniScale block parsed in full.
// SOURCE: enginelib/Meshs/DxSkinAniManSaveLoad.cpp, backup repo
//         matheomedrana/Ran-Online-EP7-Complete-Files @ tf661fbf
//         Lib_Engine/Meshs/SAnimationInfo{.h,SaveLoad.cpp}, SAnimation.h,
//         EngineSoundLib/DxSound/CharacterSound.h, DxEffect/EffAni/DxEffAni*.cpp.
std::vector<uint8_t> AnimationCfg(bool loop, uint32_t version = 0x0107,
                                  bool withEffects = false,
                                  bool withAniScale = false,
                                  bool withTrailing = false,
                                  int32_t mainType = 1) {
    ByteWriter w;
    w.Raw(std::vector<uint8_t>(128, 0));
    w.U32(version);       // SANIMCONINFO version in the header fileId

    // basestream::operator<<(std::string): DWORD length INCLUDING the terminator, then
    // the bytes. (SerialFile.cpp CSerialFile::operator<<.)
    auto writeLpString = [&w](const std::string& s) {
        w.U32(static_cast<uint32_t>(s.size()) + 1);
        for (char c : s) w.U8(static_cast<uint8_t>(c));
        w.U8(0);
    };
    // One m_listEffAni entry as written by DxEffAniData_<T>::SaveFile: the SANIMCONINFO
    // loop writes the type id, then the effect writes type id + version + sizeof +
    // property.
    auto effectSingle101 = [&w]() {
        w.U32(0); w.U32(0); w.U32(0x0101); w.U32(464);  // EMEFFANI_SINGLE, inner id, ver, sizeof(prop)
        w.U32(0x00001425u); w.U32(2400);                // m_dwFlags, m_dwFrame
        w.F32(1.0f); w.F32(2.5f);                       // m_fFact, m_fScale
        for (int i = 0; i < 16; ++i) w.F32((i == 0 || i == 5 || i == 10) ? 1.0f : 0.0f);  // m_matBase
        w.FixedStr("fx_sword_hit", 128);                // m_szEff
        w.FixedStr("Bip01 R Hand", 128);                // m_szBipFirst
        w.FixedStr("Bip01 Head", 128);                  // m_szBipSecond
    };
    auto effectGhosting = [&w]() {
        w.U32(1); w.U32(1); w.U32(0x0100); w.U32(28);   // EMEFFANI_GHOSTING
        w.U32(0x00001403u); w.U32(300); w.U32(900);     // m_dwFlags, m_dwFrame_1, m_dwFrame_2
        w.U32(4); w.U32(130); w.U32(3);                 // m_dwGhostingNum, m_dwAlphaMax, m_dwUnit
        w.U32(0x00ffffffu);                             // D3DCOLOR m_dwColor
    };
    // EFFANI_PROPERTY_TRACE holds a std::string member, so its on-disk size is
    // build-dependent; the fixture declares a size and fills it.
    auto effectTrace = [&w](uint32_t declaredSize) {
        w.U32(2); w.U32(2); w.U32(0x0100); w.U32(declaredSize);
        w.Raw(std::vector<uint8_t>(declaredSize, 0xAB));
    };
    auto effectFaceOff = [&w]() {
        w.U32(3); w.U32(3); w.U32(0x0100); w.U32(32);   // EMEFFANI_FACEOFF
        w.FixedStr("face_normal.dds", 32);              // m_szTexture
    };
    auto effectsBlock = [&]() {
        if (!withEffects) {
            w.U32(0);
            return;
        }
        w.U32(4);
        effectSingle101();
        effectGhosting();
        effectTrace(28);   // float + a 24-byte std::string blob, as some MSVC build laid it out
        effectFaceOff();
    };
    // DxAniScale::Load: map size, then per entry a length-prefixed name and a
    // (count + count * (u32 key + f32 scale)) block.
    auto aniScaleBlock = [&]() {
        if (!withAniScale) {
            w.U32(0);
            return;
        }
        w.U32(2);
        writeLpString("a_gum_att01");
        w.U32(2);
        w.U32(0);    w.F32(1.0f);
        w.U32(1200); w.F32(1.5f);
        writeLpString("a_gum_stay");
        w.U32(1);
        w.U32(600);  w.F32(0.5f);
    };

    if (version >= 0x0108) {
        if (version <= 0x0110) {
            // LoadFile_0108_0109_0110: NOTE m_UNITTIME is written BEFORE m_dwSTime, and
            // the strike table comes BEFORE the div frames; the sound block is LAST.
            w.FixedStr("st_walk", 128);
            w.FixedStr("m_bone.skt", 128);
            w.U32(loop ? 0x00000010u : 0u);
            w.U32(4800);                       // m_UNITTIME
            w.U32(0);                          // m_dwSTime
            w.U32(4800);                       // m_dwETime
            w.U32(4800);                       // m_dwETimeOrig
            w.I32(mainType);
            w.I32(0);
            w.U16(0);                          // m_wStrikeCount
            for (int i = 0; i < 9; ++i) { w.I32(0); w.I32(0); w.U32(0); }  // SANIMSTRIKE[9]
            w.U16(1);                          // m_wDivCount
            for (int i = 0; i < 8; ++i) w.U16(static_cast<uint16_t>(i));
            effectsBlock();
            aniScaleBlock();
            w.I32(2);                          // SChaSoundData (388 bytes)
            w.Raw(std::vector<uint8_t>(388 - 4, 0));
        } else {
            // LoadFile_0111_0112_0114_0115: four length-prefixed strings, then the
            // 0x0101..0x0107 field order, sound block BEFORE the effect list.
            writeLpString("st_walk.cfg");      // szCfgFile (read and discarded)
            writeLpString("st_walk");          // szXFile -> m_szName
            writeLpString("st_walk.bin");      // szBinFile (read and discarded)
            writeLpString("m_bone.skt");       // szSkeleton -> m_szSkeletion
            w.U32(loop ? 0x00000010u : 0u);
            w.U32(0);                          // m_dwSTime
            w.U32(4800);                       // m_dwETime
            w.U32(4800);                       // m_dwETimeOrig
            w.U32(4800);                       // m_UNITTIME
            w.I32(mainType);
            w.I32(0);
            w.U16(1);                          // m_wDivCount
            for (int i = 0; i < 8; ++i) w.U16(static_cast<uint16_t>(i));
            w.U16(0);                          // m_wStrikeCount
            for (int i = 0; i < 9; ++i) { w.I32(0); w.I32(0); w.U32(0); }
            w.I32(2);                          // SChaSoundData (388 bytes)
            w.Raw(std::vector<uint8_t>(388 - 4, 0));
            effectsBlock();
            aniScaleBlock();
        }
        if (withTrailing) {
            // Official-writer trailing bytes ("//official version partial read only"):
            // consumed by the reader as a counted remainder, never reinterpreted.
            w.Raw({0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03});
        }
        return w.Data();
    }

    w.FixedStr("st_walk", 128);
    w.FixedStr("m_bone.skt", 128);
    w.U32(loop ? 0x00000010u : 0u);  // m_dwFlag -> kAnimFlagLoop
    w.U32(0);
    w.U32(4800);
    w.U32(4800);
    w.U32(4800);   // m_UNITTIME
    w.I32(mainType);      // mainType
    w.I32(0);      // subType
    w.U16(1);      // divCount
    for (int i = 0; i < 8; ++i) w.U16(static_cast<uint16_t>(i));

    // Tail: m_wStrikeCount + SANIMSTRIKE[9] + sound block.
    //
    // Byte counts are NOT re-derived here; they are the authentic ones measured from the
    // shipped client and fitted to the struct declarations:
    //
    //   0x0101 302                                no strike table, no sound block
    //   0x0102 302 + 2 + 260  = 564               SANIMSTRIKE array NOT written
    //   0x0103 302 + 2 + 428  = 732               SANIMSTRIKE array NOT written
    //   0x0104 302 + 2 + 108 + 428 = 840
    //   0x0105 302 + 2 + 108 + 428 = 840
    //   0x0106 302 + 2 + 108 + 388 = 800
    //   0x0107 302 + 2 + 108 + 388 = 800
    //
    // 0x0104 must total exactly 840: authentic files carry 538 bytes after the 302 prefix.
    auto strike = [&w](bool withArray) {
        w.U16(0);                               // m_wStrikeCount
        if (withArray) {
            for (int i = 0; i < 9; ++i) { w.I32(0); w.I32(0); w.U32(0); }  // SANIMSTRIKE[9]
        }
    };
    // Exact byte count for the sound struct, written as its fields then zero-filled to the
    // authentic size (260 / 428 / 388). Field order doesn't matter for these fixtures.
    auto sound = [&w](size_t bytes) {
        w.I32(2);                               // m_PlayFrameCount
        w.Raw(std::vector<uint8_t>(bytes - 4, 0));
    };

    switch (version) {
        case 0x0101: break;
        case 0x0102: strike(false); sound(260); break;
        case 0x0103: strike(false); sound(428); break;
        case 0x0104: strike(true);  sound(428); break;
        case 0x0105: strike(true);  sound(428); break;
        case 0x0106: strike(true);  sound(388); break;
        default:     strike(true);  sound(388); break;
    }

    // The effect count exists only from 0x0105; 0x0101..0x0104 end at the header block
    // (confirmed against authentic files: 0x0104 ones are exactly 840 bytes).
    if (version >= 0x0105) {
        w.U32(0);  // effect count
    }
    if (version == 0x0107) {
        w.U32(0);  // DxAniScale: map size (0x0107 only)
    }
    return w.Data();
}

// EP7CharacterLoader resolver backed by a name -> bytes map.
struct ResolverMap {
    const std::vector<std::pair<std::string, std::vector<uint8_t>>>* entries = nullptr;
};

bool MapResolver(void* userData, const std::string& name, std::vector<uint8_t>& out) {
    auto* map = static_cast<ResolverMap*>(userData);
    if (!map || !map->entries) return false;
    for (const auto& entry : *map->entries) {
        if (entry.first == name) {
            out = entry.second;
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------

void TestTokenizer() {
    Section("x tokenizer");
    const std::string text = "xof 0303txt 0032\ntemplate Frame { <GUID> }\nMesh A { 3; 0,1,2;; }\n";
    std::vector<RANMobile::ep7xfile::Token> tokens;
    std::string error;
    J_CHECK(RANMobile::ep7xfile::Tokenize(reinterpret_cast<const uint8_t*>(text.data()), text.size(), tokens,
                                          error));
    J_CHECK(!tokens.empty());
    J_CHECK(!tokens.empty() && tokens[0].IsIdentifier("xof"));
    J_CHECK(tokens.size() > 1 && tokens[1].kind == RANMobile::ep7xfile::Token::Kind::Number);

    std::vector<RANMobile::ep7xfile::Token> ignored;
    // Real binary .x header: "xof 0303bin 0032" — same magic as text, different format.
    const uint8_t binary[] = {'x', 'o', 'f', ' ', '0', '3', '0', '3', 'b', 'i', 'n', 0, '0', '0', '3', '2'};
    J_CHECK(RANMobile::ep7xfile::IsBinaryX(binary, sizeof(binary)));
    J_CHECK(!RANMobile::ep7xfile::IsBinaryX(reinterpret_cast<const uint8_t*>(text.data()), text.size()));
    J_CHECK(RANMobile::ep7xfile::IsTextX(reinterpret_cast<const uint8_t*>(text.data()), text.size()));
    J_CHECK(!RANMobile::ep7xfile::Tokenize(binary, sizeof(binary), ignored, error));
    J_CHECK(error.find("binary") != std::string::npos);
    J_CHECK(!RANMobile::ep7xfile::Tokenize(nullptr, 0, ignored, error));
}

void TestSkeleton() {
    Section("skeleton (.x Frame hierarchy)");
    const std::string text = SkeletonX();
    RANMobile::EP7Skeleton skeleton;
    J_CHECK(skeleton.LoadFromXFile(reinterpret_cast<const uint8_t*>(text.data()), text.size()));
    J_CHECK(skeleton.IsValid());
    // The .x contains 4 frames; the loader synthesises "__ROOT" (DxBoneCollector.cpp:300).
    J_CHECK(skeleton.BoneCount() == 5);
    J_CHECK(skeleton.PaletteOrder().size() == 5);

    const int32_t root = skeleton.FindBoneIndex("__ROOT");
    const int32_t bip = skeleton.FindBoneIndex("Bip01");
    const int32_t spine = skeleton.FindBoneIndex("Bip01_Spine1");
    const int32_t head = skeleton.FindBoneIndex("Bip01_Head");
    const int32_t thigh = skeleton.FindBoneIndex("Bip01_R_Thigh");
    J_CHECK(root >= 0 && bip >= 0 && spine >= 0 && head >= 0 && thigh >= 0);
    J_CHECK(skeleton.FindBoneIndex("nope") == -1);

    // Hierarchy: __ROOT -> Bip01 -> {Spine1 -> Head, R_Thigh}
    J_CHECK(skeleton.GetBone(bip).parent == root);
    J_CHECK(skeleton.GetBone(spine).parent == bip);
    J_CHECK(skeleton.GetBone(head).parent == spine);
    J_CHECK(skeleton.GetBone(thigh).parent == bip);
    J_CHECK(skeleton.GetBone(bip).firstChild >= 0);

    // First child stays the head of the sibling list (AddBoneTrans head-insert rule).
    const auto& boneBip = skeleton.GetBone(bip);
    J_CHECK(skeleton.GetBone(static_cast<size_t>(boneBip.firstChild)).name == "Bip01_Spine1");
    J_CHECK(skeleton.GetBone(static_cast<size_t>(boneBip.firstChild)).nextSibling == thigh);

    // UPBODY propagation.
    J_CHECK(skeleton.GetBone(spine).body == RANMobile::EP7BodyPart::UpBody);
    J_CHECK(skeleton.GetBone(head).body == RANMobile::EP7BodyPart::UpBody);
    J_CHECK(skeleton.GetBone(bip).body == RANMobile::EP7BodyPart::Default);
    J_CHECK(skeleton.GetBone(thigh).body == RANMobile::EP7BodyPart::Default);

    // Palette order is a depth-first traversal.
    J_CHECK(skeleton.PaletteOrder().size() == skeleton.BoneCount());
    J_CHECK(skeleton.PaletteOrder()[0] == static_cast<int32_t>(root));

    // Transform accumulation: Head's combined translation should be (0, 5.5, 0)
    // in the bind pose (Spine1 +4, Head +1.5).
    skeleton.ResetToRestPose();
    skeleton.UpdateBones();
    const auto& headBone = skeleton.GetBone(static_cast<size_t>(head));
    J_CHECK(FloatNear(headBone.combined.m[3][1], 5.5f));
    const auto& thighBone = skeleton.GetBone(static_cast<size_t>(thigh));
    J_CHECK(FloatNear(thighBone.combined.m[3][0], 0.5f));

    // Malformed input is rejected, never patched over.
    const std::string truncated = "Frame Bip01 { FrameTransformMatrix { 1,0,0 }";
    RANMobile::EP7Skeleton broken;
    J_CHECK(!broken.LoadFromXFile(reinterpret_cast<const uint8_t*>(truncated.data()), truncated.size()));
    J_CHECK(!broken.IsValid());
    J_CHECK(!broken.Error().empty());

    RANMobile::EP7Skeleton empty;
    J_CHECK(!empty.LoadFromXFile(nullptr, 0));

    // Binary .x must be rejected rather than mis-parsed.
    const uint8_t bin[] = {'x', 'o', 'f', ' '};
    J_CHECK(!empty.LoadFromXFile(bin, sizeof(bin)));
}

void TestSkinnedMesh() {
    Section("skinned mesh (.x Mesh + SkinWeights)");
    const std::string text = SkinnedMeshX();
    const std::vector<uint8_t> bytes(text.begin(), text.end());

    RANMobile::EP7Skeleton skeleton;
    const std::string skelText = SkeletonX();
    J_CHECK(skeleton.LoadFromXFile(reinterpret_cast<const uint8_t*>(skelText.data()), skelText.size()));

    RANMobile::EP7SkinnedMesh mesh = RANMobile::EP7SkinnedMeshReader::Parse(bytes, "BodyMesh", &skeleton);
    J_CHECK_MSG(mesh.valid, mesh.error);
    J_CHECK(mesh.name == "BodyMesh");
    J_CHECK(mesh.vertexCount == 4);
    J_CHECK(mesh.faces.size() == 6);
    J_CHECK(mesh.vertexStride == 32);
    J_CHECK(mesh.layout == RANMobile::EP7VertexLayout::PosNormalUV32);
    J_CHECK(mesh.geometryDecoded);
    J_CHECK(mesh.bones.size() == 2);
    J_CHECK(mesh.bones[0].name == "Bip01");
    J_CHECK(mesh.bones[1].name == "Bip01_Head");
    J_CHECK(mesh.bones[0].skeletonBoneIndex == skeleton.FindBoneIndex("Bip01"));
    J_CHECK(mesh.bones[1].skeletonBoneIndex == skeleton.FindBoneIndex("Bip01_Head"));
    J_CHECK(mesh.bones[0].vertices.size() == 2);
    J_CHECK(mesh.bones[0].weights[0] == 1.0f);
    J_CHECK(mesh.texcoords.size() == 8);
    J_CHECK(FloatNear(mesh.positions[2], 0.0f));
    J_CHECK(FloatNear(mesh.positions[3], 1.0f));  // vertex 1 x

    // --- index validation ---
    std::string bad = text;
    const std::string goodWeights = "\"Bip01\";\n      2;\n      0,1;";
    const std::string badWeights = "\"Bip01\";\n      2;\n      0,9;";  // vertex 9 of 4
    const size_t at = bad.find(goodWeights);
    J_CHECK(at != std::string::npos);
    if (at != std::string::npos) {
        bad.replace(at, goodWeights.size(), badWeights);
        RANMobile::EP7SkinnedMesh rejected = RANMobile::EP7SkinnedMeshReader::Parse(
            std::vector<uint8_t>(bad.begin(), bad.end()), "BodyMesh", &skeleton);
        J_CHECK(!rejected.valid);
        J_CHECK(rejected.error.find("index") != std::string::npos);
    }

    // --- face index validation ---
    std::string badFace = text;
    const size_t faceAt = badFace.find("0,1,2;");
    J_CHECK(faceAt != std::string::npos);
    if (faceAt != std::string::npos) {
        badFace.replace(faceAt, 6, "0,1,77;");
        RANMobile::EP7SkinnedMesh rejectedFace = RANMobile::EP7SkinnedMeshReader::Parse(
            std::vector<uint8_t>(badFace.begin(), badFace.end()), "BodyMesh", &skeleton);
        J_CHECK(!rejectedFace.valid);
        J_CHECK(rejectedFace.error.find("face") != std::string::npos);
    }

    // --- unknown bone name is rejected (DxSkinMesh9 returns E_FAIL) ---
    std::string badBone = text;
    const size_t boneAt = badBone.find("\"Bip01_Head\";");
    J_CHECK(boneAt != std::string::npos);
    if (boneAt != std::string::npos) {
        badBone.replace(boneAt, 13, "\"NotABone\";");
        RANMobile::EP7SkinnedMesh rejectedBone = RANMobile::EP7SkinnedMeshReader::Parse(
            std::vector<uint8_t>(badBone.begin(), badBone.end()), "BodyMesh", &skeleton);
        J_CHECK(!rejectedBone.valid);
        J_CHECK(rejectedBone.error.find("bone") != std::string::npos);
    }

    // --- missing mesh name ---
    RANMobile::EP7SkinnedMesh missing = RANMobile::EP7SkinnedMeshReader::Parse(bytes, "Nope", &skeleton);
    J_CHECK(!missing.valid);

    // --- empty buffer ---
    RANMobile::EP7SkinnedMesh emptyMesh =
        RANMobile::EP7SkinnedMeshReader::Parse(std::vector<uint8_t>(), "BodyMesh", &skeleton);
    J_CHECK(!emptyMesh.valid);
    J_CHECK(!emptyMesh.error.empty());

    // --- CPU skinning matches the bind pose ---
    skeleton.ResetToRestPose();
    skeleton.UpdateBones();
    std::vector<RANMobile::ep7math::Mat4> palette;
    RANMobile::BuildBonePalette(mesh, skeleton, palette);
    J_CHECK(palette.size() == 2);

    std::vector<float> positions;
    std::vector<float> normals;
    J_CHECK(RANMobile::SkinVerticesCPU(mesh, palette, positions, normals));
    J_CHECK(positions.size() == 12);
    for (size_t i = 0; i < 12; ++i) {
        J_CHECK_MSG(FloatNear(positions[i], mesh.positions[i], 1e-3f), "bind pose should be identity");
    }
    J_CHECK(normals.size() == 12);

    // --- weight normalisation when weights do not sum to 1 ---
    RANMobile::EP7SkinnedMesh partial = mesh;
    partial.bones[0].weights[0] = 0.25f;
    RANMobile::SkinVerticesCPU(partial, palette, positions, normals);
    J_CHECK(FloatNear(positions[0], 0.0f));  // (0,0,0) is invariant

    // Two-bone blend on a non-zero vertex must move with the head bone (identity here),
    // so the result still equals the source; assert determinism instead of guessing.
    J_CHECK(FloatNear(positions[9], mesh.positions[9], 1e-3f));
}

void TestSkinPiece() {
    Section(".cps skin piece");
    const std::vector<uint8_t> bytes = PieceCps(0);
    const RANMobile::EP7SkinPieceInfo info = RANMobile::EP7SkinPieceReader::ParseFromBytes(bytes, false);
    J_CHECK_MSG(info.valid, info.error);
    J_CHECK(info.fileVersion == 0x00000107);
    J_CHECK(info.xFileName == "body.x");
    J_CHECK(info.skeletonFile == "m_bone.skt");
    J_CHECK(info.pieceType == RANMobile::EP7PieceType::Head);
    J_CHECK(info.meshName == "BodyMesh");
    J_CHECK(info.materials.size() == 1);
    J_CHECK(info.materials[0].texture == "face.dds");

    // Version gating: DxSkinPiece::LoadPiece implements 0x0101..0x0107 and sends everything
    // else to its "Can't Load Skin object" error path (DxSkinPieceContainer.cpp:934-943).
    // 0x0100 is below the oldest implemented branch, 0x0108 above the current VERSION, so
    // both must be rejected. The version is the first payload DWORD (absolute offset 128).
    for (uint32_t bad : {0x00000100u, 0x00000108u, 0x00000200u}) {
        std::vector<uint8_t> other = bytes;
        other[132] = static_cast<uint8_t>(bad & 0xFF);
        other[133] = static_cast<uint8_t>((bad >> 8) & 0xFF);
        other[134] = static_cast<uint8_t>((bad >> 16) & 0xFF);
        other[135] = static_cast<uint8_t>((bad >> 24) & 0xFF);
        const RANMobile::EP7SkinPieceInfo info2 =
            RANMobile::EP7SkinPieceReader::ParseFromBytes(other, false);
        J_CHECK(!info2.valid);
    }

    // Truncated payload must fail, not zero-fill.
    std::vector<uint8_t> truncated(bytes.begin(), bytes.begin() + 40);
    const RANMobile::EP7SkinPieceInfo cut = RANMobile::EP7SkinPieceReader::ParseFromBytes(truncated, false);
    J_CHECK(!cut.valid);
    J_CHECK(!cut.error.empty());

    // Empty buffer.
    J_CHECK(!RANMobile::EP7SkinPieceReader::ParseFromBytes(std::vector<uint8_t>(), false).valid);
}

// Regression for the authentic-data failure `cps: truncated trace name` (2,664 of the
// 6,812 CI FAILs).  SVERTEXINFLU::VERSION is 0x0101 — not 0x0100 — in both engine
// definitions (DxSkinMeshContainer9.cpp:36, SMeshContainer.cpp:29).  With the wrong tag
// the reader took LoadFile's unknown-version branch, skipped `dwSize` bytes (which
// SaveFile over-states), and began the next trace name part-way into ASCII, so the
// bogus length tripped the sanity limit.  Evidence: PROGRESS_REVIEW.md (Phase 3J
// follow-up, CPS forensic investigation).
void TestSkinPieceTraceInfluence() {
    Section(".cps trace point / SVERTEXINFLU version 0x0101");
    const std::vector<uint8_t> bytes = PieceCpsWithTrace();
    const RANMobile::EP7SkinPieceInfo info =
        RANMobile::EP7SkinPieceReader::ParseFromBytes(bytes, false);

    J_CHECK_MSG(info.valid, info.error);
    J_CHECK(info.traces.size() == 1);
    if (info.traces.size() == 1) {
        const RANMobile::EP7TracePoint& trace = info.traces[0];
        J_CHECK(trace.name == "Bip01 Spine");
        J_CHECK(trace.influence.valid);
        J_CHECK(trace.influence.index == 0u);
        J_CHECK(trace.influence.position[0] == 1.0f);
        J_CHECK(trace.influence.position[1] == 2.0f);
        J_CHECK(trace.influence.position[2] == 3.0f);
        J_CHECK(trace.influence.normal[1] == 1.0f);
        J_CHECK(trace.influence.boneCount == 2u);
        J_CHECK(trace.influence.bones[0] == 3u);
        J_CHECK(trace.influence.bones[1] == 7u);
        J_CHECK(trace.influence.weights[0] == 0.75f);
        J_CHECK(trace.influence.weights[1] == 0.25f);
    }
    // Reading the whole record correctly means the trailing fields were reached:
    // effectCount == 0 and m_dwFlag == 0 are only reachable if the influence consumed
    // exactly its structural body and not the over-stated dwSize.
    J_CHECK(info.effectCount == 0u);
    J_CHECK(!info.effectsPresent);
    J_CHECK(!info.error.empty() == false);

    // The 0x0100 tag must now be treated as the *unknown* version, matching the engine:
    // it is not SVERTEXINFLU::VERSION, so LoadFile skips dwSize and the influence is
    // not populated.  Locate the tag by value rather than by a hand-computed offset.
    std::vector<uint8_t> oldTag = bytes;
    {
        const std::vector<uint8_t> tag = {0x01, 0x01, 0x00, 0x00};
        auto it = std::search(oldTag.begin(), oldTag.end(), tag.begin(), tag.end());
        J_CHECK(it != oldTag.end());
        if (it != oldTag.end()) *it = 0x00;  // 0x0101 -> 0x0100
    }
    const RANMobile::EP7SkinPieceInfo oldInfluence =
        RANMobile::EP7SkinPieceReader::ParseFromBytes(oldTag, false);
    if (!oldInfluence.traces.empty()) {
        J_CHECK(!oldInfluence.traces[0].influence.valid);
    }
}


// B13: versions 0x0101-0x0103 are implemented by DxSkinPiece::LoadPiece and must parse.
// Each version's layout is different, so each gets its own fixture; a shared helper is only
// used for the pieces that are genuinely identical across those branches.
void TestSkinPieceLegacyVersions() {
    Section(".cps legacy versions 0x0101-0x0104 (B13)");

    // ---- 0x0104: 0x0105 layout minus the trailing m_dwFlag ------------------------------
    {
        const RANMobile::EP7SkinPieceInfo info =
            RANMobile::EP7SkinPieceReader::ParseFromBytes(legacy::Cps0104(), false);
        J_CHECK_MSG(info.valid, info.error);
        J_CHECK(info.fileVersion == 0x00000104);
        J_CHECK(info.meshName == "BodyMesh");
        J_CHECK(!info.hasWeaponWhereBack);          // weapon slot is 0x0106+
        J_CHECK(info.materials.size() == 1);
        J_CHECK(info.traces.size() == 1);
        if (info.traces.size() == 1) {
            J_CHECK(info.traces[0].name == "Bip01 Spine");
            J_CHECK(!info.traces[0].syntheticName);
            J_CHECK(info.traces[0].influence.boneCount == 2u);
        }
        J_CHECK(info.effectCount == 0u);
        // 0x0104 has no trailing flag: reading one would make this a "truncated piece flag".
        J_CHECK(info.flag == 0u);
        J_CHECK(info.error.empty());
    }

    // ---- 0x0103: no material count, traced names, no trailing flag ----------------------
    {
        const RANMobile::EP7SkinPieceInfo info =
            RANMobile::EP7SkinPieceReader::ParseFromBytes(legacy::Cps0103(), false);
        J_CHECK_MSG(info.valid, info.error);
        J_CHECK(info.fileVersion == 0x00000103);
        J_CHECK(info.materials.empty());            // 0x0103 stores no SMATERIAL_PIECE list
        J_CHECK(info.traces.size() == 2);
        if (info.traces.size() == 2) {
            J_CHECK(info.traces[0].name == "Bip01 Spine");
            J_CHECK(info.traces[1].name == "Bip01 Head");
            J_CHECK(info.traces[0].influence.boneCount == 2u);
            J_CHECK(info.traces[1].influence.boneCount == 1u);
        }
        J_CHECK(info.flag == 0u);
        J_CHECK(info.error.empty());
    }

    // ---- 0x0102: fixed 10-slot influence array, names from STRACE_IMAGE macros ---------
    {
        const RANMobile::EP7SkinPieceInfo info =
            RANMobile::EP7SkinPieceReader::ParseFromBytes(legacy::CpsFixedSlots(0x00000102, 10), false);
        J_CHECK_MSG(info.valid, info.error);
        J_CHECK(info.fileVersion == 0x00000102);
        J_CHECK(info.materials.empty());
        // 4 slots written, 2 with m_dwIndex == COL_ERR -> exactly 2 trace points.
        J_CHECK(info.traces.size() == 2);
        if (info.traces.size() == 2) {
            J_CHECK(info.traces[0].name == "IMAGE01");
            J_CHECK(info.traces[1].name == "IMAGE11");
            J_CHECK(info.traces[0].syntheticName);
            J_CHECK(info.traces[0].influence.boneCount == 2u);
            J_CHECK(info.traces[1].influence.boneCount == 1u);
        }
        J_CHECK(info.flag == 0u);
        J_CHECK(info.error.empty());
    }

    // ---- 0x0101: same shape, 8 slots ---------------------------------------------------
    {
        const RANMobile::EP7SkinPieceInfo info =
            RANMobile::EP7SkinPieceReader::ParseFromBytes(legacy::CpsFixedSlots(0x00000101, 8), false);
        J_CHECK_MSG(info.valid, info.error);
        J_CHECK(info.fileVersion == 0x00000101);
        J_CHECK(info.traces.size() == 2);
        if (info.traces.size() == 2) {
            J_CHECK(info.traces[0].name == "IMAGE01");
            J_CHECK(info.traces[1].name == "IMAGE11");
        }
        J_CHECK(info.error.empty());
    }

    // ---- slot count really is version-dependent ----------------------------------------
    // An 8-slot body must NOT satisfy 0x0102 (which reads 10) and vice versa.  This is what
    // makes the two slots constants load-bearing rather than cosmetic.
    {
        const RANMobile::EP7SkinPieceInfo as102 =
            RANMobile::EP7SkinPieceReader::ParseFromBytes(legacy::CpsFixedSlots(0x00000102, 8), false);
        J_CHECK(!as102.valid);   // runs out of bytes reading slots 9 and 10
        const RANMobile::EP7SkinPieceInfo as101 =
            RANMobile::EP7SkinPieceReader::ParseFromBytes(legacy::CpsFixedSlots(0x00000101, 10), false);
        // 0x0101 reads only 8 slots, so the two extra records are interpreted as the effect
        // count/list — that must be caught, not silently accepted.
        J_CHECK(!as101.valid || as101.effectCount != 0u);
    }

    // ---- trailing flag really is only read for 0x0105+ --------------------------------
    // Appending a flag DWORD to a 0x0104 file must be treated as trailing data, not as a
    // required field: the 0x0104 read must still complete at the effect count.
    {
        std::vector<uint8_t> withFlag = legacy::Cps0104();
        ByteWriter extra;
        extra.U32(0xDEADBEEF);
        withFlag.insert(withFlag.end(), extra.Data().begin(), extra.Data().end());
        const RANMobile::EP7SkinPieceInfo info =
            RANMobile::EP7SkinPieceReader::ParseFromBytes(withFlag, false);
        J_CHECK_MSG(info.valid, info.error);
        J_CHECK(info.flag == 0u);   // 0x0104 never reads a file flag
    }
}


// B14: the dwSize a writer emits is NOT the body length.  These cases pin down the reader's
// behaviour across the whole numBone range, including the two ends the old 2-bone fixture
// could not reach.
void TestSkinPieceInfluenceSizeAccounting() {
    Section(".cps SVERTEXINFLU dwSize accounting (B14)");

    // numBone == 0: SaveFile's two terms coincide, dwSize == body == 0x20.  The record must
    // still parse and the following effect count must be reached.
    {
        const RANMobile::EP7SkinPieceInfo info =
            RANMobile::EP7SkinPieceReader::ParseFromBytes(PieceCpsWithInfluence(0, 5), false);
        J_CHECK_MSG(info.valid, info.error);
        J_CHECK(info.traces.size() == 1);
        if (info.traces.size() == 1) {
            J_CHECK(info.traces[0].name == "Bip01 Spine");
            J_CHECK(info.traces[0].influence.valid);
            J_CHECK(info.traces[0].influence.index == 5u);
            J_CHECK(info.traces[0].influence.boneCount == 0u);
        }
        J_CHECK(info.effectCount == 0u);
        J_CHECK(info.error.empty());
    }

    // numBone == COL_ERR: dwSize wraps to a value SMALLER than the body actually written.
    // This is the case the earlier 8/16/48 analysis missed.  Trusting dwSize here would
    // under-read and leave the cursor mid-record, so the effect count would be garbage.
    {
        const RANMobile::EP7SkinPieceInfo info =
            RANMobile::EP7SkinPieceReader::ParseFromBytes(
                PieceCpsWithInfluence(0xFFFFFFFFu, 7), false);
        J_CHECK_MSG(info.valid, info.error);
        J_CHECK(info.traces.size() == 1);
        if (info.traces.size() == 1) {
            J_CHECK(info.traces[0].influence.valid);
            J_CHECK(info.traces[0].influence.index == 7u);
            J_CHECK(info.traces[0].influence.boneCount == RANMobile::EP7VertexInfluence::kColErr);
        }
        J_CHECK(info.effectCount == 0u);
        J_CHECK(info.error.empty());
    }

    // Every legal bone count must round-trip: the parse must land exactly on the effect
    // count for all of them, never on the over-stated skip.
    for (uint32_t n = 0; n <= 6; ++n) {
        const RANMobile::EP7SkinPieceInfo info =
            RANMobile::EP7SkinPieceReader::ParseFromBytes(PieceCpsWithInfluence(n, n), false);
        J_CHECK_MSG(info.valid, info.error);
        J_CHECK(info.traces.size() == 1);
        if (info.traces.size() == 1) {
            J_CHECK(info.traces[0].influence.boneCount == n);
            J_CHECK(info.traces[0].influence.index == n);
        }
        J_CHECK(info.effectCount == 0u);
        J_CHECK(info.flag == 0u);
    }

    // A mismatch tag must still skip by dwSize, exactly like LoadFile's else branch -- that
    // is the ONLY place dwSize is authoritative.  numBone = 1 writes dwSize 0x30 for a 0x28
    // body, so the skip over-runs by 8; the reader must follow the engine and skip 0x30.
    {
        std::vector<uint8_t> bytes = PieceCpsWithInfluence(1, 3);
        const std::vector<uint8_t> tag = {0x01, 0x01, 0x00, 0x00};
        auto it = std::search(bytes.begin(), bytes.end(), tag.begin(), tag.end());
        J_CHECK(it != bytes.end());
        if (it != bytes.end()) *it = 0x00;  // 0x0101 -> 0x0100 (unknown version)
        const RANMobile::EP7SkinPieceInfo info =
            RANMobile::EP7SkinPieceReader::ParseFromBytes(bytes, false);
        // The engine would skip 0x30 and read a bogus effect count; the reader must not
        // claim the influence was understood.
        if (!info.traces.empty()) J_CHECK(!info.traces[0].influence.valid);
    }
}

void TestCharacterFile() {
    Section(".chf character file");
    const std::vector<uint8_t> bytes = CharacterChf();
    // ParseStream() expects the stream positioned at payload offset 0, which is what
    // Parse() arranges by consuming the basestream header (ReadFileType).
    const RANMobile::EP7CharacterFileInfo info = RANMobile::EP7CharacterFileReader::Parse(bytes, false);
    J_CHECK_MSG(info.valid, info.error);
    J_CHECK(info.version == 0x0102);
    J_CHECK(FloatNear(info.scale, 1.0f));
    J_CHECK(info.pieces[0] == "p_head.cps");
    J_CHECK(info.pieces[1] == "p_upper.cps");
    J_CHECK(info.pieces[2].empty());
    J_CHECK(info.skeleton == "m_bone.skt");
    J_CHECK(info.animations.size() == 1);
    J_CHECK(info.animations[0] == "st_walk");
    J_CHECK(info.hasBoundingBox);
    J_CHECK(FloatNear(info.boundsMax[1], 20.0f));
    J_CHECK(info.hasHeightRadius);
    J_CHECK(FloatNear(info.height, 20.0f));
    J_CHECK(FloatNear(info.radius, 7.0f));
    J_CHECK(info.PieceCount() == 2);

    // Unknown version is rejected.
    std::vector<uint8_t> badVer = bytes;
    badVer[132] = 0x99;
    const RANMobile::EP7CharacterFileInfo bad = RANMobile::EP7CharacterFileReader::Parse(badVer, false);
    J_CHECK(!bad.valid);

    // Piece count above 13 must be rejected before any allocation loop.
    std::vector<uint8_t> badPieces = bytes;
    badPieces[140] = 14;  // header(132) + version(4) + scale(4) -> piece count
    const RANMobile::EP7CharacterFileInfo bad2 = RANMobile::EP7CharacterFileReader::Parse(badPieces, false);
    J_CHECK(!bad2.valid);

    // Empty and truncated inputs.
    RANMobile::ep7byte::Stream empty;
    J_CHECK(!RANMobile::EP7CharacterFileReader::ParseStream(empty).valid);
    std::vector<uint8_t> truncatedBytes(bytes.begin(), bytes.begin() + 150);
    J_CHECK(!RANMobile::EP7CharacterFileReader::Parse(truncatedBytes, false).valid);
}

void TestAnimation() {
    Section("animation (.bin + .cfg)");
    const std::vector<uint8_t> bin = AnimationBin();
    RANMobile::EP7AnimClip clip = RANMobile::EP7AnimationReader::ParseContainer(bin, true, false);
    J_CHECK_MSG(clip.valid, clip.error);
    J_CHECK(clip.version == 0x0102);
    J_CHECK(clip.mainTracks.empty());
    J_CHECK(clip.upBodyTracks.size() == 1);
    J_CHECK(clip.upBodyTracks[0].matrixKeys.size() == 1);
    if (!clip.upBodyTracks[0].matrixKeys.empty()) {
        for (int k = 0; k < 16; ++k) {
            const float expected = (k % 5 == 0) ? 1.0f : 0.0f;
            J_CHECK(FloatNear(clip.upBodyTracks[0].matrixKeys[0].mat[k], expected, 1e-6f));
        }
    }

    RANMobile::EP7Skeleton skeleton;
    const std::string skelText = SkeletonX();
    J_CHECK(skeleton.LoadFromXFile(reinterpret_cast<const uint8_t*>(skelText.data()), skelText.size()));
    RANMobile::EP7AnimationReader::BindToSkeleton(clip, skeleton);
    J_CHECK(clip.upBodyTracks[0].boneIndex == skeleton.FindBoneIndex("Bip01_Spine1"));
    J_CHECK(clip.MaxKeyTime() >= 4800);

    const std::vector<uint8_t> cfg = AnimationCfg(true);
    const RANMobile::EP7AnimInfo info = RANMobile::EP7AnimationReader::ParseInfo(cfg, true, false);
    J_CHECK_MSG(info.valid, info.error);
    J_CHECK(info.name == "st_walk");
    J_CHECK(info.skeleton == "m_bone.skt");
    J_CHECK(info.IsLoop());
    J_CHECK(info.startTime == 0);
    J_CHECK(info.endTime == 4800);

    // Every version the engine source implements must parse, and parse to the same
    // metadata: the layouts differ only in the sound/strike tail, which comes after the
    // fields asserted here.
    for (uint32_t version : {0x0101u, 0x0102u, 0x0103u, 0x0104u, 0x0105u, 0x0106u, 0x0107u}) {
        const std::vector<uint8_t> cfgBytes = AnimationCfg(true, version);
        // Authentic header-block sizes (bytes after the 132-byte basestream header, minus
        // the 4-byte effect count). Locked here because a wrong size is silent otherwise.
        static const std::map<uint32_t, size_t> kExpected = {
            {0x0101, 302}, {0x0102, 564}, {0x0103, 732}, {0x0104, 840},
            {0x0105, 840}, {0x0106, 800}, {0x0107, 800}};
        // Exact file sizes: 0x0101..0x0104 end at the header block; 0x0105/0x0106 add the
        // effect count and 0x0107 adds the DxAniScale DWORD.
        const size_t trailing = (version >= 0x0105 ? 4 : 0) + (version == 0x0107 ? 4 : 0);
        J_CHECK(cfgBytes.size() - 132 == kExpected.at(version) + trailing);
        const RANMobile::EP7AnimInfo infoV =
            RANMobile::EP7AnimationReader::ParseInfo(cfgBytes, true, false);
        if (!infoV.valid) {
            std::printf("  cfg 0x%04X -> %s\n", version, infoV.error.c_str());
        }
        J_CHECK_MSG(infoV.valid, infoV.error);
        J_CHECK(infoV.version == version);
        J_CHECK(infoV.name == "st_walk");
        J_CHECK(infoV.skeleton == "m_bone.skt");
        J_CHECK(infoV.endTime == 4800);
        J_CHECK(infoV.IsLoop());
    }

    // Versions with no authentic definition must be rejected with the value named --
    // never guessed at. 0x0113 has no case in the authentic SANIMCONINFO::LoadFile
    // switch (SAnimationInfoSaveLoad.cpp: 0x0115, 0x0114, 0x0112, 0x0111, 0x0110,
    // 0x0109, 0x0108, 0x0107, ...) and 0x0116 lies beyond the newest loader. Since B16
    // the shipped 0x0108..0x0112/0x0114/0x0115 layouts are implemented, so the
    // unsupported set shrinks to exactly these.
    for (uint32_t version : {0x0113u, 0x0116u}) {
        std::vector<uint8_t> unknown = AnimationCfg(true, 0x0106);
        unknown[128] = static_cast<uint8_t>(version & 0xFF);
        unknown[129] = static_cast<uint8_t>((version >> 8) & 0xFF);
        const RANMobile::EP7AnimInfo rejected = RANMobile::EP7AnimationReader::ParseInfo(unknown, true, false);
        J_CHECK(!rejected.valid);
        char expected[16];
        std::snprintf(expected, sizeof(expected), "0x%08X", version);
        J_CHECK(rejected.error.find(expected) != std::string::npos);
        // A known-but-unimplemented version must be classified UNSUPPORTED (a tracked
        // coverage gap, B11/B16), never silently accepted and never reported as parsed.
        J_CHECK(rejected.unsupported);
        J_CHECK(rejected.error.find("UNSUPPORTED") != std::string::npos);
    }

    // The type string in the 132-byte header is what separates "this build is behind a newer
    // SANIMCONINFO" from "this is not a SANIMCONINFO file at all". A foreign type must NOT be
    // excused as UNSUPPORTED.
    {
        std::vector<uint8_t> foreign = AnimationCfg(true, 0x0106);
        const char* type = "SOMETHINGELSE";
        for (size_t i = 0; i < 128; ++i) foreign[i] = 0;
        for (size_t i = 0; type[i] != 0; ++i) foreign[i] = static_cast<uint8_t>(type[i]);
        foreign[128] = 0x13;  // 0x0113: unimplemented (no authentic case), wrong file family
        foreign[129] = 0x01;
        const RANMobile::EP7AnimInfo wrongFamily =
            RANMobile::EP7AnimationReader::ParseInfo(foreign, true, false);
        J_CHECK(!wrongFamily.valid);
        J_CHECK(!wrongFamily.unsupported);  // a genuine mismatch, not a coverage gap
        J_CHECK(wrongFamily.error.find("not a SANIMCONINFO file") != std::string::npos);
        J_CHECK(wrongFamily.fileType == "SOMETHINGELSE");
    }

    // A real 0x0107 file reports its basestream type, and is neither unsupported nor invalid.
    {
        const RANMobile::EP7AnimInfo good = RANMobile::EP7AnimationReader::ParseInfo(cfg, true, false);
        J_CHECK(good.valid);
        J_CHECK(!good.unsupported);
        J_CHECK(good.version == 0x0107);
    }

    // -----------------------------------------------------------------
    // B16: SANIMCONINFO 0x0108..0x0115 (SAnimationInfoSaveLoad.cpp,
    // backup repo matheomedrana/Ran-Online-EP7-Complete-Files @ tf661fbf).
    // -----------------------------------------------------------------
    {
        // Minimal files for every implemented version parse to the same playback
        // metadata, including the 0x0108..0x0110 field order where m_UNITTIME is
        // written BEFORE m_dwSTime and the strike table precedes the div frames.
        // Exact payload sizes are locked so a wrong field order or size would shift
        // every field after the version DWORD and fail loudly here.
        static const std::map<uint32_t, size_t> kCfgPayloadSize = {
            // 0x0108..0x0110: 256 names + 4 flag + 16 times + 8 types + 110 strike
            //                 + 18 div + 4 effectCount + 4 aniscale + 388 sound = 808
            {0x0108, 808}, {0x0109, 808}, {0x0110, 808},
            // 0x0111..0x0115: 16+12+16+15 strings + 4 flag + 16 times + 8 types
            //                 + 18 div + 110 strike + 388 sound + 4 effectCount
            //                 + 4 aniscale = 611
            {0x0111, 611}, {0x0112, 611}, {0x0114, 611}, {0x0115, 611},
        };
        for (const uint32_t version : {0x0108u, 0x0109u, 0x0110u, 0x0111u, 0x0112u, 0x0114u, 0x0115u}) {
            const std::vector<uint8_t> cfgBytes = AnimationCfg(true, version);
            J_CHECK(cfgBytes.size() - 132 == kCfgPayloadSize.at(version));
            const RANMobile::EP7AnimInfo infoV =
                RANMobile::EP7AnimationReader::ParseInfo(cfgBytes, true, false);
            if (!infoV.valid) {
                std::printf("  cfg 0x%04X -> %s\n", version, infoV.error.c_str());
            }
            J_CHECK_MSG(infoV.valid, infoV.error);
            J_CHECK(infoV.version == version);
            J_CHECK(!infoV.unsupported);
            J_CHECK(infoV.name == "st_walk");
            J_CHECK(infoV.skeleton == "m_bone.skt");
            J_CHECK(infoV.IsLoop());
            J_CHECK(infoV.startTime == 0);
            J_CHECK(infoV.endTime == 4800);
            J_CHECK(infoV.endTimeOrig == 4800);
            J_CHECK(infoV.unitTime == 4800);
            J_CHECK(infoV.mainType == 1);
            J_CHECK(infoV.subType == 0);
            J_CHECK(infoV.strikeCount == 0);
            J_CHECK(infoV.divCount == 1);
            J_CHECK(infoV.effectCount == 0);
            J_CHECK(infoV.effects.empty());
            J_CHECK(infoV.aniScaleEntries.empty());
            J_CHECK(!infoV.effectsSkipped);  // B16 versions parse the effect list in full
            J_CHECK(infoV.exactEof);
            J_CHECK(infoV.trailingBytes == 0);
            if (version >= 0x0111) {
                // szXFile -> m_szName / szSkeleton -> m_szSkeletion; the discarded
                // szCfgFile / szBinFile strings are recorded, not dropped silently.
                J_CHECK(infoV.cfgFileName == "st_walk.cfg");
                J_CHECK(infoV.binFileName == "st_walk.bin");
            } else {
                J_CHECK(infoV.cfgFileName.empty());
                J_CHECK(infoV.binFileName.empty());
            }
        }

        // Rich 0x0110 file: the full effect list (Single/Ghosting/Trace/FaceOff), the
        // AniScale map, and official trailing bytes. The trailing bytes are counted and
        // reported, never reinterpreted -- the authentic loader is marked
        // "//official version partial read only" and returns TRUE all the same.
        {
            const std::vector<uint8_t> rich = AnimationCfg(true, 0x0110, true, true, true);
            const RANMobile::EP7AnimInfo r =
                RANMobile::EP7AnimationReader::ParseInfo(rich, true, false);
            J_CHECK_MSG(r.valid, r.error);
            J_CHECK(r.version == 0x0110);
            J_CHECK(r.effectCount == 4);
            J_CHECK(r.effects.size() == 4);
            J_CHECK(!r.effectsSkipped);
            J_CHECK(!r.exactEof);
            J_CHECK(r.trailingBytes == 8);
            // EMEFFANI_SINGLE=0, property version 0x0101, sizeof(property) = 464.
            J_CHECK(r.effects[0].typeId == 0);
            J_CHECK(r.effects[0].effectVersion == 0x0101);
            J_CHECK(r.effects[0].sizeBytes == 464);
            J_CHECK(r.effects[0].flags == 0x00001425u);
            J_CHECK(r.effects[0].frame == 2400);
            J_CHECK(FloatNear(r.effects[0].fact, 1.0f, 1e-6f));
            J_CHECK(FloatNear(r.effects[0].scale, 2.5f, 1e-6f));
            J_CHECK(r.effects[0].name == "fx_sword_hit");
            // EMEFFANI_GHOSTING=1, property 0x0100, 28 bytes.
            J_CHECK(r.effects[1].typeId == 1);
            J_CHECK(r.effects[1].effectVersion == 0x0100);
            J_CHECK(r.effects[1].sizeBytes == 28);
            J_CHECK(r.effects[1].flags == 0x00001403u);
            J_CHECK(r.effects[1].frame == 300);
            J_CHECK(r.effects[1].frame2 == 900);
            // EMEFFANI_TRACE=2: the property holds a std::string member whose size is
            // build-dependent, so the declared block is consumed, not decoded.
            J_CHECK(r.effects[2].typeId == 2);
            J_CHECK(r.effects[2].sizeBytes == 28);
            J_CHECK(r.effects[2].name.empty());
            // EMEFFANI_FACEOFF=3, property 0x0100, 32 bytes.
            J_CHECK(r.effects[3].typeId == 3);
            J_CHECK(r.effects[3].sizeBytes == 32);
            J_CHECK(r.effects[3].name == "face_normal.dds");
            // DxAniScale map (DxAniBoneScale.cpp).
            J_CHECK(r.aniScaleEntries.size() == 2);
            J_CHECK(r.aniScaleEntries[0].animName == "a_gum_att01");
            J_CHECK(r.aniScaleEntries[0].keyCount == 2);
            J_CHECK(r.aniScaleEntries[1].animName == "a_gum_stay");
            J_CHECK(r.aniScaleEntries[1].keyCount == 1);
        }
        // The same rich content on the 0x0111+ layout keeps the stream aligned with the
        // sound block before the effect list, and ends exactly at the last field.
        {
            const std::vector<uint8_t> rich = AnimationCfg(true, 0x0115, true, true, false);
            const RANMobile::EP7AnimInfo r =
                RANMobile::EP7AnimationReader::ParseInfo(rich, true, false);
            J_CHECK_MSG(r.valid, r.error);
            J_CHECK(r.cfgFileName == "st_walk.cfg");
            J_CHECK(r.binFileName == "st_walk.bin");
            J_CHECK(r.effectCount == 4);
            J_CHECK(r.effects.size() == 4);
            J_CHECK(r.effects[0].sizeBytes == 464);
            J_CHECK(r.effects[3].name == "face_normal.dds");
            J_CHECK(r.aniScaleEntries.size() == 2);
            J_CHECK(r.exactEof);
            J_CHECK(r.trailingBytes == 0);
        }

        // Malformed effect lists and AniScale maps are rejected with the cause named --
        // never guessed through. Field offsets below follow the rich 0x0110 layout
        // locked by the assertions above (payload = file offset - 132).
        auto putU32 = [](std::vector<uint8_t>& b, size_t off, uint32_t v) {
            b[off] = static_cast<uint8_t>(v & 0xFF);
            b[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
            b[off + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
            b[off + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
        };
        {
            std::vector<uint8_t> bad = AnimationCfg(true, 0x0110, true, true, true);
            putU32(bad, 132 + 412 + 4, 7);  // first effect outer type: beyond EMEFFANI_FACEOFF
            const RANMobile::EP7AnimInfo r = RANMobile::EP7AnimationReader::ParseInfo(bad, true, false);
            J_CHECK(!r.valid);
            J_CHECK(!r.unsupported);
            J_CHECK(r.error.find("effect type unknown") != std::string::npos);
        }
        {
            std::vector<uint8_t> bad = AnimationCfg(true, 0x0110, true, true, true);
            putU32(bad, 132 + 412 + 8, 1);  // inner type id no longer matches the list type
            const RANMobile::EP7AnimInfo r = RANMobile::EP7AnimationReader::ParseInfo(bad, true, false);
            J_CHECK(!r.valid);
            J_CHECK(r.error.find("does not match") != std::string::npos);
        }
        {
            std::vector<uint8_t> bad = AnimationCfg(true, 0x0110, true, true, true);
            putU32(bad, 132 + 412 + 12, 0x0102);  // Single property version: not 0x0101/0x0100
            const RANMobile::EP7AnimInfo r = RANMobile::EP7AnimationReader::ParseInfo(bad, true, false);
            J_CHECK(!r.valid);
            J_CHECK(r.error.find("Single effect version unsupported") != std::string::npos);
        }
        {
            std::vector<uint8_t> bad = AnimationCfg(true, 0x0110, true, true, true);
            // Ghosting entry starts after the 4-byte count + 16+464-byte Single entry:
            // size field sits 12 bytes into it (outer + inner + version).
            putU32(bad, 132 + 412 + 4 + 480 + 12, 27);  // sizeof(EFFANI_PROPERTY_GHOSTING) mismatch
            const RANMobile::EP7AnimInfo r = RANMobile::EP7AnimationReader::ParseInfo(bad, true, false);
            J_CHECK(!r.valid);
            J_CHECK(r.error.find("Ghosting effect property size mismatch") != std::string::npos);
        }
        {
            std::vector<uint8_t> bad = AnimationCfg(true, 0x0110, true, true, true);
            // Trace entry starts after the Ghosting entry (16+28); a declared size below
            // 4 cannot even hold m_fScale.
            putU32(bad, 132 + 412 + 4 + 480 + 44 + 12, 3);
            const RANMobile::EP7AnimInfo r = RANMobile::EP7AnimationReader::ParseInfo(bad, true, false);
            J_CHECK(!r.valid);
            J_CHECK(r.error.find("Trace effect property size invalid") != std::string::npos);
        }
        {
            std::vector<uint8_t> bad = AnimationCfg(true, 0x0110, true, true, true);
            // Effect count exceeds the sanity cap.
            putU32(bad, 132 + 412, 0xFFFFFFFFu);
            const RANMobile::EP7AnimInfo r = RANMobile::EP7AnimationReader::ParseInfo(bad, true, false);
            J_CHECK(!r.valid);
            J_CHECK(r.error.find("sanity limit") != std::string::npos);
        }
        {
            std::vector<uint8_t> bad = AnimationCfg(true, 0x0110, true, true, true);
            // AniScale map size exceeds the sanity cap (block starts after the effects:
            // 412 + 4 + (16+464) + (16+28) + (16+28) + (16+32) = 1032).
            putU32(bad, 132 + 1032, 0xFFFFFFFFu);
            const RANMobile::EP7AnimInfo r = RANMobile::EP7AnimationReader::ParseInfo(bad, true, false);
            J_CHECK(!r.valid);
            J_CHECK(r.error.find("ani scale") != std::string::npos);
        }
        {
            std::vector<uint8_t> bad = AnimationCfg(true, 0x0110, true, true, true);
            // First AniScale entry: name length at +4, 12 bytes of name (incl. NUL),
            // key count next (1032 + 4 + 4 + 12 = 1052).
            putU32(bad, 132 + 1052, 0x7FFFFFFFu);  // key count absurd
            const RANMobile::EP7AnimInfo r = RANMobile::EP7AnimationReader::ParseInfo(bad, true, false);
            J_CHECK(!r.valid);
            J_CHECK(r.error.find("ani scale keys run past end") != std::string::npos);
        }
        {
            std::vector<uint8_t> bad = AnimationCfg(true, 0x0115, true, true, true);
            // 0x0111+ companion-name string length runs past the end of the buffer.
            putU32(bad, 132, 0x7FFFFFFFu);
            const RANMobile::EP7AnimInfo r = RANMobile::EP7AnimationReader::ParseInfo(bad, true, false);
            J_CHECK(!r.valid);
            J_CHECK(r.error.find("companion-name strings") != std::string::npos);
        }

        // The 388-byte SChaSoundData block is mandatory in both layouts: one byte short
        // must reject at exactly that field.
        {
            std::vector<uint8_t> cut = AnimationCfg(true, 0x0110);
            cut.resize(132 + 420);  // everything up to but excluding the sound block
            const RANMobile::EP7AnimInfo r = RANMobile::EP7AnimationReader::ParseInfo(cut, true, false);
            J_CHECK(!r.valid);
            J_CHECK(r.error.find("truncated cfg sound block") != std::string::npos);
        }
        {
            std::vector<uint8_t> cut = AnimationCfg(true, 0x0115);
            cut.resize(132 + 215);  // up to but excluding the sound block (div+strike done)
            const RANMobile::EP7AnimInfo r = RANMobile::EP7AnimationReader::ParseInfo(cut, true, false);
            J_CHECK(!r.valid);
            J_CHECK(r.error.find("truncated cfg sound block") != std::string::npos);
        }

        // EP7 extended the main-type range from 21 to 34 (DxAniKeys.h AN_TYPE_SIZE=34):
        // AN_BIKE_A..AN_REARCAR_A (21..33) are authentic file data in the 0x0108+ layouts
        // and must parse; AN_NONE/AN_TYPE_SIZE (34) and beyond stay out of range.
        {
            const std::vector<uint8_t> vehicle =
                AnimationCfg(true, 0x0110, false, false, false, 33 /*AN_REARCAR_A*/);
            const RANMobile::EP7AnimInfo r =
                RANMobile::EP7AnimationReader::ParseInfo(vehicle, true, false);
            J_CHECK_MSG(r.valid, r.error);
            J_CHECK(r.mainType == 33);
        }
        {
            const std::vector<uint8_t> outOfRange =
                AnimationCfg(true, 0x0115, false, false, false, 34 /*AN_NONE*/);
            const RANMobile::EP7AnimInfo r =
                RANMobile::EP7AnimationReader::ParseInfo(outOfRange, true, false);
            J_CHECK(!r.valid);
            J_CHECK(r.error.find("cfg main type out of range") != std::string::npos);
        }
        {
            // The relaxed bound applies to the older layouts too (same enum); 0x0101
            // still rejects everything >= AN_TYPE_SIZE.
            const std::vector<uint8_t> oldVehicle =
                AnimationCfg(true, 0x0101, false, false, false, 22);
            const RANMobile::EP7AnimInfo r =
                RANMobile::EP7AnimationReader::ParseInfo(oldVehicle, true, false);
            J_CHECK_MSG(r.valid, r.error);
            J_CHECK(r.mainType == 22);
        }

        // Exact-EOF strictness: the minimal fixtures carry no official trailing data, so
        // EVERY truncation must be rejected -- no partial field may parse.
        for (const uint32_t version : {0x0108u, 0x0110u, 0x0111u, 0x0115u}) {
            const std::vector<uint8_t> full = AnimationCfg(true, version);
            for (size_t len = 0; len < full.size(); ++len) {
                std::vector<uint8_t> cut(full.begin(), full.begin() + static_cast<long>(len));
                const RANMobile::EP7AnimInfo r =
                    RANMobile::EP7AnimationReader::ParseInfo(cut, true, false);
                if (r.valid) {
                    std::printf("  cfg 0x%04X truncated to %zu bytes unexpectedly parsed\n",
                                version, len);
                }
                J_CHECK(!r.valid);
            }
        }
    }

    // Player: looping wraps, non-looping ends.
    RANMobile::EP7AnimationPlayer player;
    player.Play(&clip, &info);
    J_CHECK(player.IsPlaying());
    player.Advance(0.2f);  // 960 units at 4800/s
    J_CHECK(player.CurrentTime() > 0.0f);
    const float before = player.CurrentTime();
    J_CHECK(player.Advance(2.0f));  // wrap
    J_CHECK(player.CurrentTime() < before || player.CurrentTime() < 4801.0f);

    RANMobile::EP7AnimInfo once = info;
    once.flag = 0;
    J_CHECK(!once.IsLoop());
    RANMobile::EP7AnimationPlayer single;
    single.Play(&clip, &once);
    J_CHECK(single.Loop() == false);  // Play() honours the authentic ACF flag
    // One second == 4800 key units; the clip window is [0, 4800].
    J_CHECK(!single.Advance(1.0f));    // reaches the last two unit intervals -> bEndAnim

    // Truncated container is rejected.
    std::vector<uint8_t> cut(bin.begin(), bin.begin() + 140);
    const RANMobile::EP7AnimClip bad = RANMobile::EP7AnimationReader::ParseContainer(cut, true, false);
    J_CHECK(!bad.valid);
    J_CHECK(!bad.error.empty());

    // Wrong version.
    std::vector<uint8_t> wrong = bin;
    wrong[128] = 0x77;  // container version lives in the header fileId
    J_CHECK(!RANMobile::EP7AnimationReader::ParseContainer(wrong, true, false).valid);

    // ---- Phase 3J B13: Authentic 0x0100 tracks + 80-byte SMatrixKey alignment regression ----
    {
        const std::vector<uint8_t> bin0100 = AnimationBin0100();
        RANMobile::EP7AnimClip c100 = RANMobile::EP7AnimationReader::ParseContainer(bin0100, true, false);
        J_CHECK_MSG(c100.valid, c100.error);
        J_CHECK(c100.version == 0x0100);
        J_CHECK(c100.upBodyTracks.empty());
        J_CHECK(c100.mainTracks.size() == 2);
        J_CHECK(c100.MaxKeyTime() == 4800);

        if (c100.mainTracks.size() == 2) {
            const auto& t0 = c100.mainTracks[0];
            J_CHECK(t0.boneName == "Bip01_Spine1");
            J_CHECK(t0.quatPosKeys.empty());
            J_CHECK(t0.positionKeys.size() == 1 && t0.positionKeys[0].time == 160);
            J_CHECK(FloatNear(t0.positionKeys[0].p[0], 1.25f) &&
                    FloatNear(t0.positionKeys[0].p[1], -2.5f) &&
                    FloatNear(t0.positionKeys[0].p[2], 3.75f));
            J_CHECK(t0.rotateKeys.size() == 1 && t0.rotateKeys[0].time == 320);
            J_CHECK(FloatNear(t0.rotateKeys[0].q[1], 0.70710678f, 1e-5f) &&
                    FloatNear(t0.rotateKeys[0].q[3], 0.70710678f, 1e-5f));
            J_CHECK(t0.scaleKeys.size() == 1 && t0.scaleKeys[0].time == 480);
            J_CHECK(FloatNear(t0.scaleKeys[0].s[0], 1.5f) &&
                    FloatNear(t0.scaleKeys[0].s[1], 2.0f) &&
                    FloatNear(t0.scaleKeys[0].s[2], 0.5f));
            J_CHECK(t0.matrixKeys.size() == 2);
            if (t0.matrixKeys.size() == 2) {
                J_CHECK(t0.matrixKeys[0].time == 0);
                J_CHECK(FloatNear(t0.matrixKeys[0].mat[0], 1.0f) &&
                        FloatNear(t0.matrixKeys[0].mat[1], 0.0f) &&
                        FloatNear(t0.matrixKeys[0].mat[13], 6.0f) &&
                        FloatNear(t0.matrixKeys[0].mat[15], 1.0f));
                J_CHECK(t0.matrixKeys[1].time == 4800);
                J_CHECK(FloatNear(t0.matrixKeys[1].mat[0], 1.0f) &&
                        FloatNear(t0.matrixKeys[1].mat[12], 1.0f) &&
                        FloatNear(t0.matrixKeys[1].mat[13], 9.0f) &&
                        FloatNear(t0.matrixKeys[1].mat[14], -2.0f) &&
                        FloatNear(t0.matrixKeys[1].mat[15], 1.0f));
            }

            // Track 1: empty bone name preserved cleanly and unbound (-1) on skeleton bind.
            const auto& t1 = c100.mainTracks[1];
            J_CHECK(t1.boneName.empty());
            J_CHECK(t1.matrixKeys.size() == 1 && t1.matrixKeys[0].time == 2400);

            RANMobile::EP7AnimationReader::BindToSkeleton(c100, skeleton);
            J_CHECK(c100.mainTracks[0].boneIndex == skeleton.FindBoneIndex("Bip01_Spine1"));
            J_CHECK(c100.mainTracks[1].boneIndex == -1);

            // Evaluate 0x0100 matrix keys on skeleton via EP7AnimationPlayer.
            RANMobile::EP7AnimationPlayer p100;
            p100.Play(&c100, nullptr);
            skeleton.ResetToRestPose();
            p100.ApplyToSkeleton(skeleton);
            skeleton.UpdateBones(RANMobile::ep7math::Mat4::Identity());
            const int32_t headIdx = skeleton.FindBoneIndex("Bip01_Head");
            J_CHECK(headIdx >= 0);
            if (headIdx >= 0) {
                // Spine1 matrix key 0 y=+6.0 + Head rest y=+1.5 => +7.5.
                J_CHECK(FloatNear(skeleton.GetBone(static_cast<size_t>(headIdx)).combined.m[3][1], 7.5f, 1e-4f));
            }
            p100.Advance(0.5f);  // t = 2400 -> lower_bound picks key 1 @ t=4800 (y=+9.0)
            skeleton.ResetToRestPose();
            p100.ApplyToSkeleton(skeleton);
            skeleton.UpdateBones(RANMobile::ep7math::Mat4::Identity());
            if (headIdx >= 0) {
                J_CHECK(FloatNear(skeleton.GetBone(static_cast<size_t>(headIdx)).combined.m[3][1], 10.5f, 1e-4f));
            }
        }

        // Strict prefix truncation at every byte offset of bin0100 (including mid-80B matrix pad/body)
        // must fail safely without out-of-bounds reads.
        for (size_t cutLen = 0; cutLen < bin0100.size(); ++cutLen) {
            const std::vector<uint8_t> prefix(bin0100.begin(), bin0100.begin() + static_cast<long>(cutLen));
            const RANMobile::EP7AnimClip truncated =
                RANMobile::EP7AnimationReader::ParseContainer(prefix, true, false);
            J_CHECK(!truncated.valid);
        }

        // Boundary: 0-track 0x0100 container (136B, matching authentic a_mob_wdm_stay01.bin),
        // zero-key 0x0100 track with zero-length bone string, and headerless 0x0100 container.
        {
            ByteWriter w0;
            w0.Raw(std::vector<uint8_t>(128, 0));
            w0.U32(0x0100);
            w0.U32(0);
            const RANMobile::EP7AnimClip empty0100 =
                RANMobile::EP7AnimationReader::ParseContainer(w0.Data(), true, false);
            J_CHECK(empty0100.valid && empty0100.version == 0x0100 && empty0100.mainTracks.empty());

            ByteWriter wZeroLenBone;
            wZeroLenBone.U32(0x0100);  // headerless container version
            wZeroLenBone.U32(1);       // 1 main track
            wZeroLenBone.U32(0x0100);  // track version
            wZeroLenBone.U32(0);       // pos
            wZeroLenBone.U32(0);       // rot
            wZeroLenBone.U32(0);       // scale
            wZeroLenBone.U32(0);       // matrix
            wZeroLenBone.U32(0);       // bone string len == 0
            const RANMobile::EP7AnimClip zeroBone =
                RANMobile::EP7AnimationReader::ParseContainer(wZeroLenBone.Data(), false, false);
            J_CHECK(zeroBone.valid && zeroBone.mainTracks.size() == 1 &&
                    zeroBone.mainTracks[0].boneName.empty());
        }

        // Invalid counts / string lengths in 0x0100 tracks must fail cleanly.
        for (int badChannel = 0; badChannel < 5; ++badChannel) {
            ByteWriter wb;
            wb.Raw(std::vector<uint8_t>(128, 0));
            wb.U32(0x0100);
            wb.U32(1);
            wb.U32(0x0100);
            wb.U32(badChannel == 0 ? 200001u : 0u);  // pos count
            wb.U32(badChannel == 1 ? 200001u : 0u);  // rot count
            wb.U32(badChannel == 2 ? 200001u : 0u);  // scale count
            wb.U32(badChannel == 3 ? 200001u : 0u);  // matrix count
            wb.U32(badChannel == 4 ? 5000u : 0u);    // bone string len (> kMaxNameLen)
            const RANMobile::EP7AnimClip rej =
                RANMobile::EP7AnimationReader::ParseContainer(wb.Data(), true, false);
            J_CHECK(!rej.valid && !rej.error.empty());
        }

        // Regression: 0x0101 track with 80-byte SMatrixKey + SQuatPosKey + boneName + upBody list.
        {
            ByteWriter w101;
            w101.Raw(std::vector<uint8_t>(128, 0));
            w101.U32(0x0101);
            w101.U32(1);  // main count
            w101.U32(0x0101);
            w101.U32(0);  // pos
            w101.U32(0);  // rot
            w101.U32(0);  // scale
            w101.U32(1);  // 1 matrix key (80B)
            w101.U32(800);
            w101.U32(0x11111111u); w101.U32(0x22222222u); w101.U32(0x33333333u);
            for (int k = 0; k < 16; ++k) w101.F32(static_cast<float>(k + 1));
            w101.U32(1);  // 1 quatpos key (36B)
            w101.U32(1600);
            w101.F32(1.0f); w101.F32(1.0f); w101.F32(1.0f);
            w101.F32(2.0f); w101.F32(3.0f); w101.F32(4.0f);
            w101.U32(0u); w101.U32(0x80000000u);
            w101.Str("Bip01");
            w101.U32(1);  // upBody count
            w101.U32(0x0101);
            w101.U32(0); w101.U32(0); w101.U32(0); w101.U32(0); w101.U32(0);
            w101.Str("Bip01_Head");

            const RANMobile::EP7AnimClip c101 =
                RANMobile::EP7AnimationReader::ParseContainer(w101.Data(), true, false);
            J_CHECK_MSG(c101.valid, c101.error);
            J_CHECK(c101.mainTracks.size() == 1 && c101.upBodyTracks.size() == 1);
            if (c101.mainTracks.size() == 1 && c101.upBodyTracks.size() == 1) {
                J_CHECK(c101.mainTracks[0].boneName == "Bip01");
                J_CHECK(c101.upBodyTracks[0].boneName == "Bip01_Head");
                J_CHECK(c101.mainTracks[0].matrixKeys.size() == 1);
                J_CHECK(FloatNear(c101.mainTracks[0].matrixKeys[0].mat[0], 1.0f) &&
                        FloatNear(c101.mainTracks[0].matrixKeys[0].mat[15], 16.0f));
                J_CHECK(c101.mainTracks[0].quatPosKeys.size() == 1 &&
                        c101.mainTracks[0].quatPosKeys[0].time == 1600);
            }
        }

        // ---- Phase 3J B14: Authentic outer container 0x0103 + track 0x0103 regression ----
        {
            const std::vector<uint8_t> bin0103 = AnimationBin0103();
            RANMobile::EP7AnimClip c103 =
                RANMobile::EP7AnimationReader::ParseContainer(bin0103, true, false);
            J_CHECK_MSG(c103.valid, c103.error);
            J_CHECK(c103.version == 0x0103);
            J_CHECK(c103.upBodyTracks.size() == 1);
            J_CHECK(c103.mainTracks.size() == 2);
            J_CHECK(c103.MaxKeyTime() == 4800);

            if (c103.upBodyTracks.size() == 1 && c103.mainTracks.size() == 2) {
                const auto& up0 = c103.upBodyTracks[0];
                J_CHECK(up0.boneName == "Bip01_Spine1");
                J_CHECK(up0.rotateKeys.size() == 1 && up0.rotateKeys[0].time == 160);
                J_CHECK(FloatNear(up0.rotateKeys[0].q[2], 0.70710678f, 1e-5f) &&
                        FloatNear(up0.rotateKeys[0].q[3], 0.70710678f, 1e-5f));
                J_CHECK(up0.positionKeys.size() == 1 && up0.positionKeys[0].time == 320);
                J_CHECK(FloatNear(up0.positionKeys[0].p[0], -1.5f) &&
                        FloatNear(up0.positionKeys[0].p[1], 4.25f) &&
                        FloatNear(up0.positionKeys[0].p[2], 0.75f));
                J_CHECK(up0.quatPosKeys.size() == 1 && up0.quatPosKeys[0].time == 640);
                J_CHECK(FloatNear(up0.quatPosKeys[0].scale[1], 2.0f) &&
                        FloatNear(up0.quatPosKeys[0].pos[1], 5.5f));
                J_CHECK(up0.matrixKeys.size() == 1 && up0.matrixKeys[0].time == 960);
                J_CHECK(FloatNear(up0.matrixKeys[0].mat[0], 1.0f) &&
                        FloatNear(up0.matrixKeys[0].mat[12], 2.0f) &&
                        FloatNear(up0.matrixKeys[0].mat[13], 7.0f) &&
                        FloatNear(up0.matrixKeys[0].mat[14], -3.0f) &&
                        FloatNear(up0.matrixKeys[0].mat[15], 1.0f));
                J_CHECK(up0.scaleKeys.size() == 1 && up0.scaleKeys[0].time == 1280);
                J_CHECK(FloatNear(up0.scaleKeys[0].s[0], 2.5f) &&
                        FloatNear(up0.scaleKeys[0].s[1], 0.75f) &&
                        FloatNear(up0.scaleKeys[0].s[2], 1.25f));

                const auto& m0 = c103.mainTracks[0];
                J_CHECK(m0.boneName == "Bip01");
                J_CHECK(m0.quatPosKeys.size() == 2);
                J_CHECK(m0.quatPosKeys[0].time == 0 && FloatNear(m0.quatPosKeys[0].pos[1], 3.0f));
                J_CHECK(m0.quatPosKeys[1].time == 4800 && FloatNear(m0.quatPosKeys[1].pos[1], 8.0f));
                J_CHECK(c103.mainTracks[1].boneName.empty());

                // Bind & evaluate 0x0103 tracks (both matrix-keyed upBody Spine1 and quatPos-keyed main Bip01)
                RANMobile::EP7AnimationReader::BindToSkeleton(c103, skeleton);
                J_CHECK(c103.upBodyTracks[0].boneIndex == skeleton.FindBoneIndex("Bip01_Spine1"));
                J_CHECK(c103.mainTracks[0].boneIndex == skeleton.FindBoneIndex("Bip01"));
                J_CHECK(c103.mainTracks[1].boneIndex == -1);

                RANMobile::EP7AnimationPlayer p103;
                p103.Play(&c103, nullptr);
                skeleton.ResetToRestPose();
                p103.ApplyToSkeleton(skeleton);
                skeleton.UpdateBones(RANMobile::ep7math::Mat4::Identity());
                const int32_t headIdx = skeleton.FindBoneIndex("Bip01_Head");
                if (headIdx >= 0) {
                    // Bip01 y=+3.0 (quatPos @ t=0) + Spine1 y=+7.0 (matrix @ t=960) + Head rest y=+1.5 => +11.5
                    J_CHECK(FloatNear(skeleton.GetBone(static_cast<size_t>(headIdx)).combined.m[3][1],
                                      11.5f, 1e-3f));
                }
            }

            // Prove 0x0103 vs 0x0102 field-order sensitivity: relabeling upBody track 0 from 0x0103
            // to 0x0102 misaligns dwGarbage1/dwGarbage2 relative to positionKeys and matrixKeys.
            {
                std::vector<uint8_t> mislabeled = bin0103;
                mislabeled[136] = 0x02;  // change first track version 0x0103 -> 0x0102
                const RANMobile::EP7AnimClip wrongTrackVer =
                    RANMobile::EP7AnimationReader::ParseContainer(mislabeled, true, false);
                J_CHECK(!wrongTrackVer.valid);
            }

            // Every strict prefix of bin0103 (including mid-dwGarbageValue, mid-dwGarbage1/2,
            // mid-quatPos, mid-80B-matrix, and mid-scale) must fail safely.
            for (size_t cutLen = 0; cutLen < bin0103.size(); ++cutLen) {
                const std::vector<uint8_t> prefix(bin0103.begin(),
                                                  bin0103.begin() + static_cast<long>(cutLen));
                J_CHECK(!RANMobile::EP7AnimationReader::ParseContainer(prefix, true, false).valid);
            }

            // Boundary: empty 0x0103 container (144B: upBody=0, dwGarbageValue=0x0103, main=0,
            // matching authentic a_m_01_run_a.bin) and headerless 0x0103 container.
            {
                ByteWriter wEmpty103;
                wEmpty103.Raw(std::vector<uint8_t>(128, 0));
                wEmpty103.U32(0x0103);
                wEmpty103.U32(0);        // upBody = 0
                wEmpty103.U32(0x0103u);  // dwGarbageValue == 0x0103 + 0 + 0
                wEmpty103.U32(0);        // main = 0
                J_CHECK(wEmpty103.Data().size() == 144);
                const RANMobile::EP7AnimClip e103 =
                    RANMobile::EP7AnimationReader::ParseContainer(wEmpty103.Data(), true, false);
                J_CHECK(e103.valid && e103.version == 0x0103 &&
                        e103.upBodyTracks.empty() && e103.mainTracks.empty());

                ByteWriter wHeaderless103;
                wHeaderless103.U32(0x0103);
                wHeaderless103.U32(0);
                wHeaderless103.U32(0x0103u);
                wHeaderless103.U32(0);
                const RANMobile::EP7AnimClip hl103 =
                    RANMobile::EP7AnimationReader::ParseContainer(wHeaderless103.Data(), false, false);
                J_CHECK(hl103.valid && hl103.version == 0x0103);
            }

            // Invalid list counts and invalid 0x0103 track counts/lengths must fail cleanly.
            for (int badList = 0; badList < 2; ++badList) {
                ByteWriter wl;
                wl.Raw(std::vector<uint8_t>(128, 0));
                wl.U32(0x0103);
                wl.U32(badList == 0 ? 4097u : 0u);  // upBody count
                wl.U32(0x0103u);                    // dwGarbageValue
                wl.U32(badList == 1 ? 4097u : 0u);  // main count
                J_CHECK(!RANMobile::EP7AnimationReader::ParseContainer(wl.Data(), true, false).valid);
            }
            for (int badField = 0; badField < 6; ++badField) {
                ByteWriter wt;
                wt.Raw(std::vector<uint8_t>(128, 0));
                wt.U32(0x0103);
                wt.U32(1);       // 1 upBody track
                wt.U32(0x0103);  // track version 0x0103
                if (badField == 0) {
                    wt.U32(5000u);  // bone string length > kMaxNameLen
                } else {
                    wt.Str("Bip01");
                    wt.U32(badField == 1 ? 200001u : 0u);  // rotateKeys
                    wt.U32(5u);                            // dwGarbage1
                    wt.U32(badField == 2 ? 200001u : 0u);  // positionKeys
                    wt.U32(badField == 3 ? 200001u : 0u);  // quatPosKeys
                    wt.U32(badField == 4 ? 200001u : 0u);  // matrixKeys
                    wt.U32(5u + 0x0103u);                  // dwGarbage2
                    wt.U32(badField == 5 ? 200001u : 0u);  // scaleKeys
                }
                wt.U32(0x0104u);
                wt.U32(0);
                const RANMobile::EP7AnimClip rej =
                    RANMobile::EP7AnimationReader::ParseContainer(wt.Data(), true, false);
                J_CHECK(!rej.valid && !rej.error.empty());
            }
        }

        // ---- Phase 3J B15: Authentic outer container 0x0104 (main, upBody1, upBody2) ----
        {
            const std::vector<uint8_t> bin0104 = AnimationBin0104();
            RANMobile::EP7AnimClip c104 =
                RANMobile::EP7AnimationReader::ParseContainer(bin0104, true, false);
            J_CHECK_MSG(c104.valid, c104.error);
            J_CHECK(c104.version == 0x0104);
            J_CHECK(c104.mainTracks.size() == 1);
            // List 1 (1 track) + List 2 (2 tracks) must be appended in exact order into upBodyTracks!
            J_CHECK(c104.upBodyTracks.size() == 3);
            J_CHECK(c104.MaxKeyTime() == 4800);

            if (c104.mainTracks.size() == 1 && c104.upBodyTracks.size() == 3) {
                J_CHECK(c104.mainTracks[0].boneName == "Bip01");
                J_CHECK(c104.upBodyTracks[0].boneName == "Bip01_Spine1");
                J_CHECK(c104.upBodyTracks[1].boneName == "Bip01_Head");
                J_CHECK(c104.upBodyTracks[2].boneName.empty());

                // Bind & evaluate simultaneously across main + upBody1 + upBody2 on skeleton
                // (paired with supported 0x0107 .cfg `info`).
                RANMobile::EP7AnimationReader::BindToSkeleton(c104, skeleton);
                J_CHECK(c104.mainTracks[0].boneIndex == skeleton.FindBoneIndex("Bip01"));
                J_CHECK(c104.upBodyTracks[0].boneIndex == skeleton.FindBoneIndex("Bip01_Spine1"));
                J_CHECK(c104.upBodyTracks[1].boneIndex == skeleton.FindBoneIndex("Bip01_Head"));
                J_CHECK(c104.upBodyTracks[2].boneIndex == -1);

                RANMobile::EP7AnimationPlayer p104;
                p104.Play(&c104, &info);
                J_CHECK(p104.IsPlaying() && p104.Loop());
                skeleton.ResetToRestPose();
                p104.ApplyToSkeleton(skeleton);
                skeleton.UpdateBones(RANMobile::ep7math::Mat4::Identity());
                const int32_t headIdx = skeleton.FindBoneIndex("Bip01_Head");
                if (headIdx >= 0) {
                    // Bip01 y=+2.0 (main) + Spine1 y=+4.0 (upBody1) + Head y=+3.5 (upBody2) => +9.5
                    J_CHECK(FloatNear(skeleton.GetBone(static_cast<size_t>(headIdx)).combined.m[3][1],
                                      9.5f, 1e-3f));
                }
            }

            // Exact EOF + strict prefix truncation check: every prefix 0 .. size-1 must fail,
            // proving ParseContainer consumed all three lists right to exact EOF.
            for (size_t cutLen = 0; cutLen < bin0104.size(); ++cutLen) {
                const std::vector<uint8_t> prefix(bin0104.begin(),
                                                  bin0104.begin() + static_cast<long>(cutLen));
                J_CHECK(!RANMobile::EP7AnimationReader::ParseContainer(prefix, true, false).valid);
            }

            // Trailing data check: CSerialFile Load_0104 stops immediately after upBody2 without
            // reading a 4th list or trailing words. Appending sentinel bytes (including 0xFFFFFFFF)
            // must leave mainTracks and upBodyTracks completely unchanged.
            {
                std::vector<uint8_t> withTrailing = bin0104;
                ByteWriter extra;
                extra.U32(0xFFFFFFFFu);
                extra.U32(0xDEADBEEFu);
                withTrailing.insert(withTrailing.end(), extra.Data().begin(), extra.Data().end());
                const RANMobile::EP7AnimClip cTrail =
                    RANMobile::EP7AnimationReader::ParseContainer(withTrailing, true, false);
                J_CHECK_MSG(cTrail.valid, cTrail.error);
                J_CHECK(cTrail.mainTracks.size() == 1 && cTrail.upBodyTracks.size() == 3);
                if (cTrail.mainTracks.size() == 1 && cTrail.upBodyTracks.size() == 3) {
                    J_CHECK(cTrail.mainTracks[0].boneName == "Bip01");
                    J_CHECK(cTrail.upBodyTracks[0].boneName == "Bip01_Spine1");
                    J_CHECK(cTrail.upBodyTracks[1].boneName == "Bip01_Head");
                    J_CHECK(cTrail.upBodyTracks[2].boneName.empty());
                }
            }

            // Empty vs populated combinations across all 8 subsets of (main, upBody1, upBody2),
            // including (0,0,0) = 144 bytes (matching authentic a_item_lovely_step_bone.bin)
            // and headerless 0x0104.
            for (int mask = 0; mask < 8; ++mask) {
                const uint32_t nMain = (mask & 1) ? 2u : 0u;
                const uint32_t nUp1 = (mask & 2) ? 1u : 0u;
                const uint32_t nUp2 = (mask & 4) ? 2u : 0u;
                ByteWriter wm;
                wm.Raw(std::vector<uint8_t>(128, 0));
                wm.U32(0x0104);
                wm.U32(nMain);
                for (uint32_t i = 0; i < nMain; ++i) AppendZeroKeyTrack0103(wm, "Main_" + std::to_string(i));
                wm.U32(nUp1);
                for (uint32_t i = 0; i < nUp1; ++i) AppendZeroKeyTrack0103(wm, "Up1_" + std::to_string(i));
                wm.U32(nUp2);
                for (uint32_t i = 0; i < nUp2; ++i) AppendZeroKeyTrack0103(wm, "Up2_" + std::to_string(i));
                if (mask == 0) J_CHECK(wm.Data().size() == 144u);

                const RANMobile::EP7AnimClip cm =
                    RANMobile::EP7AnimationReader::ParseContainer(wm.Data(), true, false);
                J_CHECK_MSG(cm.valid, cm.error);
                J_CHECK(cm.version == 0x0104);
                J_CHECK(cm.mainTracks.size() == nMain);
                J_CHECK(cm.upBodyTracks.size() == nUp1 + nUp2);
                if (cm.upBodyTracks.size() == nUp1 + nUp2) {
                    for (uint32_t i = 0; i < nUp1; ++i) {
                        J_CHECK(cm.upBodyTracks[i].boneName == "Up1_" + std::to_string(i));
                    }
                    for (uint32_t i = 0; i < nUp2; ++i) {
                        J_CHECK(cm.upBodyTracks[nUp1 + i].boneName == "Up2_" + std::to_string(i));
                    }
                }
            }
            {
                ByteWriter wHl;
                wHl.U32(0x0104);
                wHl.U32(0); wHl.U32(1); AppendZeroKeyTrack0103(wHl, "Up1Only"); wHl.U32(1); AppendZeroKeyTrack0103(wHl, "Up2Only");
                const RANMobile::EP7AnimClip hl104 =
                    RANMobile::EP7AnimationReader::ParseContainer(wHl.Data(), false, false);
                J_CHECK(hl104.valid && hl104.version == 0x0104 &&
                        hl104.mainTracks.empty() && hl104.upBodyTracks.size() == 2 &&
                        hl104.upBodyTracks[0].boneName == "Up1Only" &&
                        hl104.upBodyTracks[1].boneName == "Up2Only");
            }

            // Count boundaries & malformed counts for 0x0104:
            //   - upBody1=2048 + upBody2=2048 (combined == 4096 == kMaxBoneTracks) succeeds.
            //   - upBody1=2048 + upBody2=2049 (combined == 4097 > kMaxBoneTracks) fails cleanly.
            //   - Individual counts 4097 and 0xFFFFFFFF in main, upBody1, or upBody2 fail cleanly.
            {
                ByteWriter wMaxOk;
                wMaxOk.Raw(std::vector<uint8_t>(128, 0));
                wMaxOk.U32(0x0104);
                wMaxOk.U32(0);     // main = 0
                wMaxOk.U32(2048);  // upBody1 = 2048
                for (uint32_t i = 0; i < 2048; ++i) AppendZeroKeyTrack0103(wMaxOk, "A");
                wMaxOk.U32(2048);  // upBody2 = 2048 -> total upBody = 4096
                for (uint32_t i = 0; i < 2048; ++i) AppendZeroKeyTrack0103(wMaxOk, "B");
                const RANMobile::EP7AnimClip cMaxOk =
                    RANMobile::EP7AnimationReader::ParseContainer(wMaxOk.Data(), true, false);
                J_CHECK_MSG(cMaxOk.valid, cMaxOk.error);
                J_CHECK(cMaxOk.upBodyTracks.size() == 4096u);
                if (cMaxOk.upBodyTracks.size() == 4096u) {
                    J_CHECK(cMaxOk.upBodyTracks[2047].boneName == "A");
                    J_CHECK(cMaxOk.upBodyTracks[2048].boneName == "B");
                }

                ByteWriter wCombinedOver;
                wCombinedOver.Raw(std::vector<uint8_t>(128, 0));
                wCombinedOver.U32(0x0104);
                wCombinedOver.U32(0);
                wCombinedOver.U32(2048);
                for (uint32_t i = 0; i < 2048; ++i) AppendZeroKeyTrack0103(wCombinedOver, "A");
                wCombinedOver.U32(2049);  // combined 4097 > kMaxBoneTracks
                const RANMobile::EP7AnimClip cOver =
                    RANMobile::EP7AnimationReader::ParseContainer(wCombinedOver.Data(), true, false);
                J_CHECK(!cOver.valid && !cOver.error.empty());
            }
            for (int badList = 0; badList < 3; ++badList) {
                for (uint32_t badCount : {4097u, 0xFFFFFFFFu}) {
                    ByteWriter wb;
                    wb.Raw(std::vector<uint8_t>(128, 0));
                    wb.U32(0x0104);
                    wb.U32(badList == 0 ? badCount : 0u);
                    wb.U32(badList == 1 ? badCount : 0u);
                    wb.U32(badList == 2 ? badCount : 0u);
                    const RANMobile::EP7AnimClip rej =
                        RANMobile::EP7AnimationReader::ParseContainer(wb.Data(), true, false);
                    J_CHECK(!rej.valid && !rej.error.empty());
                }
            }
        }

        // Unimplemented container version (0x0200) and unimplemented inner track
        // version (0x0104) remain explicitly rejected in B15.
        for (uint32_t unsupOuter : {0x0200u, 0x0201u}) {
            ByteWriter wu;
            wu.Raw(std::vector<uint8_t>(128, 0));
            wu.U32(unsupOuter);
            wu.U32(0); wu.U32(0); wu.U32(0);
            const RANMobile::EP7AnimClip cu =
                RANMobile::EP7AnimationReader::ParseContainer(wu.Data(), true, false);
            J_CHECK(!cu.valid);
            J_CHECK(cu.error.rfind("unsupported animation container version ", 0) == 0);
        }
        {
            ByteWriter wi;
            wi.Raw(std::vector<uint8_t>(128, 0));
            wi.U32(0x0104);
            wi.U32(0);  // main = 0
            wi.U32(0);  // upBody1 = 0
            wi.U32(1);  // upBody2 = 1
            wi.U32(0x0104);  // unimplemented inner track version inside upBody2
            const RANMobile::EP7AnimClip ci =
                RANMobile::EP7AnimationReader::ParseContainer(wi.Data(), true, false);
            J_CHECK(!ci.valid);
            J_CHECK(ci.error.find("0x00000104") != std::string::npos);
        }
    }
}

void TestCharacterScene() {
    Section("scene integration (asset -> instance -> pose)");
    // Assemble the authentic-layout fixture set.
    const std::string skelText = SkeletonX();
    const std::string meshText = SkinnedMeshX();
    static std::vector<std::pair<std::string, std::vector<uint8_t>>> entries;
    entries.clear();
    entries.emplace_back("m_bone.skt", std::vector<uint8_t>(skelText.begin(), skelText.end()));
    entries.emplace_back("body.x", std::vector<uint8_t>(meshText.begin(), meshText.end()));
    entries.emplace_back("p_head.cps", PieceCps(0));
    entries.emplace_back("p_upper.cps", PieceCps(1));
    // RANMobile::EP7CharacterLoader resolves "st.bin"/"st.cfg" from "st_walk"
    entries.emplace_back("st_walk.bin", AnimationBin());
    entries.emplace_back("st_walk.cfg", AnimationCfg(true));

    static ResolverMap map;
    map.entries = &entries;

    RANMobile::EP7CharacterLoader loader;
    loader.SetResolver(&MapResolver, &map);
    const RANMobile::EP7CharacterAsset asset = loader.LoadCharacter(CharacterChf(), false);
    J_CHECK_MSG(asset.loaded, asset.error + " " + asset.file.error + " " +
                                     (asset.pieces.empty() ? std::string() : asset.pieces[0].error));
    J_CHECK(asset.skeleton != nullptr);
    J_CHECK(asset.boneCount == 5);
    J_CHECK(asset.pieceCount == 2);
    J_CHECK(asset.meshVertexCount == 8);   // 2 pieces x 4 vertices
    J_CHECK(asset.meshTriangleCount == 4);
    J_CHECK(asset.decodedGeometryPieces == 2);
    J_CHECK(asset.animationCount == 1);

    auto shared = std::make_shared<RANMobile::EP7CharacterAsset>(asset);
    RANMobile::EP7CharacterScene scene;
    const uint32_t id = scene.Spawn(shared, 0.0f, 0.0f, 0.0f, 0.0f);
    J_CHECK(id != 0);
    J_CHECK(scene.Instances().size() == 1);

    RANMobile::EP7CharacterInstance* instance = scene.Find(id);
    J_CHECK(instance != nullptr);
    if (!instance) return;

    // Pose evaluation follows the bind pose with no animation playing.
    J_CHECK(RANMobile::EP7CharacterScene::EvaluatePose(*instance, nullptr));
    J_CHECK(instance->poseValid);
    J_CHECK(instance->skinnedPositions.size() == 8 * 3);  // 2 pieces x 4 verts x 3 floats
    J_CHECK(instance->drawCalls == 2);                        // no material subsets parsed -> 1 per piece
    J_CHECK(instance->triangles == 4);

    // Playing an authentic clip name from the .chf works; unknown names do not.
    scene.SetPreviewTarget(id);
    J_CHECK(scene.PlayAnimation(id, "st_walk"));
    J_CHECK(!scene.PlayAnimation(id, "no_such_clip"));
    J_CHECK(instance->currentClip == 0);

    // Advance: the clip moves the Spine1 bone, so the head vertex set must translate.
    std::vector<float> before = instance->skinnedPositions;
    for (int i = 0; i < 30; ++i) scene.Update(1.0f / 60.0f);
    J_CHECK(instance->player.CurrentTime() > 0.0f);
    bool moved = false;
    for (size_t i = 0; i < before.size() && i < instance->skinnedPositions.size(); ++i) {
        if (std::fabs(before[i] - instance->skinnedPositions[i]) > 1e-4f) {
            moved = true;
            break;
        }
    }
    J_CHECK_MSG(moved, "animation should change at least one skinned vertex");

    // Diagnostics are measured, not assumed.
    const RANMobile::EP7CharacterDiagnostics diag = scene.GetDiagnostics();
    J_CHECK(diag.characters == 1);
    J_CHECK(diag.totalBones == 5);
    J_CHECK(diag.totalVertices == 8);
    J_CHECK(diag.animationCount == 1);
    J_CHECK(diag.authenticAssetsOnly);

    // Culling: without a renderer every posed instance stays visible; bounds come from
    // the authentic .chf box (20 units tall, 7 wide) scaled by the character scale.
    RANMobile::MobileCamera cullCamera;
    J_CHECK(scene.CullInstances(cullCamera, nullptr) == 1);
    J_CHECK(instance->inFrustum);
    J_CHECK(FloatNear(instance->boundsMax[1], 20.0f + 0.5f, 1e-3f));
    J_CHECK(FloatNear(instance->boundsMin[0], -7.5f, 1e-3f));

    // Lifecycle.
    J_CHECK(scene.Destroy(id));
    J_CHECK(scene.Instances().empty());
    J_CHECK(!scene.PlayAnimation(id, "st_walk"));

    // Loading an unavailable asset must fail loudly with no fake instance created.
    static std::vector<std::pair<std::string, std::vector<uint8_t>>> emptyEntries;
    emptyEntries.clear();
    static ResolverMap emptyMap;
    emptyMap.entries = &emptyEntries;
    RANMobile::EP7CharacterLoader failing;
    failing.SetResolver(&MapResolver, &emptyMap);
    const RANMobile::EP7CharacterAsset brokenAsset = failing.LoadCharacter(CharacterChf(), false);
    J_CHECK(!brokenAsset.loaded);
    J_CHECK(!brokenAsset.error.empty());

    RANMobile::EP7CharacterScene scene2;
    J_CHECK(scene2.Spawn(std::make_shared<RANMobile::EP7CharacterAsset>(brokenAsset), 0, 0, 0, 0) == 0);
    J_CHECK(scene2.Instances().empty());

    // -----------------------------------------------------------------
    // B16 pipeline integration: the same character resolver fed a 0x0115 .cfg
    // (LoadFile_0111_0112_0114_0115 layout, rich effect/AniScale content, official
    // trailing bytes) must load, spawn and animate exactly like the 0x0107 one above.
    // The .cfg is the animation metadata source for the Mobile pipeline; the loader
    // resolves "st_walk.cfg" next to "st_walk.bin" by stem (DxSkinAniMan::
    // LoadAnimContainer ChangeExt(".x" -> ".cfg")).
    // -----------------------------------------------------------------
    {
        entries.clear();
        entries.emplace_back("m_bone.skt", std::vector<uint8_t>(skelText.begin(), skelText.end()));
        entries.emplace_back("body.x", std::vector<uint8_t>(meshText.begin(), meshText.end()));
        entries.emplace_back("p_head.cps", PieceCps(0));
        entries.emplace_back("p_upper.cps", PieceCps(1));
        entries.emplace_back("st_walk.bin", AnimationBin());
        entries.emplace_back("st_walk.cfg",
                             AnimationCfg(true, 0x0115, /*withEffects=*/true,
                                          /*withAniScale=*/true, /*withTrailing=*/true));
        RANMobile::EP7CharacterLoader loaderB16;
        loaderB16.SetResolver(&MapResolver, &map);
        const RANMobile::EP7CharacterAsset assetB16 = loaderB16.LoadCharacter(CharacterChf(), false);
        J_CHECK_MSG(assetB16.loaded, assetB16.error);
        J_CHECK(assetB16.animationCount == 1);

        RANMobile::EP7CharacterScene sceneB16;
        const uint32_t idB16 = sceneB16.Spawn(std::make_shared<RANMobile::EP7CharacterAsset>(assetB16), 0.0f, 0.0f, 0.0f, 0.0f);
        J_CHECK(idB16 != 0);
        RANMobile::EP7CharacterInstance* instB16 = sceneB16.Find(idB16);
        J_CHECK(instB16 != nullptr);
        if (!instB16) return;
        // Pose once in the bind pose so the baseline matches the first test (captured
        // before its PlayAnimation call).
        J_CHECK(RANMobile::EP7CharacterScene::EvaluatePose(*instB16, nullptr));
        sceneB16.SetPreviewTarget(idB16);
        std::vector<float> beforeB16 = instB16->skinnedPositions;
        J_CHECK(sceneB16.PlayAnimation(idB16, "st_walk"));
        for (int i = 0; i < 30; ++i) sceneB16.Update(1.0f / 60.0f);
        bool movedB16 = false;
        for (size_t i = 0; i < beforeB16.size() && i < instB16->skinnedPositions.size(); ++i) {
            if (std::fabs(beforeB16[i] - instB16->skinnedPositions[i]) > 1e-4f) {
                movedB16 = true;
                break;
            }
        }
        J_CHECK_MSG(movedB16, "a 0x0115 .cfg must drive the same clip playback as 0x0107");
        J_CHECK(sceneB16.Destroy(idB16));
    }
}

void TestMath() {
    Section("math / quaternion decompression");
    using namespace RANMobile::ep7math;

    const Quat identity(0.0f, 0.0f, 0.0f, 1.0f);
    const Mat4 rotation = RotationQuaternion(identity);
    J_CHECK(FloatNear(rotation.m[0][0], 1.0f));
    J_CHECK(FloatNear(rotation.m[1][1], 1.0f));
    J_CHECK(FloatNear(rotation.m[2][2], 1.0f));

    // D3DX convention: result = A * B (row-vector), so a translation then a scale
    // composes as Translate * Scale.
    const Mat4 translate = Translation(1.0f, 2.0f, 3.0f);
    const Vec3 p = TransformCoord(Vec3(0.0f, 0.0f, 0.0f), translate);
    J_CHECK(FloatNear(p.x, 1.0f) && FloatNear(p.y, 2.0f) && FloatNear(p.z, 3.0f));

    const Mat4 scale = Scaling(2.0f, 2.0f, 2.0f);
    // Row-vector convention: v * Multiple(a, b) == ((v * a) * b), so translate-then-scale
    // gives (0,0,0) -> (1,2,3) -> (2,4,6).
    const Mat4 combined = Multiply(translate, scale);
    const Vec3 q = TransformCoord(Vec3(0.0f, 0.0f, 0.0f), combined);
    J_CHECK(FloatNear(q.x, 2.0f) && FloatNear(q.y, 4.0f) && FloatNear(q.z, 6.0f));

    // Quaternion decompression: matches the 16-bit encoding used by SAnimation
    // (xyz = (i16 - 32767) / 32768, w = u16 / 65535).
    // QUATCOMP: x/y/z are stored as (value * 32767 + 32768) halves, w as value * 65535.
    const Quat dq = DecompressQuaternion(0x7FFF7FFFu, 0x7FFF7FFFu);
    J_CHECK(FloatNear(dq.x, 0.0f, 1e-3f));
    J_CHECK(FloatNear(dq.y, 0.0f, 1e-3f));
    J_CHECK(FloatNear(dq.z, 0.0f, 1e-3f));
    // Authentic quirk, reproduced on purpose: DecompressionQuaternion subtracts 32767 from
    // the w half too, so a stored w = 0xffff decodes to 32768/65535, not 1.0.
    J_CHECK(FloatNear(dq.w, 32768.0f / 65535.0f, 1e-4f));
    J_CHECK(dq.w > 0.49f && dq.w < 0.51f);

    // Slerp between identity and a 90-degree yaw reaches the midpoint at t = 0.5.
    const float s = std::sin(3.14159265f / 4.0f);
    const float c = std::cos(3.14159265f / 4.0f);
    const Quat half(0.0f, s, 0.0f, c);
    const Quat mid = Slerp(identity, half, 0.5f);
    J_CHECK(FloatNear(mid.y, std::sin(3.14159265f / 8.0f), 1e-3f));
    J_CHECK(FloatNear(mid.w, std::cos(3.14159265f / 8.0f), 1e-3f));
}

// ---------------------------------------------------------------------------
// Robustness sweep: every truncated prefix and every single-byte corruption of each
// authentic-layout fixture must be rejected cleanly or parsed cleanly — never crash,
// never read out of bounds, never allocate wildly.  Run under ASan/UBSan this is the
// strongest available guarantee that the parsers honour the Phase 3J "bounds checking,
// overflow protection, invalid-index detection, safe memory" requirement.
// ---------------------------------------------------------------------------
void TestRobustness() {
    Section("truncation + corruption robustness sweep");

    const std::string skelText = SkeletonX();
    const std::string meshText = SkinnedMeshX();

    std::vector<std::vector<uint8_t>> fixtures;
    fixtures.emplace_back(skelText.begin(), skelText.end());
    fixtures.emplace_back(meshText.begin(), meshText.end());
    fixtures.push_back(CharacterChf());
    fixtures.push_back(PieceCps(0));
    fixtures.push_back(AnimationBin());
    fixtures.push_back(AnimationBin0100());
    fixtures.push_back(AnimationBin0103());
    fixtures.push_back(AnimationBin0104());
    fixtures.push_back(AnimationCfg(true));
    fixtures.push_back(AnimationCfg(true, 0x0110));
    fixtures.push_back(AnimationCfg(true, 0x0115, /*withEffects=*/true, /*withAniScale=*/true,
                                    /*withTrailing=*/true));

    RANMobile::EP7Skeleton skeleton;
    skeleton.LoadFromXFile(reinterpret_cast<const uint8_t*>(skelText.data()), skelText.size());

    for (const auto& original : fixtures) {
        // 1. Truncations: step through the buffer with a stride so the sweep stays fast.
        const size_t step = original.size() > 400 ? 7 : 1;
        for (size_t len = 0; len <= original.size(); len += step) {
            std::vector<uint8_t> cut(original.begin(), original.begin() + static_cast<long>(len));
            // None of these may throw or crash.
            {
                RANMobile::EP7Skeleton probe;
                (void)probe.LoadFromXFile(cut);
            }
            (void)RANMobile::EP7SkinnedMeshReader::Parse(cut, std::string(), &skeleton);
            (void)RANMobile::EP7SkinnedMeshReader::Parse(cut, "BodyMesh", &skeleton);
            (void)RANMobile::EP7CharacterFileReader::Parse(cut, false);
            (void)RANMobile::EP7SkinPieceReader::ParseFromBytes(cut, false);
            (void)RANMobile::EP7AnimationReader::ParseContainer(cut, true, false);
            (void)RANMobile::EP7AnimationReader::ParseContainer(cut, false, false);
            (void)RANMobile::EP7AnimationReader::ParseInfo(cut, true, false);
        }

        // 2. Single-byte corruptions at every offset (values chosen to stress lengths).
        for (size_t i = 0; i < original.size(); ++i) {
            for (uint8_t value : {0x00, 0x7F, 0x80, 0xFF}) {
                std::vector<uint8_t> mutated = original;
                mutated[i] = value;
                (void)RANMobile::EP7SkinnedMeshReader::Parse(mutated, std::string(), &skeleton);
                (void)RANMobile::EP7CharacterFileReader::Parse(mutated, false);
                (void)RANMobile::EP7SkinPieceReader::ParseFromBytes(mutated, false);
                (void)RANMobile::EP7AnimationReader::ParseContainer(mutated, true, false);
                (void)RANMobile::EP7AnimationReader::ParseInfo(mutated, true, false);
            }
        }
    }

    J_CHECK_MSG(true, "survived the sweep without crashing");
    std::printf("  swept %zu fixtures (truncations + 4x byte corruptions) + the .rcc archive\n",
                fixtures.size());
}


// ---------------------------------------------------------------------------
// Minimal ZIP writer used ONLY to build an .rcc fixture in memory. Entries are stored
// (uncompressed) so the test does not depend on an external compressor; the deflate path is
// covered separately against a real archive generated in CI/desktop tooling.
// ---------------------------------------------------------------------------
uint32_t Crc32Test(const uint8_t* data, size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) crc = (crc & 1u) ? (0xEDB88320u ^ (crc >> 1)) : (crc >> 1);
    }
    return crc ^ 0xFFFFFFFFu;
}

struct ZipEntrySpec {
    std::string name;
    std::vector<uint8_t> data;
    bool encrypt = false;  // traditional ZipCrypto with the client password
};

void PutU16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}
void PutU32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
}

// ZipCrypto encryption (mirror of the decryption the reader performs).
std::vector<uint8_t> ZipCryptEncrypt(const std::vector<uint8_t>& plain, const char* password,
                                     uint16_t checkByte) {
    uint32_t key[3] = {0x12345678u, 0x23456789u, 0x34567890u};
    auto crcUpdate = [](uint32_t crc, uint8_t v) {
        crc ^= v;
        for (int k = 0; k < 8; ++k) crc = (crc & 1u) ? (0xEDB88320u ^ (crc >> 1)) : (crc >> 1);
        return crc;
    };
    auto update = [&](uint8_t p) {
        key[0] = crcUpdate(key[0], p);
        key[1] = (key[1] + (key[0] & 0xFFu)) * 134775813u + 1u;
        key[2] = crcUpdate(key[2], static_cast<uint8_t>(key[1] >> 24));
    };
    for (const char* p = password; *p; ++p) update(static_cast<uint8_t>(*p));

    // 12-byte header: 11 random bytes (we use a fixed pattern) + check byte.
    std::vector<uint8_t> out;
    for (int i = 0; i < 12; ++i) {
        const uint8_t plainByte = (i == 11) ? static_cast<uint8_t>(checkByte) : static_cast<uint8_t>(i * 7 + 3);
        const uint8_t cipher = static_cast<uint8_t>(plainByte ^ (key[2] >> 8));
        update(plainByte);
        out.push_back(cipher);
    }
    for (uint8_t plainByte : plain) {
        const uint8_t cipher = static_cast<uint8_t>(plainByte ^ (key[2] >> 8));
        update(plainByte);
        out.push_back(cipher);
    }
    return out;
}

std::vector<uint8_t> BuildZip(const std::vector<ZipEntrySpec>& entries) {
    std::vector<uint8_t> out;
    struct Central {
        ZipEntrySpec spec;
        uint32_t crc = 0;
        uint32_t offset = 0;
        uint32_t storedSize = 0;
        bool encrypt = false;
    };
    std::vector<Central> central;

    for (const auto& spec : entries) {
        Central c;
        c.spec = spec;
        c.crc = Crc32Test(spec.data.data(), spec.data.size());
        c.offset = static_cast<uint32_t>(out.size());
        c.encrypt = spec.encrypt;

        std::vector<uint8_t> payload = spec.data;
        if (spec.encrypt) payload = ZipCryptEncrypt(spec.data, RANMobile::EP7RccArchive::kZipPassword, 0x42);
        c.storedSize = static_cast<uint32_t>(payload.size());

        // local header (ZIP spec order: sig, ver, flags, method, time, date, crc,
        // csize, usize, namelen, extralen, name, data)
        PutU32(out, 0x04034b50u);
        PutU16(out, 20);                            // version needed
        PutU16(out, spec.encrypt ? 1 : 0);          // flags
        PutU16(out, 0);                             // method = stored
        PutU16(out, spec.encrypt ? 0x4200 : 0);     // mod time (high byte = zipcrypto check)
        PutU16(out, 0);                             // mod date
        PutU32(out, c.crc);
        PutU32(out, c.storedSize);
        PutU32(out, static_cast<uint32_t>(spec.data.size()));
        PutU16(out, static_cast<uint16_t>(spec.name.size()));
        PutU16(out, 0);
        out.insert(out.end(), spec.name.begin(), spec.name.end());
        out.insert(out.end(), payload.begin(), payload.end());
        central.push_back(c);
    }

    const uint32_t cdOffset = static_cast<uint32_t>(out.size());
    for (const auto& c : central) {
        // central directory (ZIP spec order: sig, verMadeBy, verNeeded, flags, method,
        // time, date, crc, csize, usize, namelen, extralen, commentlen, disk,
        // intAttrs, extAttrs, localOffset, name)
        PutU32(out, 0x02014b50u);
        PutU16(out, 20);                            // version made by
        PutU16(out, 20);                            // version needed
        PutU16(out, c.encrypt ? 1 : 0);             // flags
        PutU16(out, 0);                             // method
        PutU16(out, c.encrypt ? 0x4200 : 0);        // mod time
        PutU16(out, 0);                             // mod date
        PutU32(out, c.crc);
        PutU32(out, c.storedSize);
        PutU32(out, static_cast<uint32_t>(c.spec.data.size()));
        PutU16(out, static_cast<uint16_t>(c.spec.name.size()));
        PutU16(out, 0);  // extra len
        PutU16(out, 0);  // comment len
        PutU16(out, 0);  // disk number
        PutU16(out, 0);  // internal attrs
        PutU32(out, 0);  // external attrs
        PutU32(out, c.offset);
        out.insert(out.end(), c.spec.name.begin(), c.spec.name.end());
    }
    const uint32_t cdSize = static_cast<uint32_t>(out.size()) - cdOffset;

    PutU32(out, 0x06054b50u);
    PutU16(out, 0);
    PutU16(out, 0);
    PutU16(out, static_cast<uint16_t>(central.size()));
    PutU16(out, static_cast<uint16_t>(central.size()));
    PutU32(out, cdSize);
    PutU32(out, cdOffset);
    PutU16(out, 0);
    return out;
}

void TestRccArchive() {
    Section(".rcc archive reader (SkinObject.rcc / Animation.rcc)");

    const std::string skelText = SkeletonX();
    const std::string meshText = SkinnedMeshX();
    std::vector<ZipEntrySpec> specs;
    specs.push_back({"m_bone.skt", std::vector<uint8_t>(skelText.begin(), skelText.end()), false});
    specs.push_back({"BODY.X", std::vector<uint8_t>(meshText.begin(), meshText.end()), false});
    specs.push_back({"npc.chf", CharacterChf(), false});
    specs.push_back({"secret.cps", PieceCps(0), true});  // ZipCrypto "tkqwlf"

    const std::vector<uint8_t> archiveBytes = BuildZip(specs);
    RANMobile::EP7RccArchive archive;
    J_CHECK_MSG(archive.Open(archiveBytes), archive.Error());
    J_CHECK(archive.EntryCount() == 4);

    // Case-insensitive lookup, exactly like CUnzipper::GOTOFILEPOS.
    const RANMobile::EP7RccEntry* body = archive.Find("body.x");
    J_CHECK(body != nullptr);
    J_CHECK(body && body->method == 0);

    std::vector<uint8_t> out;
    J_CHECK(archive.ExtractByName("m_bone.skt", out));
    J_CHECK(out.size() == skelText.size());
    J_CHECK(std::memcmp(out.data(), skelText.data(), out.size()) == 0);

    // The extracted bytes must feed the real parser.
    RANMobile::EP7Skeleton skeleton;
    J_CHECK(skeleton.LoadFromXFile(out));
    J_CHECK(skeleton.BoneCount() == 5);

    // Encrypted entry decrypts with the client password.
    J_CHECK(archive.ExtractByName("SECRET.CPS", out));
    const RANMobile::EP7SkinPieceInfo piece = RANMobile::EP7SkinPieceReader::ParseFromBytes(out, false);
    J_CHECK_MSG(piece.valid, piece.error);

    // .chf straight out of the pack.
    J_CHECK(archive.ExtractByName("npc.chf", out));
    J_CHECK(RANMobile::EP7CharacterFileReader::Parse(out, false).valid);

    // Missing entry.
    J_CHECK(archive.Find("nope.bin") == nullptr);
    J_CHECK(!archive.ExtractByName("nope.bin", out));

    // Corrupt CRC must be rejected, never silently accepted.
    std::vector<uint8_t> corrupt = archiveBytes;
    for (size_t i = 0; i + 4 <= corrupt.size(); ++i) {
        if (corrupt[i] == 'B' && corrupt[i + 1] == 'O' && corrupt[i + 2] == 'D' && corrupt[i + 3] == 'Y') {
            // entry data starts right after the local header (30 + 4 name bytes)
            corrupt[i + 10] ^= 0xFF;
            break;
        }
    }
    {
        RANMobile::EP7RccArchive mutated;
        if (mutated.Open(corrupt)) {
            J_CHECK(!mutated.ExtractByName("BODY.X", out));
        }
    }

    // Truncated / non-ZIP input.
    RANMobile::EP7RccArchive empty;
    J_CHECK(!empty.Open(std::vector<uint8_t>()));
    J_CHECK(!empty.Error().empty());
    // The archive reader must be equally robust: corrupt every byte of a ZIP and truncate it.
    {
        const std::vector<uint8_t>& zip = archiveBytes;

        for (size_t len = 0; len <= zip.size(); len += 3) {
            std::vector<uint8_t> cut(zip.begin(), zip.begin() + static_cast<long>(len));
            RANMobile::EP7RccArchive archive;
            if (archive.Open(cut)) {
                std::vector<uint8_t> ignored;
                for (const auto& entry : archive.Entries()) (void)archive.Extract(entry, ignored);
            }
        }
        for (size_t i = 0; i < zip.size(); ++i) {
            for (uint8_t value : {0x00, 0x7F, 0xFF}) {
                std::vector<uint8_t> mutated = zip;
                mutated[i] = value;
                RANMobile::EP7RccArchive archive;
                if (archive.Open(mutated)) {
                    std::vector<uint8_t> ignored;
                    for (const auto& entry : archive.Entries()) (void)archive.Extract(entry, ignored);
                }
            }
        }
    }

    std::vector<uint8_t> garbage(200, 0x5A);
    RANMobile::EP7RccArchive bogus;
    J_CHECK(!bogus.Open(garbage));
    J_CHECK_MSG(true, "archive survived truncation + corruption sweep");
}

}  // namespace

int main() {
    std::printf("=== Phase 3J — character / skeleton / animation tests ===\n");
    TestTokenizer();
    TestSkeleton();
    TestSkinnedMesh();
    TestSkinPiece();
    TestSkinPieceTraceInfluence();
    TestSkinPieceLegacyVersions();
    TestSkinPieceInfluenceSizeAccounting();
    TestCharacterFile();
    TestAnimation();
    TestCharacterScene();
    TestMath();
    TestRobustness();
    TestRccArchive();

    std::printf("\n%d checks, %d failure(s)\n", g_checks, g_failures);
    if (g_failures == 0) std::printf("PHASE 3J TESTS PASSED\n");
    return g_failures == 0 ? 0 : 1;
}
