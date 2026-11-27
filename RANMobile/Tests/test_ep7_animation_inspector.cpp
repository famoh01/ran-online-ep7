//
// test_ep7_animation_inspector.cpp — Unit & sanitizer test suite for the
// Phase 3J B12 read-only animation container forensic inspector.
//

#define main ep7_anim_inspector_cli_main
#include "../Tools/inspect_ep7_animation_containers.cpp"
#undef main

#include <cstdio>
#include <cstdlib>

namespace {

int g_pass = 0;
int g_fail = 0;

void Check(bool ok, const char* expr, int line) {
    if (ok) {
        ++g_pass;
    } else {
        ++g_fail;
        std::fprintf(stderr, "FAIL line %d: %s\n", line, expr);
    }
}

#define CHECK(expr) Check((expr), #expr, __LINE__)

void AppendU32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(static_cast<uint8_t>(v & 0xFFu));
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
    b.push_back(static_cast<uint8_t>((v >> 16) & 0xFFu));
    b.push_back(static_cast<uint8_t>((v >> 24) & 0xFFu));
}

void AppendF32(std::vector<uint8_t>& b, float f) {
    uint32_t u = 0;
    std::memcpy(&u, &f, sizeof(u));
    AppendU32(b, u);
}

void AppendString(std::vector<uint8_t>& b, const std::string& s) {
    AppendU32(b, static_cast<uint32_t>(s.size()));
    b.insert(b.end(), s.begin(), s.end());
}

std::vector<uint8_t> MakeHeader(const std::string& type, uint32_t ver,
                                uint8_t dirtyPadByte = 0) {
    std::vector<uint8_t> b(128, dirtyPadByte);
    std::memcpy(b.data(), type.data(), std::min<size_t>(type.size(), 127));
    if (type.size() < 128) b[type.size()] = 0;
    AppendU32(b, ver);
    return b;
}

void AppendTrack0100(std::vector<uint8_t>& b, const std::string& bone) {
    AppendU32(b, 0x0100u);
    AppendU32(b, 1u);  // 1 posKey (16 B)
    AppendU32(b, 0u);
    AppendF32(b, 1.0f);
    AppendF32(b, 2.0f);
    AppendF32(b, 3.0f);
    AppendU32(b, 0u);  // 0 rotKeys
    AppendU32(b, 0u);  // 0 scaleKeys
    AppendU32(b, 0u);  // 0 matrixKeys
    AppendString(b, bone);
}

void AppendTrack0102(std::vector<uint8_t>& b, const std::string& bone,
                     uint32_t nQuatPos = 0, uint32_t nMatrix = 0) {
    AppendU32(b, 0x0102u);
    AppendString(b, bone);
    AppendU32(b, 1u);  // 1 rotKey (20 B)
    AppendU32(b, 0u);
    AppendF32(b, 0.0f);
    AppendF32(b, 0.0f);
    AppendF32(b, 0.0f);
    AppendF32(b, 1.0f);
    AppendU32(b, 0u);  // 0 posKeys
    AppendU32(b, 0u);  // garbage1
    AppendU32(b, nQuatPos);
    for (uint32_t i = 0; i < nQuatPos; ++i) {
        // SQuatPosKey = 36 bytes: u32 m_dwTime, f32 m_vScale[3], f32 m_vPos[3], u32 m_dwQuatComp1/2
        AppendU32(b, i * 160u);
        AppendF32(b, 1.0f); AppendF32(b, 1.0f); AppendF32(b, 1.0f);
        AppendF32(b, 0.0f); AppendF32(b, 0.0f); AppendF32(b, 0.0f);
        AppendU32(b, 0u);   AppendU32(b, 0x80000000u);
    }
    AppendU32(b, 0u);  // garbage2
    AppendU32(b, nMatrix);
    for (uint32_t i = 0; i < nMatrix; ++i) {
        // SMatrixKey = 80 bytes: u32 m_dwTime + 12B Win32 align16 padding + 16 floats (64B)
        AppendU32(b, i * 160u);
        AppendU32(b, 0xCCCCCCCCu);
        AppendU32(b, 0xCCCCCCCCu);
        AppendU32(b, 0xCCCCCCCCu);
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                AppendF32(b, (r == c) ? 1.0f : 0.0f);
            }
        }
    }
    AppendU32(b, 0u);  // 0 scaleKeys
}

void AppendTrack0103(std::vector<uint8_t>& b, const std::string& bone) {
    // SAnimation::SaveToFile (0x0103): bone, rotKeys, g1=bone.len, posKeys,
    // quatPosKeys, matrixKeys, g2=bone.len+0x0103, scaleKeys
    AppendU32(b, 0x0103u);
    AppendString(b, bone);
    AppendU32(b, 0u);                                          // rotKeys
    AppendU32(b, static_cast<uint32_t>(bone.size()));          // garbage1
    AppendU32(b, 0u);                                          // posKeys
    AppendU32(b, 1u);                                          // 1 quatPosKey (36B)
    AppendU32(b, 0u);
    AppendF32(b, 1.0f); AppendF32(b, 1.0f); AppendF32(b, 1.0f);
    AppendF32(b, 0.0f); AppendF32(b, 0.0f); AppendF32(b, 0.0f);
    AppendU32(b, 0u);   AppendU32(b, 0x80000000u);
    AppendU32(b, 0u);                                          // matrixKeys
    AppendU32(b, static_cast<uint32_t>(bone.size()) + 0x0103u);// garbage2
    AppendU32(b, 0u);                                          // scaleKeys
}

void TestSha256Vectors() {
    CHECK(EP7AnimInspect::Sha256::SelfTest());
    const std::string msgStr = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    const std::vector<uint8_t> msg(msgStr.begin(), msgStr.end());
    CHECK(EP7AnimInspect::Sha256::DigestHex(msg) ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

void TestJsonEscapeStrictAscii() {
    const std::string raw = std::string("Bip01_\xBF\xDE\x01\"\\");
    const std::string esc = EP7AnimInspect::JsonEscape(raw);
    CHECK(esc == "Bip01_\\u00bf\\u00de\\u0001\\\"\\\\");
    for (unsigned char c : esc) {
        CHECK(c >= 0x20 && c <= 0x7E);
    }
}

void TestParsed0102WithQuatPosExactEof() {
    std::vector<uint8_t> b = MakeHeader("AnimContainer", 0x0102u);
    AppendU32(b, 0u);  // upBody count = 0
    AppendU32(b, 1u);  // main count = 1
    AppendTrack0102(b, std::string("Bip01\0", 6), /*nQuatPos=*/2, /*nMatrix=*/0);

    const auto rec = EP7AnimInspect::InspectSingleBin("data/animation/ok.bin", b, false, true);
    CHECK(rec.inDataAnimationDir);
    CHECK(rec.bucket == EP7AnimInspect::BinBucket::Parsed);
    CHECK(rec.readerValid);
    CHECK(rec.trailingBytes == 0);
    CHECK(rec.sourceSpec.ok && rec.sourceSpec.exactEof);
    CHECK(rec.sourceSpec.tracksQuatPos == 1);
}

void TestMatrixKey80ByteExactEofInB13Reader() {
    // 1 track with 1 true 80-byte SMatrixKey (4B time + 12B align16 pad + 64B mat) and 0 scaleKeys:
    // Both production EP7AnimationReader and VerifyAgainstBackupEngineSourceSpec now consume
    // all 80 bytes per SMatrixKey and hit exact EOF with 0 trailing bytes.
    std::vector<uint8_t> b = MakeHeader("AnimContainer", 0x0102u);
    AppendU32(b, 0u);
    AppendU32(b, 1u);
    AppendTrack0102(b, "Bip01", /*nQuatPos=*/0, /*nMatrix=*/1);

    const auto rec = EP7AnimInspect::InspectSingleBin("data/animation/mat80.bin", b, false, true);
    CHECK(rec.bucket == EP7AnimInspect::BinBucket::Parsed);
    CHECK(rec.readerValid);
    CHECK(rec.trailingBytes == 0u);
    CHECK(rec.sourceSpec.ok && rec.sourceSpec.exactEof);
    CHECK(rec.sourceSpec.trailingBytes == 0u);
    CHECK(rec.sourceSpec.tracksMatrix == 1u);
}

void TestParsed0100WithEmptyBoneAndParserRejectedInner0103() {
    // B13: authentic 0x0100 container + 0x0100 track with empty bone name now parses to exact EOF.
    {
        std::vector<uint8_t> b = MakeHeader("AnimContainer", 0x0100u);
        AppendU32(b, 1u);
        AppendTrack0100(b, "");  // empty bone name valid in SAnimation::Load_0100

        const auto rec = EP7AnimInspect::InspectSingleBin("data/animation/old0100.bin", b, false, true);
        CHECK(rec.bucket == EP7AnimInspect::BinBucket::Parsed);
        CHECK(rec.readerValid);
        CHECK(rec.trailingBytes == 0u);
        CHECK(rec.trackVersionsSeen.count(0x0100u) == 1u);
        CHECK(rec.sourceSpec.ok && rec.sourceSpec.exactEof);
        CHECK(rec.sourceSpec.emptyBoneNames == 1u);
    }

    // Supported outer version (0x0103) carrying an unimplemented 0x0104 inner track is still
    // classified ParserRejected and pinpointed by the inspector trace.
    {
        std::vector<uint8_t> b = MakeHeader("AnimContainer", 0x0103u);
        AppendU32(b, 0u);       // upBody = 0
        AppendU32(b, 0x0104u);  // dwGarbageValue = 0x0103 + 0 + 1
        AppendU32(b, 1u);       // main = 1
        AppendU32(b, 0x0104u);  // unimplemented inner track version

        const auto rec = EP7AnimInspect::InspectSingleBin("data/animation/bad_inner0104.bin", b, false, true);
        CHECK(rec.bucket == EP7AnimInspect::BinBucket::ParserRejected);
        CHECK(!rec.readerValid);
        CHECK(rec.failList == "main");
        CHECK(rec.failTrackIndex == 0u);
        CHECK(rec.failOffset == 144u);
        CHECK(rec.failTrackVersion == 0x0104u);
    }
}

void TestContainer0103And0104And0200SourceSpecExactEof() {
    // B14: 0x0103 (upBody, garbage=(0x0103 + upBody + main), main) now parses to exact EOF
    // in BOTH production EP7AnimationReader and VerifyAgainstBackupEngineSourceSpec!
    {
        std::vector<uint8_t> b = MakeHeader("AnimContainer", 0x0103u);
        AppendU32(b, 1u);  // upBody = 1
        AppendTrack0103(b, "Bip01 Spine1");
        AppendU32(b, 0x0103u + 1u + 2u);  // inter-list garbage formula
        AppendU32(b, 2u);                 // main = 2
        AppendTrack0103(b, "Bip01");
        AppendTrack0103(b, "Bip01 Pelvis");

        const auto rec = EP7AnimInspect::InspectSingleBin("data/animation/v0103.bin", b, false, true);
        CHECK(rec.bucket == EP7AnimInspect::BinBucket::Parsed);
        CHECK(rec.readerValid);
        CHECK(rec.trailingBytes == 0u);
        CHECK(rec.upBodyParsed == 1u && rec.mainParsed == 2u);
        CHECK(rec.trackVersionsSeen.count(0x0103u) == 1u && rec.trackVersionsSeen.at(0x0103u) == 3u);
        CHECK(rec.sourceSpec.ok && rec.sourceSpec.exactEof);
        CHECK(rec.sourceSpec.container0103GarbageMatchesFormula);
    }

    // 0x0104: main, upBody1, upBody2
    {
        std::vector<uint8_t> b = MakeHeader("AnimContainer", 0x0104u);
        AppendU32(b, 1u);  // main
        AppendTrack0103(b, "Bip01");
        AppendU32(b, 1u);  // upBody1
        AppendTrack0103(b, "Bip01 Spine1");
        AppendU32(b, 0u);  // upBody2

        const auto rec = EP7AnimInspect::InspectSingleBin("data/animation/v0104.bin", b, false, true);
        CHECK(rec.bucket == EP7AnimInspect::BinBucket::UnsupportedOuterVersion);
        CHECK(!rec.readerValid);
        CHECK(rec.sourceSpec.ok && rec.sourceSpec.exactEof);
        CHECK(rec.sourceSpec.list0Count == 1u);
        CHECK(rec.sourceSpec.list1Count == 1u);
        CHECK(rec.sourceSpec.list2Count == 0u);
    }

    // 0x0200: EMBYTECRYPT_BIN encoded payload (upBody, main)
    {
        std::vector<uint8_t> plainPayload;
        AppendU32(plainPayload, 0u);  // upBody = 0
        AppendU32(plainPayload, 1u);  // main = 1
        AppendTrack0103(plainPayload, "Bip01");

        const uint8_t* encTable =
            EP7AnimInspect::EncodeTableFor(EP7AnimInspect::ProbeCipher::ByteCryptBin1);
        CHECK(encTable != nullptr);
        std::vector<uint8_t> b = MakeHeader("AnimContainer", 0x0200u);
        for (uint8_t ch : plainPayload) b.push_back(encTable[ch]);

        const auto rec = EP7AnimInspect::InspectSingleBin("data/animation/v0200.bin", b, false, true);
        CHECK(rec.bucket == EP7AnimInspect::BinBucket::UnsupportedOuterVersion);
        CHECK(!rec.readerValid);
        CHECK(rec.matchingCipherName == "EMBYTECRYPT_BIN");
        CHECK(rec.sourceSpec.ok && rec.sourceSpec.exactEof);
    }
}

void TestDirectorySurveySeparatesAnimDirAndPreservesHashes() {
    const fs::path tmp = fs::temp_directory_path() / "ep7_b12_inspector_test_v2";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp / "data" / "animation");
    fs::create_directories(tmp / "data" / "glogic");

    std::vector<uint8_t> okBin = MakeHeader("AnimContainer", 0x0102u);
    AppendU32(okBin, 0u);
    AppendU32(okBin, 0u);

    std::vector<uint8_t> ok0100Bin = MakeHeader("AnimContainer", 0x0100u);
    AppendU32(ok0100Bin, 1u);
    AppendTrack0100(ok0100Bin, "Bip01");

    std::vector<uint8_t> ok0103Bin = MakeHeader("AnimContainer", 0x0103u);
    AppendU32(ok0103Bin, 0u);
    AppendU32(ok0103Bin, 0x0104u);
    AppendU32(ok0103Bin, 1u);
    AppendTrack0103(ok0103Bin, "Bip01");

    std::vector<uint8_t> rejBin = MakeHeader("AnimContainer", 0x0103u);
    AppendU32(rejBin, 0u);
    AppendU32(rejBin, 0x0104u);
    AppendU32(rejBin, 1u);
    AppendU32(rejBin, 0x0104u);  // unimplemented inner track version

    std::vector<uint8_t> unsupBin = MakeHeader("AnimContainer", 0x0104u);
    AppendU32(unsupBin, 0u);
    AppendU32(unsupBin, 0u);
    AppendU32(unsupBin, 0u);

    std::vector<uint8_t> glogicBin = {0x01, 0x02, 0x03, 0x04};  // non-animation truncated .bin

    auto writeBytes = [](const fs::path& p, const std::vector<uint8_t>& d) {
        std::ofstream f(p, std::ios::binary);
        f.write(reinterpret_cast<const char*>(d.data()), static_cast<std::streamsize>(d.size()));
    };
    writeBytes(tmp / "data" / "animation" / "a_ok.bin", okBin);
    writeBytes(tmp / "data" / "animation" / "a_ok0100.bin", ok0100Bin);
    writeBytes(tmp / "data" / "animation" / "a_ok0103.bin", ok0103Bin);
    writeBytes(tmp / "data" / "animation" / "a_rej.bin", rejBin);
    writeBytes(tmp / "data" / "animation" / "a_unsup.bin", unsupBin);
    writeBytes(tmp / "data" / "glogic" / "quest.bin", glogicBin);

    const std::string beforeHash = EP7AnimInspect::Sha256::DigestHex(rejBin);
    const auto sum = EP7AnimInspect::RunDirectorySurvey(tmp, 2);

    CHECK(sum.totalLooseBin == 6);
    CHECK(sum.totalAnimDirBin == 5);
    CHECK(sum.totalNonAnimDirBin == 1);
    CHECK(sum.animDirParsed == 3);
    CHECK(sum.animDirParserRejected == 1);
    CHECK(sum.animDirUnsupportedOuter == 1);
    CHECK(sum.animDirInvalidHeader == 0);
    CHECK(sum.countInvalidHeader == 1);
    CHECK(sum.sampleHashesUnchangedAfterRun);

    std::vector<uint8_t> afterBytes;
    CHECK(EP7AnimInspect::ReadFileBytes(tmp / "data" / "animation" / "a_rej.bin", afterBytes));
    CHECK(EP7AnimInspect::Sha256::DigestHex(afterBytes) == beforeHash);

    const std::string json = EP7AnimInspect::FormatJsonReport(sum);
    CHECK(json.find("\"total_data_animation_bin\": 5") != std::string::npos);
    CHECK(json.find("EXACT_EOF") != std::string::npos);
    for (unsigned char c : json) {
        CHECK(c == '\n' || c == '\r' || c == '\t' || (c >= 0x20 && c <= 0x7E));
    }

    fs::remove_all(tmp, ec);
}

}  // namespace

int main() {
    TestSha256Vectors();
    TestJsonEscapeStrictAscii();
    TestParsed0102WithQuatPosExactEof();
    TestMatrixKey80ByteExactEofInB13Reader();
    TestParsed0100WithEmptyBoneAndParserRejectedInner0103();
    TestContainer0103And0104And0200SourceSpecExactEof();
    TestDirectorySurveySeparatesAnimDirAndPreservesHashes();

    std::printf("test_ep7_animation_inspector: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
