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
// error string. The tool exits non-zero only on impossible internal states, so CI can use
// its report to record what is genuinely verified.
//
// USAGE
//   verify_ep7_character_assets <ep7-data-root> [--max-files N] [--json out.json]
//
// The root is the extracted client folder that contains `skinobject/` and/or `data/`.
//
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../Data/Importer/EP7Animation.h"
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

struct Counters {
    uint32_t chfTotal = 0, chfOk = 0;
    uint32_t cpsTotal = 0, cpsOk = 0;
    uint32_t xTotal = 0, xSkeletonOk = 0, xMeshOk = 0;
    uint32_t binTotal = 0, binOk = 0;
    uint32_t cfgTotal = 0, cfgOk = 0;
    uint32_t firstVersionSeenNonZero = 0;
};

void ReportFile(const char* kind, const std::string& path, bool ok, const std::string& detail) {
    std::printf("  [%s] %-10s %s%s%s\n", ok ? " OK " : "FAIL", kind, path.c_str(),
                detail.empty() ? "" : " — ", detail.c_str());
}

}  // namespace

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

    // Collect candidate files (recursive, "find" is available on the runner).
    std::string listCommand = "find '" + root + "' -type f 2>/dev/null";
    std::FILE* pipe = popen(listCommand.c_str(), "r");
    if (!pipe) {
        std::fprintf(stderr, "cannot list %s\n", root.c_str());
        return 2;
    }

    std::vector<std::string> paths;
    char line[4096];
    while (std::fgets(line, sizeof(line), pipe)) {
        std::string path(line);
        while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) path.pop_back();
        if (path.empty()) continue;
        const std::string ext = ExtractExtension(path);
        if (EndsWithNoCase(ext, ".chf") || EndsWithNoCase(ext, ".cps") || EndsWithNoCase(ext, ".x") ||
            EndsWithNoCase(ext, ".bin") || EndsWithNoCase(ext, ".cfg") || EndsWithNoCase(ext, ".skt") ||
            EndsWithNoCase(ext, ".abf")) {
            paths.push_back(path);
            if (paths.size() >= maxFiles) break;
        }
    }
    pclose(pipe);

    std::sort(paths.begin(), paths.end());

    std::printf("=== Phase 3J authentic character asset verification ===\n");
    std::printf("root: %s\n", root.c_str());
    std::printf("candidate files: %zu\n\n", paths.size());

    Counters counters;
    std::vector<uint8_t> bytes;
    std::string error;

    for (const auto& path : paths) {
        if (!ReadWholeFile(path, bytes, error)) {
            ReportFile("read", path, false, error);
            continue;
        }
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
            continue;
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
            continue;
        }

        if (EndsWithNoCase(ext, ".skt") || EndsWithNoCase(ext, ".abf")) {
            // Skeleton-only candidates.
            RANMobile::EP7Skeleton skeleton;
            if (skeleton.LoadFromXFile(bytes)) {
                ++counters.xTotal;
                ++counters.xSkeletonOk;
                char detail[128];
                std::snprintf(detail, sizeof(detail), "bones=%zu", skeleton.BoneCount());
                ReportFile("skeleton", path, true, detail);
            } else {
                ++counters.xTotal;
                ReportFile("skeleton", path, false, skeleton.Error());
            }
            continue;
        }

        if (EndsWithNoCase(ext, ".x")) {
            // Could be a skeleton or a skinned mesh.
            ++counters.xTotal;
            RANMobile::EP7Skeleton skeleton;
            if (skeleton.LoadFromXFile(bytes)) {
                ++counters.xSkeletonOk;
                char detail[128];
                std::snprintf(detail, sizeof(detail), "skeleton bones=%zu", skeleton.BoneCount());
                ReportFile("x-skeleton", path, true, detail);
                continue;
            }
            const RANMobile::EP7SkinnedMesh mesh = RANMobile::EP7SkinnedMeshReader::Parse(bytes, std::string(), nullptr);
            if (mesh.valid) {
                ++counters.xMeshOk;
                char detail[256];
                std::snprintf(detail, sizeof(detail), "mesh=%s verts=%u tris=%zu bones=%zu stride=%u decoded=%s",
                              mesh.name.c_str(), mesh.vertexCount, mesh.faces.size() / 3, mesh.bones.size(),
                              mesh.vertexStride, mesh.geometryDecoded ? "yes" : "no");
                ReportFile("x-mesh", path, true, detail);
            } else {
                ReportFile("x", path, false, skeleton.Error() + " | " + mesh.error);
            }
            continue;
        }

        if (EndsWithNoCase(ext, ".bin")) {
            ++counters.binTotal;
            // Try with the basestream header first (CSerialFile writes one), then without.
            RANMobile::EP7AnimClip clip = RANMobile::EP7AnimationReader::ParseContainer(bytes, true, false);
            if (!clip.valid) {
                clip = RANMobile::EP7AnimationReader::ParseContainer(bytes, false, false);
            }
            if (clip.valid) {
                ++counters.binOk;
                char detail[256];
                std::snprintf(detail, sizeof(detail), "v0x%04X mainTracks=%zu upBodyTracks=%zu maxKeyTime=%u",
                              clip.version, clip.mainTracks.size(), clip.upBodyTracks.size(), clip.MaxKeyTime());
                ReportFile("bin", path, true, detail);
            } else {
                ReportFile("bin", path, false, clip.error);
            }
            continue;
        }

        if (EndsWithNoCase(ext, ".cfg")) {
            ++counters.cfgTotal;
            RANMobile::EP7AnimInfo info = RANMobile::EP7AnimationReader::ParseInfo(bytes, true, false);
            if (!info.valid) {
                info = RANMobile::EP7AnimationReader::ParseInfo(bytes, false, false);
            }
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
            continue;
        }
    }

    std::printf("\n=== SUMMARY (verified against authentic files only) ===\n");
    std::printf("chf  : %u ok / %u\n", counters.chfOk, counters.chfTotal);
    std::printf("cps  : %u ok / %u\n", counters.cpsOk, counters.cpsTotal);
    std::printf("x    : %u skeleton / %u mesh / %u parsed of %u\n", counters.xSkeletonOk, counters.xMeshOk,
                counters.xSkeletonOk + counters.xMeshOk, counters.xTotal);
    std::printf("bin  : %u ok / %u\n", counters.binOk, counters.binTotal);
    std::printf("cfg  : %u ok / %u\n", counters.cfgOk, counters.cfgTotal);

    const uint32_t parsed = counters.chfOk + counters.cpsOk + counters.xSkeletonOk + counters.xMeshOk +
                            counters.binOk + counters.cfgOk;
    const uint32_t attempted = counters.chfTotal + counters.cpsTotal + counters.xTotal + counters.binTotal +
                               counters.cfgTotal;
    std::printf("\nparsed %u of %u candidate authentic files\n", parsed, attempted);
    if (attempted == 0) {
        std::printf("NOTE: no authentic candidate files found under the given root.\n");
        std::printf("      Nothing was verified. This is reported, not silently treated as success.\n");
    }
    return 0;
}
