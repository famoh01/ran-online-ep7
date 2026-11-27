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
    CHECK(g_unsupportedReasons.count("bin: unsupported animation container version 0x12345678 (implemented: 0x0100, 0x0101, 0x0102)") == 1);

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
    std::printf("Verifier regression: %d checks, %d failures\n", checks, failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
