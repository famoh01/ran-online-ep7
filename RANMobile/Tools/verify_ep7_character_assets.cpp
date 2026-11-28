//
// verify_ep7_character_assets — Authentic EP7 character-asset verification probe
//
// PURPOSE
// -------
// Host unit tests in this repository cannot ship the original EP7 client bytes (they are
// multi-gigabyte release archives and are excluded from Git). Everything the Phase 3J
// parsers claim about real files must therefore be verified against the *actual*
// original data. This tool does that: it walks an extracted EP7 client tree, feeds every
// authentic .chf / .cps / .skt / .x / .bin / .cfg file through the parsers, and reports
// exactly what parsed and what did not.
//
// It never invents data: if a file fails, it is reported as failed with the parser's
// error string. Each asset is classified as PASS, FAIL or UNSUPPORTED:
//   PASS        bytes parsed by the real parser
//   FAIL        the parser rejected data it should have accepted (a reader defect, or a
//               corrupt/undecryptable entry)
//   UNSUPPORTED the bytes are real, but the format/version has no definition anywhere in
//               this repository (BLOCKERS.md B11). Reported, counted and warned about --
//               never reported as parsed.
// Exit codes:
//   0 = PASS         all pack entries parsed
//   1 = FAIL         at least one pack entry failed  (the build is wrong)
//   2 = UNSUPPORTED  nothing failed, some formats are unknown (the build is incomplete)
//   3 = NOT FOUND    no character entry found in any pack; nothing was verified
//
// USAGE
//   verify_ep7_character_assets <ep7-data-root> [--max-files N]
//
// The root is an extracted client folder. Character assets are NOT loose files in the EP7
// client: GLOGIC::InitFile (enginelib/G-Logic/GLogic.cpp:40-62) loads them from two
// ZIP-based packs through CUnzipper::UnzipToMemory —
//     SkinObject.rcc   .chf / .cps / skeleton .x / mesh .x
//     Animation.rcc    .bin animation containers / .cfg metadata
// so this tool first walks the directory tree for loose files and then opens every
// *.rcc/*.zip it can find and verifies the entries inside them. That is the only way to
// reach the authentic bytes.
//
#include <algorithm>
#include <cctype>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstdio>
#include <map>
#include <cstring>
#include <string>
#include <vector>

#include "../Data/Importer/EP7Animation.h"
#include "../Data/Importer/EP7RccArchive.h"
#include "../Data/Importer/EP7CharacterFile.h"
#include "../Data/Importer/EP7Skeleton.h"
#include "../Data/Importer/EP7SkinPiece.h"
#include "../Data/Importer/EP7SkinnedMesh.h"
#include "../Data/Importer/EP7TextureDecoder.h"
#include "../Engine/Scene/EP7CharacterScene.h"

namespace {

bool ReadWholeFile(const std::string& path, std::vector<uint8_t>& out, std::string& error) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        error = "cannot open";
        return false;
    }
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    if (size < 0) {
        std::fclose(file);
        error = "cannot size";
        return false;
    }
    std::fseek(file, 0, SEEK_SET);
    out.resize(static_cast<size_t>(size));
    const size_t read = size > 0 ? std::fread(out.data(), 1, out.size(), file) : 0;
    std::fclose(file);
    if (read != out.size()) {
        error = "short read";
        return false;
    }
    return true;
}

bool EndsWithNoCase(const std::string& value, const char* suffix) {
    const size_t len = std::strlen(suffix);
    if (value.size() < len) return false;
    for (size_t i = 0; i < len; ++i) {
        char a = value[value.size() - len + i];
        char b = suffix[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

std::string ExtractExtension(const std::string& path) {
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return std::string();
    return path.substr(dot);
}

// The client root contains many unrelated .bin/.x/.cfg files (cFileList.bin, effect data,
// terrain). A probe that parses every one of them reports failures for files the engine
// never treats as character assets. Character animation data lives under data/animation
// (and the packs), so loose-file scanning is limited to that subtree; pack entries are
// always in scope because the engine only loads character assets from the packs.
bool IsInCharacterDataPath(const std::string& path) {
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower.find("/data/animation/") != std::string::npos ||
           lower.find("/data/character/") != std::string::npos ||
           lower.find("\\data\\animation\\") != std::string::npos ||
           lower.find("\\data\\character\\") != std::string::npos;
}

// Outcome of verifying one asset. UNSUPPORTED is NOT a pass and NOT a failure: it means the
// bytes belong to a format version this build has no definition for (BLOCKERS.md B11). It is
// counted, printed and reported separately so it can never be mistaken for either.
enum class Outcome { Pass, Fail, Unsupported };

struct Counters {
    uint32_t chfTotal = 0, chfOk = 0;
    uint32_t cpsTotal = 0, cpsOk = 0;
    uint32_t xTotal = 0, xSkeletonOk = 0, xMeshOk = 0;
    uint32_t binTotal = 0, binOk = 0, binUnsupported = 0;
    uint32_t binInvalidHeader = 0, binUnrecognizedType = 0, binParserRejected = 0;
    uint32_t cfgTotal = 0, cfgOk = 0, cfgUnsupported = 0;
    uint32_t extractFailures = 0;
    uint32_t packOk = 0, packFailed = 0;   // entries inside .rcc/.zip (engine-authoritative)
    uint32_t looseOk = 0, looseFailed = 0; // loose files under data/animation|character
    uint32_t packUnsupported = 0;          // pack entries of a format with no definition
    uint32_t looseUnsupported = 0;
    uint32_t firstVersionSeenNonZero = 0;
};

// Reasons are aggregated so the summary survives CI's annotation cap (~10 per check run): a
// client with 400 unimplemented animation versions must still say so in one line. Failures
// and unsupported formats are accumulated in separate maps.
std::map<std::string, uint32_t> g_failureReasons;
std::map<std::string, uint32_t> g_unsupportedReasons;

std::string ReasonKey(const std::string& detail) {
    // Keep the leading clause only; trailing numbers/paths vary per file.
    const size_t cut = detail.find(" — ");
    std::string key = (cut == std::string::npos) ? detail : detail.substr(0, cut);
    if (key.size() > 120) key.resize(120);
    return key;
}

// Three-way report. The tag is deliberately unambiguous in a log that is read by humans and
// by grep: [ OK ], [FAIL] and [UNSUP] are distinct tokens.
void ReportOutcome(const char* kind, const std::string& path, Outcome outcome,
                   const std::string& detail) {
    const char* tag = outcome == Outcome::Pass ? " OK " : (outcome == Outcome::Fail ? "FAIL" : "UNSUP");
    std::printf("  [%s] %-10s %s%s%s\n", tag, kind, path.c_str(),
                detail.empty() ? "" : " — ", detail.c_str());
    if (outcome == Outcome::Fail) ++g_failureReasons[std::string(kind) + ": " + ReasonKey(detail)];
    if (outcome == Outcome::Unsupported) ++g_unsupportedReasons[std::string(kind) + ": " + ReasonKey(detail)];
}

// ---------------------------------------------------------------------------
// B18: entry cache + texture evidence.  Additive only — the B11-B17 outcome model
// above is untouched.  Pack entries of character-relevant formats (and shipped
// textures) are cached so the B18 deep-dive can load ONE authentic character and
// resolve its .cps material texture names the way TextureManager does.
// ---------------------------------------------------------------------------
struct B18Evidence {
    std::map<std::string, std::vector<uint8_t>> entries;   // lowercase entry name -> bytes
    size_t cacheBytes = 0;
    bool cacheFull = false;
    uint32_t mtfTotal = 0, mtfDecryptOk = 0, mtfDecodeOk = 0;
    uint32_t ddsTotal = 0, ddsOk = 0;
    std::vector<std::string> chfNames;                     // for the deep-dive pick
    static constexpr size_t kCacheCap = 160u * 1024 * 1024;
};
B18Evidence g_b18;

void B18RememberEntry(const std::string& name, const std::vector<uint8_t>& bytes) {
    // B19 fix: small character-data files (.chf/.cps/.x/.abf/.bin/.cfg) are always
    // cached regardless of the cap — SkinObject.rcc's big textures exhausted the
    // 160 MB budget before the first .chf was seen, leaving the bundle-plan and
    // deep-dive without a character.  The cap now only throttles bulk textures.
    const bool smallMeta = EndsWithNoCase(name, ".chf") || EndsWithNoCase(name, ".cps") ||
                           EndsWithNoCase(name, ".x") || EndsWithNoCase(name, ".abf") ||
                           EndsWithNoCase(name, ".bin") || EndsWithNoCase(name, ".cfg");
    if (!smallMeta) {
        if (g_b18.cacheFull) return;
        if (g_b18.cacheBytes + bytes.size() > B18Evidence::kCacheCap) {
            g_b18.cacheFull = true;
            return;
        }
    }
    std::string lower;
    lower.reserve(name.size());
    for (char c : name) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    const size_t slash = lower.find_last_of('/');
    if (slash != std::string::npos) lower = lower.substr(slash + 1);
    if (g_b18.entries.find(lower) != g_b18.entries.end()) return;
    g_b18.entries[lower] = bytes;
    g_b18.cacheBytes += bytes.size();
}

bool B18Resolve(const std::string& name, std::vector<uint8_t>& out) {
    std::string lower;
    lower.reserve(name.size());
    for (char c : name) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    const auto it = g_b18.entries.find(lower);
    if (it == g_b18.entries.end()) return false;
    out = it->second;
    return true;
}

// Decrypt+decode one texture buffer using the CMemoryTexture naming rules.
bool B18DecodeTexture(const std::string& name, const std::vector<uint8_t>& bytes,
                      uint32_t& outW, uint32_t& outH, std::string& outFmt) {
    std::vector<uint8_t> raw;
    int fileType = -1;
    if (EndsWithNoCase(name, ".mtf")) {
        if (!RANMobile::EP7TextureDecoder::DecryptMTF(bytes, raw, fileType)) return false;
    } else {
        raw = bytes;
    }
    RANMobile::DecodedTexture tex;
    if (raw.size() >= 4 && std::memcmp(raw.data(), "DDS ", 4) == 0) {
        outFmt = "dds";
        tex = RANMobile::EP7TextureDecoder::DecodeDDS(raw);
    } else if (fileType == 1) {
        outFmt = "tga(mtf)";
        tex = RANMobile::EP7TextureDecoder::DecodeTGA(raw);
    } else {
        outFmt = "tga";
        tex = RANMobile::EP7TextureDecoder::DecodeTGA(raw);
        if (!tex.valid) {
            outFmt = "dds?";
            tex = RANMobile::EP7TextureDecoder::DecodeDDS(raw);
        }
    }
    if (!tex.valid) return false;
    outW = tex.width;
    outH = tex.height;
    return true;
}

// B20: unsupported-version census (see VerifySingleAsset .chf branch).
struct ChfVersionExample {
    std::string name;
    size_t size = 0;
    std::string hexHead;  // payload head from file offset 132, hex
};
std::map<uint32_t, int> g_chfUnsupportedVersions;
std::map<uint32_t, std::vector<ChfVersionExample>> g_chfUnsupportedExamples;

}  // namespace

// Verifies one asset buffer. `path` is only used for reporting (it may be
// "<archive>:/entry/name" for packed entries).
Outcome VerifySingleAsset(const std::string& path, const std::vector<uint8_t>& bytes, Counters& counters) {
    const std::string ext = ExtractExtension(path);

    if (EndsWithNoCase(ext, ".chf")) {
        ++counters.chfTotal;
        const RANMobile::EP7CharacterFileInfo info = RANMobile::EP7CharacterFileReader::Parse(bytes, false);
        if (info.valid) {
            ++counters.chfOk;
            char detail[256];
            std::snprintf(detail, sizeof(detail), "v0x%04X pieces=%u skeleton=%s anims=%zu", info.version,
                          info.PieceCount(), info.skeleton.c_str(), info.animations.size());
            ReportOutcome("chf", path, Outcome::Pass, detail);
            g_b18.chfNames.push_back(path);
        } else {
            ReportOutcome("chf", path, Outcome::Fail, info.error);
            // B20: census of the versions the pack actually ships. `info.version`
            // is recorded by the parser BEFORE the unsupported-version gate, so
            // this is the real on-disk value. For the first example of each
            // distinct version, keep name + size + a hex dump of the payload
            // head (version DWORD + the first 72 payload bytes at file offset
            // 132) -- the layout evidence needed to extend the parser without
            // guessing. Bytes are raw pack-entry bytes (this pack stores entries
            // unencoded; every working format classification proves that).
            if (info.error.find("unsupported .chf version") != std::string::npos) {
                ++g_chfUnsupportedVersions[info.version];
                auto& exs = g_chfUnsupportedExamples[info.version];
                bool distinct = true;
                for (const auto& e : exs) {
                    if (e.size == bytes.size()) distinct = false;
                }
                if (exs.size() < 3 && distinct) {
                    ChfVersionExample ex;
                    ex.name = path;
                    ex.size = bytes.size();
                    // Payload head from file offset 132: version DWORD + the first
                    // 188 payload bytes (scale / counts / piece slots / strings in
                    // the 0x0100-0x0102 layouts) -- enough to align an unknown
                    // version's fields against the known templates.
                    const size_t payload = 132;
                    const size_t take = std::min<size_t>(188, bytes.size() > payload ? bytes.size() - payload : 0);
                    static const char* hexd = "0123456789abcdef";
                    std::string hx;
                    hx.reserve(take * 2);
                    for (size_t i = 0; i < take; ++i) {
                        hx.push_back(hexd[bytes[payload + i] >> 4]);
                        hx.push_back(hexd[bytes[payload + i] & 0xF]);
                    }
                    ex.hexHead = hx;
                    exs.push_back(ex);
                }
            }
        }
        return info.valid ? Outcome::Pass : Outcome::Fail;
    }

    if (EndsWithNoCase(ext, ".cps")) {
        ++counters.cpsTotal;
        const RANMobile::EP7SkinPieceInfo info = RANMobile::EP7SkinPieceReader::ParseFromBytes(bytes, false);
        if (info.valid) {
            ++counters.cpsOk;
            char detail[320];
            std::snprintf(detail, sizeof(detail), "v0x%04X type=%d mesh=%s skeleton=%s materials=%zu traces=%zu",
                          info.fileVersion, static_cast<int>(info.pieceType), info.meshName.c_str(),
                          info.skeletonFile.c_str(), info.materials.size(), info.traces.size());
            ReportOutcome("cps", path, Outcome::Pass, detail);
        } else {
            ReportOutcome("cps", path, Outcome::Fail, info.error);
        }
        return info.valid ? Outcome::Pass : Outcome::Fail;
    }

    if (EndsWithNoCase(ext, ".skt") || EndsWithNoCase(ext, ".abf")) {
        RANMobile::EP7Skeleton skeleton;
        ++counters.xTotal;
        const bool ok = skeleton.LoadFromXFile(bytes);
        if (ok) {
            ++counters.xSkeletonOk;
            char detail[128];
            std::snprintf(detail, sizeof(detail), "bones=%zu", skeleton.BoneCount());
            ReportOutcome("skeleton", path, Outcome::Pass, detail);
        } else {
            ReportOutcome("skeleton", path, Outcome::Fail, skeleton.Error());
        }
        return ok ? Outcome::Pass : Outcome::Fail;
    }

    if (EndsWithNoCase(ext, ".x")) {
        // The client stores skeletons and skinned meshes both as .x. A skinned mesh is
        // wrapped in Frame objects too, so "does a skeleton parse?" does NOT identify a
        // skeleton — the presence of a SkinWeights object does. Order matters: check the
        // mesh first, then fall back to the skeleton.
        ++counters.xTotal;
        const RANMobile::EP7SkinnedMesh mesh =
            RANMobile::EP7SkinnedMeshReader::Parse(bytes, std::string(), nullptr);
        if (mesh.valid) {
            ++counters.xMeshOk;
            char detail[256];
            std::snprintf(detail, sizeof(detail), "mesh=%s verts=%u tris=%zu bones=%zu stride=%u decoded=%s",
                          mesh.name.c_str(), mesh.vertexCount, mesh.faces.size() / 3, mesh.bones.size(),
                          mesh.vertexStride, mesh.geometryDecoded ? "yes" : "no");
            ReportOutcome("x-mesh", path, Outcome::Pass, detail);
            return Outcome::Pass;
        }
        RANMobile::EP7Skeleton skeleton;
        if (skeleton.LoadFromXFile(bytes)) {
            ++counters.xSkeletonOk;
            char detail[128];
            std::snprintf(detail, sizeof(detail), "skeleton bones=%zu", skeleton.BoneCount());
            ReportOutcome("x-skeleton", path, Outcome::Pass, detail);
            return Outcome::Pass;
        }
        ReportOutcome("x", path, Outcome::Fail, mesh.error + " | " + skeleton.Error());
        return Outcome::Fail;
    }

    if (EndsWithNoCase(ext, ".bin")) {
        ++counters.binTotal;
        // SAnimation.cpp:22,1573-1617: SetFileType("AnimContainer", VERSION).
        // SerialFile.cpp:57-65: 128 type bytes, then the LE version DWORD.
        // Printable ASCII is expected here, not evidence of an unsupported format.
        // Validate identity here: ParseContainer reads but does not validate the type.
        // Bytes after the NUL in the 128-byte field need not be zero (StringCchCopy).
        constexpr char type[] = "AnimContainer";
        std::string fingerprint;
        for (size_t i = 0; i < std::min<size_t>(bytes.size(), 16); ++i) {
            char hex[4];
            std::snprintf(hex, sizeof(hex), "%02X", bytes[i]);
            fingerprint += hex;
        }
        const std::string diagnostic = " — prefix16=" + fingerprint +
                                       " size=" + std::to_string(bytes.size());
        if (bytes.size() < RANMobile::ep7byte::Stream::kHeaderSize) {
            ++counters.binInvalidHeader;
            ReportOutcome("bin", path, Outcome::Fail, "invalid/truncated container header" + diagnostic);
            return Outcome::Fail;
        }
        if (std::memcmp(bytes.data(), type, sizeof(type)) != 0) {
            ++counters.binUnrecognizedType;
            // Could be foreign data, corruption, or an undocumented encoding. The bytes
            // alone do not prove which; never invent an unsupported container version.
            ReportOutcome("bin", path, Outcome::Fail, "unrecognized container type (foreign or damaged)" + diagnostic);
            return Outcome::Fail;
        }
        // No headerless retry: it can mask a recognized header's real version/error.
        const RANMobile::EP7AnimClip clip =
            RANMobile::EP7AnimationReader::ParseContainer(bytes, true, false);
        if (clip.valid) {
            ++counters.binOk;
            char detail[256];
            std::snprintf(detail, sizeof(detail), "v0x%04X mainTracks=%zu upBodyTracks=%zu maxKeyTime=%u",
                          clip.version, clip.mainTracks.size(), clip.upBodyTracks.size(), clip.MaxKeyTime());
            ReportOutcome("bin", path, Outcome::Pass, detail);
            return Outcome::Pass;
        }
        // Use the parser's explicit version rejection, not a guessed byte signature.
        if (clip.error.rfind("unsupported animation container version ", 0) == 0) {
            ++counters.binUnsupported;
            ReportOutcome("bin", path, Outcome::Unsupported, clip.error + diagnostic);
            return Outcome::Unsupported;
        }
        ++counters.binParserRejected;
        // A rejected payload may be malformed OR expose a reader defect. Do not claim
        // to distinguish those without further evidence. Preserve the actual error.
        ReportOutcome("bin", path, Outcome::Fail, "recognized container parser rejection: " + clip.error + diagnostic);
        return Outcome::Fail;
    }

    if (EndsWithNoCase(ext, ".cfg")) {
        ++counters.cfgTotal;
        const RANMobile::EP7AnimInfo info = RANMobile::EP7AnimationReader::ParseInfo(bytes, true, false);
        if (info.valid) {
            ++counters.cfgOk;
            char detail[320];
            std::snprintf(detail, sizeof(detail), "name=%s skeleton=%s main=%d sub=%d window=%u..%u loop=%s",
                          info.name.c_str(), info.skeleton.c_str(), info.mainType, info.subType,
                          info.startTime, info.endTime, info.IsLoop() ? "yes" : "no");
            ReportOutcome("cfg", path, Outcome::Pass, detail);
        } else if (info.unsupported) {
            ++counters.cfgUnsupported;
            ReportOutcome("cfg", path, Outcome::Unsupported, info.error);
        } else {
            ReportOutcome("cfg", path, Outcome::Fail, info.error);
        }
        return info.valid ? Outcome::Pass : (info.unsupported ? Outcome::Unsupported : Outcome::Fail);
    }

    return Outcome::Fail;  // recognised character extension we have no reader for
}

bool IsCharacterAssetName(const std::string& name) {
    const std::string ext = ExtractExtension(name);
    return EndsWithNoCase(ext, ".chf") || EndsWithNoCase(ext, ".cps") || EndsWithNoCase(ext, ".x") ||
           EndsWithNoCase(ext, ".bin") || EndsWithNoCase(ext, ".cfg") || EndsWithNoCase(ext, ".skt") ||
           EndsWithNoCase(ext, ".abf");
}

bool IsArchiveName(const std::string& name) {
    return EndsWithNoCase(name, ".rcc") || EndsWithNoCase(name, ".zip");
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr,
                     "usage: %s <ep7-data-root> [--max-files N]\n"
                     "Verifies authentic EP7 character assets against the Phase 3J parsers.\n",
                     argv[0]);
        return 2;
    }

    const std::string root = argv[1];
    size_t maxFiles = 20000;
    std::string bundleDir;
    std::string bundleChf;
    bool bundleQuick = false;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--max-files") == 0 && i + 1 < argc) {
            maxFiles = static_cast<size_t>(std::strtoul(argv[++i], nullptr, 10));
        } else if (std::strcmp(argv[i], "--bundle-plan") == 0 && i + 2 < argc) {
            bundleDir = argv[++i];
            bundleChf = argv[++i];
            bundleQuick = true;
        }
    }

    // Collect every file under the root ("find" is available on the CI runner).
    std::string listCommand = "find '" + root + "' -type f 2>/dev/null";
    std::FILE* pipe = popen(listCommand.c_str(), "r");
    if (!pipe) {
        std::fprintf(stderr, "cannot list %s\n", root.c_str());
        return 2;
    }

    std::vector<std::string> looseFiles;
    std::vector<std::string> archives;
    size_t skippedUnrelated = 0;
    char line[4096];
    while (std::fgets(line, sizeof(line), pipe)) {
        std::string path(line);
        while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) path.pop_back();
        if (path.empty()) continue;
        if (IsArchiveName(path)) {
            archives.push_back(path);
        } else if (IsCharacterAssetName(path) && IsInCharacterDataPath(path)) {
            looseFiles.push_back(path);
        } else if (IsCharacterAssetName(path)) {
            ++skippedUnrelated;
        }
        if (looseFiles.size() + archives.size() >= maxFiles) break;
    }
    pclose(pipe);

    std::sort(looseFiles.begin(), looseFiles.end());
    std::sort(archives.begin(), archives.end());

    std::printf("=== Phase 3J authentic character asset verification ===\n");
    std::printf("root: %s\n", root.c_str());
    // Two independent populations, reported separately: pack entries are what the engine
    // actually loads, loose files are only informative.
    std::printf("loose candidate files (data/animation, data/character): %zu\n", looseFiles.size());
    std::printf("  ignored .bin/.x/.cfg elsewhere in the tree (not character data): %zu\n", skippedUnrelated);
    std::printf("archives (.rcc/.zip): %zu\n\n", archives.size());

    Counters counters;
    std::vector<uint8_t> bytes;
    std::string error;

    for (const auto& path : looseFiles) {
        if (!ReadWholeFile(path, bytes, error)) {
            ReportOutcome("read", path, Outcome::Fail, error);
            continue;
        }
        switch (VerifySingleAsset(path, bytes, counters)) {
            case Outcome::Pass: ++counters.looseOk; break;
            case Outcome::Fail: ++counters.looseFailed; break;
            case Outcome::Unsupported: ++counters.looseUnsupported; break;
        }
    }

    const Counters looseSnapshot = counters;

    // The authentic assets live inside these packs (GLOGIC::InitFile).
    for (const auto& archivePath : archives) {
        std::printf("\n--- archive %s\n", archivePath.c_str());
        if (!ReadWholeFile(archivePath, bytes, error)) {
            ReportOutcome("read", archivePath, Outcome::Fail, error);
            continue;
        }
        RANMobile::EP7RccArchive archive;
        if (!archive.Open(bytes)) {
            ReportOutcome("archive", archivePath, Outcome::Fail, archive.Error());
            continue;
        }
        std::printf("     %zu entries\n", archive.EntryCount());

        size_t checked = 0;
        for (const auto& entry : archive.Entries()) {
            // B19 FIX: shipped textures (.mtf/.dds) must be extracted and cached too —
            // they sit OUTSIDE the character-asset name filter.  The B18 hook lived
            // after the filter, so texture evidence and the texture cache stayed empty.
            const bool isTexture = EndsWithNoCase(entry.name, ".mtf") || EndsWithNoCase(entry.name, ".dds");
            if (!isTexture && !IsCharacterAssetName(entry.name)) continue;
            if (bundleQuick) {
                // Bundle mode walks metadata only: extract what the bundle can use
                // (textures + chf, which drive the selection) and skip full
                // classification — the diagnostic job runs it separately.
                if (!isTexture && !EndsWithNoCase(entry.name, ".chf")) continue;
                std::vector<uint8_t> bytes;
                if (!archive.Extract(entry, bytes)) continue;
                if (isTexture) {
                    ++g_b18.mtfTotal;
                    std::vector<uint8_t> raw;
                    int fileType = -1;
                    if (RANMobile::EP7TextureDecoder::DecryptMTF(bytes, raw, fileType)) {
                        ++g_b18.mtfDecryptOk;
                        uint32_t tw = 0;
                        uint32_t th = 0;
                        std::string fmt;
                        if (B18DecodeTexture(entry.name, bytes, tw, th, fmt)) ++g_b18.mtfDecodeOk;
                    }
                }
                B18RememberEntry(entry.name, bytes);
                continue;
            }
            if (checked >= maxFiles) break;
            ++checked;

            const std::string label = archivePath + ":" + entry.name;
            std::vector<uint8_t> entryBytes;
            if (!archive.Extract(entry, entryBytes)) {
                if (isTexture) {
                    // Textures are outside the outcome model: an unextractable texture
                    // is logged and skipped, never counted as a pack failure.
                    char detail[256];
                    std::snprintf(detail, sizeof(detail),
                                  "could not extract texture (method=%u flags=0x%04X csize=%u usize=%u)",
                                  entry.method, entry.flags, entry.compressedSize, entry.uncompressedSize);
                    ReportOutcome("extract", label, Outcome::Fail, detail);
                    continue;
                }
                // Report the method/flag/size triple: if the shipped packs ever use zip64,
                // AES or a method other than stored/deflate, this line says so instead of
                // leaving a mystery.
                char detail[256];
                std::snprintf(detail, sizeof(detail),
                              "could not extract (method=%u flags=0x%04X csize=%u usize=%u) — corrupt, "
                              "another password, or an unsupported method",
                              entry.method, entry.flags, entry.compressedSize, entry.uncompressedSize);
                ReportOutcome("extract", label, Outcome::Fail, detail);
                // An entry we could not even decompress is an UNVERIFIED character asset. It
                // must count against the pass rate; otherwise "2 entries, 1 unreadable" would
                // still print "parsed 1 of 1" and exit 0.
                ++counters.extractFailures;
                ++counters.packFailed;
                continue;
            }
            // B18: shipped textures (.mtf/.dds) are classified additively — they are
            // not part of the B11-B17 character-entry outcome model, but the evidence
            // (decrypt/decode rates) and the bytes are kept for the deep-dive below.
            if (isTexture) {
                if (EndsWithNoCase(entry.name, ".mtf")) {
                    ++g_b18.mtfTotal;
                    std::vector<uint8_t> raw;
                    int fileType = -1;
                    if (RANMobile::EP7TextureDecoder::DecryptMTF(entryBytes, raw, fileType)) {
                        ++g_b18.mtfDecryptOk;
                        uint32_t tw = 0;
                        uint32_t th = 0;
                        std::string fmt;
                        if (B18DecodeTexture(entry.name, entryBytes, tw, th, fmt)) ++g_b18.mtfDecodeOk;
                    }
                } else {
                    ++g_b18.ddsTotal;
                    uint32_t tw = 0;
                    uint32_t th = 0;
                    std::string fmt;
                    if (B18DecodeTexture(entry.name, entryBytes, tw, th, fmt)) ++g_b18.ddsOk;
                }
                B18RememberEntry(entry.name, entryBytes);
                continue;
            }
            B18RememberEntry(entry.name, entryBytes);
            switch (VerifySingleAsset(label, entryBytes, counters)) {
                case Outcome::Pass: ++counters.packOk; break;
                case Outcome::Fail: ++counters.packFailed; break;
                case Outcome::Unsupported: ++counters.packUnsupported; break;
            }
        }
        if (checked == 0) std::printf("     (no character assets inside)\n");
    }

    // ---------------------------------------------------------------------------
    // B19 bundle-plan: write ONE authentic character's dependency set (chf + cps +
    // x + textures, exactly what the loader/resolver requested) to a directory for
    // APK bundling.  Runs only when --bundle-plan is given; exit code is unchanged
    // (a bundle failure is printed, not turned into a verdict change).
    // ---------------------------------------------------------------------------
    if (!bundleDir.empty()) {
        // Create the output directory (best effort — CI pre-creates it too).
#ifdef _WIN32
        std::system(("mkdir " + bundleDir).c_str());
#else
        mkdir(bundleDir.c_str(), 0777);
#endif
        std::string chfName;
        for (const auto& kv : g_b18.entries) {
            if (!EndsWithNoCase(kv.first, ".chf")) continue;
            if (kv.first.find(bundleChf) == std::string::npos) continue;
            chfName = kv.first;
            break;
        }
        if (chfName.empty()) {
            std::printf("\nbundle-plan: no .chf matching '%s' found in packs (cached entries: "
                        "%zu, cacheBytes: %zu, cacheFull: %d)\n",
                        bundleChf.c_str(), g_b18.entries.size(), g_b18.cacheBytes,
                        g_b18.cacheFull ? 1 : 0);
        } else {
            std::map<std::string, std::vector<uint8_t>> written;
            std::vector<std::string> requestedOrder;
            std::pair<std::map<std::string, std::vector<uint8_t>>*, std::vector<std::string>*>
                bundleCtx{&written, &requestedOrder};
            RANMobile::EP7CharacterLoader loader;
            loader.SetResolver([](void* ud, const std::string& name, std::vector<uint8_t>& out) {
                auto* ctx = static_cast<std::pair<std::map<std::string, std::vector<uint8_t>>*,
                                                  std::vector<std::string>*>*>(ud);
                if (B18Resolve(name, out)) {
                    (*ctx->first)[name] = out;
                    ctx->second->push_back(name);
                    return true;
                }
                ctx->second->push_back(name);
                return false;
            }, &bundleCtx);
            std::vector<uint8_t> chfBytes;
            if (B18Resolve(chfName, chfBytes)) {
                written[chfName] = chfBytes;
                requestedOrder.push_back(chfName);
                const RANMobile::EP7CharacterScene::AssetPtr asset =
                    std::make_shared<RANMobile::EP7CharacterAsset>(
                        loader.LoadCharacter(chfBytes, false));
                std::printf("bundle-plan: chf '%s' loaded=%d pieces=%u\n", chfName.c_str(),
                            asset->loaded ? 1 : 0, asset->pieceCount);
                // Texture names via the production ladder (cps materials + .x subsets).
                // B19 FIX: pieces is EMPTY when the .chf did not load (e.g. the
                // documented unsupported-version gap) — indexing it without the
                // loaded check was an out-of-bounds read (segfault).
                for (uint32_t slot = 0;
                     asset->loaded && slot < RANMobile::kEP7PieceSize; ++slot) {
                    const RANMobile::EP7CharacterPiece& piece = asset->pieces[slot];
                    if (!piece.loaded) continue;
                    std::map<std::string, int> names;
                    for (const auto& mat : piece.info.materials) {
                        if (!mat.texture.empty()) names[mat.texture] = 1;
                    }
                    for (const auto& subset : piece.mesh.subsets) {
                        if (!subset.texture.empty()) names[subset.texture] = 2;
                    }
                    for (const auto& kv : names) {
                        std::vector<uint8_t> bytes;
                        std::string hit;
                        const std::string& n = kv.first;
                        if (B18Resolve(n, bytes)) hit = n;
                        const size_t dot = n.find_last_of('.');
                        if (hit.empty() && dot != std::string::npos &&
                            B18Resolve(n.substr(0, dot) + ".mtf", bytes)) {
                            hit = n.substr(0, dot) + ".mtf";
                        }
                        if (hit.empty() && B18Resolve(n + ".mtf", bytes)) hit = n + ".mtf";
                        if (!hit.empty() && written.find(hit) == written.end()) {
                            written[hit] = bytes;
                            requestedOrder.push_back(hit);
                        }
                    }
                }
                // B19: the SkinObject.rcc .chf population is of parser-unsupported
                // versions (documented B11 gap), so a loadable character normally does
                // not exist and the loader records no material texture names.  Bundle
                // the three smallest shipped textures anyway so the emulator harness
                // can render ORIGINAL client texture bytes through the GPU pipeline.
                {
                    std::vector<std::pair<size_t, std::string>> texSizes;
                    for (const auto& kv : g_b18.entries) {
                        if (!EndsWithNoCase(kv.first, ".mtf") && !EndsWithNoCase(kv.first, ".dds")) continue;
                        texSizes.emplace_back(kv.second.size(), kv.first);
                    }
                    std::sort(texSizes.begin(), texSizes.end());
                    for (size_t k = 0; k < texSizes.size() && k < 3; ++k) {
                        if (written.find(texSizes[k].second) != written.end()) continue;
                        std::vector<uint8_t> bytes;
                        if (B18Resolve(texSizes[k].second, bytes)) {
                            written[texSizes[k].second] = bytes;
                            requestedOrder.push_back(texSizes[k].second);
                        }
                    }
                }
                std::sort(requestedOrder.begin(), requestedOrder.end());
                requestedOrder.erase(std::unique(requestedOrder.begin(), requestedOrder.end()),
                                     requestedOrder.end());
                size_t totalBytes = 0;
                bool allWritten = true;
                for (const auto& name : requestedOrder) {
                    const auto it = written.find(name);
                    if (it == written.end()) continue;
                    const std::string path = bundleDir + "/" + name;
                    std::FILE* f = std::fopen(path.c_str(), "wb");
                    if (!f) {
                        allWritten = false;
                        continue;
                    }
                    std::fwrite(it->second.data(), 1, it->second.size(), f);
                    std::fclose(f);
                    totalBytes += it->second.size();
                }
                std::FILE* manifest = std::fopen((bundleDir + "/manifest.txt").c_str(), "wb");
                if (manifest) {
                    for (const auto& name : requestedOrder) std::fprintf(manifest, "%s\n", name.c_str());
                    std::fclose(manifest);
                }
                std::printf("bundle-plan: %zu files, %zu bytes -> %s (allWritten=%d)\n",
                            requestedOrder.size(), totalBytes, bundleDir.c_str(),
                            allWritten ? 1 : 0);
            }
        }
    }

    // ---------------------------------------------------------------------------
    // B18 evidence: load the FIRST valid .chf through the full character loader and
    // report the GPU skinning/texture pipeline inputs built from that authentic data.
    // Additive, informational: the B11-B17 outcome model above is untouched, but the
    // numbers come only from authentic pack entries (no synthetic fixtures involved).
    // ---------------------------------------------------------------------------
    {
        std::string chfName;
        for (const auto& kv : g_b18.entries) {
            if (EndsWithNoCase(kv.first, ".chf")) {
                chfName = kv.first;
                break;
            }
        }
        std::printf("\n=== B18 GPU skinning / texture evidence ===\n");
        std::printf("pack texture entries: %u .mtf (%u decrypt ok, %u decoded), %u .dds (%u decoded)\n",
                    g_b18.mtfTotal, g_b18.mtfDecryptOk, g_b18.mtfDecodeOk, g_b18.ddsTotal, g_b18.ddsOk);
        if (chfName.empty()) {
            std::printf("no valid .chf entry cached; character deep-dive skipped\n");
        } else {
            RANMobile::EP7CharacterLoader loader;
            loader.SetResolver([](void*, const std::string& name, std::vector<uint8_t>& out) {
                return B18Resolve(name, out);
            }, nullptr);
            std::vector<uint8_t> chfBytes;
            if (!B18Resolve(chfName, chfBytes)) {
                std::printf("deep-dive: chf bytes unavailable\n");
            } else {
                const RANMobile::EP7CharacterScene::AssetPtr asset =
                    std::make_shared<RANMobile::EP7CharacterAsset>(
                        loader.LoadCharacter(chfBytes, false));
                std::printf("deep-dive character: %s loaded=%d pieces=%u anims=%zu skeleton=%s\n",
                            chfName.c_str(), asset->loaded ? 1 : 0, asset->pieceCount,
                            asset->clips.size(), asset->file.skeleton.c_str());
                if (asset->loaded) {
                    RANMobile::EP7Skeleton skel = *asset->skeleton;
                    for (uint32_t slot = 0; slot < RANMobile::kEP7PieceSize; ++slot) {
                        const RANMobile::EP7CharacterPiece& piece = asset->pieces[slot];
                        if (!piece.loaded) continue;
                        // GPU skinning streams (DxSkinMesh9_HLSL BLENDWEIGHT/BLENDINDICES parity).
                        uint32_t influenced = 0;
                        uint32_t maxInfl = 0;
                        if (piece.gpuStreams.valid) {
                            for (uint32_t v = 0; v < piece.gpuStreams.vertexCount; ++v) {
                                uint32_t count = 0;
                                for (int i = 0; i < 4; ++i) {
                                    if (piece.gpuStreams.boneWeights[static_cast<size_t>(v) * 4 + i] > 0.0f) ++count;
                                }
                                if (count > 0) ++influenced;
                                if (count > maxInfl) maxInfl = count;
                            }
                        }
                        std::printf("piece[%u] %s: verts=%u tris=%zu skinBones=%zu gpuStreams=%s "
                                    "influenced=%u/%u maxInfl=%u palette=%zu\n",
                                    slot, piece.info.xFileName.c_str(), piece.mesh.vertexCount,
                                    piece.mesh.faces.size() / 3, piece.mesh.bones.size(),
                                    piece.gpuStreams.valid ? "valid" : "INVALID",
                                    influenced, piece.mesh.vertexCount, maxInfl,
                                    piece.mesh.bones.size());
                        // Palette composition at rest (offset * combined, DxSkinMesh9_CPU.cpp:625 parity).
                        std::vector<RANMobile::ep7math::Mat4> palette;
                        RANMobile::BuildBonePalette(piece.mesh, skel, palette);
                        if (palette.size() != piece.mesh.bones.size()) {
                            std::printf("  [B18-FAIL] palette size %zu != skin bone count %zu\n",
                                        palette.size(), piece.mesh.bones.size());
                        }
                        // Texture resolution: .cps material names first (SMATERIAL_PIECE::LoadTexture
                        // parity), then the .x MeshMaterialList names, through the ladder.
                        std::map<std::string, int> names;
                        for (const auto& mat : piece.info.materials) {
                            if (!mat.texture.empty()) names[mat.texture] = 1;
                        }
                        for (const auto& subset : piece.mesh.subsets) {
                            if (!subset.texture.empty() && names.find(subset.texture) == names.end()) {
                                names[subset.texture] = 2;
                            }
                        }
                        for (const auto& kv : names) {
                            std::vector<uint8_t> texBytes;
                            std::string hit;
                            const std::string& n = kv.first;
                            if (B18Resolve(n, texBytes)) hit = n;
                            const size_t dot = n.find_last_of('.');
                            if (hit.empty() && dot != std::string::npos &&
                                B18Resolve(n.substr(0, dot) + ".mtf", texBytes)) {
                                hit = n.substr(0, dot) + ".mtf";
                            }
                            if (hit.empty() && B18Resolve(n + ".mtf", texBytes)) hit = n + ".mtf";
                            if (hit.empty()) {
                                std::printf("  texture '%s' (cps=%d): NOT RESOLVED from packs\n",
                                            kv.first.c_str(), kv.second);
                            } else {
                                uint32_t tw = 0;
                                uint32_t th = 0;
                                std::string fmt;
                                if (B18DecodeTexture(hit, texBytes, tw, th, fmt)) {
                                    std::printf("  texture '%s' (cps=%d) -> '%s' [%s] %ux%u\n",
                                                kv.first.c_str(), kv.second, hit.c_str(),
                                                fmt.c_str(), tw, th);
                                } else {
                                    std::printf("  texture '%s' (cps=%d) -> '%s' DECODE FAILED\n",
                                                kv.first.c_str(), kv.second, hit.c_str());
                                }
                            }
                        }
                        if (names.empty()) {
                            std::printf("  (no material texture names recorded)\n");
                        }
                    }
                }
            }
        }
        std::printf("(B18 evidence is informational; the verdict below is the B11-B16 outcome model)\n");
    }

    std::printf("\n=== SUMMARY (verified against authentic files only) ===\n");
    std::printf("chf  : %u ok / %u\n", counters.chfOk, counters.chfTotal);
    std::printf("cps  : %u ok / %u\n", counters.cpsOk, counters.cpsTotal);
    std::printf("x    : %u skeleton / %u mesh / %u parsed of %u\n", counters.xSkeletonOk, counters.xMeshOk,
                counters.xSkeletonOk + counters.xMeshOk, counters.xTotal);
    std::printf("bin  : %u ok / %u (%u unsupported format)\n", counters.binOk, counters.binTotal,
                counters.binUnsupported);
    auto printBins = [](const char* scope, const Counters& end, const Counters& start) {
        std::printf("bin classifications (%s): %u parsed, %u unsupported-version, %u invalid-header, "
                    "%u unrecognized-type, %u parser-rejected; total=%u\n", scope,
                    end.binOk - start.binOk, end.binUnsupported - start.binUnsupported,
                    end.binInvalidHeader - start.binInvalidHeader,
                    end.binUnrecognizedType - start.binUnrecognizedType,
                    end.binParserRejected - start.binParserRejected, end.binTotal - start.binTotal);
    };
    printBins("all", counters, Counters{});
    printBins("loose", looseSnapshot, Counters{});
    printBins("pack", counters, looseSnapshot);
    std::printf("cfg  : %u ok / %u (%u unsupported version)\n", counters.cfgOk, counters.cfgTotal,
                counters.cfgUnsupported);
    std::printf("extract failures (could not decompress/decrypt): %u\n", counters.extractFailures);

    // "0 ok / 0" is ambiguous between "all fine" and "never seen". Say which.
    struct Expectation {
        const char* ext;
        uint32_t total;
    };
    const Expectation expectations[] = {{".chf", counters.chfTotal}, {".cps", counters.cpsTotal},
                                        {".x", counters.xTotal},     {".bin", counters.binTotal},
                                        {".cfg", counters.cfgTotal}};
    bool anyMissing = false;
    for (const auto& e : expectations) {
        if (e.total == 0) {
            if (!anyMissing) std::printf("\nno sample found for: ");
            std::printf("%s ", e.ext);
            anyMissing = true;
        }
    }
    if (anyMissing) {
        std::printf("\n(that category is untested against real data — not a pass)   \n");
    }

    if (!g_chfUnsupportedVersions.empty()) {
        std::printf("\nchf version census (real pack data; parser-unsupported versions):\n");
        for (const auto& [ver, count] : g_chfUnsupportedVersions) {
            std::printf("  chf-census: 0x%04X x%d\n", ver, count);
        }
        for (const auto& [ver, exs] : g_chfUnsupportedExamples) {
            for (const auto& ex : exs) {
                std::printf("  chf-example: v0x%04X %s size=%zu\n", ver, ex.name.c_str(), ex.size);
                std::printf("  chf-hex: v0x%04X %s\n", ver, ex.hexHead.c_str());
            }
        }
    }
    if (!g_failureReasons.empty()) {
        std::printf("\nfailure reasons (aggregated, so none are lost to log/annotation caps):\n");
        for (const auto& [reason, count] : g_failureReasons) {
            std::printf("  %4u x %s\n", count, reason.c_str());
        }
    }

    const uint32_t parsed = counters.chfOk + counters.cpsOk + counters.xSkeletonOk + counters.xMeshOk +
                            counters.binOk + counters.cfgOk;
    const uint32_t attempted = counters.chfTotal + counters.cpsTotal + counters.xTotal + counters.binTotal +
                               counters.cfgTotal + counters.extractFailures;
    std::printf("\nparsed %u of %u candidate authentic files\n", parsed, attempted);
    if (attempted == 0) {
        std::printf("NOTE: no authentic candidate files found under the given root.\n");
        std::printf("      Nothing was verified. This is reported, not silently treated as success.\n");
    }

    // Exit codes are deliberately distinct so CI and callers cannot mistake "found nothing"
    // for "everything passed":
    //   0 = at least one authentic entry was found and every one of them parsed
    //   1 = authentic entries were found but at least one failed to parse
    //   2 = no authentic candidate was found at all; nothing was verified
    std::printf("\npack entries (what the engine actually loads): %u parsed, %u failed, %u unsupported\n",
                counters.packOk, counters.packFailed, counters.packUnsupported);
    std::printf("loose files (informational only):            %u parsed, %u failed, %u unsupported\n",
                counters.looseOk, counters.looseFailed, counters.looseUnsupported);

    if (!g_unsupportedReasons.empty()) {
        std::printf("\nunsupported formats (real data, no definition in this build -- B11):\n");
        for (const auto& [reason, count] : g_unsupportedReasons) {
            std::printf("  %4u x %s\n", count, reason.c_str());
        }
    }

    // The verdict is about pack entries ONLY. The engine resolves character assets through
    // GLOGIC::openfile_basestream(bENGLIB_ZIPFILE, ...), i.e. from SkinObject.rcc /
    // Animation.rcc. Loose files are reported for information; failing on data the engine
    // never reads would be a false alarm, and ignoring pack failures would be the opposite.
    //
    // Three outcomes, three exit codes -- and UNSUPPORTED outranks a clean pass because a
    // build that cannot read half the client is not "verified", even if nothing is broken:
    //   0  PASS         every pack entry parsed
    //   1  FAIL         at least one pack entry is genuinely broken (reader defect)
    //   2  UNSUPPORTED  nothing failed, but some entries are of a format this build has no
    //                   definition for (B11) -- coverage gap, tracked, not a defect
    //   3  NOT FOUND    no character entry was found inside any pack; nothing verified
    if (counters.packOk == 0 && counters.packFailed == 0 && counters.packUnsupported == 0) {
        std::printf("\nVERDICT: NOT FOUND — no character entries were found inside any pack. "
                    "Nothing was verified.\n");
        return 3;
    }
    if (counters.packFailed != 0) {
        std::printf("\nVERDICT: FAIL — %u pack entr(y/ies) could not be parsed. This build cannot "
                    "load that data.\n", counters.packFailed);
        return 1;
    }
    if (counters.packUnsupported != 0) {
        std::printf("\nVERDICT: UNSUPPORTED — %u pack entr(y/ies) parsed cleanly, but %u are of a "
                    "format this build has no definition for (see B11). No reader defect was "
                    "observed; coverage is incomplete.\n",
                    counters.packOk, counters.packUnsupported);
        return 2;
    }
    std::printf("\nVERDICT: PASS — every character entry inside the packs parsed.\n");
    (void)parsed;
    (void)attempted;
    return 0;
}
