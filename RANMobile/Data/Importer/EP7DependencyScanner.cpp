#include "EP7DependencyScanner.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <queue>
#include <unordered_set>

namespace fs = std::filesystem;

namespace RANMobile {

std::string EP7DependencyScanner::ToLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string EP7DependencyScanner::GetExtension(const std::string& s) {
    auto pos = s.find_last_of('.');
    if (pos == std::string::npos) return "";
    return ToLower(s.substr(pos));
}

void EP7DependencyScanner::IndexClientDirectory(const std::string& rootDir) {
    m_fileIndex.clear();
    if (!fs::exists(rootDir)) return;
    for (const auto& entry : fs::recursive_directory_iterator(rootDir, fs::directory_options::skip_permission_denied)) {
        if (entry.is_regular_file()) {
            std::string fname = ToLower(entry.path().filename().string());
            uint64_t sz = entry.file_size();
            m_fileIndex[fname] = {entry.path().string(), sz};
        }
    }
}

std::vector<std::string> EP7DependencyScanner::ExtractAssetReferences(const std::string& filePath) {
    std::vector<std::string> refs;
    std::ifstream f(filePath, std::ios::binary);
    if (!f.is_open()) return refs;
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    static const char* kExts[] = {
        ".dds", ".mtf", ".tga", ".png", ".jpg", ".x", ".cps", ".chf",
        ".abl", ".abf", ".cfg", ".bin", ".egp", ".wld", ".lev"
    };

    std::string cur;
    for (unsigned char c : data) {
        if (c >= 32 && c < 127 && c != '\\' && c != '/' && c != '"' && c != '\'' && c != ' ') {
            cur.push_back(static_cast<char>(c));
        } else {
            if (cur.size() >= 4 && cur.size() <= 64) {
                std::string low = ToLower(cur);
                for (const char* ext : kExts) {
                    size_t elen = std::string(ext).size();
                    if (low.size() > elen && low.compare(low.size() - elen, elen, ext) == 0) {
                        refs.push_back(cur);
                        break;
                    }
                }
            }
            cur.clear();
        }
    }
    return refs;
}

std::vector<ScannedAsset> EP7DependencyScanner::ScanRecursive(const std::string& entryFilename, int maxDepth) {
    std::vector<ScannedAsset> results;
    std::unordered_set<std::string> visited;
    std::queue<std::pair<std::string, int>> q;
    q.push({entryFilename, 0});
    visited.insert(ToLower(entryFilename));

    while (!q.empty()) {
        auto [name, depth] = q.front();
        q.pop();

        ScannedAsset asset;
        asset.name = name;
        asset.extension = GetExtension(name);
        auto it = m_fileIndex.find(ToLower(name));
        if (it != m_fileIndex.end()) {
            asset.found = true;
            asset.resolvedPath = it->second.first;
            asset.sizeBytes = it->second.second;
            if (depth < maxDepth && asset.extension != ".dds" && asset.extension != ".mtf" &&
                asset.extension != ".tga" && asset.extension != ".png" && asset.extension != ".jpg") {
                for (const auto& ref : ExtractAssetReferences(asset.resolvedPath)) {
                    std::string lowRef = ToLower(ref);
                    if (visited.insert(lowRef).second) {
                        q.push({ref, depth + 1});
                    }
                }
            }
        }
        results.push_back(asset);
    }
    return results;
}

} // namespace RANMobile
