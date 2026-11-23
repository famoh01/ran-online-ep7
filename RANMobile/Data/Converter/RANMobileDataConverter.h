#pragma once

#include <string>
#include <vector>

namespace RANMobile {

struct AssetConversionResult {
    bool success = false;
    std::string sourcePath;
    std::string outputPath;
    std::string status; // "CONVERTED" or "DATA NOT FOUND IN SOURCE"
};

class RANMobileDataConverter {
public:
    static AssetConversionResult ConvertAsset(const std::string& sourcePath, const std::string& outputDir);
};

} // namespace RANMobile
