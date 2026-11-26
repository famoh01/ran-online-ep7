#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace RANMobile {

enum class EP7TextureFormat {
    Unknown, DXT1, DXT3, DXT5,
    A8R8G8B8, X8R8G8B8, R8G8B8,
    A1R5G5B5, X1R5G5B5, A4R4G4B4, R5G6B5
};

// Original TextureContainer distinguishes these modes by D3D format and by
// filename suffixes _a / _a1 / _a2 (enginelib/DxCommon/TextureManager.cpp:246-314).
enum class EP7AlphaMode {
    None,
    Hard,      // alpha test / cutout
    Soft,      // blended
    Soft01,
    Soft02
};

struct DDSHeaderInfo {
    bool valid = false;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 0;
    uint32_t mipMapCount = 0;      // reported, but only level 0 is decoded
    uint32_t flags = 0;
    uint32_t pixelFlags = 0;
    uint32_t fourCC = 0;
    EP7TextureFormat format = EP7TextureFormat::Unknown;
    bool hasAlpha = false;
    std::string error;
};

struct DecodedTexture {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> rgba8; // width * height * 4
    DDSHeaderInfo header;
    EP7AlphaMode alphaMode = EP7AlphaMode::None;
    bool valid = false;
    std::string error;
};

class EP7TextureDecoder {
public:
    static constexpr uint8_t TEX_XOR_DATA = 0x47;
    static constexpr uint8_t TEX_DIFF_DATA = 0x03;
    static constexpr int TEX_VERSION = 0x100;
    static constexpr size_t TEX_HEADER_SIZE = 12;

    // .mtf container defined by enginelib/DxCommon/EncryptTextureDef.h and
    // MemoryTexture.cpp:66-101.
    static bool DecryptMTF(const std::vector<uint8_t>& mtfData,
                           std::vector<uint8_t>& outRaw, int& outFileType);

    static DDSHeaderInfo InspectDDS(const std::vector<uint8_t>& ddsData);
    static EP7AlphaMode AlphaModeFromName(const std::string& fileName,
                                          EP7TextureFormat format,
                                          bool pixelAlpha);

    // Decodes DDS level 0 (DXT1/3/5 plus verified uncompressed formats) to RGBA8.
    static DecodedTexture DecodeDDS(const std::vector<uint8_t>& ddsData,
                                    const std::string& sourceName = std::string());
};

} // namespace RANMobile
