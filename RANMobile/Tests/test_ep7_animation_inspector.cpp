//
// test_ep7_animation_inspector.cpp — host-only unit & sanitizer regression tests for
// Phase 3J B12 inspect_ep7_animation_containers.cpp.
//
// IMPORTANT PROVENANCE NOTE:
// All byte buffers constructed in this file are STRUCTURAL TEST FIXTURES derived from
// verified C++ serialization rules in enginelib/Meshs/SAnimation.cpp and
// Lib_Engine/Common/ByteCrypt*.h. They are NOT authentic EP7 game assets and are never
// counted as authentic client asset validation.
//

#define main EP7AnimInspectorCliMain
#include "../Tools/inspect_ep7_animation_containers.cpp"
#undef main

#include <cstdlib>

namespace {

int g_checks = 0;
int g_failures = 0;

#define I_CHECK(cond)                                                              \
    do {                                                                           \
        ++g_checks;                                                                \
        if (!(cond)) {                                                             \
            ++g_failures;                                                          \
            std::printf("CHECK FAILED line %d: %s\n", __LINE__, #cond);            \
        }                                                                          \
    } while (0)

struct FixtureWriter {
    std::vector<uint8_t> data;

    void U8(uint8_t v) { data.push_back(v); }
    void U32(uint32_t v) {
        for (int i = 0; i < 4; ++i) data.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFFu));
    }
    void F32(float f) {
        uint32_t u = 0;
        std::memcpy(&u, &f, 4);
        U32(u);
    }
    void StdStr(const char* s) {
        const uint32_t lenWithNul = static_cast<uint32_t>(std::strlen(s) + 1);
        U32(lenWithNul);
        data.insert(data.end(), reinterpret_cast<const uint8_t*>(s),
                    reinterpret_cast<const uint8_t*>(s) + lenWithNul);
    }
    void Header(const char* fileType, uint32_t version, uint8_t padByte = 0x00) {
        const size_t len = std::strlen(fileType);
        std::vector<uint8_t> hdr(128, padByte);
        std::memcpy(hdr.data(), fileType, len + 1);
        data.insert(data.end(), hdr.begin(), hdr.end());
        U32(version);
    }
    void Track0102(const char* boneName) {
        U32(0x0102u);
        StdStr(boneName);
        U32(1);  // rotate keys
        U32(0); F32(0.0f); F32(0.0f); F32(0.0f); F32(1.0f);
        U32(1);  // pos keys
        U32(0); F32(1.0f); F32(2.0f); F32(3.0f);
        U32(static_cast<uint32_t>(std::strlen(boneName)));  // dwGabageData
        U32(0);                                             // quatPos keys
        U32(static_cast<uint32_t>(std::strlen(boneName)) + 0x0102u);
        U32(0);  // matrix keys
        U32(0);  // scale keys
    }
    void Track0101(const char* boneName) {
        U32(0x0101u);
        U32(1);  // pos keys
        U32(0); F32(0.0f); F32(0.0f); F32(0.0f);
        U32(0);  // rot keys
        U32(0);  // scale keys
        U32(0);  // quatPos keys
        U32(0);  // matrix keys
        StdStr(boneName);
    }
    // Structural fixture from enginelib/Meshs/SAnimation.cpp:1418-1462 (SAnimation::Load_0100)
    void Track0100(const char* boneName) {
        U32(0x0100u);
        U32(1);  // pos keys
        U32(0); F32(0.0f); F32(1.0f); F32(0.0f);
        U32(1);  // rot keys
        U32(0); F32(0.0f); F32(0.0f); F32(0.0f); F32(1.0f);
        U32(0);  // scale keys
        U32(0);  // matrix keys
        StdStr(boneName);
    }
};

void TestSha256() {
    I_CHECK(EP7AnimInspect::Sha256::SelfTest());
    // Multi-block NIST vector: 1,000,000 x 'a' -> cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0
    std::vector<uint8_t> millionA(1000000u, static_cast<uint8_t>('a'));
    I_CHECK(EP7AnimInspect::Sha256::DigestHex(millionA) ==
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

void TestSupportedContainersAndTrackRejections() {
    using namespace EP7AnimInspect;

    // 1. Structural fixture: outer 0x0102 with 1 upBody (0x0102) and 1 main (0x0101).
    {
        FixtureWriter w;
        w.Header("AnimContainer", 0x0102u, 0x00);
        w.U32(1);
        w.Track0102("Bip01 Spine");
        w.U32(1);
        w.Track0101("Bip01 Pelvis");
        const BinInspectionRecord r = InspectSingleBin("structural_0102.bin", w.data, false, true);
        I_CHECK(r.bucket == BinBucket::Parsed);
        I_CHECK(r.readerValid);
        I_CHECK(r.zeroPaddedHeader);
        I_CHECK(r.trailingBytes == 0);
        I_CHECK(r.upBodyDeclared == 1 && r.mainDeclared == 1);
        I_CHECK(r.trackVersionsSeen.at(0x0102u) == 1);
        I_CHECK(r.trackVersionsSeen.at(0x0101u) == 1);
    }

    // 2. Structural fixture: outer 0x0100 with SAnimation::Load_0100 track (0x0100).
    // Unmodified EP7AnimationReader rejects track 0x0100 at ReadBoneTrack; shadow walker
    // proves SAnimation::Load_0100 parses cleanly to exact EOF.
    {
        FixtureWriter w;
        w.Header("AnimContainer", 0x0100u, 0xCC);
        w.U32(1);
        w.Track0100("Bip01");
        const BinInspectionRecord r =
            InspectSingleBin("structural_track0100.bin", w.data, false, true);
        I_CHECK(r.bucket == BinBucket::ParserRejected);
        I_CHECK(!r.readerValid);
        I_CHECK(!r.zeroPaddedHeader);
        I_CHECK(r.readerError == "unsupported animation version");
        I_CHECK(r.failList == "main");
        I_CHECK(r.failTrackIndex == 0);
        I_CHECK(r.failOffset == 136u);
        I_CHECK(r.failTrackVersion == 0x0100u);
        I_CHECK(r.load0100ShadowTested);
        I_CHECK(r.load0100ShadowValidToEof);
    }

    // 3. Structural fixture: outer 0x0102 with an unimplemented track version 0x0103 at main[1].
    {
        FixtureWriter w;
        w.Header("AnimContainer", 0x0102u);
        w.U32(0);  // upBody count
        w.U32(2);  // main count
        w.Track0102("Bip01");
        const size_t expectedFailOffset = w.data.size();
        w.U32(0x0103u);
        w.StdStr("Bip01 Head");
        const BinInspectionRecord r =
            InspectSingleBin("structural_track0103.bin", w.data, false, true);
        I_CHECK(r.bucket == BinBucket::ParserRejected);
        I_CHECK(!r.readerValid);
        I_CHECK(r.failList == "main");
        I_CHECK(r.failTrackIndex == 1);
        I_CHECK(r.failOffset == expectedFailOffset);
        I_CHECK(r.failTrackVersion == 0x0103u);
        I_CHECK(!r.load0100ShadowTested);
    }

    // 4. Unsupported outer versions (0x0103, 0x0104, 0x0200) must never be reported as parsed.
    for (uint32_t ver : {0x0103u, 0x0104u, 0x0200u}) {
        FixtureWriter w;
        w.Header("AnimContainer", ver);
        w.U32(0);
        w.U32(0);
        const BinInspectionRecord r = InspectSingleBin("structural_outer.bin", w.data, false, true);
        I_CHECK(r.bucket == BinBucket::UnsupportedOuterVersion);
        I_CHECK(!r.readerValid);
        I_CHECK(r.outerVersion == ver);
    }

    // 5. ByteCrypt probe detection: encode post-header payload with EMBYTECRYPT_BIN.
    {
        FixtureWriter plain;
        plain.Header("AnimContainer", 0x0104u);
        plain.U32(1);
        plain.Track0102("Bip01");
        plain.U32(0);
        const uint8_t* enc = EncodeTableFor(ProbeCipher::ByteCryptBin1);
        I_CHECK(enc != nullptr);
        for (size_t i = 132; i < plain.data.size(); ++i) {
            plain.data[i] = enc[plain.data[i]];
        }
        const BinInspectionRecord r =
            InspectSingleBin("structural_encrypted_bin.bin", plain.data, false, true);
        I_CHECK(r.bucket == BinBucket::UnsupportedOuterVersion);
        I_CHECK(r.matchingCipherName == "EMBYTECRYPT_BIN");
    }

    // 6. Invalid header & foreign fileType.
    {
        std::vector<uint8_t> tiny(40, 0x41);
        I_CHECK(InspectSingleBin("tiny.bin", tiny, false, false).bucket ==
                BinBucket::InvalidHeader);

        FixtureWriter foreign;
        foreign.Header("DxSkinPiece", 0x0102u);
        foreign.U32(0);
        I_CHECK(InspectSingleBin("foreign.bin", foreign.data, false, false).bucket ==
                BinBucket::UnrecognizedType);
    }
}

void TestDirectorySurveyAndZeroMutation() {
    using namespace EP7AnimInspect;
    const fs::path tmpDir =
        fs::temp_directory_path() / ("ep7_b12_inspector_test_" + std::to_string(12345));
    std::error_code ec;
    fs::remove_all(tmpDir, ec);
    fs::create_directories(tmpDir / "data" / "animation");

    FixtureWriter okBin;
    okBin.Header("AnimContainer", 0x0102u);
    okBin.U32(0);
    okBin.U32(1);
    okBin.Track0102("Bip01");

    FixtureWriter rejBin;
    rejBin.Header("AnimContainer", 0x0101u);
    rejBin.U32(1);
    rejBin.Track0100("Bip01");
    rejBin.U32(0);

    FixtureWriter unsupBin;
    unsupBin.Header("AnimContainer", 0x0104u);
    unsupBin.U32(0);
    unsupBin.U32(0);

    auto writeBytes = [](const fs::path& p, const std::vector<uint8_t>& b) {
        std::ofstream f(p, std::ios::binary);
        f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
    };

    const fs::path pOk = tmpDir / "data" / "animation" / "s_ok.bin";
    const fs::path pRej = tmpDir / "data" / "animation" / "a_gbox_stay.bin";
    const fs::path pUnsup = tmpDir / "data" / "animation" / "s_0104.bin";
    writeBytes(pOk, okBin.data);
    writeBytes(pRej, rejBin.data);
    writeBytes(pUnsup, unsupBin.data);

    const std::string shaBefore = Sha256::DigestHex(rejBin.data);
    const SurveySummary sum = RunDirectorySurvey(tmpDir);
    I_CHECK(sum.totalLooseBin == 3);
    I_CHECK(sum.countParsed == 1);
    I_CHECK(sum.countParserRejected == 1);
    I_CHECK(sum.countUnsupportedOuter == 1);
    I_CHECK(sum.rejectedLoad0100Tested == 1);
    I_CHECK(sum.rejectedLoad0100ExactEof == 1);
    I_CHECK(sum.sampleHashesUnchangedAfterRun);

    std::vector<uint8_t> diskAfter;
    I_CHECK(ReadFileBytes(pRej, diskAfter));
    I_CHECK(Sha256::DigestHex(diskAfter) == shaBefore);

    const std::string json = FormatJsonReport(sum);
    I_CHECK(json.find("\"parsed\": 1") != std::string::npos);
    I_CHECK(json.find("\"parser_rejected\": 1") != std::string::npos);
    I_CHECK(json.find("\"unsupported_version\": 1") != std::string::npos);
    I_CHECK(json.find(shaBefore) != std::string::npos);

    fs::remove_all(tmpDir, ec);
}

}  // namespace

int main() {
    TestSha256();
    TestSupportedContainersAndTrackRejections();
    TestDirectorySurveyAndZeroMutation();
    std::printf("Animation inspector regression: %d checks, %d failed\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
