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

    // B18: decodes .tga (the TEXTURE_TGA branch of the .mtf container,
    // MemoryTexture.cpp EncryptTexture). Supported: uncompressed type 2 / RLE type 10
    // in 24-bit BGR and 32-bit BGRA, uncompressed colour-mapped type 1 and greyscale
    // type 3; origin bit honoured. Anything else returns invalid (never guessed).
    static DecodedTexture DecodeTGA(const std::vector<uint8_t>& tgaData);
};

} // namespace RANMobile
