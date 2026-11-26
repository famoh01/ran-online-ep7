#include "EP7TextureDecoder.h"
#include "EP7HashVerify.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace RANMobile {

namespace {

constexpr uint32_t kDDSD_MIPMAPCOUNT = 0x00020000;
constexpr uint32_t kDDPF_ALPHAPIXELS = 0x00000001;
constexpr uint32_t kDDPF_FOURCC = 0x00000004;
constexpr uint32_t kFCC_DXT1 = 0x31545844;
constexpr uint32_t kFCC_DXT3 = 0x33545844;
constexpr uint32_t kFCC_DXT5 = 0x35545844;

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

inline uint16_t ReadU16BE(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

struct MaskChannel {
    uint32_t mask = 0;
    uint32_t shift = 0;
    uint32_t max = 0;
};

MaskChannel DescribeMask(uint32_t mask) {
    MaskChannel ch;
    ch.mask = mask;
    if (mask == 0) return ch;
    while ((mask & 1u) == 0u) {
        mask >>= 1;
        ++ch.shift;
    }
    ch.max = mask;
    return ch;
}

uint8_t ReadChannel(uint32_t pixel, const MaskChannel& ch) {
    if (ch.mask == 0) return 255;
    uint32_t value = (pixel & ch.mask) >> ch.shift;
    if (ch.max == 0) return 0;
    return static_cast<uint8_t>((value * 255u + ch.max / 2u) / ch.max);
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
    for (int i = 0; i < 6; ++i) bits |= (static_cast<uint64_t>(block[2 + i]) << (8 * i));
    for (uint32_t y = 0; y < 4; ++y) {
        for (uint32_t x = 0; x < 4; ++x) {
            uint32_t code = static_cast<uint32_t>((bits >> (3 * (y * 4 + x))) & 0x7);
            outRgba[y * stride + x * 4 + 3] = alphas[code];
        }
    }
}

bool ContainsTokenCI(const std::string& lowerName, const char* token) {
    return lowerName.find(token) != std::string::npos;
}

void DecodeTgaPixel(const uint8_t* p, uint8_t bpp, bool grayscale, bool hasAlpha16, uint8_t out[4]) {
    if (grayscale && bpp == 8) {
        out[0] = out[1] = out[2] = p[0];
        out[3] = 255;
    } else if (bpp == 16) {
        const uint16_t v = ReadU16(p);
        out[0] = static_cast<uint8_t>(((v >> 10) & 0x1F) * 255 / 31);
        out[1] = static_cast<uint8_t>(((v >> 5) & 0x1F) * 255 / 31);
        out[2] = static_cast<uint8_t>((v & 0x1F) * 255 / 31);
        out[3] = (!hasAlpha16 || (v & 0x8000u)) ? 255 : 0;
    } else if (bpp == 24) {
        out[0] = p[2];
        out[1] = p[1];
        out[2] = p[0];
        out[3] = 255;
    } else { // 32-bpp BGRA
        out[0] = p[2];
        out[1] = p[1];
        out[2] = p[0];
        out[3] = p[3];
    }
}

// Baseline JPEG decoder helpers (ITU-T T.81 SOF0 8-bit)
struct JpegHuffTable {
    bool defined = false;
    std::array<uint8_t, 16> counts{};
    std::vector<uint8_t> symbols;
    std::array<int, 16> minCode{};
    std::array<int, 16> maxCode{};
    std::array<int, 16> valPtr{};
};

void BuildHuffTable(JpegHuffTable& h) {
    int code = 0;
    int idx = 0;
    for (int i = 0; i < 16; ++i) {
        if (h.counts[i] == 0) {
            h.maxCode[i] = -1;
            h.valPtr[i] = 0;
        } else {
            h.valPtr[i] = idx;
            h.minCode[i] = code;
            idx += h.counts[i];
            code += h.counts[i];
            h.maxCode[i] = code - 1;
        }
        code <<= 1;
    }
}

struct JpegBitReader {
    const uint8_t* data = nullptr;
    size_t size = 0;
    size_t pos = 0;
    uint32_t bitBuf = 0;
    int bitCount = 0;
    bool markerHit = false;

    bool EnsureBits(int n) {
        while (bitCount < n) {
            if (pos >= size) return false;
            uint8_t b = data[pos++];
            if (b == 0xFF) {
                if (pos >= size) return false;
                uint8_t next = data[pos++];
                if (next == 0x00) {
                    // Byte-stuffed 0xFF
                } else if (next >= 0xD0 && next <= 0xD7) {
                    // Restart marker shouldn't happen mid-MCU unless byte-aligned
                    bitBuf = 0;
                    bitCount = 0;
                    markerHit = true;
                    return false;
                } else {
                    pos -= 2;
                    markerHit = true;
                    return false;
                }
            }
            bitBuf = (bitBuf << 8) | b;
            bitCount += 8;
        }
        return true;
    }

    int ReadBit() {
        if (!EnsureBits(1)) return -1;
        int bit = static_cast<int>((bitBuf >> (bitCount - 1)) & 1u);
        --bitCount;
        return bit;
    }

    int ReadBits(int n) {
        if (n == 0) return 0;
        if (!EnsureBits(n)) return -1;
        int val = static_cast<int>((bitBuf >> (bitCount - n)) & ((1u << n) - 1u));
        bitCount -= n;
        return val;
    }

    void ResetForRestart() {
        bitBuf = 0;
        bitCount = 0;
        markerHit = false;
    }
};

int DecodeHuffSymbol(JpegBitReader& br, const JpegHuffTable& h) {
    int code = 0;
    for (int len = 0; len < 16; ++len) {
        int bit = br.ReadBit();
        if (bit < 0) return -1;
        code = (code << 1) | bit;
        if (h.maxCode[len] >= 0 && code <= h.maxCode[len]) {
            const size_t symIdx = static_cast<size_t>(h.valPtr[len] + (code - h.minCode[len]));
            if (symIdx >= h.symbols.size()) return -1;
            return h.symbols[symIdx];
        }
    }
    return -1;
}

int ExtendSign(int val, int bits) {
    if (bits <= 0) return 0;
    const int vt = 1 << (bits - 1);
    return (val < vt) ? (val - ((1 << bits) - 1)) : val;
}

uint8_t ClampByte(float v) {
    if (v <= 0.0f) return 0;
    if (v >= 255.0f) return 255;
    return static_cast<uint8_t>(v + 0.5f);
}

void InverseDCT8x8(const int32_t coeffs[64], uint8_t out[64]) {
    static const double kCos[8][8] = {
        {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
        {0.98078528, 0.83146961, 0.55557023, 0.19509032, -0.19509032, -0.55557023, -0.83146961, -0.98078528},
        {0.92387953, 0.38268343, -0.38268343, -0.92387953, -0.92387953, -0.38268343, 0.38268343, 0.92387953},
        {0.83146961, -0.19509032, -0.98078528, -0.55557023, 0.55557023, 0.98078528, 0.19509032, -0.83146961},
        {0.70710678, -0.70710678, -0.70710678, 0.70710678, 0.70710678, -0.70710678, -0.70710678, 0.70710678},
        {0.55557023, -0.98078528, 0.19509032, 0.83146961, -0.83146961, -0.19509032, 0.98078528, -0.55557023},
        {0.38268343, -0.92387953, 0.92387953, -0.38268343, -0.38268343, 0.92387953, -0.92387953, 0.38268343},
        {0.19509032, -0.55557023, 0.83146961, -0.98078528, 0.98078528, -0.83146961, 0.55557023, -0.19509032}
    };
    static const double kScale[8] = {
        0.7071067811865475, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0
    };
    double tmp[64]{};
    for (int y = 0; y < 8; ++y) {
        for (int u = 0; u < 8; ++u) {
            const double c = static_cast<double>(coeffs[u * 8 + y]) * kScale[u];
            if (c == 0.0) continue;
            for (int x = 0; x < 8; ++x) {
                tmp[x * 8 + y] += c * kCos[u][x];
            }
        }
    }
    for (int x = 0; x < 8; ++x) {
        for (int y = 0; y < 8; ++y) {
            double s = 0.0;
            for (int v = 0; v < 8; ++v) {
                s += tmp[x * 8 + v] * kScale[v] * kCos[v][y];
            }
            const double val = s * 0.25 + 128.0;
            out[x * 8 + y] = val <= 0.0 ? 0 : (val >= 255.0 ? 255 : static_cast<uint8_t>(val + 0.5));
        }
    }
}

constexpr uint8_t kJpegZigZag[64] = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

} // namespace

bool EP7TextureDecoder::DecryptMTF(const std::vector<uint8_t>& mtfData,
                                   std::vector<uint8_t>& outRaw, int& outFileType) {
    if (mtfData.size() <= TEX_HEADER_SIZE) return false;
    int nVersion = 0;
    int nSize = 0;
    int nFileType = 0;
    std::memcpy(&nVersion, mtfData.data(), 4);
    std::memcpy(&nSize, mtfData.data() + 4, 4);
    std::memcpy(&nFileType, mtfData.data() + 8, 4);

    if (nVersion != TEX_VERSION) return false;
    if (nSize <= 0 || mtfData.size() != static_cast<size_t>(nSize) + TEX_HEADER_SIZE) return false;

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

DDSHeaderInfo EP7TextureDecoder::InspectDDS(const std::vector<uint8_t>& data) {
    DDSHeaderInfo info;
    if (data.size() < 128 || std::memcmp(data.data(), "DDS ", 4) != 0) {
        info.error = "NOT A DDS FILE";
        return info;
    }
    info.flags = ReadU32(data.data() + 8);
    info.height = ReadU32(data.data() + 12);
    info.width = ReadU32(data.data() + 16);
    info.depth = ReadU32(data.data() + 24);
    info.mipMapCount = (info.flags & kDDSD_MIPMAPCOUNT) ? ReadU32(data.data() + 28) : 1;
    if (info.width == 0 || info.height == 0 || info.width > 4096 || info.height > 4096) {
        info.error = "INVALID DIMENSIONS";
        return info;
    }
    if (info.depth > 1) {
        info.error = "VOLUME TEXTURES UNSUPPORTED";
        return info;
    }
    const uint32_t pfSize = ReadU32(data.data() + 76);
    if (pfSize != 32) {
        info.error = "INVALID PIXEL FORMAT SIZE";
        return info;
    }
    info.pixelFlags = ReadU32(data.data() + 80);
    info.fourCC = ReadU32(data.data() + 84);
    const uint32_t bpp = ReadU32(data.data() + 88);
    const uint32_t alphaMask = ReadU32(data.data() + 104);
    info.hasAlpha = (info.pixelFlags & kDDPF_ALPHAPIXELS) != 0 && alphaMask != 0;

    if (info.pixelFlags & kDDPF_FOURCC) {
        if (info.fourCC == kFCC_DXT1) info.format = EP7TextureFormat::DXT1;
        else if (info.fourCC == kFCC_DXT3) info.format = EP7TextureFormat::DXT3;
        else if (info.fourCC == kFCC_DXT5) info.format = EP7TextureFormat::DXT5;
        else {
            info.error = "UNSUPPORTED FOURCC";
            return info;
        }
    } else if (bpp == 32 && alphaMask == 0xFF000000 && ReadU32(data.data() + 92) == 0xFF0000) {
        info.format = EP7TextureFormat::A8R8G8B8;
    } else if (bpp == 32 && alphaMask == 0) {
        info.format = EP7TextureFormat::X8R8G8B8;
    } else if (bpp == 24) {
        info.format = EP7TextureFormat::R8G8B8;
    } else if (bpp == 16 && ReadU32(data.data() + 92) == 0xF800 && ReadU32(data.data() + 96) == 0x07E0) {
        info.format = EP7TextureFormat::R5G6B5;
    } else if (bpp == 16 && alphaMask == 0x8000) {
        info.format = EP7TextureFormat::A1R5G5B5;
    } else if (bpp == 16 && alphaMask == 0xF000) {
        info.format = EP7TextureFormat::A4R4G4B4;
    } else {
        info.error = "UNSUPPORTED UNCOMPRESSED FORMAT";
        return info;
    }
    info.valid = true;
    return info;
}

EP7AlphaMode EP7TextureDecoder::AlphaModeFromName(const std::string& name,
                                                   EP7TextureFormat format,
                                                   bool pixelAlpha) {
    const std::string lower = EP7HashVerify::LowerAscii(name);
    const bool a = ContainsTokenCI(lower, "_a.");
    const bool a1 = ContainsTokenCI(lower, "_a1.");
    const bool a2 = ContainsTokenCI(lower, "_a2.");
    if (format == EP7TextureFormat::DXT5) {
        if (a1) return EP7AlphaMode::Soft01;
        if (a2) return EP7AlphaMode::Soft02;
        return EP7AlphaMode::Soft;
    }
    if (format == EP7TextureFormat::DXT1) {
        return (a || a1 || a2) ? EP7AlphaMode::Hard : EP7AlphaMode::None;
    }
    if (format == EP7TextureFormat::DXT3 || format == EP7TextureFormat::A8R8G8B8 ||
        format == EP7TextureFormat::A1R5G5B5 || format == EP7TextureFormat::A4R4G4B4) {
        return EP7AlphaMode::Hard;
    }
    if (format == EP7TextureFormat::TGA && pixelAlpha) {
        if (a1) return EP7AlphaMode::Soft01;
        if (a2) return EP7AlphaMode::Soft02;
        return a ? EP7AlphaMode::Hard : EP7AlphaMode::Soft;
    }
    if (pixelAlpha) return EP7AlphaMode::Hard;
    return EP7AlphaMode::None;
}

DecodedTexture EP7TextureDecoder::DecodeDDS(const std::vector<uint8_t>& ddsData,
                                             const std::string& sourceName) {
    DecodedTexture tex;
    tex.header = InspectDDS(ddsData);
    if (!tex.header.valid) {
        tex.error = tex.header.error;
        return tex;
    }
    tex.width = tex.header.width;
    tex.height = tex.header.height;
    tex.alphaMode = AlphaModeFromName(sourceName, tex.header.format, tex.header.hasAlpha);
    tex.rgba8.assign(static_cast<size_t>(tex.width) * tex.height * 4, 255);

    const uint8_t* src = ddsData.data() + 128;
    const size_t avail = ddsData.size() - 128;
    const bool compressed = tex.header.format == EP7TextureFormat::DXT1 ||
                            tex.header.format == EP7TextureFormat::DXT3 ||
                            tex.header.format == EP7TextureFormat::DXT5;
    const uint32_t bppBits = ReadU32(ddsData.data() + 88);
    const size_t bytesPerPixel = bppBits / 8;
    const size_t neededLevel0 = compressed
        ? static_cast<size_t>((tex.width + 3) / 4) * ((tex.height + 3) / 4) *
          (tex.header.format == EP7TextureFormat::DXT1 ? 8 : 16)
        : static_cast<size_t>(tex.width) * tex.height * bytesPerPixel;
    if (avail < neededLevel0) {
        tex.error = "TRUNCATED LEVEL 0 DATA";
        tex.rgba8.clear();
        return tex;
    }

    if (compressed) {
        const uint32_t bw = (tex.width + 3) / 4;
        const uint32_t bh = (tex.height + 3) / 4;
        const size_t blockSize = tex.header.format == EP7TextureFormat::DXT1 ? 8 : 16;
        for (uint32_t by = 0; by < bh; ++by) {
            for (uint32_t bx = 0; bx < bw; ++bx) {
                uint8_t blockRgba[4 * 4 * 4]{};
                const uint8_t* blk = src + (by * bw + bx) * blockSize;
                if (tex.header.format == EP7TextureFormat::DXT1) DecodeDXT1Block(blk, blockRgba, 16);
                else if (tex.header.format == EP7TextureFormat::DXT3) DecodeDXT3Block(blk, blockRgba, 16);
                else DecodeDXT5Block(blk, blockRgba, 16);
                for (uint32_t y = 0; y < 4 && (by * 4 + y) < tex.height; ++y) {
                    for (uint32_t x = 0; x < 4 && (bx * 4 + x) < tex.width; ++x) {
                        const size_t dst = ((by * 4 + y) * tex.width + (bx * 4 + x)) * 4;
                        std::memcpy(&tex.rgba8[dst], &blockRgba[(y * 4 + x) * 4], 4);
                    }
                }
            }
        }
    } else {
        const MaskChannel r = DescribeMask(ReadU32(ddsData.data() + 92));
        const MaskChannel g = DescribeMask(ReadU32(ddsData.data() + 96));
        const MaskChannel b = DescribeMask(ReadU32(ddsData.data() + 100));
        const MaskChannel a = DescribeMask(ReadU32(ddsData.data() + 104));
        for (uint32_t y = 0; y < tex.height; ++y) {
            for (uint32_t x = 0; x < tex.width; ++x) {
                const size_t off = (static_cast<size_t>(y) * tex.width + x) * bytesPerPixel;
                uint32_t pixel = 0;
                std::memcpy(&pixel, src + off, bytesPerPixel);
                uint8_t* dst = &tex.rgba8[(static_cast<size_t>(y) * tex.width + x) * 4];
                dst[0] = ReadChannel(pixel, r);
                dst[1] = ReadChannel(pixel, g);
                dst[2] = ReadChannel(pixel, b);
                dst[3] = a.mask ? ReadChannel(pixel, a) : 255;
            }
        }
    }
    tex.valid = true;
    return tex;
}

DecodedTexture EP7TextureDecoder::DecodeTGA(const std::vector<uint8_t>& tgaData,
                                            const std::string& sourceName) {
    DecodedTexture tex;
    if (tgaData.size() < 18) {
        tex.error = "TRUNCATED TGA HEADER";
        return tex;
    }

    const uint8_t idLen = tgaData[0];
    const uint8_t colorMapType = tgaData[1];
    const uint8_t imageType = tgaData[2];
    const uint16_t colorMapLen = ReadU16(tgaData.data() + 5);
    const uint8_t colorMapEntryBits = tgaData[7];
    const uint16_t width = ReadU16(tgaData.data() + 12);
    const uint16_t height = ReadU16(tgaData.data() + 14);
    const uint8_t bpp = tgaData[16];
    const uint8_t descriptor = tgaData[17];

    const bool isRgb = (imageType == 2 || imageType == 10);
    const bool isGray = (imageType == 3 || imageType == 11);
    const bool isRle = (imageType == 10 || imageType == 11);
    if ((!isRgb && !isGray) || colorMapType > 1) {
        tex.error = "UNSUPPORTED TGA IMAGE TYPE";
        return tex;
    }
    if (width == 0 || height == 0 || width > 4096 || height > 4096) {
        tex.error = "INVALID TGA DIMENSIONS";
        return tex;
    }
    if ((isGray && bpp != 8) || (isRgb && bpp != 16 && bpp != 24 && bpp != 32)) {
        tex.error = "UNSUPPORTED TGA BIT DEPTH";
        return tex;
    }

    const size_t colorMapBytes = colorMapType == 1
        ? static_cast<size_t>(colorMapLen) * ((colorMapEntryBits + 7u) / 8u)
        : 0u;
    size_t pos = 18u + idLen + colorMapBytes;
    if (pos > tgaData.size()) {
        tex.error = "TRUNCATED TGA METADATA";
        return tex;
    }

    const size_t bytesPerPixel = bpp / 8u;
    const size_t pixelCount = static_cast<size_t>(width) * height;
    std::vector<uint8_t> topDown(pixelCount * 4u, 255);
    const bool topToBottom = (descriptor & 0x20u) != 0u;
    const bool hasAlpha16 = (bpp == 16) && ((descriptor & 0x0Fu) != 0u);

    auto writePixel = [&](size_t idx, const uint8_t rgba[4]) {
        const size_t x = idx % width;
        const size_t yRaw = idx / width;
        const size_t y = topToBottom ? yRaw : (static_cast<size_t>(height) - 1u - yRaw);
        std::memcpy(&topDown[(y * width + x) * 4u], rgba, 4);
    };

    if (!isRle) {
        const size_t needed = pixelCount * bytesPerPixel;
        if (tgaData.size() - pos < needed) {
            tex.error = "TRUNCATED UNCOMPRESSED TGA PIXELS";
            return tex;
        }
        for (size_t i = 0; i < pixelCount; ++i) {
            uint8_t rgba[4]{};
            DecodeTgaPixel(&tgaData[pos + i * bytesPerPixel], bpp, isGray, hasAlpha16, rgba);
            writePixel(i, rgba);
        }
    } else {
        size_t i = 0;
        while (i < pixelCount) {
            if (pos >= tgaData.size()) {
                tex.error = "TRUNCATED RLE TGA STREAM";
                return tex;
            }
            const uint8_t packet = tgaData[pos++];
            const size_t count = static_cast<size_t>(packet & 0x7Fu) + 1u;
            if (i + count > pixelCount) {
                tex.error = "CORRUPT RLE TGA PACKET COUNT";
                return tex;
            }
            if (packet & 0x80u) {
                if (tgaData.size() - pos < bytesPerPixel) {
                    tex.error = "TRUNCATED RLE TGA RUN";
                    return tex;
                }
                uint8_t rgba[4]{};
                DecodeTgaPixel(&tgaData[pos], bpp, isGray, hasAlpha16, rgba);
                pos += bytesPerPixel;
                for (size_t k = 0; k < count; ++k) writePixel(i++, rgba);
            } else {
                const size_t rawBytes = count * bytesPerPixel;
                if (tgaData.size() - pos < rawBytes) {
                    tex.error = "TRUNCATED RLE TGA LITERAL";
                    return tex;
                }
                for (size_t k = 0; k < count; ++k) {
                    uint8_t rgba[4]{};
                    DecodeTgaPixel(&tgaData[pos + k * bytesPerPixel], bpp, isGray, hasAlpha16, rgba);
                    writePixel(i++, rgba);
                }
                pos += rawBytes;
            }
        }
    }

    tex.width = width;
    tex.height = height;
    tex.rgba8 = std::move(topDown);
    tex.header.valid = true;
    tex.header.width = width;
    tex.header.height = height;
    tex.header.format = EP7TextureFormat::TGA;
    tex.header.hasAlpha = (bpp == 32) || hasAlpha16;
    tex.alphaMode = AlphaModeFromName(sourceName, EP7TextureFormat::TGA, tex.header.hasAlpha);
    tex.valid = true;
    return tex;
}

DecodedTexture EP7TextureDecoder::DecodeJPEG(const std::vector<uint8_t>& jpgData,
                                             const std::string& sourceName) {
    DecodedTexture tex;
    (void)sourceName;
    if (jpgData.size() < 4 || jpgData[0] != 0xFF || jpgData[1] != 0xD8) {
        tex.error = "NOT A JPEG FILE";
        return tex;
    }

    std::array<std::array<uint16_t, 64>, 4> qtables{};
    std::array<bool, 4> qtableDefined{};
    std::array<JpegHuffTable, 4> dcTables{};
    std::array<JpegHuffTable, 4> acTables{};

    struct ComponentInfo {
        uint8_t id = 0;
        uint8_t hSamp = 1;
        uint8_t vSamp = 1;
        uint8_t qId = 0;
        uint8_t dcId = 0;
        uint8_t acId = 0;
        int32_t dcPred = 0;
    };
    std::vector<ComponentInfo> comps;
    uint16_t width = 0;
    uint16_t height = 0;
    uint16_t restartInterval = 0;
    bool sofSeen = false;

    size_t pos = 2;
    while (pos < jpgData.size()) {
        if (jpgData[pos] != 0xFF) {
            ++pos;
            continue;
        }
        while (pos < jpgData.size() && jpgData[pos] == 0xFF) ++pos;
        if (pos >= jpgData.size()) break;
        const uint8_t marker = jpgData[pos++];
        if (marker == 0x00 || marker == 0xD8 || (marker >= 0xD0 && marker <= 0xD7)) continue;
        if (marker == 0xD9) break; // EOI

        if (pos + 2 > jpgData.size()) {
            tex.error = "TRUNCATED JPEG MARKER LENGTH";
            return tex;
        }
        const uint16_t segLen = ReadU16BE(&jpgData[pos]);
        if (segLen < 2 || pos + segLen > jpgData.size()) {
            tex.error = "INVALID JPEG SEGMENT LENGTH";
            return tex;
        }
        const uint8_t* seg = &jpgData[pos + 2];
        const size_t segDataLen = segLen - 2u;
        pos += segLen;

        if (marker == 0xDB) { // DQT
            size_t p = 0;
            while (p < segDataLen) {
                const uint8_t info = seg[p++];
                const uint8_t prec = info >> 4;
                const uint8_t id = info & 0x0Fu;
                if (id >= 4 || prec > 1) {
                    tex.error = "UNSUPPORTED JPEG DQT";
                    return tex;
                }
                const size_t entryBytes = prec ? 128u : 64u;
                if (p + entryBytes > segDataLen) {
                    tex.error = "TRUNCATED JPEG DQT";
                    return tex;
                }
                for (int k = 0; k < 64; ++k) {
                    qtables[id][kJpegZigZag[k]] = prec
                        ? ReadU16BE(&seg[p + static_cast<size_t>(k) * 2u])
                        : seg[p + static_cast<size_t>(k)];
                }
                qtableDefined[id] = true;
                p += entryBytes;
            }
        } else if (marker == 0xC4) { // DHT
            size_t p = 0;
            while (p < segDataLen) {
                if (p + 17 > segDataLen) {
                    tex.error = "TRUNCATED JPEG DHT";
                    return tex;
                }
                const uint8_t tcTh = seg[p++];
                const uint8_t tc = tcTh >> 4;
                const uint8_t th = tcTh & 0x0Fu;
                if (tc > 1 || th >= 4) {
                    tex.error = "INVALID JPEG DHT INDEX";
                    return tex;
                }
                JpegHuffTable& h = (tc == 0) ? dcTables[th] : acTables[th];
                size_t totalSyms = 0;
                for (int i = 0; i < 16; ++i) {
                    h.counts[i] = seg[p + static_cast<size_t>(i)];
                    totalSyms += h.counts[i];
                }
                p += 16;
                if (p + totalSyms > segDataLen) {
                    tex.error = "TRUNCATED JPEG DHT SYMBOLS";
                    return tex;
                }
                h.symbols.assign(seg + p, seg + p + totalSyms);
                BuildHuffTable(h);
                h.defined = true;
                p += totalSyms;
            }
        } else if (marker == 0xDD) { // DRI
            if (segDataLen < 2) {
                tex.error = "TRUNCATED JPEG DRI";
                return tex;
            }
            restartInterval = ReadU16BE(seg);
        } else if (marker == 0xC0) { // SOF0 Baseline
            if (segDataLen < 6) {
                tex.error = "TRUNCATED JPEG SOF0";
                return tex;
            }
            const uint8_t precision = seg[0];
            height = ReadU16BE(&seg[1]);
            width = ReadU16BE(&seg[3]);
            const uint8_t nComp = seg[5];
            if (precision != 8 || (nComp != 1 && nComp != 3) ||
                width == 0 || height == 0 || width > 4096 || height > 4096 ||
                segDataLen < 6u + static_cast<size_t>(nComp) * 3u) {
                tex.error = "UNSUPPORTED JPEG SOF0 PARAMETERS";
                return tex;
            }
            comps.resize(nComp);
            for (uint8_t c = 0; c < nComp; ++c) {
                comps[c].id = seg[6u + c * 3u];
                const uint8_t hv = seg[7u + c * 3u];
                comps[c].hSamp = hv >> 4;
                comps[c].vSamp = hv & 0x0Fu;
                comps[c].qId = seg[8u + c * 3u];
                if (comps[c].hSamp == 0 || comps[c].vSamp == 0 ||
                    comps[c].hSamp > 2 || comps[c].vSamp > 2 ||
                    comps[c].qId >= 4 || !qtableDefined[comps[c].qId]) {
                    tex.error = "UNSUPPORTED JPEG SAMPLING OR DQT";
                    return tex;
                }
            }
            sofSeen = true;
        } else if (marker == 0xC2 || marker == 0xC1) {
            tex.error = "NON-BASELINE JPEG (PROGRESSIVE/EXTENDED) UNSUPPORTED";
            return tex;
        } else if (marker == 0xDA) { // SOS
            if (!sofSeen || segDataLen < 1u + comps.size() * 2u + 3u) {
                tex.error = "INVALID JPEG SOS HEADER";
                return tex;
            }
            const uint8_t scanComps = seg[0];
            if (scanComps != comps.size()) {
                tex.error = "NON-INTERLEAVED JPEG SCAN UNSUPPORTED";
                return tex;
            }
            for (size_t i = 0; i < scanComps; ++i) {
                const uint8_t cid = seg[1u + i * 2u];
                const uint8_t tdTa = seg[2u + i * 2u];
                bool matched = false;
                for (auto& comp : comps) {
                    if (comp.id == cid) {
                        comp.dcId = tdTa >> 4;
                        comp.acId = tdTa & 0x0Fu;
                        if (comp.dcId >= 4 || comp.acId >= 4 ||
                            !dcTables[comp.dcId].defined || !acTables[comp.acId].defined) {
                            tex.error = "MISSING JPEG HUFFMAN TABLE";
                            return tex;
                        }
                        matched = true;
                        break;
                    }
                }
                if (!matched) {
                    tex.error = "UNKNOWN JPEG SCAN COMPONENT";
                    return tex;
                }
            }

            uint8_t maxH = 1, maxV = 1;
            for (const auto& c : comps) {
                maxH = std::max(maxH, c.hSamp);
                maxV = std::max(maxV, c.vSamp);
            }
            const uint32_t mcuW = maxH * 8u;
            const uint32_t mcuH = maxV * 8u;
            const uint32_t mcuCols = (width + mcuW - 1u) / mcuW;
            const uint32_t mcuRows = (height + mcuH - 1u) / mcuH;

            std::vector<std::vector<uint8_t>> planes(comps.size());
            const uint32_t bufW = mcuCols * mcuW;
            const uint32_t bufH = mcuRows * mcuH;
            for (size_t c = 0; c < comps.size(); ++c) {
                planes[c].assign(static_cast<size_t>(bufW) * bufH, 128);
            }

            JpegBitReader br;
            br.data = jpgData.data();
            br.size = jpgData.size();
            br.pos = pos;

            uint32_t mcuCounter = 0;
            for (uint32_t my = 0; my < mcuRows; ++my) {
                for (uint32_t mx = 0; mx < mcuCols; ++mx) {
                    if (restartInterval > 0 && mcuCounter > 0 && (mcuCounter % restartInterval) == 0) {
                        br.ResetForRestart();
                        if (br.pos + 2 <= br.size && br.data[br.pos] == 0xFF &&
                            br.data[br.pos + 1] >= 0xD0 && br.data[br.pos + 1] <= 0xD7) {
                            br.pos += 2;
                        }
                        for (auto& c : comps) c.dcPred = 0;
                    }
                    for (size_t ci = 0; ci < comps.size(); ++ci) {
                        auto& comp = comps[ci];
                        const auto& qt = qtables[comp.qId];
                        const auto& dcT = dcTables[comp.dcId];
                        const auto& acT = acTables[comp.acId];
                        const int scaleX = maxH / comp.hSamp;
                        const int scaleY = maxV / comp.vSamp;
                        for (uint8_t vy = 0; vy < comp.vSamp; ++vy) {
                            for (uint8_t hx = 0; hx < comp.hSamp; ++hx) {
                                int32_t block[64]{};
                                const int dcLen = DecodeHuffSymbol(br, dcT);
                                if (dcLen < 0 || dcLen > 11) {
                                    tex.error = "JPEG DC DECODE ERROR";
                                    return tex;
                                }
                                int dcDiff = br.ReadBits(dcLen);
                                if (dcDiff < 0) {
                                    tex.error = "TRUNCATED JPEG DC BITS";
                                    return tex;
                                }
                                comp.dcPred += ExtendSign(dcDiff, dcLen);
                                block[0] = comp.dcPred * static_cast<int32_t>(qt[0]);

                                int k = 1;
                                while (k < 64) {
                                    const int rs = DecodeHuffSymbol(br, acT);
                                    if (rs < 0) {
                                        tex.error = "JPEG AC DECODE ERROR";
                                        return tex;
                                    }
                                    if (rs == 0) break; // EOB
                                    const int run = rs >> 4;
                                    const int size = rs & 0x0F;
                                    k += run;
                                    if (size == 0) {
                                        if (run == 15) {
                                            ++k;
                                            continue;
                                        }
                                        break;
                                    }
                                    if (k >= 64) {
                                        tex.error = "JPEG AC INDEX OVERFLOW";
                                        return tex;
                                    }
                                    const int acBits = br.ReadBits(size);
                                    if (acBits < 0) {
                                        tex.error = "TRUNCATED JPEG AC BITS";
                                        return tex;
                                    }
                                    const uint8_t nat = kJpegZigZag[k];
                                    block[nat] = ExtendSign(acBits, size) * static_cast<int32_t>(qt[nat]);
                                    ++k;
                                }

                                uint8_t pixels[64]{};
                                InverseDCT8x8(block, pixels);
                                const uint32_t baseX = mx * mcuW + hx * 8u * static_cast<uint32_t>(scaleX);
                                const uint32_t baseY = my * mcuH + vy * 8u * static_cast<uint32_t>(scaleY);
                                for (uint32_t py = 0; py < 8; ++py) {
                                    for (uint32_t px = 0; px < 8; ++px) {
                                        const uint8_t v = pixels[py * 8u + px];
                                        for (int sy = 0; sy < scaleY; ++sy) {
                                            for (int sx = 0; sx < scaleX; ++sx) {
                                                const uint32_t ox = baseX + px * static_cast<uint32_t>(scaleX) + static_cast<uint32_t>(sx);
                                                const uint32_t oy = baseY + py * static_cast<uint32_t>(scaleY) + static_cast<uint32_t>(sy);
                                                planes[ci][static_cast<size_t>(oy) * bufW + ox] = v;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    ++mcuCounter;
                }
            }

            tex.width = width;
            tex.height = height;
            tex.rgba8.resize(static_cast<size_t>(width) * height * 4u);
            for (uint32_t y = 0; y < height; ++y) {
                for (uint32_t x = 0; x < width; ++x) {
                    const size_t srcIdx = static_cast<size_t>(y) * bufW + x;
                    uint8_t* dst = &tex.rgba8[(static_cast<size_t>(y) * width + x) * 4u];
                    if (comps.size() == 1) {
                        const uint8_t g = planes[0][srcIdx];
                        dst[0] = dst[1] = dst[2] = g;
                        dst[3] = 255;
                    } else {
                        const float Y = static_cast<float>(planes[0][srcIdx]);
                        const float Cb = static_cast<float>(planes[1][srcIdx]) - 128.0f;
                        const float Cr = static_cast<float>(planes[2][srcIdx]) - 128.0f;
                        dst[0] = ClampByte(Y + 1.402f * Cr);
                        dst[1] = ClampByte(Y - 0.344136f * Cb - 0.714136f * Cr);
                        dst[2] = ClampByte(Y + 1.772f * Cb);
                        dst[3] = 255;
                    }
                }
            }
            tex.header.valid = true;
            tex.header.width = width;
            tex.header.height = height;
            tex.header.format = EP7TextureFormat::JPEG;
            tex.header.hasAlpha = false;
            tex.alphaMode = EP7AlphaMode::None;
            tex.valid = true;
            return tex;
        }
    }

    tex.error = "INCOMPLETE JPEG STREAM (NO SOS)";
    return tex;
}

DecodedTexture EP7TextureDecoder::DecodeTextureFile(const std::vector<uint8_t>& rawData,
                                                    const std::string& sourceName) {
    DecodedTexture out;
    if (rawData.empty()) {
        out.error = "EMPTY TEXTURE DATA";
        return out;
    }

    const std::string lower = EP7HashVerify::LowerAscii(sourceName);
    auto endsWith = [&](const std::string& suffix) {
        return lower.size() >= suffix.size() &&
               lower.compare(lower.size() - suffix.size(), suffix.size(), suffix) == 0;
    };

    const std::vector<uint8_t>* payload = &rawData;
    std::vector<uint8_t> decrypted;
    int mtfType = -1;
    if (endsWith(".mtf")) {
        if (!DecryptMTF(rawData, decrypted, mtfType)) {
            out.error = "FAILED TO DECRYPT MTF TEXTURE";
            return out;
        }
        payload = &decrypted;
    }

    if (payload->size() >= 4 && std::memcmp(payload->data(), "DDS ", 4) == 0) {
        return DecodeDDS(*payload, sourceName);
    }
    if (payload->size() >= 2 && (*payload)[0] == 0xFF && (*payload)[1] == 0xD8) {
        return DecodeJPEG(*payload, sourceName);
    }
    if (endsWith(".dds") || mtfType == 0) {
        return DecodeDDS(*payload, sourceName);
    }
    if (endsWith(".jpg") || endsWith(".jpeg")) {
        return DecodeJPEG(*payload, sourceName);
    }
    if (endsWith(".tga") || mtfType == 1) {
        return DecodeTGA(*payload, sourceName);
    }

    // Fallback sniff for extensionless TGA vs unknown
    if (payload->size() >= 18) {
        DecodedTexture tga = DecodeTGA(*payload, sourceName);
        if (tga.valid) return tga;
    }
    out.error = "UNRECOGNIZED TEXTURE FORMAT: " + sourceName;
    return out;
}

} // namespace RANMobile
