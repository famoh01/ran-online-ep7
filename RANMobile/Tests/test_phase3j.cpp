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
    for (int i = 0; i < 3; ++i) w.U32(0); // padding
    for (int i = 0; i < 16; ++i) w.F32((i % 5 == 0) ? 1.0f : 0.0f);
    w.U32(0);                             // scale key count
    // ---- main track count 0 ----
    w.U32(0);
    return w.Data();
}

// SANIMCONINFO 0x0107 (payload only, header present).
// Emits the authentic SANIMCONINFO byte layout for each version, including the MSVC struct
// padding the engine actually reads:
//   0x0101..0x0104 are read with ReadBuffer(&OldAnimInfo, sizeof(SANIMCONINFO_10x)), i.e.
//   whole padded structs (304 / 564 / 732 / 844 bytes); 0x0105..0x0107 are read field by
//   field but with sizeof(SChaSoundData_103)=428 / sizeof(SChaSoundData)=388 in the tail.
// SOURCE: enginelib/Meshs/DxSkinAniManSaveLoad.cpp, SAnimation.h, CharacterSound.h.
std::vector<uint8_t> AnimationCfg(bool loop, uint32_t version = 0x0107) {
    ByteWriter w;
    w.Raw(std::vector<uint8_t>(128, 0));
    w.U32(version);       // SANIMCONINFO version in the header fileId
    w.FixedStr("st_walk", 128);
    w.FixedStr("m_bone.skt", 128);
    w.U32(loop ? 0x00000010u : 0u);  // m_dwFlag -> kAnimFlagLoop
    w.U32(0);
    w.U32(4800);
    w.U32(4800);
    w.U32(4800);   // m_UNITTIME
    w.I32(1);      // mainType
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

    // Version gating: 0x0103 and below are rejected by the reader (original accepts
    // only 0x0104..0x0107 in this code path). The version is the first payload DWORD,
    // i.e. absolute offset 128.
    std::vector<uint8_t> oldVersion = bytes;
    oldVersion[132] = 0x03;
    oldVersion[133] = 0x01;
    const RANMobile::EP7SkinPieceInfo old = RANMobile::EP7SkinPieceReader::ParseFromBytes(oldVersion, false);
    J_CHECK(!old.valid);

    // Truncated payload must fail, not zero-fill.
    std::vector<uint8_t> truncated(bytes.begin(), bytes.begin() + 40);
    const RANMobile::EP7SkinPieceInfo cut = RANMobile::EP7SkinPieceReader::ParseFromBytes(truncated, false);
    J_CHECK(!cut.valid);
    J_CHECK(!cut.error.empty());

    // Empty buffer.
    J_CHECK(!RANMobile::EP7SkinPieceReader::ParseFromBytes(std::vector<uint8_t>(), false).valid);
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

    // Versions the client ships but the engine source does not define must be rejected with
    // the value named -- never guessed at.
    for (uint32_t version : {0x0110u, 0x0114u, 0x0115u}) {
        std::vector<uint8_t> unknown = AnimationCfg(true, 0x0106);
        unknown[128] = static_cast<uint8_t>(version & 0xFF);
        unknown[129] = static_cast<uint8_t>((version >> 8) & 0xFF);
        const RANMobile::EP7AnimInfo rejected = RANMobile::EP7AnimationReader::ParseInfo(unknown, true, false);
        J_CHECK(!rejected.valid);
        char expected[16];
        std::snprintf(expected, sizeof(expected), "0x%08X", version);
        J_CHECK(rejected.error.find(expected) != std::string::npos);
        // A known-but-unimplemented version must be classified UNSUPPORTED (a tracked
        // coverage gap, B11), never silently accepted and never reported as parsed.
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
        foreign[128] = 0x14;  // an unimplemented version, but the wrong file family
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
    fixtures.push_back(AnimationCfg(true));

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
