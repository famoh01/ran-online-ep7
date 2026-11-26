#include "EP7SceneData.h"
#include "EP7SerialFile.h"
#include "../../Engine/Core/Logger.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace RANMobile {

namespace {

std::string Trim(const std::string& input) {
    const size_t begin = input.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return std::string();
    const size_t end = input.find_last_not_of(" \t\r\n");
    return input.substr(begin, end - begin + 1);
}

std::vector<std::string> SplitIniValues(const std::string& line) {
    std::vector<std::string> out;
    std::string token;
    for (char ch : line) {
        if (ch == ',' || ch == '[' || ch == ']' || ch == '\t') {
            out.push_back(Trim(token));
            token.clear();
        } else {
            token.push_back(ch);
        }
    }
    out.push_back(Trim(token));
    return out;
}

bool ParseBool(const std::string& value) {
    return value == "1" || value == "true" || value == "TRUE";
}

bool ParseU32(const std::string& value, uint32_t& out) {
    try {
        const unsigned long parsed = std::stoul(value);
        out = static_cast<uint32_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<uint8_t> ReadFileBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return {};
    const std::streamsize size = in.tellg();
    if (size < 0) return {};
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (size > 0 && !in.read(reinterpret_cast<char*>(data.data()), size)) return {};
    return data;
}

fs::path FindFileRecursive(const fs::path& root, const std::string& fileName) {
    std::error_code ec;
    for (fs::recursive_directory_iterator it(root, ec), end; it != end && !ec; it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().filename() == fileName) return it->path();
    }
    return {};
}

} // namespace

std::string EP7MapListReport::ToString() const {
    std::ostringstream oss;
    oss << "MAP LIST: " << (valid ? "VALID" : "UNKNOWN/ENCODED") << "\n";
    oss << "MapKeySize: " << mapKeySize << "\n";
    oss << "Entries: " << entries.size() << "\n";
    for (const auto& e : errors) oss << "ERROR: " << e << "\n";
    return oss.str();
}

EP7MapListReport EP7MapListParser::Parse(const std::string& text) {
    EP7MapListReport report;
    std::istringstream in(text);
    std::string line;
    std::string section;
    uint32_t mapIndex = 0;

    while (std::getline(in, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == ';') continue;
        if (trimmed.front() == '[' && trimmed.back() == ']') {
            section = Trim(trimmed.substr(1, trimmed.size() - 2));
            continue;
        }
        const size_t eq = trimmed.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = Trim(trimmed.substr(0, eq));
        const std::string value = trimmed.substr(eq + 1);

        if (section == "MAP_LIST_INFO" && key == "MapKeySize") {
            if (!ParseU32(Trim(value), report.mapKeySize)) report.errors.push_back("Invalid MapKeySize");
            continue;
        }
        if (section != "MAP_LIST" || key != "MAP") continue;
        if (report.mapKeySize == 0) {
            report.errors.push_back("MAP encountered before MapKeySize");
            continue;
        }
        uint32_t tmp = 0, tmp2 = 0, tmp3 = 0;
        const std::vector<std::string> v = SplitIniValues(value);
        if (v.size() < report.mapKeySize) {
            report.errors.push_back("MAP row " + std::to_string(mapIndex) + " has " +
                                    std::to_string(v.size()) + " fields, expected " +
                                    std::to_string(report.mapKeySize));
            ++mapIndex;
            continue;
        }
        EP7MapListEntry entry;
        entry.index = mapIndex++;
        entry.used = ParseBool(v[0]);
        if (!entry.used) continue;
        entry.file = v[1];
        if (!ParseU32(v[2], tmp) || !ParseU32(v[3], tmp2) || !ParseU32(v[4], tmp3)) {
            report.errors.push_back("Non-numeric ID in MAP row " + std::to_string(entry.index));
            ++mapIndex;
            continue;
        }
        entry.mainID = static_cast<uint16_t>(tmp);
        entry.subID = static_cast<uint16_t>(tmp2);
        entry.fieldSID = tmp3;
        entry.mapName = v[16];
        entry.bgm = v[17];
        entry.loadingImage = v[18];
        entry.instantMap = v.size() > 19 && ParseBool(v[19]);
        report.entries.push_back(entry);
    }
    report.valid = !report.entries.empty();
    return report;
}

EP7LevelHeader EP7LevelHeaderParser::Parse(const std::vector<uint8_t>& data) {
    EP7LevelHeader out;
    const EP7SerialFileView file = EP7SerialFile::Read(data);
    if (!file.valid) {
        out.error = file.error.empty() ? "INVALID SERIAL HEADER" : file.error;
        return out;
    }
    out.fileType = file.fileType;
    out.fileVersion = file.version;
    constexpr uint32_t kGLLevelVersion = 0x0104; // GLLevelFile::VERSION
    if (out.fileVersion != kGLLevelVersion) {
        out.error = "UNSUPPORTED GLLEVEL VERSION";
        return out;
    }

    size_t pos = 0;
    uint32_t headVersion = 0, blockSize = 0;
    if (!EP7SerialFile::ReadU32(file.payload, pos, headVersion) ||
        !EP7SerialFile::ReadU32(file.payload, pos, blockSize)) {
        out.error = "TRUNCATED SLEVEL_HEAD";
        return out;
    }
    out.headVersion = headVersion;
    constexpr uint32_t kSLevelHeadVersion = 0x0101; // SLEVEL_HEAD::VERSION
    if (headVersion != kSLevelHeadVersion) {
        out.error = "UNSUPPORTED SLEVEL_HEAD VERSION";
        return out;
    }
    const size_t blockStart = pos;
    if (!EP7SerialFile::ReadString(file.payload, pos, out.wldFile) ||
        !EP7SerialFile::ReadString(file.payload, pos, out.mapName) ||
        !EP7SerialFile::ReadU32(file.payload, pos, out.bright) ||
        !EP7SerialFile::ReadU32(file.payload, pos, out.division)) {
        out.error = "MALFORMED SLEVEL_HEAD";
        return out;
    }
    if (blockSize < (pos - blockStart) || blockSize > file.payload.size() - blockStart) {
        out.error = "SLEVEL_HEAD SIZE MISMATCH";
        return out;
    }
    out.valid = !out.wldFile.empty();
    return out;
}

std::string EP7SceneIndex::ToString() const {
    std::ostringstream oss;
    oss << "SCENE INDEX root=" << sourceRoot << " valid=" << (valid ? "YES" : "NO")
        << " entries=" << scenes.size() << "\n";
    for (const auto& s : scenes) {
        oss << "  " << s.mapName << " | glmap=" << s.glmapFile
            << " | wld=" << s.wldFile << " | loading=" << s.loadingImage << "\n";
        if (!s.error.empty()) oss << "    ERROR: " << s.error << "\n";
    }
    for (const auto& d : diagnostics) oss << "DIAGNOSTIC: " << d << "\n";
    return oss.str();
}

EP7SceneIndex EP7SceneIndexBuilder::Build(const std::string& root) {
    EP7SceneIndex index;
    index.sourceRoot = root;
    std::error_code ec;
    const fs::path base(root);
    if (!fs::exists(base, ec) || !fs::is_directory(base, ec)) {
        index.diagnostics.push_back("ROOT NOT FOUND: " + root);
        return index;
    }

    const fs::path manifestPath = FindFileRecursive(base, "mapslist.mst");
    if (manifestPath.empty()) {
        index.diagnostics.push_back("mapslist.mst NOT FOUND");
        return index;
    }
    std::ifstream manifest(manifestPath, std::ios::binary);
    if (!manifest) {
        index.diagnostics.push_back("mapslist.mst UNREADABLE");
        return index;
    }
    const std::string text((std::istreambuf_iterator<char>(manifest)), std::istreambuf_iterator<char>());
    const EP7MapListReport list = EP7MapListParser::Parse(text);
    if (!list.valid) {
        index.diagnostics.push_back("mapslist.mst NOT PLAIN TEXT OR NO USABLE MAP ROWS (encoded mapslist is unsupported)");
        for (const auto& e : list.errors) index.diagnostics.push_back(e);
        return index;
    }

    size_t headersFound = 0;
    for (const EP7MapListEntry& entry : list.entries) {
        EP7SceneEntry scene;
        scene.glmapFile = entry.file;
        scene.mapName = entry.mapName;
        scene.loadingImage = entry.loadingImage;
        scene.mainID = entry.mainID;
        scene.subID = entry.subID;

        const fs::path glmapPath = FindFileRecursive(base, entry.file);
        if (glmapPath.empty()) {
            scene.error = "GLMAP NOT FOUND";
        } else {
            const EP7LevelHeader header = EP7LevelHeaderParser::Parse(ReadFileBytes(glmapPath));
            if (!header.valid) {
                scene.error = "GLMAP HEADER: " + header.error;
            } else {
                scene.wldFile = header.wldFile;
                if (header.mapName.empty() || scene.mapName.empty()) scene.mapName = header.mapName;
                ++headersFound;
            }
        }
        index.scenes.push_back(scene);
    }
    index.valid = headersFound > 0;
    if (!index.valid) index.diagnostics.push_back("NO VALID GLLEVEL HEADERS RESOLVED");
    Logger::Info("EP7SceneIndex", index.ToString());
    return index;
}

} // namespace RANMobile
