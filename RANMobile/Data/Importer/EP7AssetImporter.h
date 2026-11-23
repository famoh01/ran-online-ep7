#pragma once

#include <string>
#include <vector>

namespace RANMobile {

struct ImportedAssetInfo {
    std::string originalId;
    std::string format; // .x, .mxf, .cps, .chf, .dds, .mtf, .wld, .mnsf, .isf, .ssf
    std::string sourcePath;
    bool found = false;
};

class EP7AssetImporter {
public:
    static ImportedAssetInfo InspectAsset(const std::string& originalId, const std::string& sourcePath);
};

} // namespace RANMobile
