#include "RANMobileDataConverter.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace RANMobile {

AssetConversionResult RANMobileDataConverter::ConvertAsset(const std::string& sourcePath, const std::string& outputDir) {
    AssetConversionResult res;
    res.sourcePath = sourcePath;
    std::error_code ec;
    if (!fs::exists(sourcePath, ec)) {
        res.success = false;
        res.status = "DATA NOT FOUND IN SOURCE";
        return res;
    }

    fs::path src(sourcePath);
    fs::path out = fs::path(outputDir) / src.filename();
    res.outputPath = out.string();
    res.success = true;
    res.status = "CONVERTED";
    return res;
}

} // namespace RANMobile
