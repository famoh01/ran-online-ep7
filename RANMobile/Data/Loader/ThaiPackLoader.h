#pragma once

#include <string>
#include <vector>
#include <cstddef>

namespace RANMobile {

struct ThaiPackStats {
    bool found = false;
    size_t ntkCount = 0;
    size_t qstCount = 0;
    size_t totalFiles = 0;
};

class ThaiPackLoader {
public:
    static ThaiPackStats InspectThaiPack(const std::string& thaiPackDir);
};

} // namespace RANMobile
