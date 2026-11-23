#include "ThaiPackLoader.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace RANMobile {

ThaiPackStats ThaiPackLoader::InspectThaiPack(const std::string& thaiPackDir) {
    ThaiPackStats stats;
    std::error_code ec;
    if (!fs::exists(thaiPackDir, ec) || !fs::is_directory(thaiPackDir, ec)) {
        return stats;
    }

    stats.found = true;
    for (const auto& entry : fs::recursive_directory_iterator(thaiPackDir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        stats.totalFiles++;
        auto ext = entry.path().extension().string();
        if (ext == ".ntk" || ext == ".NTK") {
            stats.ntkCount++;
        } else if (ext == ".qst" || ext == ".QST") {
            stats.qstCount++;
        }
    }
    return stats;
}

} // namespace RANMobile
