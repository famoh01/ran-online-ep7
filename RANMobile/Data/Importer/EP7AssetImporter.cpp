#include "EP7AssetImporter.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace RANMobile {

ImportedAssetInfo EP7AssetImporter::InspectAsset(const std::string& originalId, const std::string& sourcePath) {
    ImportedAssetInfo info;
    info.originalId = originalId;
    info.sourcePath = sourcePath;
    fs::path p(sourcePath);
    info.format = p.extension().string();
    std::error_code ec;
    info.found = fs::exists(p, ec);
    return info;
}

} // namespace RANMobile
