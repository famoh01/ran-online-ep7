#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace RANMobile {

struct EP7WldHeader {
    std::string fileType;
    uint32_t version = 0;
    bool valid = false;
};

struct EP7ChfHeader {
    uint32_t version = 0;
    bool valid = false;
};

struct EP7CpsHeader {
    uint32_t version = 0;
    bool valid = false;
};

class EP7WorldCharParser {
public:
    static constexpr uint32_t WLD_MIN_VERSION = 0x0108;
    static constexpr uint32_t WLD_MAX_VERSION = 0x0114; // DxLandMan::VERSION
    static constexpr uint32_t CPS_VERSION     = 0x00000107; // DxSkinPiece::VERSION

    // Parse .wld CSerialFile header (FILETYPESIZE=128 + uint32 version)
    static EP7WldHeader ParseWldHeader(const std::vector<uint8_t>& data);

    // Parse .chf header (uint32 dwVer: 0x0100, 0x0101, or current VERSION)
    static EP7ChfHeader ParseChfHeader(const std::vector<uint8_t>& data);

    // Parse .cps header (uint32 dwVer <= 0x00000107)
    static EP7CpsHeader ParseCpsHeader(const std::vector<uint8_t>& data);
};

} // namespace RANMobile
