#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace RANMobile {

struct DecodedTexture {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> rgba8; // width * height * 4
    bool valid = false;
};

class EP7TextureDecoder {
public:
    static constexpr uint8_t TEX_XOR_DATA = 0x47;
    static constexpr uint8_t TEX_DIFF_DATA = 0x03;
    static constexpr size_t TEX_HEADER_SIZE = 12;

    // Decrypts .mtf buffer into raw .dds / .tga bytes (verified against MemoryTexture.cpp:97-98)
    static bool DecryptMTF(const std::vector<uint8_t>& mtfData, std::vector<uint8_t>& outRaw, int& outFileType);

    // Decodes .dds (DXT1, DXT3, DXT5, uncompressed RGBA8/BGRA8) into RGBA8
    static DecodedTexture DecodeDDS(const std::vector<uint8_t>& ddsData);
};

} // namespace RANMobile
