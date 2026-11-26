#include "EP7TextureDecoder.h"
#include <algorithm>
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

bool ContainsToken(const std::string& name, const char* token) {
    return name.find(token) != std::string::npos;
}

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

    // Original DecryptTexture() requires TEX_VERSION and exact file size
    // (enginelib/DxCommon/MemoryTexture.cpp:76-88).
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
    // Mirrors TextureContainer::LoadImageData (enginelib/DxCommon/TextureManager.cpp:246-314).
    const bool a = ContainsToken(name, "_a.");
    const bool a1 = ContainsToken(name, "_a1.");
    const bool a2 = ContainsToken(name, "_a2.");
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
    const size_t neededLevel0 = compressed
        ? static_cast<size_t>((tex.width + 3) / 4) * ((tex.height + 3) / 4) *
          (tex.header.format == EP7TextureFormat::DXT1 ? 8 : 16)
        : static_cast<size_t>(tex.width) * tex.height * 4;
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
        const uint32_t bytesPerPixel = ReadU32(ddsData.data() + 88) / 8;
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

} // namespace RANMobile
