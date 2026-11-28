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

void DecodeDXT3Block(const uint8_t* block, uint8_t* outRgba, uint32_t stride) {
    // 8 bytes explicit 4-bit alpha, followed by 8 bytes DXT1 color
    DecodeDXT1Block(block + 8, outRgba, stride);
    for (uint32_t y = 0; y < 4; ++y) {
        uint16_t rowAlpha = ReadU16(block + y * 2);
        for (uint32_t x = 0; x < 4; ++x) {
            uint8_t a4 = static_cast<uint8_t>((rowAlpha >> (x * 4)) & 0xF);
            outRgba[y * stride + x * 4 + 3] = static_cast<uint8_t>(a4 * 17);
        }
    }
}

void DecodeDXT5Block(const uint8_t* block, uint8_t* outRgba, uint32_t stride) {
    // 8 bytes interpolated alpha, followed by 8 bytes DXT1 color
    DecodeDXT1Block(block + 8, outRgba, stride);
    uint8_t a0 = block[0];
    uint8_t a1 = block[1];
    uint8_t alphas[8]{};
    alphas[0] = a0;
    alphas[1] = a1;
    if (a0 > a1) {
        for (int i = 1; i <= 6; ++i)
            alphas[1 + i] = static_cast<uint8_t>(((7 - i) * a0 + i * a1) / 7);
    } else {
        for (int i = 1; i <= 4; ++i)
            alphas[1 + i] = static_cast<uint8_t>(((5 - i) * a0 + i * a1) / 5);
        alphas[6] = 0;
        alphas[7] = 255;
    }
    uint64_t bits = 0;
    for (int i = 0; i < 6; ++i) {
        bits |= (static_cast<uint64_t>(block[2 + i]) << (8 * i));
    }
    for (uint32_t y = 0; y < 4; ++y) {
        for (uint32_t x = 0; x < 4; ++x) {
            uint32_t code = static_cast<uint32_t>((bits >> (3 * (y * 4 + x))) & 0x7);
            outRgba[y * stride + x * 4 + 3] = alphas[code];
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

    if (fourCC == 0x31545844 || fourCC == 0x33545844 || fourCC == 0x35545844) { // DXT1 / DXT3 / DXT5
        uint32_t bw = (width + 3) / 4;
        uint32_t bh = (height + 3) / 4;
        size_t blockSize = (fourCC == 0x31545844) ? 8 : 16;
        if (srcAvail < static_cast<size_t>(bw) * bh * blockSize) return tex;
        for (uint32_t by = 0; by < bh; ++by) {
            for (uint32_t bx = 0; bx < bw; ++bx) {
                uint8_t blockRgba[4 * 4 * 4]{};
                const uint8_t* blk = src + (by * bw + bx) * blockSize;
                if (fourCC == 0x31545844) DecodeDXT1Block(blk, blockRgba, 16);
                else if (fourCC == 0x33545844) DecodeDXT3Block(blk, blockRgba, 16);
                else DecodeDXT5Block(blk, blockRgba, 16);
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

// ---------------------------------------------------------------------------
// B18: minimal TGA decode for the TEXTURE_TGA branch of the .mtf container
// (MemoryTexture.cpp EncryptTexture writes TEXTURE_DDS / TEXTURE_TGA only).
// Supported subset (the formats EP7-era D3DX-authored TGAs use):
//   * uncompressed type 2 and RLE type 10, 24-bit (BGR) and 32-bit (BGRA),
//   * origin bit handled (bottom-left default flipped to top-left rows),
//   * colour-mapped (type 1) and greyscale (type 3) uncompressed images.
// Anything else (Huffman/V Beyond, 15-bit) returns invalid rather than guessing.
// ---------------------------------------------------------------------------
DecodedTexture EP7TextureDecoder::DecodeTGA(const std::vector<uint8_t>& tgaData) {
    DecodedTexture tex;
    if (tgaData.size() < 18) return tex;

    const uint8_t idLength = tgaData[0];
    const uint8_t cmapType = tgaData[1];
    const uint8_t imageType = tgaData[2];
    const uint16_t cmapFirst = ReadU16(tgaData.data() + 3);
    const uint16_t cmapCount = ReadU16(tgaData.data() + 5);
    const uint8_t cmapBits = tgaData[7];
    const uint16_t width = ReadU16(tgaData.data() + 12);
    const uint16_t height = ReadU16(tgaData.data() + 14);
    const uint8_t bpp = tgaData[16];
    const uint8_t descriptor = tgaData[17];

    const bool topOrigin = (descriptor & 0x20) != 0;

    if (width == 0 || height == 0 || width > 4096 || height > 4096) return tex;
    if (bpp != 24 && bpp != 32 && bpp != 8) return tex;

    const bool rle = (imageType == 10);
    const bool cmapImage = (imageType == 1);
    const bool greyImage = (imageType == 3);
    if (!rle && imageType != 2 && !cmapImage && !greyImage) return tex;
    if (cmapImage && cmapType != 1) return tex;
    if (cmapImage && cmapBits != 24 && cmapBits != 32) return tex;
    if (!cmapImage && cmapType != 0 && cmapType != 1) return tex;  // palette may be present but unused

    size_t pos = 18 + idLength;
    std::vector<uint8_t> cmapRgba;
    if (cmapType == 1) {
        const size_t entryBytes = static_cast<size_t>(cmapBits) / 8;
        if (tgaData.size() - pos < static_cast<size_t>(cmapCount) * entryBytes) return tex;
        cmapRgba.resize(static_cast<size_t>(cmapCount) * 4);
        for (uint32_t i = 0; i < cmapCount; ++i) {
            const uint8_t* e = tgaData.data() + pos + static_cast<size_t>(i) * entryBytes;
            uint8_t* d = &cmapRgba[static_cast<size_t>(i) * 4];
            d[0] = e[2];
            d[1] = e[1];
            d[2] = e[0];
            d[3] = (cmapBits == 32) ? e[3] : 255;
        }
        pos += static_cast<size_t>(cmapCount) * entryBytes;
    }

    tex.width = width;
    tex.height = height;
    tex.rgba8.assign(static_cast<size_t>(width) * height * 4, 255);
    uint8_t* dst = tex.rgba8.data();

    // Row-major writer that honours the origin bit.
    auto pixelAt = [&dst, width, &tex, topOrigin](uint32_t row, uint32_t col) -> uint8_t* {
        const uint32_t y = topOrigin ? row : (tex.height - 1 - row);
        return dst + (static_cast<size_t>(y) * width + col) * 4;
    };

    const size_t pixelBytes = (cmapImage || greyImage) ? 1 : static_cast<size_t>(bpp) / 8;
    auto readPixel = [&](uint8_t* outRgba) -> bool {
        if (pos + pixelBytes > tgaData.size()) return false;
        if (cmapImage) {
            const uint8_t idx = tgaData[pos++];
            if (idx < cmapFirst || static_cast<size_t>(idx) - cmapFirst >= cmapRgba.size() / 4) return false;
            std::memcpy(outRgba, &cmapRgba[(static_cast<size_t>(idx) - cmapFirst) * 4], 4);
        } else if (greyImage) {
            const uint8_t g = tgaData[pos++];
            outRgba[0] = outRgba[1] = outRgba[2] = g;
            outRgba[3] = 255;
        } else {
            const uint8_t b = tgaData[pos + 0];
            const uint8_t g = tgaData[pos + 1];
            const uint8_t r = tgaData[pos + 2];
            const uint8_t a = (bpp == 32) ? tgaData[pos + 3] : 255;
            outRgba[0] = r;
            outRgba[1] = g;
            outRgba[2] = b;
            outRgba[3] = a;
            pos += pixelBytes;
        }
        return true;
    };

    if (!rle) {
        for (uint32_t row = 0; row < height; ++row) {
            for (uint32_t col = 0; col < width; ++col) {
                if (!readPixel(pixelAt(row, col))) return tex;
            }
        }
    } else {
        uint32_t remaining = width * height;
        uint32_t row = 0;
        uint32_t col = 0;
        auto put = [&](const uint8_t* px) {
            std::memcpy(pixelAt(row, col), px, 4);
            if (++col == width) {
                col = 0;
                ++row;
            }
            --remaining;
        };
        uint8_t px[4];
        while (remaining > 0) {
            if (pos >= tgaData.size()) return tex;
            const uint8_t packet = tgaData[pos++];
            const uint8_t count = static_cast<uint8_t>((packet & 0x7F) + 1);
            if (packet & 0x80) {
                if (!readPixel(px)) return tex;
                for (uint8_t i = 0; i < count && remaining > 0; ++i) put(px);
            } else {
                for (uint8_t i = 0; i < count && remaining > 0; ++i) {
                    if (!readPixel(px)) return tex;
                    put(px);
                }
            }
        }
    }

    tex.valid = true;
    return tex;
}

} // namespace RANMobile
