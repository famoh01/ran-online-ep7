// Verifier-only regression suite. Include the actual CLI translation unit so tests
// exercise VerifySingleAsset/counters/reporting, NOT a duplicate of the classifier.
#define main EP7VerifierCliMain
#include "../Tools/verify_ep7_character_assets.cpp"
#undef main

#include <cstdlib>

namespace {
int checks = 0, failures = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    ++failures; std::printf("CHECK FAILED line %d: %s\n", __LINE__, #condition); \
} } while (0)

void U32(std::vector<uint8_t>& b, uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>(v >> (i * 8)));
}
void SetU32(std::vector<uint8_t>& b, size_t at, uint32_t v) {
    for (int i = 0; i < 4; ++i) b[at + i] = static_cast<uint8_t>(v >> (i * 8));
}
std::vector<uint8_t> Header(const char* type, uint32_t version) {
    // Source ABI fixture, NOT authentic client bytes. Nonzero bytes after the
    // terminator are intentional: SerialFile uses StringCchCopy, not zero-fill.
    std::vector<uint8_t> b(128, 0xa5);
    std::memcpy(b.data(), type, std::strlen(type) + 1);
    U32(b, version); // SerialFile.cpp:57-65: DWORD starts at byte 128.
    return b;
}
std::vector<uint8_t> Container(uint32_t version, bool track = false) {
    auto b = Header("AnimContainer", version);
    // SAnimContainer::SaveToFile: 0x0102 up-body list then main list.
    // Load_0101: main then up-body. Load_0100: main only.
    U32(b, track ? 1 : 0);
    if (track) {
        // SAnimation::SaveToFile (SAnimation.cpp:1354 onwards), VERSION 0x0102:
        // name, rotate/position counts, name length, quatpos count, name length,
        // matrix count, scale count. Zero-key named track is source-permitted.
        U32(b, 0x0102);
        const char name[] = "Bip01";
        U32(b, sizeof(name));
        b.insert(b.end(), name, name + sizeof(name));
        U32(b, 0); U32(b, 0); U32(b, sizeof(name) - 1);
        U32(b, 0); U32(b, sizeof(name) - 1 + 0x0102); U32(b, 0); U32(b, 0);
    }
    if (version != 0x0100) U32(b, 0);
    return b;
}
Outcome Verify(const std::vector<uint8_t>& b, Counters& c, const char* path = "fixture.bin") {
    return VerifySingleAsset(path, b, c);
}
}

int main() {
    for (uint32_t version : {0x0100u, 0x0101u, 0x0102u}) {
        const auto b = Container(version);
        CHECK(std::memcmp(b.data(), "AnimContainer", 14) == 0);
        CHECK(b[128] == (version & 0xff) && b[129] == 1 && b[130] == 0 && b[131] == 0);
        Counters c;
        CHECK(Verify(b, c) == Outcome::Pass); // Old printable-ASCII gate fails this.
        CHECK(c.binTotal == 1 && c.binOk == 1 && c.binUnsupported == 0);
    }
    const auto valid = Container(0x0102, true);
    Counters c;
    CHECK(Verify(valid, c, "fixture.BIN") == Outcome::Pass);
    CHECK(c.binOk == 1);
    const auto clip = RANMobile::EP7AnimationReader::ParseContainer(valid, true, false);
    CHECK(clip.valid && clip.version == 0x0102);
    CHECK(clip.upBodyTracks.size() == 1 && clip.mainTracks.empty());
    CHECK(clip.upBodyTracks[0].boneName == "Bip01");

    // Every strict prefix of the nonempty valid fixture must fail safely.
    for (size_t n = 0; n < valid.size(); ++n) {
        Counters cut;
        CHECK(Verify(std::vector<uint8_t>(valid.begin(), valid.begin() + n), cut) == Outcome::Fail);
        CHECK(cut.binOk == 0 && cut.binUnsupported == 0);
        CHECK(cut.binInvalidHeader + cut.binParserRejected == 1);
    }
    auto unsupported = Container(0x0102);
    SetU32(unsupported, 128, 0x12345678);
    Counters u;
    CHECK(Verify(unsupported, u) == Outcome::Unsupported);
    CHECK(u.binUnsupported == 1 && u.binOk == 0 && u.binParserRejected == 0);
    CHECK(g_unsupportedReasons.count("bin: unsupported animation container version 0x12345678 (implemented: 0x0100, 0x0101, 0x0102, 0x0103)") == 1);

    auto invalid = Container(0x0102);
    SetU32(invalid, 132, 0xffffffff); // Count guard, not an unsupported version.
    Counters i;
    CHECK(Verify(invalid, i) == Outcome::Fail);
    CHECK(i.binParserRejected == 1 && i.binUnsupported == 0);
    auto badTrack = valid;
    SetU32(badTrack, 136, 0x9999);
    Counters t;
    CHECK(Verify(badTrack, t) == Outcome::Fail);
    CHECK(t.binParserRejected == 1 && t.binUnsupported == 0);

    // Parser alone accepts an unrelated type; verifier must NOT accept it.
    auto foreign = Container(0x0102);
    std::memcpy(foreign.data(), "ForeignFormat\0", 14);
    CHECK(RANMobile::EP7AnimationReader::ParseContainer(foreign, true, false).valid);
    Counters f;
    CHECK(Verify(foreign, f) == Outcome::Fail);
    CHECK(f.binUnrecognizedType == 1 && f.binOk == 0 && f.binUnsupported == 0);
    auto unterminated = Container(0x0102);
    unterminated[13] = 'X';
    Counters z;
    CHECK(Verify(unterminated, z) == Outcome::Fail);
    auto zeroHeader = Container(0x0102);
    std::fill(zeroHeader.begin(), zeroHeader.begin() + 128, 0);
    Counters zero;
    CHECK(Verify(zeroHeader, zero) == Outcome::Fail);
    CHECK(zero.binUnrecognizedType == 1);

    // No unsafe/headerless retry may turn a bad header into a pass.
    std::vector<uint8_t> headerless;
    U32(headerless, 0x0100); U32(headerless, 0);
    CHECK(RANMobile::EP7AnimationReader::ParseContainer(headerless, false, false).valid);
    Counters h;
    CHECK(Verify(headerless, h) == Outcome::Fail);
    CHECK(h.binInvalidHeader == 1);

    // Existing unrelated-extension classifier behavior remains unchanged.
    Counters other;
    CHECK(VerifySingleAsset("a.cfg", Header("SANIMCONINFO", 0x0108), other) == Outcome::Unsupported);
    CHECK(other.cfgTotal == 1 && other.cfgUnsupported == 1);
    CHECK(VerifySingleAsset("a.chf", {}, other) == Outcome::Fail);
    CHECK(VerifySingleAsset("a.cps", {}, other) == Outcome::Fail);
    CHECK(VerifySingleAsset("a.skt", {}, other) == Outcome::Fail);
    const std::string skeleton = "xof 0303txt 0032\nFrame Bip01 { }\n";
    CHECK(VerifySingleAsset("a.x", {skeleton.begin(), skeleton.end()}, other) == Outcome::Pass);
    CHECK(other.chfTotal == 1 && other.cpsTotal == 1 && other.xTotal == 2 && other.xSkeletonOk == 1);
    CHECK(other.binTotal == 0);
    CHECK(IsCharacterAssetName("a.BIN") && !IsCharacterAssetName("a.txt"));
    CHECK(IsInCharacterDataPath("root/data/animation/a.bin"));
    CHECK(!IsInCharacterDataPath("root/data/terrain/a.bin"));

    // Phase 3J B13: 0x0100 container with 0x0100 tracks (pos, rot, scale, 80B SMatrixKey
    // with 12B align16 padding, empty bone name + named bone) must pass and reject every
    // strict byte prefix safely.
    {
        auto b100 = Header("AnimContainer", 0x0100u);
        U32(b100, 2u);  // 2 main tracks
        // Track 0: named bone with pos + rot + scale + 1x 80B SMatrixKey
        U32(b100, 0x0100u);
        U32(b100, 1u); U32(b100, 160u); U32(b100, 0x3F800000u); U32(b100, 0u); U32(b100, 0u);
        U32(b100, 1u); U32(b100, 320u); U32(b100, 0u); U32(b100, 0u); U32(b100, 0u); U32(b100, 0x3F800000u);
        U32(b100, 1u); U32(b100, 480u); U32(b100, 0x3F800000u); U32(b100, 0x3F800000u); U32(b100, 0x3F800000u);
        U32(b100, 1u); U32(b100, 640u);
        U32(b100, 0xDEADBEEFu); U32(b100, 0xCAFEBABEu); U32(b100, 0x13579BDFu);  // 12B align16 pad
        for (int k = 0; k < 16; ++k) U32(b100, (k % 5 == 0) ? 0x3F800000u : 0u);
        const char bone0[] = "Bip01";
        U32(b100, sizeof(bone0));
        b100.insert(b100.end(), bone0, bone0 + sizeof(bone0));
        // Track 1: empty bone name ("")
        U32(b100, 0x0100u);
        U32(b100, 0u); U32(b100, 0u); U32(b100, 0u); U32(b100, 0u);
        U32(b100, 1u); b100.push_back(0);  // empty NUL-terminated string

        Counters c100;
        CHECK(Verify(b100, c100, "a_gum_stay.bin") == Outcome::Pass);
        CHECK(c100.binOk == 1 && c100.binParserRejected == 0 && c100.binUnsupported == 0);
        const auto parsed100 = RANMobile::EP7AnimationReader::ParseContainer(b100, true, false);
        CHECK(parsed100.valid && parsed100.mainTracks.size() == 2);
        CHECK(parsed100.mainTracks[0].boneName == "Bip01");
        CHECK(parsed100.mainTracks[0].matrixKeys.size() == 1 &&
              parsed100.mainTracks[0].matrixKeys[0].mat[0] == 1.0f &&
              parsed100.mainTracks[0].matrixKeys[0].mat[15] == 1.0f);
        CHECK(parsed100.mainTracks[1].boneName.empty());

        for (size_t n = 0; n < b100.size(); ++n) {
            Counters cut;
            CHECK(Verify(std::vector<uint8_t>(b100.begin(), b100.begin() + n), cut) == Outcome::Fail);
            CHECK(cut.binOk == 0 && cut.binUnsupported == 0);
            CHECK(cut.binInvalidHeader + cut.binParserRejected == 1);
        }
    }

    // Phase 3J B14: 0x0103 outer container (upBody, dwGarbageValue = 0x0103 + up + main, main)
    // with 0x0103 inner tracks (bone, rot, dwGarbage1, pos, quatPos, 80B matrix, dwGarbage2, scale)
    // must pass and reject every strict byte prefix safely; 0x0104 and 0x0200 remain UNSUPPORTED.
    {
        auto b103 = Header("AnimContainer", 0x0103u);
        U32(b103, 1u);  // upBody count = 1
        U32(b103, 0x0103u);
        const char upBone[] = "Bip01_Spine1";
        U32(b103, sizeof(upBone));
        b103.insert(b103.end(), upBone, upBone + sizeof(upBone));
        U32(b103, 1u); U32(b103, 160u); U32(b103, 0u); U32(b103, 0u); U32(b103, 0u); U32(b103, 0x3F800000u);  // rot
        U32(b103, sizeof(upBone) - 1);  // dwGarbage1 == bone.len
        U32(b103, 1u); U32(b103, 320u); U32(b103, 0x3F800000u); U32(b103, 0u); U32(b103, 0u);  // pos
        U32(b103, 1u); U32(b103, 480u);  // quatPos (36B)
        U32(b103, 0x3F800000u); U32(b103, 0x3F800000u); U32(b103, 0x3F800000u);
        U32(b103, 0u); U32(b103, 0u); U32(b103, 0u);
        U32(b103, (32767u << 16) | 32767u); U32(b103, (32767u << 16) | 65535u);
        U32(b103, 1u); U32(b103, 640u);  // matrix (80B)
        U32(b103, 0xDEADBEEFu); U32(b103, 0xCAFEBABEu); U32(b103, 0x13579BDFu);
        for (int k = 0; k < 16; ++k) U32(b103, (k % 5 == 0) ? 0x3F800000u : 0u);
        U32(b103, static_cast<uint32_t>(sizeof(upBone) - 1) + 0x0103u);  // dwGarbage2 == bone.len + 0x0103
        U32(b103, 1u); U32(b103, 800u); U32(b103, 0x3F800000u); U32(b103, 0x3F800000u); U32(b103, 0x3F800000u);  // scale

        U32(b103, 0x0103u + 1u + 1u);  // inter-list dwGarbageValue == 0x0103 + numUpBody + numMain
        U32(b103, 1u);                 // main count = 1
        U32(b103, 0x0103u);
        const char mainBone[] = "Bip01";
        U32(b103, sizeof(mainBone));
        b103.insert(b103.end(), mainBone, mainBone + sizeof(mainBone));
        U32(b103, 0u);                                                   // rot
        U32(b103, sizeof(mainBone) - 1);                                 // dwGarbage1
        U32(b103, 0u); U32(b103, 0u); U32(b103, 0u);                     // pos, quatPos, matrix
        U32(b103, static_cast<uint32_t>(sizeof(mainBone) - 1) + 0x0103u);// dwGarbage2
        U32(b103, 0u);                                                   // scale

        Counters c103;
        CHECK(Verify(b103, c103, "a_event_pierrot.bin") == Outcome::Pass);
        CHECK(c103.binOk == 1 && c103.binParserRejected == 0 && c103.binUnsupported == 0);
        const auto parsed103 = RANMobile::EP7AnimationReader::ParseContainer(b103, true, false);
        CHECK(parsed103.valid && parsed103.version == 0x0103u);
        CHECK(parsed103.upBodyTracks.size() == 1 && parsed103.mainTracks.size() == 1);
        CHECK(parsed103.upBodyTracks[0].boneName == "Bip01_Spine1");
        CHECK(parsed103.mainTracks[0].boneName == "Bip01");

        for (size_t n = 0; n < b103.size(); ++n) {
            Counters cut;
            CHECK(Verify(std::vector<uint8_t>(b103.begin(), b103.begin() + n), cut) == Outcome::Fail);
            CHECK(cut.binOk == 0 && cut.binUnsupported == 0);
            CHECK(cut.binInvalidHeader + cut.binParserRejected == 1);
        }
    }

    for (uint32_t unsupVer : {0x0104u, 0x0200u}) {
        Counters cu;
        CHECK(Verify(Header("AnimContainer", unsupVer), cu) == Outcome::Unsupported);
        CHECK(cu.binUnsupported == 1 && cu.binOk == 0 && cu.binParserRejected == 0);
    }

    std::printf("Verifier regression: %d checks, %d failures\n", checks, failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
