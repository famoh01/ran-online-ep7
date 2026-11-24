#include "EP7TextureDecoder.h"
#include <cstring>
#include <algorithm>

namespace RANMobile {

namespace {
inline uint32_t ReadU32(const uint8_t* p) {
    uint32_t v = 0;
    std::memcpy(&v, p, 4);
    return v;
}

inline uint16_t ReadU16(const uint8_t* p) {
    uint16_t v = 0;
    std::memcpy(&v, p, 2);
    return v;
}

void DecodeRGB565(uint16_t c, uint8_t& r, uint8_t& g, uint8_t& b) {
    r = static_cast<uint8_t>(((c >> 11) & 0x1F) * 255 / 31);
    g = static_cast<uint8_t>(((c >> 5) & 0x3F) * 255 / 63);
    b = static_cast<uint8_t>((c & 0x1F) * 255 / 31);
}

void DecodeDXT1Block(const uint8_t* block, uint8_t* outRgba, uint32_t stride) {
    uint16_t c0 = ReadU16(block);
    uint16_t c1 = ReadU16(block + 2);
    uint32_t idx = ReadU32(block + 4);

    uint8_t colors[4][4]{};
    DecodeRGB565(c0, colors[0][0], colors[0][1], colors[0][2]); colors[0][3] = 255;
    DecodeRGB565(c1, colors[1][0], colors[1][1], colors[1][2]); colors[1][3] = 255;

    if (c0 > c1) {
        for (int i = 0; i < 3; ++i) {
            colors[2][i] = static_cast<uint8_t>((2 * colors[0][i] + colors[1][i]) / 3);
            colors[3][i] = static_cast<uint8_t>((colors[0][i] + 2 * colors[1][i]) / 3);
        }
        colors[2][3] = 255;
        colors[3][3] = 255;
    } else {
        for (int i = 0; i < 3; ++i) {
            colors[2][i] = static_cast<uint8_t>((colors[0][i] + colors[1][i]) / 2);
            colors[3][i] = 0;
        }
        colors[2][3] = 255;
        colors[3][3] = 0;
    }

    for (uint32_t y = 0; y < 4; ++y) {
        for (uint32_t x = 0; x < 4; ++x) {
            uint32_t code = (idx >> (2 * (y * 4 + x))) & 0x3;
            std::memcpy(outRgba + y * stride + x * 4, colors[code], 4);
        }
    }
}
} // namespace

bool EP7TextureDecoder::DecryptMTF(const std::vector<uint8_t>& mtfData, std::vector<uint8_t>& outRaw, int& outFileType) {
    if (mtfData.size() <= TEX_HEADER_SIZE) return false;
    int nVersion = 0;
    int nSize = 0;
    int nFileType = 0;
    std::memcpy(&nVersion, mtfData.data(), 4);
    std::memcpy(&nSize, mtfData.data() + 4, 4);
    std::memcpy(&nFileType, mtfData.data() + 8, 4);

    if (nSize <= 0 || static_cast<size_t>(nSize) > mtfData.size() - TEX_HEADER_SIZE) {
        return false;
    }

    outFileType = nFileType;
    outRaw.resize(static_cast<size_t>(nSize));
    for (int i = 0; i < nSize; ++i) {
        uint8_t b = mtfData[TEX_HEADER_SIZE + static_cast<size_t>(i)];
        b = static_cast<uint8_t>(b + TEX_DIFF_DATA);
        b = static_cast<uint8_t>(b ^ TEX_XOR_DATA);
        outRaw[static_cast<size_t>(i)] = b;
    }
    return true;
}

DecodedTexture EP7TextureDecoder::DecodeDDS(const std::vector<uint8_t>& ddsData) {
    DecodedTexture tex;
    if (ddsData.size() < 128) return tex;
    if (std::memcmp(ddsData.data(), "DDS ", 4) != 0) return tex;

    uint32_t height = ReadU32(ddsData.data() + 12);
    uint32_t width = ReadU32(ddsData.data() + 16);
    uint32_t fourCC = ReadU32(ddsData.data() + 84);
    if (width == 0 || height == 0 || width > 4096 || height > 4096) return tex;

    tex.width = width;
    tex.height = height;
    tex.rgba8.assign(static_cast<size_t>(width) * height * 4, 255);

    const uint8_t* src = ddsData.data() + 128;
    size_t srcAvail = ddsData.size() - 128;

    if (fourCC == 0x31545844) { // 'DXT1'
        uint32_t bw = (width + 3) / 4;
        uint32_t bh = (height + 3) / 4;
        if (srcAvail < static_cast<size_t>(bw) * bh * 8) return tex;
        for (uint32_t by = 0; by < bh; ++by) {
            for (uint32_t bx = 0; bx < bw; ++bx) {
                uint8_t blockRgba[4 * 4 * 4]{};
                DecodeDXT1Block(src + (by * bw + bx) * 8, blockRgba, 16);
                for (uint32_t y = 0; y < 4 && (by * 4 + y) < height; ++y) {
                    for (uint32_t x = 0; x < 4 && (bx * 4 + x) < width; ++x) {
                        size_t dstIdx = ((by * 4 + y) * width + (bx * 4 + x)) * 4;
                        std::memcpy(&tex.rgba8[dstIdx], &blockRgba[(y * 4 + x) * 4], 4);
                    }
                }
            }
        }
        tex.valid = true;
    }
    return tex;
}

} // namespace RANMobile
