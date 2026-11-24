#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace RANMobile {

struct ScannedAsset {
    std::string name;
    std::string extension;
    bool found = false;
    uint64_t sizeBytes = 0;
    std::string resolvedPath;
};

class EP7DependencyScanner {
public:
    void IndexClientDirectory(const std::string& rootDir);
    std::vector<ScannedAsset> ScanRecursive(const std::string& entryFilename, int maxDepth = 4);

private:
    std::vector<std::string> ExtractAssetReferences(const std::string& filePath);
    static std::string ToLower(const std::string& s);
    static std::string GetExtension(const std::string& s);

    std::unordered_map<std::string, std::pair<std::string, uint64_t>> m_fileIndex;
};

} // namespace RANMobile
