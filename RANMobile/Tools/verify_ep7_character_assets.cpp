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
// error string. Exit codes distinguish the three honest outcomes:
//   0 = authentic character entries were found and every one parsed
//   1 = entries were found but at least one failed to parse or could not be extracted
//   2 = nothing authentic was found, so nothing was verified
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

struct Counters {
    uint32_t chfTotal = 0, chfOk = 0;
    uint32_t cpsTotal = 0, cpsOk = 0;
    uint32_t xTotal = 0, xSkeletonOk = 0, xMeshOk = 0;
    uint32_t binTotal = 0, binOk = 0;
    uint32_t cfgTotal = 0, cfgOk = 0;
    uint32_t extractFailures = 0;
    uint32_t packOk = 0, packFailed = 0;   // entries inside .rcc/.zip (engine-authoritative)
    uint32_t looseOk = 0, looseFailed = 0; // loose files under data/animation|character
    uint32_t firstVersionSeenNonZero = 0;
};

// Failure reasons are aggregated so the summary survives CI's annotation cap (~10 per check
// run): a client with 400 unimplemented animation versions must still say so in one line.
std::map<std::string, uint32_t> g_failureReasons;

std::string ReasonKey(const std::string& detail) {
    // Keep the leading clause only; trailing numbers/paths vary per file.
    const size_t cut = detail.find(" — ");
    std::string key = (cut == std::string::npos) ? detail : detail.substr(0, cut);
    if (key.size() > 120) key.resize(120);
    return key;
}

void ReportFile(const char* kind, const std::string& path, bool ok, const std::string& detail) {
    std::printf("  [%s] %-10s %s%s%s\n", ok ? " OK " : "FAIL", kind, path.c_str(),
                detail.empty() ? "" : " — ", detail.c_str());
    if (!ok) ++g_failureReasons[std::string(kind) + ": " + ReasonKey(detail)];
}

}  // namespace

// Verifies one asset buffer. `path` is only used for reporting (it may be
// "<archive>:/entry/name" for packed entries).
bool VerifySingleAsset(const std::string& path, const std::vector<uint8_t>& bytes, Counters& counters) {
    const std::string ext = ExtractExtension(path);

    if (EndsWithNoCase(ext, ".chf")) {
        ++counters.chfTotal;
        const RANMobile::EP7CharacterFileInfo info = RANMobile::EP7CharacterFileReader::Parse(bytes, false);
        if (info.valid) {
            ++counters.chfOk;
            char detail[256];
            std::snprintf(detail, sizeof(detail), "v0x%04X pieces=%u skeleton=%s anims=%zu", info.version,
                          info.PieceCount(), info.skeleton.c_str(), info.animations.size());
            ReportFile("chf", path, true, detail);
        } else {
            ReportFile("chf", path, false, info.error);
        }
        return info.valid;
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
            ReportFile("cps", path, true, detail);
        } else {
            ReportFile("cps", path, false, info.error);
        }
        return info.valid;
    }

    if (EndsWithNoCase(ext, ".skt") || EndsWithNoCase(ext, ".abf")) {
        RANMobile::EP7Skeleton skeleton;
        ++counters.xTotal;
        const bool ok = skeleton.LoadFromXFile(bytes);
        if (ok) {
            ++counters.xSkeletonOk;
            char detail[128];
            std::snprintf(detail, sizeof(detail), "bones=%zu", skeleton.BoneCount());
            ReportFile("skeleton", path, true, detail);
        } else {
            ReportFile("skeleton", path, false, skeleton.Error());
        }
        return ok;
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
            ReportFile("x-mesh", path, true, detail);
            return true;
        }
        RANMobile::EP7Skeleton skeleton;
        if (skeleton.LoadFromXFile(bytes)) {
            ++counters.xSkeletonOk;
            char detail[128];
            std::snprintf(detail, sizeof(detail), "skeleton bones=%zu", skeleton.BoneCount());
            ReportFile("x-skeleton", path, true, detail);
            return true;
        }
        ReportFile("x", path, false, mesh.error + " | " + skeleton.Error());
        return false;
    }

    if (EndsWithNoCase(ext, ".bin")) {
        ++counters.binTotal;
        // A .bin that begins with printable ASCII is not an EP7 basestream container at all
        // (the client's own containers start with a small version DWORD). Name the magic so
        // an unknown format is self-explanatory rather than just "unsupported version".
        if (bytes.size() >= 4 && std::isprint(bytes[0]) && std::isprint(bytes[1]) &&
            std::isprint(bytes[2]) && std::isprint(bytes[3])) {
            char magic[5] = {static_cast<char>(bytes[0]), static_cast<char>(bytes[1]),
                             static_cast<char>(bytes[2]), static_cast<char>(bytes[3]), 0};
            char detail[192];
            std::snprintf(detail, sizeof(detail),
                          "not an EP7 basestream container — starts with magic '%s' (0x%02X%02X%02X%02X); "
                          "the engine only loads SAnimContainer basestreams from Animation.rcc",
                          magic, bytes[0], bytes[1], bytes[2], bytes[3]);
            ReportFile("bin", path, false, detail);
            return false;
        }
        // Try with the basestream header first (CSerialFile writes one), then without.
        RANMobile::EP7AnimClip clip = RANMobile::EP7AnimationReader::ParseContainer(bytes, true, false);
        if (!clip.valid) clip = RANMobile::EP7AnimationReader::ParseContainer(bytes, false, false);
        if (clip.valid) {
            ++counters.binOk;
            char detail[256];
            std::snprintf(detail, sizeof(detail), "v0x%04X mainTracks=%zu upBodyTracks=%zu maxKeyTime=%u",
                          clip.version, clip.mainTracks.size(), clip.upBodyTracks.size(), clip.MaxKeyTime());
            ReportFile("bin", path, true, detail);
        } else {
            ReportFile("bin", path, false, clip.error);
        }
        return clip.valid;
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
            ReportFile("cfg", path, true, detail);
        } else {
            ReportFile("cfg", path, false, info.error);
        }
        return info.valid;
    }

    return false;
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
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--max-files") == 0 && i + 1 < argc) {
            maxFiles = static_cast<size_t>(std::strtoul(argv[++i], nullptr, 10));
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
            ReportFile("read", path, false, error);
            continue;
        }
        if (VerifySingleAsset(path, bytes, counters)) ++counters.looseOk; else ++counters.looseFailed;
    }

    // The authentic assets live inside these packs (GLOGIC::InitFile).
    for (const auto& archivePath : archives) {
        std::printf("\n--- archive %s\n", archivePath.c_str());
        if (!ReadWholeFile(archivePath, bytes, error)) {
            ReportFile("read", archivePath, false, error);
            continue;
        }
        RANMobile::EP7RccArchive archive;
        if (!archive.Open(bytes)) {
            ReportFile("archive", archivePath, false, archive.Error());
            continue;
        }
        std::printf("     %zu entries\n", archive.EntryCount());

        size_t checked = 0;
        for (const auto& entry : archive.Entries()) {
            if (!IsCharacterAssetName(entry.name)) continue;
            if (checked >= maxFiles) break;
            ++checked;

            const std::string label = archivePath + ":" + entry.name;
            std::vector<uint8_t> entryBytes;
            if (!archive.Extract(entry, entryBytes)) {
                // Report the method/flag/size triple: if the shipped packs ever use zip64,
                // AES or a method other than stored/deflate, this line says so instead of
                // leaving a mystery.
                char detail[256];
                std::snprintf(detail, sizeof(detail),
                              "could not extract (method=%u flags=0x%04X csize=%u usize=%u) — corrupt, "
                              "another password, or an unsupported method",
                              entry.method, entry.flags, entry.compressedSize, entry.uncompressedSize);
                ReportFile("extract", label, false, detail);
                // An entry we could not even decompress is an UNVERIFIED character asset. It
                // must count against the pass rate; otherwise "2 entries, 1 unreadable" would
                // still print "parsed 1 of 1" and exit 0.
                ++counters.extractFailures;
                ++counters.packFailed;
                continue;
            }
            if (VerifySingleAsset(label, entryBytes, counters)) ++counters.packOk; else ++counters.packFailed;
        }
        if (checked == 0) std::printf("     (no character assets inside)\n");
    }

    std::printf("\n=== SUMMARY (verified against authentic files only) ===\n");
    std::printf("chf  : %u ok / %u\n", counters.chfOk, counters.chfTotal);
    std::printf("cps  : %u ok / %u\n", counters.cpsOk, counters.cpsTotal);
    std::printf("x    : %u skeleton / %u mesh / %u parsed of %u\n", counters.xSkeletonOk, counters.xMeshOk,
                counters.xSkeletonOk + counters.xMeshOk, counters.xTotal);
    std::printf("bin  : %u ok / %u\n", counters.binOk, counters.binTotal);
    std::printf("cfg  : %u ok / %u\n", counters.cfgOk, counters.cfgTotal);
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
    std::printf("\npack entries (what the engine actually loads): %u parsed, %u failed\n",
                counters.packOk, counters.packFailed);
    std::printf("loose files (informational only):            %u parsed, %u failed\n",
                counters.looseOk, counters.looseFailed);

    // The verdict is about pack entries ONLY. The engine resolves character assets through
    // GLOGIC::openfile_basestream(bENGLIB_ZIPFILE, ...), i.e. from SkinObject.rcc /
    // Animation.rcc. Loose files are reported for information; failing the build on data
    // the engine never reads would be a false alarm, and ignoring pack failures would be
    // the opposite. Say which is which.
    if (counters.packOk == 0 && counters.packFailed == 0) {
        std::printf("\nVERDICT: no character entries were found inside any pack. Nothing was verified.\n");
        return 2;
    }
    if (counters.packFailed != 0) {
        std::printf("\nVERDICT: %u pack entr(y/ies) FAILED to parse — this build cannot load that data.\n",
                    counters.packFailed);
        return 1;
    }
    std::printf("\nVERDICT: every character entry inside the packs parsed.\n");
    (void)parsed;
    (void)attempted;
    return 0;
}
