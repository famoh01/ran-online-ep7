#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace RANMobile {
    class ResourceManager;
}

namespace RANMobile {

struct AssetPipelineItem {
    std::string relativePath;
    std::string format;
    std::string category;
    size_t sizeBytes = 0;
    bool accepted = false;
    std::string reason;
};

struct AssetPipelineReport {
    std::string root;
    bool rootExists = false;
    size_t filesScanned = 0;
    size_t accepted = 0;
    size_t rejected = 0;
    size_t bytes = 0;
    std::map<std::string, size_t> acceptedByFormat;
    std::vector<AssetPipelineItem> items;
    std::vector<std::string> diagnostics;

    std::string ToString() const;
};

// Inspects files against formats documented in SOURCE_ASSET_INVENTORY.md.
// No file is converted, generated or modified; only existing bytes are counted
// and optionally registered in the in-memory ResourceManager.
class EP7AssetPipeline {
public:
    static AssetPipelineReport ScanDirectory(const std::string& root,
                                             ResourceManager* resources = nullptr);
    static AssetPipelineItem InspectFile(const std::string& root, const std::string& fullPath);
};

} // namespace RANMobile
