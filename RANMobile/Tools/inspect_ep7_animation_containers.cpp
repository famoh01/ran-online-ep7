//
// inspect_ep7_animation_containers.cpp — Phase 3J B12 read-only forensic inspector
// for authentic EP7 SAnimContainer (.bin) and companion SANIMCONINFO (.cfg) assets.
//
// PURPOSE
// -------
// Reproducibly inspects authentic EP7 animation containers without modifying either
// original asset files or production EP7AnimationReader behavior:
//   1. Computes RFC 6234 SHA-256 digests and exact byte sizes for inspected files,
//      and re-verifies loose sample SHA-256s at completion to prove zero mutation.
//   2. Invokes unmodified RANMobile::EP7AnimationReader::ParseContainer() and
//      reproduces the B11 classification buckets (parsed, unsupported-outer,
//      invalid-header, unrecognized-type, parser-rejected), reporting both
//      data/animation/-scoped counts (matching verify_ep7_character_assets) and
//      tree-wide .bin counts.
//   3. Traces 0x0100..0x0102 track streams step-by-step to pinpoint exact rejection
//      sites (list, track index, byte offset, 32-bit track version word, surrounding
//      bytes, and trailing bytes on accepted files).
//   4. Runs a read-only shadow verification pass implementing the exact container
//      (0x0100..0x0104, 0x0200 + EMBYTECRYPT_BIN) and track (0x0100..0x0103) rules
//      verified in backup-repo Lib_Engine/Meshs/SAnimationSaveLoad.cpp
//      (sha256 e07686104ca1bbbfb431c63c8e79a954b4fb366ef36b048205e2a776d5da644b),
//      measuring exact EOF convergence across all authentic .bin assets without
//      claiming unsupported files as parsed by the production reader.
//

#include "../Data/Importer/EP7Animation.h"
#include "../Data/Importer/EP7ByteFormat.h"
#include "../Data/Importer/EP7RccArchive.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace EP7AnimInspect {

// ---------------------------------------------------------------------------
// Minimal self-contained RFC 6234 SHA-256 (read-only digest computation).
// ---------------------------------------------------------------------------
class Sha256 {
public:
    Sha256() { Reset(); }

    void Reset() {
        m_state[0] = 0x6a09e667u;
        m_state[1] = 0xbb67ae85u;
        m_state[2] = 0x3c6ef372u;
        m_state[3] = 0xa54ff53au;
        m_state[4] = 0x510e527fu;
        m_state[5] = 0x9b05688cu;
        m_state[6] = 0x1f83d9abu;
        m_state[7] = 0x5be0cd19u;
        m_bitLen = 0;
        m_bufLen = 0;
    }

    void Update(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            m_buf[m_bufLen++] = data[i];
            if (m_bufLen == 64) {
                Transform(m_buf.data());
                m_bitLen += 512;
                m_bufLen = 0;
            }
        }
    }

    std::string FinalHex() {
        uint64_t totalBits = m_bitLen + static_cast<uint64_t>(m_bufLen) * 8u;
        m_buf[m_bufLen++] = 0x80u;
        if (m_bufLen > 56) {
            while (m_bufLen < 64) m_buf[m_bufLen++] = 0;
            Transform(m_buf.data());
            m_bufLen = 0;
        }
        while (m_bufLen < 56) m_buf[m_bufLen++] = 0;
        for (int i = 7; i >= 0; --i) {
            m_buf[m_bufLen++] = static_cast<uint8_t>((totalBits >> (i * 8)) & 0xFFu);
        }
        Transform(m_buf.data());

        char hex[65];
        for (int i = 0; i < 8; ++i) {
            std::snprintf(hex + i * 8, 9, "%08x", m_state[i]);
        }
        hex[64] = '\0';
        return std::string(hex);
    }

    static std::string DigestHex(const std::vector<uint8_t>& bytes) {
        Sha256 h;
        if (!bytes.empty()) h.Update(bytes.data(), bytes.size());
        return h.FinalHex();
    }

    static bool SelfTest() {
        const std::vector<uint8_t> empty;
        const std::vector<uint8_t> abc = {'a', 'b', 'c'};
        return DigestHex(empty) ==
                   "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" &&
               DigestHex(abc) ==
                   "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    }

private:
    static uint32_t Ror(uint32_t x, uint32_t n) { return (x >> n) | (x << (32u - n)); }

    void Transform(const uint8_t block[64]) {
        static constexpr uint32_t k[64] = {
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
            0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
            0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
            0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
            0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
            0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
            0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
            0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
            0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
            0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
            0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
                   (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
                   static_cast<uint32_t>(block[i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = Ror(w[i - 15], 7) ^ Ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = Ror(w[i - 2], 17) ^ Ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
        uint32_t e = m_state[4], f = m_state[5], g = m_state[6], h = m_state[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = Ror(e, 6) ^ Ror(e, 11) ^ Ror(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t temp1 = h + S1 + ch + k[i] + w[i];
            uint32_t S0 = Ror(a, 2) ^ Ror(a, 13) ^ Ror(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        m_state[0] += a;
        m_state[1] += b;
        m_state[2] += c;
        m_state[3] += d;
        m_state[4] += e;
        m_state[5] += f;
        m_state[6] += g;
        m_state[7] += h;
    }

    uint32_t m_state[8]{};
    uint64_t m_bitLen = 0;
    std::array<uint8_t, 64> m_buf{};
    size_t m_bufLen = 0;
};

// ---------------------------------------------------------------------------
// Verified read-only ByteCrypt lookup tables from backup repo
// matheomedrana/Ran-Online-EP7-Complete-Files @ f661fbf89612cc77f95a7c9b935ff06b22bc55e3:
//   Lib_Engine/Common/ByteCryptDef.h     sha256 c647aee9...fbda
//   Lib_Engine/Common/ByteCryptDefVer1.h sha256 fe42183b...9120
//   Lib_Engine/Common/ByteCryptDefVer2.h sha256 2a3c6d8c...ef01
// ---------------------------------------------------------------------------
enum class ProbeCipher {
    None = 0,
    CompByte,
    ByteCryptOld,
    ByteCryptRan2,
    ByteCryptBin1,
    ByteCryptBin2,
    ByteCryptContainer1,
    ByteCryptContainer2,
};

inline const char* ProbeCipherName(ProbeCipher c) {
    switch (c) {
        case ProbeCipher::None: return "raw";
        case ProbeCipher::CompByte: return "compbyte";
        case ProbeCipher::ByteCryptOld: return "EMBYTECRYPT_OLD";
        case ProbeCipher::ByteCryptRan2: return "EMBYTECRYPT_RAN2";
        case ProbeCipher::ByteCryptBin1: return "EMBYTECRYPT_BIN";
        case ProbeCipher::ByteCryptBin2: return "EMBYTECRYPT_BIN2";
        case ProbeCipher::ByteCryptContainer1: return "EMBYTECRYPT_CONTAINER";
        case ProbeCipher::ByteCryptContainer2: return "EMBYTECRYPT_CONTAINER2";
    }
    return "unknown";
}

inline const uint8_t* EncodeTableFor(ProbeCipher c) {
    static const uint8_t kOld[256] = {
        0xCD, 0xF0, 0xA2, 0xE2, 0xED, 0x88, 0xE3, 0x98, 0xBC, 0x66, 0xC1, 0x7C, 0xF4, 0xDB, 0x47, 0x96,
        0xF6, 0x6C, 0x5E, 0x11, 0xAF, 0x2F, 0x40, 0x42, 0x41, 0x07, 0xDE, 0x4C, 0x8A, 0x63, 0x4D, 0x51,
        0xC0, 0x9B, 0x38, 0x27, 0x19, 0x03, 0x97, 0x65, 0x3D, 0x44, 0xAC, 0xA7, 0x18, 0xA0, 0x61, 0x13,
        0xB3, 0xB4, 0xC6, 0x21, 0x15, 0xE0, 0xC5, 0x0F, 0x78, 0xC4, 0xEF, 0x2C, 0x53, 0x26, 0x2E, 0x67,
        0x54, 0x5F, 0xD5, 0xC8, 0xAA, 0x17, 0x46, 0x95, 0xA3, 0x94, 0xFB, 0xBA, 0xD3, 0xBD, 0x64, 0x2A,
        0xBF, 0x34, 0x48, 0x35, 0x43, 0xD7, 0xF5, 0xCF, 0x90, 0x92, 0x2D, 0xB5, 0x5D, 0x93, 0x99, 0x50,
        0x74, 0x72, 0x31, 0x04, 0x58, 0x10, 0x5A, 0x7F, 0xFF, 0xCA, 0x55, 0x37, 0xB2, 0xDD, 0xE5, 0x0A,
        0x0D, 0x69, 0xB1, 0x3A, 0x00, 0x3C, 0xEA, 0x22, 0x32, 0x8D, 0xF2, 0x9C, 0x86, 0x1C, 0xB0, 0x76,
        0x30, 0x01, 0xD2, 0x06, 0xBB, 0x77, 0xF3, 0x80, 0xE8, 0xA6, 0x05, 0xEC, 0x89, 0x49, 0xFD, 0xD9,
        0xD6, 0xD4, 0x45, 0x6F, 0x4F, 0xB8, 0x33, 0x57, 0xD8, 0x87, 0xA8, 0x9E, 0xF9, 0x5C, 0x23, 0xB6,
        0x6B, 0xEB, 0x7E, 0x1F, 0x02, 0xFE, 0x85, 0xE9, 0x12, 0xC9, 0xAE, 0x08, 0x9F, 0x52, 0x25, 0x71,
        0x09, 0x3F, 0x29, 0x68, 0x3B, 0x1A, 0xE7, 0x91, 0x59, 0x7A, 0x6E, 0x8E, 0x56, 0xA4, 0x1D, 0x1E,
        0xA9, 0x9A, 0xDF, 0x70, 0x8C, 0x4E, 0x4B, 0xDC, 0xEE, 0x36, 0x8F, 0xC3, 0x83, 0x82, 0xE4, 0x8B,
        0x79, 0x6D, 0x0C, 0xA1, 0x7D, 0x39, 0x4A, 0xBE, 0xFA, 0xAB, 0xD1, 0xC7, 0x28, 0x7B, 0xCC, 0xF7,
        0xB7, 0xAD, 0x62, 0xB9, 0xD0, 0xF8, 0xF1, 0x0E, 0x1B, 0xCB, 0xDA, 0xE6, 0x2B, 0x60, 0x16, 0xC2,
        0x81, 0x0B, 0x73, 0xA5, 0x20, 0x84, 0x5B, 0x24, 0x9D, 0x75, 0xE1, 0xCE, 0x14, 0x6A, 0x3E, 0xFC};

    static const uint8_t kRan2[256] = {
        0x2A, 0x45, 0x55, 0xE7, 0xED, 0x36, 0xD7, 0xB6, 0x81, 0x16, 0x75, 0x64, 0x93, 0x19, 0xD5, 0x6D,
        0x1D, 0x15, 0x49, 0x9D, 0x6A, 0xF0, 0xCC, 0xA3, 0xC0, 0x72, 0x0E, 0xFF, 0x88, 0x4F, 0x77, 0xF8,
        0x73, 0x01, 0xA0, 0x50, 0x2C, 0xBF, 0x28, 0xEC, 0xCF, 0x10, 0xD1, 0xD3, 0xFD, 0xDB, 0x6F, 0x59,
        0x1A, 0x70, 0x26, 0x6E, 0x20, 0x94, 0x76, 0x1F, 0x46, 0x32, 0xB3, 0x1C, 0xE4, 0xBE, 0x31, 0x99,
        0xA7, 0x85, 0xDC, 0xA9, 0xEF, 0xC9, 0x8D, 0x25, 0x08, 0xF3, 0x00, 0x74, 0x95, 0x84, 0x7B, 0xE1,
        0xCD, 0x3C, 0xA1, 0x80, 0x8F, 0x79, 0xB7, 0x68, 0xF1, 0x2F, 0x0A, 0xFA, 0x56, 0xEE, 0xBB, 0x27,
        0xC1, 0x3D, 0x3A, 0x4C, 0x63, 0xD6, 0x3E, 0xA4, 0xD9, 0x43, 0x4D, 0x0D, 0x7E, 0x5E, 0x9C, 0xFB,
        0x83, 0xAB, 0xEB, 0x12, 0xB2, 0x92, 0x8B, 0xAD, 0x39, 0x09, 0xD0, 0x21, 0x35, 0x14, 0x0F, 0xB4,
        0x29, 0x11, 0xCB, 0x78, 0x34, 0x52, 0xC3, 0x65, 0x51, 0xBA, 0xDA, 0xA2, 0x87, 0x96, 0xF5, 0x17,
        0x86, 0x47, 0x2B, 0x89, 0x2E, 0xAF, 0x33, 0x9E, 0xA5, 0xC6, 0xD2, 0xF7, 0xC8, 0x54, 0x66, 0x23,
        0x4A, 0x37, 0xBD, 0x9F, 0xF6, 0x58, 0x57, 0xE9, 0x7F, 0xAC, 0x02, 0x3F, 0x41, 0x61, 0x13, 0x2D,
        0x7C, 0xE5, 0x6B, 0x5B, 0x62, 0x7A, 0x8A, 0x90, 0x04, 0xC5, 0xBC, 0x30, 0x0B, 0x98, 0x97, 0xA6,
        0xD8, 0x40, 0x24, 0x48, 0xE2, 0x07, 0x5F, 0x5D, 0xDE, 0x82, 0xF2, 0x06, 0xF4, 0x7D, 0xF9, 0xE8,
        0x44, 0xD4, 0xB9, 0x03, 0x5C, 0x69, 0xC7, 0x6C, 0x67, 0xB5, 0x53, 0x71, 0x8E, 0xCA, 0xE6, 0x42,
        0xE0, 0x0C, 0x05, 0xFE, 0xEA, 0x60, 0xDF, 0x5A, 0x1E, 0xAE, 0x1B, 0xC4, 0x4B, 0xAA, 0xB8, 0x38,
        0x91, 0xDD, 0xA8, 0x3B, 0x4E, 0xE3, 0xCE, 0xC2, 0xB1, 0xFC, 0x9B, 0x18, 0x8C, 0xB0, 0x9A, 0x22};

    static const uint8_t kBin1[256] = {
        0x00, 0xF0, 0xA2, 0xE2, 0xBC, 0x66, 0xC1, 0x7C, 0xF4, 0xDB, 0x47, 0x96, 0xED, 0x88, 0xE3, 0x98,
        0x74, 0x72, 0x31, 0x04, 0xFF, 0xCA, 0x55, 0x37, 0xB2, 0xDD, 0xE5, 0x0A, 0x58, 0x10, 0x5A, 0x7F,
        0xD6, 0xD4, 0x45, 0x6F, 0xD8, 0x87, 0xA8, 0x9E, 0xF9, 0x5C, 0x23, 0xB6, 0x4F, 0xB8, 0x33, 0x57,
        0x6B, 0xEB, 0x7E, 0x1F, 0x12, 0xC9, 0xAE, 0x08, 0x9F, 0x52, 0x25, 0x71, 0x02, 0xFE, 0x85, 0xE9,
        0xA9, 0x9A, 0xDF, 0x70, 0xEE, 0x36, 0x8F, 0xC3, 0x83, 0x82, 0xE4, 0x8B, 0x8C, 0x4E, 0x4B, 0xDC,
        0x0D, 0x69, 0xB1, 0x3A, 0x32, 0x8D, 0xF2, 0x9C, 0x86, 0x1C, 0xB0, 0x76, 0xCD, 0x3C, 0xEA, 0x22,
        0x54, 0x5F, 0xD5, 0xC8, 0xA3, 0x94, 0xFB, 0xBA, 0xD3, 0xBD, 0x64, 0x2A, 0xAA, 0x17, 0x46, 0x95,
        0x09, 0x3F, 0x29, 0x68, 0x59, 0x7A, 0x6E, 0x8E, 0x56, 0xA4, 0x1D, 0x1E, 0x3B, 0x1A, 0xE7, 0x91,
        0xB7, 0xAD, 0x62, 0xB9, 0x1B, 0xCB, 0xDA, 0xE6, 0x2B, 0x60, 0x16, 0xC2, 0xD0, 0xF8, 0xF1, 0x0E,
        0xBF, 0x34, 0x48, 0x35, 0x90, 0x92, 0x2D, 0xB5, 0x5D, 0x93, 0x99, 0x50, 0x43, 0xD7, 0xF5, 0xCF,
        0x30, 0x01, 0xD2, 0x06, 0xE8, 0xA6, 0x05, 0xEC, 0x89, 0x49, 0xFD, 0xD9, 0xBB, 0x77, 0xF3, 0x80,
        0x81, 0x0B, 0x73, 0xA5, 0x9D, 0x75, 0xE1, 0xCE, 0x14, 0x6A, 0x3E, 0xFC, 0x20, 0x84, 0x5B, 0x24,
        0xB3, 0xB4, 0xC6, 0x21, 0x78, 0xC4, 0xEF, 0x2C, 0x53, 0x26, 0x2E, 0x67, 0x15, 0xE0, 0xC5, 0x0F,
        0xF6, 0x6C, 0x5E, 0x11, 0x41, 0x07, 0xDE, 0x4C, 0x8A, 0x63, 0x4D, 0x51, 0xAF, 0x2F, 0x40, 0x42,
        0xC0, 0x9B, 0x38, 0x27, 0x3D, 0x44, 0xAC, 0xA7, 0x18, 0xA0, 0x61, 0x13, 0x19, 0x03, 0x97, 0x65,
        0x79, 0x6D, 0x0C, 0xA1, 0xFA, 0xAB, 0xD1, 0xC7, 0x28, 0x7B, 0xCC, 0xF7, 0x7D, 0x39, 0x4A, 0xBE};

    static const uint8_t kBin2[256] = {
        0x00, 0xF0, 0xA2, 0xE2, 0xBC, 0x66, 0xC1, 0x7C, 0xF4, 0xDB, 0x47, 0x96, 0xED, 0x88, 0xE3, 0x98,
        0x81, 0x0B, 0x73, 0xA5, 0x9D, 0x75, 0xE1, 0xCE, 0x14, 0x6A, 0x3E, 0xFC, 0x20, 0x84, 0x5B, 0x24,
        0xB3, 0xB4, 0xC6, 0x21, 0x78, 0xC4, 0xEF, 0x2C, 0x53, 0x26, 0x2E, 0x67, 0x15, 0xE0, 0xC5, 0x0F,
        0x79, 0x6D, 0x0C, 0xA1, 0xFA, 0xAB, 0xD1, 0xC7, 0x28, 0x7B, 0xCC, 0xF7, 0x7D, 0x39, 0x4A, 0xBE,
        0x30, 0x01, 0xD2, 0x06, 0xE8, 0xA6, 0x05, 0xEC, 0x89, 0x49, 0xFD, 0xD9, 0xBB, 0x77, 0xF3, 0x80,
        0xD6, 0xD4, 0x45, 0x6F, 0xD8, 0x87, 0xA8, 0x9E, 0xF9, 0x5C, 0x23, 0xB6, 0x4F, 0xB8, 0x33, 0x57,
        0x6B, 0xEB, 0x7E, 0x1F, 0x12, 0xC9, 0xAE, 0x08, 0x9F, 0x52, 0x25, 0x71, 0x02, 0xFE, 0x85, 0xE9,
        0x54, 0x5F, 0xD5, 0xC8, 0xA3, 0x94, 0xFB, 0xBA, 0xD3, 0xBD, 0x64, 0x2A, 0xAA, 0x17, 0x46, 0x95,
        0xB7, 0xAD, 0x62, 0xB9, 0x1B, 0xCB, 0xDA, 0xE6, 0x2B, 0x60, 0x16, 0xC2, 0xD0, 0xF8, 0xF1, 0x0E,
        0xA9, 0x9A, 0xDF, 0x70, 0xEE, 0x36, 0x8F, 0xC3, 0x83, 0x82, 0xE4, 0x8B, 0x8C, 0x4E, 0x4B, 0xDC,
        0x0D, 0x69, 0xB1, 0x3A, 0x32, 0x8D, 0xF2, 0x9C, 0x86, 0x1C, 0xB0, 0x76, 0xCD, 0x3C, 0xEA, 0x22,
        0x74, 0x72, 0x31, 0x04, 0xFF, 0xCA, 0x55, 0x37, 0xB2, 0xDD, 0xE5, 0x0A, 0x58, 0x10, 0x5A, 0x7F,
        0xC0, 0x9B, 0x38, 0x27, 0x3D, 0x44, 0xAC, 0xA7, 0x18, 0xA0, 0x61, 0x13, 0x19, 0x03, 0x97, 0x65,
        0xF6, 0x6C, 0x5E, 0x11, 0x41, 0x07, 0xDE, 0x4C, 0x8A, 0x63, 0x4D, 0x51, 0xAF, 0x2F, 0x40, 0x42,
        0xBF, 0x34, 0x48, 0x35, 0x90, 0x92, 0x2D, 0xB5, 0x5D, 0x93, 0x99, 0x50, 0x43, 0xD7, 0xF5, 0xCF,
        0x09, 0x3F, 0x29, 0x68, 0x59, 0x7A, 0x6E, 0x8E, 0x56, 0xA4, 0x1D, 0x1E, 0x3B, 0x1A, 0xE7, 0x91};

    static const uint8_t kContainer1[256] = {
        0x00, 0xF0, 0xA2, 0xE2, 0xF4, 0xDB, 0x47, 0x96, 0xAF, 0x2F, 0x40, 0x42, 0x41, 0x07, 0xDE, 0x4C,
        0xC0, 0x9B, 0x38, 0x27, 0x19, 0x03, 0x97, 0x65, 0x18, 0xA0, 0x61, 0x13, 0xBC, 0x66, 0xC1, 0x7C,
        0x7D, 0x39, 0x4A, 0xBE, 0xFA, 0xAB, 0xD1, 0xC7, 0x83, 0x82, 0xE4, 0x8B, 0xA9, 0x9A, 0xDF, 0x70,
        0x14, 0x6A, 0x3E, 0xFC, 0x8C, 0x4E, 0x4B, 0xDC, 0xD0, 0xF8, 0xF1, 0x0E, 0x28, 0x7B, 0xCC, 0xF7,
        0x12, 0xC9, 0xAE, 0x08, 0x02, 0xFE, 0x85, 0xE9, 0x09, 0x3F, 0x29, 0x68, 0x56, 0xA4, 0x1D, 0x1E,
        0x54, 0x5F, 0xD5, 0xC8, 0xAA, 0x17, 0x46, 0x95, 0x90, 0x92, 0x2D, 0xB5, 0x5D, 0x93, 0x99, 0x50,
        0x3D, 0x44, 0xAC, 0xA7, 0x32, 0x8D, 0xF2, 0x9C, 0x78, 0xC4, 0xEF, 0x2C, 0x74, 0x72, 0x31, 0x04,
        0xF6, 0x6C, 0x5E, 0x11, 0x8A, 0x63, 0x4D, 0x51, 0xED, 0x88, 0xE3, 0x98, 0x15, 0xE0, 0xC5, 0x0F,
        0x53, 0x26, 0x2E, 0x67, 0xD3, 0xBD, 0x64, 0x2A, 0xB3, 0xB4, 0xC6, 0x21, 0x43, 0xD7, 0xF5, 0xCF,
        0xB7, 0xAD, 0x62, 0xB9, 0x79, 0x6D, 0x0C, 0xA1, 0x9D, 0x75, 0xE1, 0xCE, 0xEE, 0x36, 0x8F, 0xC3,
        0x1B, 0xCB, 0xDA, 0xE6, 0x2B, 0x60, 0x16, 0xC2, 0x81, 0x0B, 0x73, 0xA5, 0x20, 0x84, 0x5B, 0x24,
        0x6B, 0xEB, 0x7E, 0x1F, 0x9F, 0x52, 0x25, 0x71, 0x59, 0x7A, 0x6E, 0x8E, 0x3B, 0x1A, 0xE7, 0x91,
        0xF9, 0x5C, 0x23, 0xB6, 0x58, 0x10, 0x5A, 0x7F, 0xBB, 0x77, 0xF3, 0x80, 0x86, 0x1C, 0xB0, 0x76,
        0x4F, 0xB8, 0x33, 0x57, 0xD6, 0xD4, 0x45, 0x6F, 0x89, 0x49, 0xFD, 0xD9, 0xE8, 0xA6, 0x05, 0xEC,
        0xA3, 0x94, 0xFB, 0xBA, 0xB2, 0xDD, 0xE5, 0x0A, 0xBF, 0x34, 0x48, 0x35, 0xCD, 0x3C, 0xEA, 0x22,
        0x30, 0x01, 0xD2, 0x06, 0x0D, 0x69, 0xB1, 0x3A, 0xD8, 0x87, 0xA8, 0x9E, 0xFF, 0xCA, 0x55, 0x37};

    static const uint8_t kContainer2[256] = {
        0x00, 0xF0, 0xA2, 0xE2, 0xF4, 0xDB, 0x47, 0x96, 0xAF, 0x2F, 0x40, 0x42, 0x41, 0x07, 0xDE, 0x4C,
        0xA3, 0x94, 0xFB, 0xBA, 0xB2, 0xDD, 0xE5, 0x0A, 0xBF, 0x34, 0x48, 0x35, 0xCD, 0x3C, 0xEA, 0x22,
        0x30, 0x01, 0xD2, 0x06, 0x0D, 0x69, 0xB1, 0x3A, 0xD8, 0x87, 0xA8, 0x9E, 0xFF, 0xCA, 0x55, 0x37,
        0x4F, 0xB8, 0x33, 0x57, 0xD6, 0xD4, 0x45, 0x6F, 0x89, 0x49, 0xFD, 0xD9, 0xE8, 0xA6, 0x05, 0xEC,
        0xC0, 0x9B, 0x38, 0x27, 0x19, 0x03, 0x97, 0x65, 0x18, 0xA0, 0x61, 0x13, 0xBC, 0x66, 0xC1, 0x7C,
        0x14, 0x6A, 0x3E, 0xFC, 0x8C, 0x4E, 0x4B, 0xDC, 0xD0, 0xF8, 0xF1, 0x0E, 0x28, 0x7B, 0xCC, 0xF7,
        0xF9, 0x5C, 0x23, 0xB6, 0x58, 0x10, 0x5A, 0x7F, 0xBB, 0x77, 0xF3, 0x80, 0x86, 0x1C, 0xB0, 0x76,
        0x12, 0xC9, 0xAE, 0x08, 0x02, 0xFE, 0x85, 0xE9, 0x09, 0x3F, 0x29, 0x68, 0x56, 0xA4, 0x1D, 0x1E,
        0x1B, 0xCB, 0xDA, 0xE6, 0x2B, 0x60, 0x16, 0xC2, 0x81, 0x0B, 0x73, 0xA5, 0x20, 0x84, 0x5B, 0x24,
        0x6B, 0xEB, 0x7E, 0x1F, 0x9F, 0x52, 0x25, 0x71, 0x59, 0x7A, 0x6E, 0x8E, 0x3B, 0x1A, 0xE7, 0x91,
        0xB7, 0xAD, 0x62, 0xB9, 0x79, 0x6D, 0x0C, 0xA1, 0x9D, 0x75, 0xE1, 0xCE, 0xEE, 0x36, 0x8F, 0xC3,
        0x3D, 0x44, 0xAC, 0xA7, 0x32, 0x8D, 0xF2, 0x9C, 0x78, 0xC4, 0xEF, 0x2C, 0x74, 0x72, 0x31, 0x04,
        0xF6, 0x6C, 0x5E, 0x11, 0x8A, 0x63, 0x4D, 0x51, 0xED, 0x88, 0xE3, 0x98, 0x15, 0xE0, 0xC5, 0x0F,
        0x53, 0x26, 0x2E, 0x67, 0xD3, 0xBD, 0x64, 0x2A, 0xB3, 0xB4, 0xC6, 0x21, 0x43, 0xD7, 0xF5, 0xCF,
        0x54, 0x5F, 0xD5, 0xC8, 0xAA, 0x17, 0x46, 0x95, 0x90, 0x92, 0x2D, 0xB5, 0x5D, 0x93, 0x99, 0x50,
        0x7D, 0x39, 0x4A, 0xBE, 0xFA, 0xAB, 0xD1, 0xC7, 0x83, 0x82, 0xE4, 0x8B, 0xA9, 0x9A, 0xDF, 0x70};

    switch (c) {
        case ProbeCipher::ByteCryptOld: return kOld;
        case ProbeCipher::ByteCryptRan2: return kRan2;
        case ProbeCipher::ByteCryptBin1: return kBin1;
        case ProbeCipher::ByteCryptBin2: return kBin2;
        case ProbeCipher::ByteCryptContainer1: return kContainer1;
        case ProbeCipher::ByteCryptContainer2: return kContainer2;
        default: return nullptr;
    }
}

inline void DecodePayloadCopy(const uint8_t* src, size_t len, ProbeCipher cipher,
                              std::vector<uint8_t>& out) {
    out.assign(src, src + len);
    if (cipher == ProbeCipher::None || len == 0) return;
    if (cipher == ProbeCipher::CompByte) {
        RANMobile::ep7byte::CompByteDecode(out.data(), out.size());
        return;
    }
    const uint8_t* enc = EncodeTableFor(cipher);
    if (!enc) return;
    uint8_t dec[256]{};
    for (int i = 0; i < 256; ++i) dec[enc[i]] = static_cast<uint8_t>(i);
    for (size_t i = 0; i < len; ++i) out[i] = dec[out[i]];
}

inline uint32_t ReadLE32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

inline std::string HexSlice(const std::vector<uint8_t>& b, size_t offset, size_t maxLen) {
    if (offset >= b.size()) return "";
    const size_t n = std::min(maxLen, b.size() - offset);
    std::string out;
    out.reserve(n * 2);
    char buf[4];
    for (size_t i = 0; i < n; ++i) {
        std::snprintf(buf, sizeof(buf), "%02x", b[offset + i]);
        out += buf;
    }
    return out;
}

inline std::string AsciiPreview(const uint8_t* p, size_t len) {
    std::string out;
    out.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        uint8_t ch = p[i];
        out.push_back((ch >= 0x20 && ch <= 0x7E) ? static_cast<char>(ch) : '.');
    }
    return out;
}

inline std::string LowerAscii(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

inline std::string HexU32(uint32_t v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%04X", v);
    if (v > 0xFFFFu) std::snprintf(buf, sizeof(buf), "0x%08X", v);
    return buf;
}

// Strict RFC 8259 ASCII-safe JSON string escape (escapes control bytes and >= 0x80 CP949 bytes).
inline std::string JsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20 || c >= 0x80) {
                    char b[8];
                    std::snprintf(b, sizeof(b), "\\u%04x", c);
                    out += b;
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Classification & forensic trace structures
// ---------------------------------------------------------------------------
enum class BinBucket {
    Parsed = 0,
    UnsupportedOuterVersion,
    InvalidHeader,
    UnrecognizedType,
    ParserRejected,
};

inline const char* BinBucketName(BinBucket b) {
    switch (b) {
        case BinBucket::Parsed: return "parsed";
        case BinBucket::UnsupportedOuterVersion: return "unsupported-version";
        case BinBucket::InvalidHeader: return "invalid-header";
        case BinBucket::UnrecognizedType: return "unrecognized-type";
        case BinBucket::ParserRejected: return "parser-rejected";
    }
    return "unknown";
}

struct CompanionCfgMeta {
    bool found = false;
    std::string source;     // "rcc:<archive>#<entry>" or "loose:<relpath>"
    std::string sha256;
    size_t byteSize = 0;
    uint32_t rawVersion = 0;
    std::string fileType;
    bool parsedValid = false;
    bool unsupported = false;
    std::string skeleton;
    uint32_t flag = 0;
    uint32_t startTime = 0;
    uint32_t endTime = 0;
    uint32_t unitTime = 0;
};

struct CipherProbeMatch {
    ProbeCipher cipher = ProbeCipher::None;
    uint32_t d0 = 0;
    uint32_t d1 = 0;
    uint32_t d2 = 0;
    uint32_t d3 = 0;
    bool looksLikeTrackList = false;
    std::string decodedPrefixHex;
    std::string decodedAscii;
};

// Read-only result of validating a container against the full backup-engine
// Lib_Engine/Meshs/SAnimationSaveLoad.cpp specification (0x0100..0x0104, 0x0200 +
// EMBYTECRYPT_BIN, and track versions 0x0100..0x0103).
struct SourceSpecShadowResult {
    bool attempted = false;
    bool ok = false;
    bool exactEof = false;
    size_t bytesConsumed = 0;
    size_t trailingBytes = 0;
    uint32_t list0Count = 0;
    uint32_t list1Count = 0;
    uint32_t list2Count = 0;
    uint32_t container0103Garbage = 0;
    bool container0103GarbageMatchesFormula = false;
    std::map<uint32_t, uint32_t> trackVersions;
    uint32_t tracksPos = 0;
    uint32_t tracksRot = 0;
    uint32_t tracksScale = 0;
    uint32_t tracksQuatPos = 0;
    uint32_t tracksMatrix = 0;
    uint32_t emptyBoneNames = 0;
    std::string note;
};

struct BinInspectionRecord {
    std::string relPath;
    bool inDataAnimationDir = false;
    bool fromRcc = false;
    size_t byteSize = 0;
    std::string sha256;

    // 132-byte CSerialFile header fields
    bool hasFullHeader = false;
    std::string fileType;
    bool zeroPaddedHeader = true;
    uint32_t outerVersion = 0;

    // Unmodified EP7AnimationReader outcome
    BinBucket bucket = BinBucket::InvalidHeader;
    bool readerValid = false;
    std::string readerError;

    // Step-by-step trace of unmodified 0x0100..0x0102 reader
    uint32_t upBodyDeclared = 0;
    uint32_t mainDeclared = 0;
    uint32_t upBodyParsed = 0;
    uint32_t mainParsed = 0;
    size_t bytesConsumed = 0;
    size_t trailingBytes = 0;
    std::map<uint32_t, uint32_t> trackVersionsSeen;

    // Rejection pinpoint (when bucket == ParserRejected)
    std::string failList;
    uint32_t failTrackIndex = 0;
    size_t failOffset = 0;
    uint32_t failTrackVersion = 0;
    uint32_t failNextDword = 0;
    std::string failWindowHex;
    std::string failAsciiPreview;

    // Full SAnimationSaveLoad.cpp source-derived read-only shadow verification
    SourceSpecShadowResult sourceSpec;

    // Hex slices for reproducible format analysis
    std::string headerPrefix16Hex;
    std::string headerTail16Hex;
    std::string payloadPrefix64Hex;

    // Read-only payload probes
    std::string matchingCipherName;
    std::vector<CipherProbeMatch> cipherProbes;

    CompanionCfgMeta cfg;
};

// ---------------------------------------------------------------------------
// 1. Step-by-step trace mirroring UNMODIFIED EP7AnimationReader (0x0100..0x0102
//    outer, 0x0101..0x0102 track, including EP7AnimationReader's exact key sizes).
// ---------------------------------------------------------------------------
inline bool TraceUnmodifiedReaderBoneTrack(RANMobile::ep7byte::Stream& s,
                                           BinInspectionRecord* rec,
                                           const char* listName,
                                           uint32_t trackIndex) {
    static constexpr uint32_t kMaxKeys = 200000;
    const size_t trackStart = s.Position();
    uint32_t version = 0;
    if (!s.ReadU32(version)) {
        if (rec) {
            rec->failList = listName;
            rec->failTrackIndex = trackIndex;
            rec->failOffset = trackStart;
        }
        return false;
    }

    // Mirror current production EP7AnimationReader (0x0100..0x0103 tracks with
    // verified 80-byte SMatrixKey stride: 4B time + 12B align16 pad + 64B mat).
    if (version == 0x0103u) {
        std::string bone;
        uint32_t nRot = 0, g1 = 0, nPos = 0, nQuat = 0, nMat = 0, g2 = 0, nScale = 0;
        if (!s.ReadString(bone)) return false;
        if (!s.ReadU32(nRot) || nRot > kMaxKeys ||
            !s.Skip(static_cast<size_t>(nRot) * RANMobile::EP7AnimationReader::kRotateKeySize))
            return false;
        if (!s.ReadU32(g1)) return false;
        if (!s.ReadU32(nPos) || nPos > kMaxKeys ||
            !s.Skip(static_cast<size_t>(nPos) * RANMobile::EP7AnimationReader::kPositionKeySize))
            return false;
        if (!s.ReadU32(nQuat) || nQuat > kMaxKeys ||
            !s.Skip(static_cast<size_t>(nQuat) * RANMobile::EP7AnimationReader::kQuatPosKeySize))
            return false;
        if (!s.ReadU32(nMat) || nMat > kMaxKeys ||
            !s.CanReadElements(nMat, RANMobile::EP7AnimationReader::kMatrixKeySize) ||
            !s.Skip(static_cast<size_t>(nMat) * RANMobile::EP7AnimationReader::kMatrixKeySize))
            return false;
        if (!s.ReadU32(g2)) return false;
        if (!s.ReadU32(nScale) || nScale > kMaxKeys ||
            !s.Skip(static_cast<size_t>(nScale) * RANMobile::EP7AnimationReader::kScaleKeySize))
            return false;
        if (rec) rec->trackVersionsSeen[version]++;
        return true;
    }

    if (version == 0x0102u) {
        std::string bone;
        uint32_t nRot = 0, nPos = 0, g1 = 0, nQuat = 0, g2 = 0, nMat = 0, nScale = 0;
        if (!s.ReadString(bone)) return false;
        if (!s.ReadU32(nRot) || nRot > kMaxKeys ||
            !s.Skip(static_cast<size_t>(nRot) * RANMobile::EP7AnimationReader::kRotateKeySize))
            return false;
        if (!s.ReadU32(nPos) || nPos > kMaxKeys ||
            !s.Skip(static_cast<size_t>(nPos) * RANMobile::EP7AnimationReader::kPositionKeySize))
            return false;
        if (!s.ReadU32(g1)) return false;
        if (!s.ReadU32(nQuat) || nQuat > kMaxKeys ||
            !s.Skip(static_cast<size_t>(nQuat) * RANMobile::EP7AnimationReader::kQuatPosKeySize))
            return false;
        if (!s.ReadU32(g2)) return false;
        if (!s.ReadU32(nMat) || nMat > kMaxKeys ||
            !s.CanReadElements(nMat, RANMobile::EP7AnimationReader::kMatrixKeySize) ||
            !s.Skip(static_cast<size_t>(nMat) * RANMobile::EP7AnimationReader::kMatrixKeySize))
            return false;
        if (!s.ReadU32(nScale) || nScale > kMaxKeys ||
            !s.Skip(static_cast<size_t>(nScale) * RANMobile::EP7AnimationReader::kScaleKeySize))
            return false;
        if (rec) rec->trackVersionsSeen[version]++;
        return true;
    }

    if (version == 0x0101u) {
        std::string bone;
        uint32_t nPos = 0, nRot = 0, nScale = 0, nMat = 0, nQuat = 0;
        if (!s.ReadU32(nPos) || nPos > kMaxKeys ||
            !s.Skip(static_cast<size_t>(nPos) * RANMobile::EP7AnimationReader::kPositionKeySize))
            return false;
        if (!s.ReadU32(nRot) || nRot > kMaxKeys ||
            !s.Skip(static_cast<size_t>(nRot) * RANMobile::EP7AnimationReader::kRotateKeySize))
            return false;
        if (!s.ReadU32(nScale) || nScale > kMaxKeys ||
            !s.Skip(static_cast<size_t>(nScale) * RANMobile::EP7AnimationReader::kScaleKeySize))
            return false;
        if (!s.ReadU32(nMat) || nMat > kMaxKeys ||
            !s.CanReadElements(nMat, RANMobile::EP7AnimationReader::kMatrixKeySize) ||
            !s.Skip(static_cast<size_t>(nMat) * RANMobile::EP7AnimationReader::kMatrixKeySize))
            return false;
        if (!s.ReadU32(nQuat) || nQuat > kMaxKeys ||
            !s.Skip(static_cast<size_t>(nQuat) * RANMobile::EP7AnimationReader::kQuatPosKeySize))
            return false;
        if (!s.ReadString(bone)) return false;
        if (rec) rec->trackVersionsSeen[version]++;
        return true;
    }

    if (version == 0x0100u) {
        std::string bone;
        uint32_t nPos = 0, nRot = 0, nScale = 0, nMat = 0;
        if (!s.ReadU32(nPos) || nPos > kMaxKeys ||
            !s.Skip(static_cast<size_t>(nPos) * RANMobile::EP7AnimationReader::kPositionKeySize))
            return false;
        if (!s.ReadU32(nRot) || nRot > kMaxKeys ||
            !s.Skip(static_cast<size_t>(nRot) * RANMobile::EP7AnimationReader::kRotateKeySize))
            return false;
        if (!s.ReadU32(nScale) || nScale > kMaxKeys ||
            !s.Skip(static_cast<size_t>(nScale) * RANMobile::EP7AnimationReader::kScaleKeySize))
            return false;
        if (!s.ReadU32(nMat) || nMat > kMaxKeys ||
            !s.CanReadElements(nMat, RANMobile::EP7AnimationReader::kMatrixKeySize) ||
            !s.Skip(static_cast<size_t>(nMat) * RANMobile::EP7AnimationReader::kMatrixKeySize))
            return false;
        if (!s.ReadString(bone)) return false;
        if (rec) rec->trackVersionsSeen[version]++;
        return true;
    }

    if (rec) {
        rec->failList = listName;
        rec->failTrackIndex = trackIndex;
        rec->failOffset = trackStart;
        rec->failTrackVersion = version;
        if (s.Remaining() >= 4) {
            uint32_t nxt = 0;
            if (s.ReadU32(nxt)) rec->failNextDword = nxt;
        }
    }
    s.Fail("unsupported animation version");
    return false;
}

inline bool TraceUnmodifiedReaderContainer(const std::vector<uint8_t>& bytes,
                                           uint32_t outerVersion,
                                           BinInspectionRecord* rec) {
    static constexpr uint32_t kMaxTracks = 4096;
    RANMobile::ep7byte::Stream s;
    s.Attach(bytes, false);
    std::string hdrType;
    uint32_t hdrVer = 0;
    if (!s.ReadFileType(hdrType, hdrVer)) return false;

    auto walkList = [&](const char* name, uint32_t* declaredOut, uint32_t* parsedOut) -> bool {
        const size_t cntPos = s.Position();
        uint32_t count = 0;
        if (!s.ReadU32(count) || count > kMaxTracks) {
            if (rec) {
                rec->failList = name;
                rec->failOffset = cntPos;
                rec->failTrackVersion = count;
            }
            return false;
        }
        if (declaredOut) *declaredOut = count;
        for (uint32_t i = 0; i < count; ++i) {
            if (!TraceUnmodifiedReaderBoneTrack(s, rec, name, i)) return false;
            if (parsedOut) *parsedOut = i + 1;
        }
        return true;
    };

    bool ok = false;
    if (outerVersion == 0x0103u) {
        uint32_t interGarbage = 0;
        ok = walkList("upBody", rec ? &rec->upBodyDeclared : nullptr,
                      rec ? &rec->upBodyParsed : nullptr) &&
             s.ReadU32(interGarbage) &&
             walkList("main", rec ? &rec->mainDeclared : nullptr,
                      rec ? &rec->mainParsed : nullptr);
    } else if (outerVersion == 0x0102u) {
        ok = walkList("upBody", rec ? &rec->upBodyDeclared : nullptr,
                      rec ? &rec->upBodyParsed : nullptr) &&
             walkList("main", rec ? &rec->mainDeclared : nullptr,
                      rec ? &rec->mainParsed : nullptr);
    } else if (outerVersion == 0x0101u) {
        ok = walkList("main", rec ? &rec->mainDeclared : nullptr,
                      rec ? &rec->mainParsed : nullptr) &&
             walkList("upBody", rec ? &rec->upBodyDeclared : nullptr,
                      rec ? &rec->upBodyParsed : nullptr);
    } else if (outerVersion == 0x0100u) {
        ok = walkList("main", rec ? &rec->mainDeclared : nullptr,
                      rec ? &rec->mainParsed : nullptr);
    }
    if (ok && rec) {
        rec->bytesConsumed = s.Position();
        rec->trailingBytes = bytes.size() - s.Position();
    }
    return ok;
}

// ---------------------------------------------------------------------------
// 2. Read-only shadow verifier implementing the verified backup-engine source
//    Lib_Engine/Meshs/SAnimationSaveLoad.cpp:
//      - SAnimation::LoadFromFile: 0x0100, 0x0101, 0x0102, 0x0103
//        (SPositionKey=16B, SRotateKey=20B, SScaleKey=16B, SQuatPosKey=36B,
//         SMatrixKey=80B with 12-byte align16 padding)
//      - SAnimContainer::LoadAnimFromFile:
//        0x0100, 0x0101, 0x0102, 0x0103, 0x0104, 0x0200 (EMBYTECRYPT_BIN)
// ---------------------------------------------------------------------------
inline SourceSpecShadowResult VerifyAgainstBackupEngineSourceSpec(
    const std::vector<uint8_t>& rawFileBytes, uint32_t outerVersion) {
    SourceSpecShadowResult res;
    if (outerVersion != 0x0100u && outerVersion != 0x0101u && outerVersion != 0x0102u &&
        outerVersion != 0x0103u && outerVersion != 0x0104u && outerVersion != 0x0200u) {
        return res;
    }
    res.attempted = true;
    if (rawFileBytes.size() < RANMobile::ep7byte::Stream::kHeaderSize) {
        res.note = "truncated header";
        return res;
    }

    std::vector<uint8_t> decoded = rawFileBytes;
    if (outerVersion >= 0x0200u) {
        // SAnimationSaveLoad.cpp:311-312:
        //   if ( dwVersion >= VERSION_ENCODE ) SFile.SetEncodeType( EMBYTECRYPT_BIN );
        std::vector<uint8_t> decPayload;
        DecodePayloadCopy(rawFileBytes.data() + 132, rawFileBytes.size() - 132,
                          ProbeCipher::ByteCryptBin1, decPayload);
        std::memcpy(decoded.data() + 132, decPayload.data(), decPayload.size());
    }

    static constexpr uint32_t kMaxTracks = 4096;
    static constexpr uint32_t kMaxKeys = 262144;

    RANMobile::ep7byte::Stream s;
    s.Attach(std::move(decoded), false);
    std::string hdrType;
    uint32_t hdrVer = 0;
    if (!s.ReadFileType(hdrType, hdrVer)) {
        res.note = "ReadFileType failed";
        return res;
    }

    auto readTrackSpec = [&](const char* listTag, uint32_t idx) -> bool {
        const size_t tStart = s.Position();
        uint32_t tVer = 0;
        if (!s.ReadU32(tVer)) {
            res.note = std::string(listTag) + "[" + std::to_string(idx) +
                       "] truncated track version at " + std::to_string(tStart);
            return false;
        }
        res.trackVersions[tVer]++;

        std::string bone;
        uint32_t nPos = 0, nRot = 0, nScale = 0, nQuat = 0, nMat = 0, g1 = 0, g2 = 0;

        auto failAt = [&](const char* msg) {
            res.note = std::string(listTag) + "[" + std::to_string(idx) + "] v" + HexU32(tVer) +
                       " at " + std::to_string(tStart) + ": " + msg;
            return false;
        };

        if (tVer == 0x0100u) {
            if (!s.ReadU32(nPos) || nPos > kMaxKeys || !s.Skip(static_cast<size_t>(nPos) * 16u))
                return failAt("posKeys");
            if (!s.ReadU32(nRot) || nRot > kMaxKeys || !s.Skip(static_cast<size_t>(nRot) * 20u))
                return failAt("rotKeys");
            if (!s.ReadU32(nScale) || nScale > kMaxKeys || !s.Skip(static_cast<size_t>(nScale) * 16u))
                return failAt("scaleKeys");
            if (!s.ReadU32(nMat) || nMat > kMaxKeys || !s.Skip(static_cast<size_t>(nMat) * 80u))
                return failAt("matrixKeys");
            if (!s.ReadString(bone)) return failAt("boneName");
        } else if (tVer == 0x0101u) {
            if (!s.ReadU32(nPos) || nPos > kMaxKeys || !s.Skip(static_cast<size_t>(nPos) * 16u))
                return failAt("posKeys");
            if (!s.ReadU32(nRot) || nRot > kMaxKeys || !s.Skip(static_cast<size_t>(nRot) * 20u))
                return failAt("rotKeys");
            if (!s.ReadU32(nScale) || nScale > kMaxKeys || !s.Skip(static_cast<size_t>(nScale) * 16u))
                return failAt("scaleKeys");
            if (!s.ReadU32(nMat) || nMat > kMaxKeys || !s.Skip(static_cast<size_t>(nMat) * 80u))
                return failAt("matrixKeys");
            if (!s.ReadU32(nQuat) || nQuat > kMaxKeys || !s.Skip(static_cast<size_t>(nQuat) * 36u))
                return failAt("quatPosKeys");
            if (!s.ReadString(bone)) return failAt("boneName");
        } else if (tVer == 0x0102u) {
            if (!s.ReadString(bone)) return failAt("boneName");
            if (!s.ReadU32(nRot) || nRot > kMaxKeys || !s.Skip(static_cast<size_t>(nRot) * 20u))
                return failAt("rotKeys");
            if (!s.ReadU32(nPos) || nPos > kMaxKeys || !s.Skip(static_cast<size_t>(nPos) * 16u))
                return failAt("posKeys");
            if (!s.ReadU32(g1)) return failAt("garbage1");
            if (!s.ReadU32(nQuat) || nQuat > kMaxKeys || !s.Skip(static_cast<size_t>(nQuat) * 36u))
                return failAt("quatPosKeys");
            if (!s.ReadU32(g2)) return failAt("garbage2");
            if (!s.ReadU32(nMat) || nMat > kMaxKeys || !s.Skip(static_cast<size_t>(nMat) * 80u))
                return failAt("matrixKeys");
            if (!s.ReadU32(nScale) || nScale > kMaxKeys || !s.Skip(static_cast<size_t>(nScale) * 16u))
                return failAt("scaleKeys");
        } else if (tVer == 0x0103u) {
            // SAnimation::Load_0103 (SAnimationSaveLoad.cpp:192-248):
            // boneName, rotKeys, dwGarbage1, posKeys, quatPosKeys, matrixKeys, dwGarbage2, scaleKeys
            if (!s.ReadString(bone)) return failAt("boneName");
            if (!s.ReadU32(nRot) || nRot > kMaxKeys || !s.Skip(static_cast<size_t>(nRot) * 20u))
                return failAt("rotKeys");
            if (!s.ReadU32(g1)) return failAt("garbage1");
            if (!s.ReadU32(nPos) || nPos > kMaxKeys || !s.Skip(static_cast<size_t>(nPos) * 16u))
                return failAt("posKeys");
            if (!s.ReadU32(nQuat) || nQuat > kMaxKeys || !s.Skip(static_cast<size_t>(nQuat) * 36u))
                return failAt("quatPosKeys");
            if (!s.ReadU32(nMat) || nMat > kMaxKeys || !s.Skip(static_cast<size_t>(nMat) * 80u))
                return failAt("matrixKeys");
            if (!s.ReadU32(g2)) return failAt("garbage2");
            if (!s.ReadU32(nScale) || nScale > kMaxKeys || !s.Skip(static_cast<size_t>(nScale) * 16u))
                return failAt("scaleKeys");
        } else {
            return failAt("unknown track version");
        }

        if (bone.empty()) res.emptyBoneNames++;
        if (nPos > 0) res.tracksPos++;
        if (nRot > 0) res.tracksRot++;
        if (nScale > 0) res.tracksScale++;
        if (nQuat > 0) res.tracksQuatPos++;
        if (nMat > 0) res.tracksMatrix++;
        return true;
    };

    auto readListSpec = [&](const char* listTag, uint32_t& outCount) -> bool {
        uint32_t cnt = 0;
        if (!s.ReadU32(cnt) || cnt > kMaxTracks) {
            res.note = std::string(listTag) + ": invalid track count";
            return false;
        }
        outCount = cnt;
        for (uint32_t i = 0; i < cnt; ++i) {
            if (!readTrackSpec(listTag, i)) return false;
        }
        return true;
    };

    bool ok = false;
    if (outerVersion == 0x0100u) {
        ok = readListSpec("main", res.list0Count);
    } else if (outerVersion == 0x0101u) {
        ok = readListSpec("main", res.list0Count) && readListSpec("upBody", res.list1Count);
    } else if (outerVersion == 0x0102u || outerVersion == 0x0200u) {
        ok = readListSpec("upBody", res.list0Count) && readListSpec("main", res.list1Count);
    } else if (outerVersion == 0x0103u) {
        if (readListSpec("upBody", res.list0Count) && s.ReadU32(res.container0103Garbage) &&
            readListSpec("main", res.list1Count)) {
            ok = true;
            res.container0103GarbageMatchesFormula =
                (res.container0103Garbage == (0x0103u + res.list0Count + res.list1Count));
        }
    } else if (outerVersion == 0x0104u) {
        ok = readListSpec("main", res.list0Count) && readListSpec("upBody1", res.list1Count) &&
             readListSpec("upBody2", res.list2Count);
    }

    res.ok = ok;
    if (ok) {
        res.bytesConsumed = s.Position();
        res.trailingBytes = rawFileBytes.size() - s.Position();
        res.exactEof = (res.trailingBytes == 0);
        res.note = res.exactEof
                       ? "exact EOF"
                       : ("parsed with " + std::to_string(res.trailingBytes) + " trailing bytes");
    }
    return res;
}

inline CipherProbeMatch EvaluateCipherProbe(const uint8_t* payload, size_t payloadLen,
                                            ProbeCipher cipher) {
    CipherProbeMatch m;
    m.cipher = cipher;
    const size_t sampleLen = std::min<size_t>(payloadLen, 96u);
    std::vector<uint8_t> dec;
    DecodePayloadCopy(payload, sampleLen, cipher, dec);
    m.decodedPrefixHex = HexSlice(dec, 0, std::min<size_t>(dec.size(), 32u));
    m.decodedAscii = AsciiPreview(dec.data(), std::min<size_t>(dec.size(), 40u));
    if (dec.size() >= 16) {
        m.d0 = ReadLE32(dec.data() + 0);
        m.d1 = ReadLE32(dec.data() + 4);
        m.d2 = ReadLE32(dec.data() + 8);
        m.d3 = ReadLE32(dec.data() + 12);
        const bool caseEmptyFirst =
            (m.d0 == 0 && m.d1 <= 256u &&
             (m.d1 == 0 || (m.d2 >= 0x0100u && m.d2 <= 0x0205u)));
        const bool caseNonEmptyFirst =
            (m.d0 >= 1u && m.d0 <= 256u && m.d1 >= 0x0100u && m.d1 <= 0x0205u);
        m.looksLikeTrackList = caseEmptyFirst || caseNonEmptyFirst;
    }
    return m;
}

inline bool IsUnderDataAnimation(const std::string& relPath) {
    const std::string low = LowerAscii(relPath);
    return low.rfind("data/animation/", 0) == 0 ||
           low.find("/data/animation/") != std::string::npos;
}

inline BinInspectionRecord InspectSingleBin(const std::string& relPath,
                                            const std::vector<uint8_t>& bytes,
                                            bool fromRcc,
                                            bool populateDetailedProbes) {
    BinInspectionRecord r;
    r.relPath = relPath;
    r.inDataAnimationDir = IsUnderDataAnimation(relPath);
    r.fromRcc = fromRcc;
    r.byteSize = bytes.size();
    r.sha256 = Sha256::DigestHex(bytes);
    r.headerPrefix16Hex = HexSlice(bytes, 0, 16);

    if (bytes.size() < RANMobile::ep7byte::Stream::kHeaderSize) {
        r.hasFullHeader = false;
        r.bucket = BinBucket::InvalidHeader;
        r.readerError = "truncated file header";
        return r;
    }

    r.hasFullHeader = true;
    r.headerTail16Hex = HexSlice(bytes, 116, 16);
    r.payloadPrefix64Hex = HexSlice(bytes, 132, 64);

    size_t typeLen = 0;
    while (typeLen < RANMobile::ep7byte::Stream::kFileTypeSize && bytes[typeLen] != 0) ++typeLen;
    if (typeLen == RANMobile::ep7byte::Stream::kFileTypeSize) {
        r.bucket = BinBucket::InvalidHeader;
        r.readerError = "unterminated 128-byte CSerialFile type string";
        return r;
    }
    r.fileType.assign(reinterpret_cast<const char*>(bytes.data()), typeLen);
    for (size_t i = typeLen + 1; i < RANMobile::ep7byte::Stream::kFileTypeSize; ++i) {
        if (bytes[i] != 0) {
            r.zeroPaddedHeader = false;
            break;
        }
    }
    r.outerVersion = ReadLE32(bytes.data() + RANMobile::ep7byte::Stream::kFileTypeSize);

    if (r.fileType != "AnimContainer") {
        r.bucket = BinBucket::UnrecognizedType;
        r.readerError = "unrecognized header type '" + r.fileType + "'";
        return r;
    }

    // 1. Invoke UNMODIFIED production EP7AnimationReader::ParseContainer.
    const RANMobile::EP7AnimClip clip =
        RANMobile::EP7AnimationReader::ParseContainer(bytes, true, false);
    r.readerValid = clip.valid;
    r.readerError = clip.error;

    const bool outerSupported =
        (r.outerVersion == 0x0100u || r.outerVersion == 0x0101u || r.outerVersion == 0x0102u ||
         r.outerVersion == 0x0103u);

    if (!outerSupported) {
        r.bucket = BinBucket::UnsupportedOuterVersion;
    } else if (clip.valid) {
        r.bucket = BinBucket::Parsed;
    } else {
        r.bucket = BinBucket::ParserRejected;
    }

    if (outerSupported) {
        const bool traceOk = TraceUnmodifiedReaderContainer(bytes, r.outerVersion, &r);
        if (!traceOk && r.bucket == BinBucket::ParserRejected) {
            const size_t winStart = (r.failOffset >= 16) ? (r.failOffset - 16) : 0;
            r.failWindowHex = HexSlice(bytes, winStart, 80);
            if (r.failOffset < bytes.size()) {
                const size_t avail = std::min<size_t>(48u, bytes.size() - r.failOffset);
                r.failAsciiPreview = AsciiPreview(bytes.data() + r.failOffset, avail);
            }
        }
    }

    // 2. Run read-only backup-engine SAnimationSaveLoad.cpp specification verifier.
    r.sourceSpec = VerifyAgainstBackupEngineSourceSpec(bytes, r.outerVersion);

    // 3. Evaluate payload cipher probes on unsupported outer versions (or when requested).
    if (!outerSupported || populateDetailedProbes) {
        static constexpr ProbeCipher kAllCiphers[] = {
            ProbeCipher::None,
            ProbeCipher::CompByte,
            ProbeCipher::ByteCryptOld,
            ProbeCipher::ByteCryptRan2,
            ProbeCipher::ByteCryptBin1,
            ProbeCipher::ByteCryptBin2,
            ProbeCipher::ByteCryptContainer1,
            ProbeCipher::ByteCryptContainer2,
        };
        const uint8_t* payload = bytes.data() + RANMobile::ep7byte::Stream::kHeaderSize;
        const size_t payloadLen = bytes.size() - RANMobile::ep7byte::Stream::kHeaderSize;
        for (ProbeCipher c : kAllCiphers) {
            CipherProbeMatch pm = EvaluateCipherProbe(payload, payloadLen, c);
            if (pm.looksLikeTrackList && r.matchingCipherName.empty()) {
                r.matchingCipherName = ProbeCipherName(c);
            }
            if (populateDetailedProbes) {
                r.cipherProbes.push_back(std::move(pm));
            }
        }
    }

    return r;
}

inline bool ReadFileBytes(const fs::path& p, std::vector<uint8_t>& out) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const auto sz = f.tellg();
    if (sz < 0) return false;
    out.resize(static_cast<size_t>(sz));
    f.seekg(0);
    return sz == 0 || static_cast<bool>(f.read(reinterpret_cast<char*>(out.data()), sz));
}

inline CompanionCfgMeta ParseCompanionCfg(const std::string& sourceLabel,
                                          const std::vector<uint8_t>& bytes) {
    CompanionCfgMeta m;
    m.found = true;
    m.source = sourceLabel;
    m.byteSize = bytes.size();
    m.sha256 = Sha256::DigestHex(bytes);
    if (bytes.size() >= RANMobile::ep7byte::Stream::kHeaderSize) {
        size_t len = 0;
        while (len < 128 && bytes[len] != 0) ++len;
        m.fileType.assign(reinterpret_cast<const char*>(bytes.data()), len);
        m.rawVersion = ReadLE32(bytes.data() + 128);
    }
    const RANMobile::EP7AnimInfo info =
        RANMobile::EP7AnimationReader::ParseInfo(bytes, true, false);
    m.parsedValid = info.valid;
    m.unsupported = info.unsupported;
    m.skeleton = info.skeleton;
    m.flag = info.flag;
    m.startTime = info.startTime;
    m.endTime = info.endTime;
    m.unitTime = info.unitTime;
    return m;
}

struct SurveySummary {
    size_t totalLooseBin = 0;
    size_t totalAnimDirBin = 0;
    size_t totalNonAnimDirBin = 0;
    size_t totalRccBin = 0;
    size_t totalCfgIndexed = 0;

    // Tree-wide B11 buckets
    size_t countParsed = 0;
    size_t countUnsupportedOuter = 0;
    size_t countInvalidHeader = 0;
    size_t countUnrecognizedType = 0;
    size_t countParserRejected = 0;

    // data/animation/ scoped B11 buckets (reconciles directly with verify_ep7_character_assets)
    size_t animDirParsed = 0;
    size_t animDirUnsupportedOuter = 0;
    size_t animDirInvalidHeader = 0;
    size_t animDirUnrecognizedType = 0;
    size_t animDirParserRejected = 0;

    size_t animContainerZeroPadded = 0;
    size_t animContainerDirtyPadded = 0;

    std::map<uint32_t, size_t> animOuterVersionCounts;
    std::map<std::string, size_t> outerVersionByBucket;
    std::map<std::string, size_t> binVsCfgVersion;

    // Unmodified reader metrics on accepted files
    std::map<size_t, size_t> acceptedTrailingBytes;
    std::map<uint32_t, size_t> acceptedTrackVersions;

    // 1,209 parser-rejected breakdown
    std::map<std::string, size_t> rejectedReasonBreakdown;

    // Source-derived shadow verification across all AnimContainer files
    std::map<std::string, size_t> sourceSpecOutcomeByOuter;
    std::map<std::string, size_t> sourceSpecTrackVerByOuter;
    std::map<std::string, size_t> sourceSpecKeyChannelsByOuter;
    size_t container0103Total = 0;
    size_t container0103GarbageFormulaExact = 0;
    size_t container0104List0NonEmpty = 0;
    size_t container0104List1NonEmpty = 0;
    size_t container0104List2NonEmpty = 0;
    size_t filesWithEmptyBoneNames = 0;
    size_t acceptedFilesWithMatrixKeysAffectedBy68ByteBug = 0;

    // Cipher probe match counts per unsupported outer version
    std::map<std::string, size_t> unsupportedCipherHits;

    // Selected representative authentic samples (populated with full probes)
    std::vector<BinInspectionRecord> samples;
    bool sampleHashesUnchangedAfterRun = true;
};

inline std::string StemLower(const std::string& filename) {
    std::string base = filename;
    const auto slash = base.find_last_of("/\\");
    if (slash != std::string::npos) base = base.substr(slash + 1);
    const auto dot = base.find_last_of('.');
    if (dot != std::string::npos) base = base.substr(0, dot);
    return LowerAscii(base);
}

inline SurveySummary RunDirectorySurvey(const fs::path& rootDir, size_t maxSamplesPerBucket = 3) {
    SurveySummary sum;

    std::vector<fs::path> rccFiles;
    std::vector<fs::path> binFiles;
    std::vector<fs::path> looseCfgFiles;

    std::error_code ec;
    for (fs::recursive_directory_iterator it(rootDir, ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        const std::string ext = LowerAscii(it->path().extension().string());
        if (ext == ".rcc") rccFiles.push_back(it->path());
        else if (ext == ".bin") binFiles.push_back(it->path());
        else if (ext == ".cfg") looseCfgFiles.push_back(it->path());
    }
    std::sort(rccFiles.begin(), rccFiles.end());
    std::sort(binFiles.begin(), binFiles.end());
    std::sort(looseCfgFiles.begin(), looseCfgFiles.end());

    std::map<std::string, CompanionCfgMeta> cfgByStem;
    struct RccBinEntry {
        std::string relLabel;
        std::vector<uint8_t> bytes;
    };
    std::vector<RccBinEntry> rccBins;

    for (const fs::path& rccPath : rccFiles) {
        std::vector<uint8_t> arcBytes;
        if (!ReadFileBytes(rccPath, arcBytes)) continue;
        RANMobile::EP7RccArchive arc;
        if (!arc.Open(arcBytes)) continue;
        const std::string relArc = fs::relative(rccPath, rootDir, ec).generic_string();
        for (const auto& entry : arc.Entries()) {
            const std::string ext = LowerAscii(fs::path(entry.name).extension().string());
            if (ext == ".cfg") {
                std::vector<uint8_t> cfgBytes;
                if (arc.Extract(entry, cfgBytes)) {
                    cfgByStem[StemLower(entry.name)] =
                        ParseCompanionCfg("rcc:" + relArc + "#" + entry.name, cfgBytes);
                }
            } else if (ext == ".bin") {
                std::vector<uint8_t> b;
                if (arc.Extract(entry, b)) {
                    rccBins.push_back({relArc + "#" + entry.name, std::move(b)});
                }
            }
        }
    }
    for (const fs::path& cfgPath : looseCfgFiles) {
        std::vector<uint8_t> cfgBytes;
        if (!ReadFileBytes(cfgPath, cfgBytes)) continue;
        const std::string relCfg = fs::relative(cfgPath, rootDir, ec).generic_string();
        cfgByStem[StemLower(relCfg)] = ParseCompanionCfg("loose:" + relCfg, cfgBytes);
    }
    sum.totalCfgIndexed = cfgByStem.size();
    sum.totalLooseBin = binFiles.size();
    sum.totalRccBin = rccBins.size();

    static const std::set<std::string> kB11NamedSamples = {
        "a_3boss_01_stay.bin", "a_gbox_stay.bin",    "a_gum_att01.bin", "a_gum_gumkyuck.bin",
        "a_gum_jingong.bin",   "a_gum_lose.bin",     "a_gum_mix.bin",   "a_gum_nanpi.bin"};

    struct BucketBestSmallest {
        size_t byteSize = static_cast<size_t>(-1);
        fs::path fullPath;
        std::string relPath;
        bool fromRcc = false;
        std::vector<uint8_t> rccBytes;
    };
    std::map<std::string, size_t> bucketSampleCounts;
    std::map<std::string, BucketBestSmallest> smallestPerBucket;
    std::set<std::string> selectedSampleRelPaths;
    std::vector<std::pair<fs::path, std::string>> verifyUnchangedLooseSamples;

    auto processOne = [&](const std::string& relPath, const fs::path* diskPath,
                          const std::vector<uint8_t>& bytes, bool fromRcc) {
        BinInspectionRecord rec = InspectSingleBin(relPath, bytes, fromRcc, false);

        if (rec.inDataAnimationDir) sum.totalAnimDirBin++;
        else sum.totalNonAnimDirBin++;

        const auto cfgIt = cfgByStem.find(StemLower(relPath));
        if (cfgIt != cfgByStem.end()) rec.cfg = cfgIt->second;

        switch (rec.bucket) {
            case BinBucket::Parsed:
                sum.countParsed++;
                if (rec.inDataAnimationDir) sum.animDirParsed++;
                break;
            case BinBucket::UnsupportedOuterVersion:
                sum.countUnsupportedOuter++;
                if (rec.inDataAnimationDir) sum.animDirUnsupportedOuter++;
                break;
            case BinBucket::InvalidHeader:
                sum.countInvalidHeader++;
                if (rec.inDataAnimationDir) sum.animDirInvalidHeader++;
                break;
            case BinBucket::UnrecognizedType:
                sum.countUnrecognizedType++;
                if (rec.inDataAnimationDir) sum.animDirUnrecognizedType++;
                break;
            case BinBucket::ParserRejected:
                sum.countParserRejected++;
                if (rec.inDataAnimationDir) sum.animDirParserRejected++;
                break;
        }

        const std::string verStr = HexU32(rec.outerVersion);
        const std::string bucketKey = verStr + ":" + BinBucketName(rec.bucket);
        if (rec.fileType == "AnimContainer") {
            if (rec.zeroPaddedHeader) sum.animContainerZeroPadded++;
            else sum.animContainerDirtyPadded++;
            sum.animOuterVersionCounts[rec.outerVersion]++;
            sum.outerVersionByBucket[bucketKey]++;

            const std::string cfgVerStr =
                rec.cfg.found ? HexU32(rec.cfg.rawVersion) : std::string("no-cfg");
            sum.binVsCfgVersion[verStr + " -> cfg " + cfgVerStr]++;
        }

        // Source-spec shadow verification aggregation
        bool forceAnomalySample = false;
        if (rec.sourceSpec.attempted) {
            std::string specOutcome;
            if (!rec.sourceSpec.ok) {
                specOutcome = "FAIL (" + rec.sourceSpec.note + ")";
                forceAnomalySample = true;
            } else if (!rec.sourceSpec.exactEof) {
                specOutcome = "OK_WITH_TRAILING (" + std::to_string(rec.sourceSpec.trailingBytes) + "B)";
                forceAnomalySample = true;
            } else {
                specOutcome = "EXACT_EOF";
            }
            sum.sourceSpecOutcomeByOuter[verStr + " : " + specOutcome]++;
            for (const auto& tkv : rec.sourceSpec.trackVersions) {
                sum.sourceSpecTrackVerByOuter[verStr + " -> track " + HexU32(tkv.first)] +=
                    tkv.second;
            }
            if (rec.sourceSpec.tracksPos > 0)
                sum.sourceSpecKeyChannelsByOuter[verStr + " : pos_files"]++;
            if (rec.sourceSpec.tracksRot > 0)
                sum.sourceSpecKeyChannelsByOuter[verStr + " : rot_files"]++;
            if (rec.sourceSpec.tracksScale > 0)
                sum.sourceSpecKeyChannelsByOuter[verStr + " : scale_files"]++;
            if (rec.sourceSpec.tracksQuatPos > 0)
                sum.sourceSpecKeyChannelsByOuter[verStr + " : quatpos_files"]++;
            if (rec.sourceSpec.tracksMatrix > 0)
                sum.sourceSpecKeyChannelsByOuter[verStr + " : matrix_files"]++;
            if (rec.sourceSpec.emptyBoneNames > 0) {
                sum.filesWithEmptyBoneNames++;
            }
            if (rec.bucket == BinBucket::Parsed && rec.sourceSpec.tracksMatrix > 0 &&
                rec.trailingBytes > 0) {
                sum.acceptedFilesWithMatrixKeysAffectedBy68ByteBug++;
            }
            if (rec.outerVersion == 0x0103u && rec.sourceSpec.ok) {
                sum.container0103Total++;
                if (rec.sourceSpec.container0103GarbageMatchesFormula) {
                    sum.container0103GarbageFormulaExact++;
                }
            }
            if (rec.outerVersion == 0x0104u && rec.sourceSpec.ok) {
                if (rec.sourceSpec.list0Count > 0) sum.container0104List0NonEmpty++;
                if (rec.sourceSpec.list1Count > 0) sum.container0104List1NonEmpty++;
                if (rec.sourceSpec.list2Count > 0) sum.container0104List2NonEmpty++;
            }
        }

        std::string sampleSubKey = bucketKey;
        if (rec.bucket == BinBucket::Parsed) {
            sum.acceptedTrailingBytes[rec.trailingBytes]++;
            for (const auto& kv : rec.trackVersionsSeen) {
                sum.acceptedTrackVersions[kv.first] += kv.second;
            }
        } else if (rec.bucket == BinBucket::ParserRejected) {
            const std::string idxTag = (rec.failTrackIndex == 0) ? "idx=0" : "idx>0";
            const std::string reasonKey = verStr + " | list=" + rec.failList + " | " + idxTag +
                                          " | trackVer=" + HexU32(rec.failTrackVersion) +
                                          " | err=" + rec.readerError;
            sum.rejectedReasonBreakdown[reasonKey]++;
            sampleSubKey = reasonKey;
        } else if (rec.bucket == BinBucket::UnsupportedOuterVersion) {
            const std::string hit =
                rec.matchingCipherName.empty() ? "none" : rec.matchingCipherName;
            sum.unsupportedCipherHits[verStr + " | probe=" + hit]++;
            sampleSubKey = verStr + "|probe=" + hit;
        }

        if (rec.fileType == "AnimContainer") {
            auto& sm = smallestPerBucket[sampleSubKey];
            if (bytes.size() < sm.byteSize) {
                sm.byteSize = bytes.size();
                sm.relPath = relPath;
                sm.fromRcc = fromRcc;
                if (diskPath) sm.fullPath = *diskPath;
                else sm.rccBytes = bytes;
            }
        }

        const std::string baseLower = LowerAscii(fs::path(relPath).filename().string());
        const bool isNamedB11 = (kB11NamedSamples.count(baseLower) > 0);
        const bool shouldSample =
            rec.fileType == "AnimContainer" &&
            (isNamedB11 || forceAnomalySample || bucketSampleCounts[sampleSubKey] < maxSamplesPerBucket);
        if (shouldSample) {
            if (!isNamedB11 && !forceAnomalySample) bucketSampleCounts[sampleSubKey]++;
            if (selectedSampleRelPaths.insert(relPath).second) {
                BinInspectionRecord fullRec = InspectSingleBin(relPath, bytes, fromRcc, true);
                if (cfgIt != cfgByStem.end()) fullRec.cfg = cfgIt->second;
                sum.samples.push_back(std::move(fullRec));
                if (diskPath) verifyUnchangedLooseSamples.emplace_back(*diskPath, rec.sha256);
            }
        }
    };

    for (const fs::path& p : binFiles) {
        std::vector<uint8_t> bytes;
        if (!ReadFileBytes(p, bytes)) continue;
        const std::string rel = fs::relative(p, rootDir, ec).generic_string();
        processOne(rel, &p, bytes, false);
    }
    for (const auto& rb : rccBins) {
        processOne(rb.relLabel, nullptr, rb.bytes, true);
    }

    for (const auto& kv : smallestPerBucket) {
        const auto& sm = kv.second;
        if (sm.relPath.empty() || !selectedSampleRelPaths.insert(sm.relPath).second) continue;
        std::vector<uint8_t> bytes;
        if (sm.fromRcc) {
            bytes = sm.rccBytes;
        } else {
            if (!ReadFileBytes(sm.fullPath, bytes)) continue;
        }
        BinInspectionRecord fullRec = InspectSingleBin(sm.relPath, bytes, sm.fromRcc, true);
        const auto cfgIt = cfgByStem.find(StemLower(sm.relPath));
        if (cfgIt != cfgByStem.end()) fullRec.cfg = cfgIt->second;
        if (!sm.fromRcc) verifyUnchangedLooseSamples.emplace_back(sm.fullPath, fullRec.sha256);
        sum.samples.push_back(std::move(fullRec));
    }

    std::sort(sum.samples.begin(), sum.samples.end(),
              [](const BinInspectionRecord& a, const BinInspectionRecord& b) {
                  return a.relPath < b.relPath;
              });

    for (const auto& pair : verifyUnchangedLooseSamples) {
        std::vector<uint8_t> afterBytes;
        if (!ReadFileBytes(pair.first, afterBytes) ||
            Sha256::DigestHex(afterBytes) != pair.second) {
            sum.sampleHashesUnchangedAfterRun = false;
        }
    }

    return sum;
}

inline std::string FormatJsonReport(const SurveySummary& s) {
    std::ostringstream o;
    o << "{\n";
    o << "  \"total_loose_bin\": " << s.totalLooseBin << ",\n";
    o << "  \"total_data_animation_bin\": " << s.totalAnimDirBin << ",\n";
    o << "  \"total_non_animation_bin\": " << s.totalNonAnimDirBin << ",\n";
    o << "  \"total_rcc_bin\": " << s.totalRccBin << ",\n";
    o << "  \"total_cfg_indexed\": " << s.totalCfgIndexed << ",\n";
    o << "  \"data_animation_b11_buckets\": {\n";
    o << "    \"parsed\": " << s.animDirParsed << ",\n";
    o << "    \"unsupported_version\": " << s.animDirUnsupportedOuter << ",\n";
    o << "    \"invalid_header\": " << s.animDirInvalidHeader << ",\n";
    o << "    \"unrecognized_type\": " << s.animDirUnrecognizedType << ",\n";
    o << "    \"parser_rejected\": " << s.animDirParserRejected << "\n";
    o << "  },\n";
    o << "  \"tree_wide_b11_buckets\": {\n";
    o << "    \"parsed\": " << s.countParsed << ",\n";
    o << "    \"unsupported_version\": " << s.countUnsupportedOuter << ",\n";
    o << "    \"invalid_header\": " << s.countInvalidHeader << ",\n";
    o << "    \"unrecognized_type\": " << s.countUnrecognizedType << ",\n";
    o << "    \"parser_rejected\": " << s.countParserRejected << "\n";
    o << "  },\n";
    o << "  \"anim_container_header_padding\": {\n";
    o << "    \"zero_padded\": " << s.animContainerZeroPadded << ",\n";
    o << "    \"nonzero_trailing_bytes\": " << s.animContainerDirtyPadded << "\n";
    o << "  },\n";
    o << "  \"sample_hashes_unchanged_after_run\": "
      << (s.sampleHashesUnchangedAfterRun ? "true" : "false") << ",\n";

    auto emitMapStr = [&](const char* key, const std::map<std::string, size_t>& m, bool comma) {
        o << "  \"" << key << "\": {\n";
        size_t idx = 0;
        for (const auto& kv : m) {
            o << "    \"" << JsonEscape(kv.first) << "\": " << kv.second
              << (++idx < m.size() ? "," : "") << "\n";
        }
        o << "  }" << (comma ? "," : "") << "\n";
    };

    std::map<std::string, size_t> verStrMap;
    for (const auto& kv : s.animOuterVersionCounts) verStrMap[HexU32(kv.first)] = kv.second;
    emitMapStr("anim_outer_version_counts", verStrMap, true);
    emitMapStr("outer_version_by_bucket", s.outerVersionByBucket, true);
    emitMapStr("bin_vs_cfg_version", s.binVsCfgVersion, true);

    std::map<std::string, size_t> trailMap;
    for (const auto& kv : s.acceptedTrailingBytes) trailMap[std::to_string(kv.first)] = kv.second;
    emitMapStr("accepted_trailing_bytes", trailMap, true);

    std::map<std::string, size_t> accTrkVer;
    for (const auto& kv : s.acceptedTrackVersions) accTrkVer[HexU32(kv.first)] = kv.second;
    emitMapStr("accepted_track_versions", accTrkVer, true);

    emitMapStr("rejected_reason_breakdown", s.rejectedReasonBreakdown, true);
    emitMapStr("source_spec_shadow_outcomes", s.sourceSpecOutcomeByOuter, true);
    emitMapStr("source_spec_track_versions_by_outer", s.sourceSpecTrackVerByOuter, true);
    emitMapStr("source_spec_key_channels_by_outer", s.sourceSpecKeyChannelsByOuter, true);

    o << "  \"source_spec_invariants\": {\n";
    o << "    \"container_0103_total\": " << s.container0103Total << ",\n";
    o << "    \"container_0103_garbage_formula_exact\": "
      << s.container0103GarbageFormulaExact << ",\n";
    o << "    \"container_0104_list0_main_nonempty\": " << s.container0104List0NonEmpty << ",\n";
    o << "    \"container_0104_list1_upbody1_nonempty\": " << s.container0104List1NonEmpty << ",\n";
    o << "    \"container_0104_list2_upbody2_nonempty\": " << s.container0104List2NonEmpty << ",\n";
    o << "    \"files_with_empty_bone_names\": " << s.filesWithEmptyBoneNames << ",\n";
    o << "    \"accepted_files_with_matrix_keys_affected_by_68byte_bug\": "
      << s.acceptedFilesWithMatrixKeysAffectedBy68ByteBug << "\n";
    o << "  },\n";

    emitMapStr("unsupported_cipher_probe_hits", s.unsupportedCipherHits, true);

    o << "  \"samples\": [\n";
    for (size_t i = 0; i < s.samples.size(); ++i) {
        const auto& r = s.samples[i];
        o << "    {\n";
        o << "      \"path\": \"" << JsonEscape(r.relPath) << "\",\n";
        o << "      \"byte_size\": " << r.byteSize << ",\n";
        o << "      \"sha256\": \"" << r.sha256 << "\",\n";
        o << "      \"file_type\": \"" << JsonEscape(r.fileType) << "\",\n";
        o << "      \"zero_padded_header\": " << (r.zeroPaddedHeader ? "true" : "false") << ",\n";
        o << "      \"outer_version\": \"" << HexU32(r.outerVersion) << "\",\n";
        o << "      \"bucket\": \"" << BinBucketName(r.bucket) << "\",\n";
        o << "      \"reader_valid\": " << (r.readerValid ? "true" : "false") << ",\n";
        o << "      \"reader_error\": \"" << JsonEscape(r.readerError) << "\",\n";
        o << "      \"fail_list\": \"" << JsonEscape(r.failList) << "\",\n";
        o << "      \"fail_track_index\": " << r.failTrackIndex << ",\n";
        o << "      \"fail_offset\": " << r.failOffset << ",\n";
        o << "      \"fail_track_version\": \"" << HexU32(r.failTrackVersion) << "\",\n";
        o << "      \"fail_window_hex\": \"" << r.failWindowHex << "\",\n";
        o << "      \"source_spec_ok\": " << (r.sourceSpec.ok ? "true" : "false") << ",\n";
        o << "      \"source_spec_exact_eof\": " << (r.sourceSpec.exactEof ? "true" : "false") << ",\n";
        o << "      \"source_spec_lists\": [" << r.sourceSpec.list0Count << ", "
          << r.sourceSpec.list1Count << ", " << r.sourceSpec.list2Count << "],\n";
        o << "      \"source_spec_empty_bones\": " << r.sourceSpec.emptyBoneNames << ",\n";
        o << "      \"source_spec_note\": \"" << JsonEscape(r.sourceSpec.note) << "\",\n";
        o << "      \"header_prefix16_hex\": \"" << r.headerPrefix16Hex << "\",\n";
        o << "      \"payload_prefix64_hex\": \"" << r.payloadPrefix64Hex << "\",\n";
        o << "      \"matching_cipher_probe\": \"" << JsonEscape(r.matchingCipherName) << "\",\n";
        o << "      \"cfg\": {\n";
        o << "        \"found\": " << (r.cfg.found ? "true" : "false") << ",\n";
        o << "        \"source\": \"" << JsonEscape(r.cfg.source) << "\",\n";
        o << "        \"byte_size\": " << r.cfg.byteSize << ",\n";
        o << "        \"sha256\": \"" << r.cfg.sha256 << "\",\n";
        o << "        \"version\": \"" << HexU32(r.cfg.rawVersion) << "\",\n";
        o << "        \"skeleton\": \"" << JsonEscape(r.cfg.skeleton) << "\",\n";
        o << "        \"flag\": \"" << HexU32(r.cfg.flag) << "\",\n";
        o << "        \"start_time\": " << r.cfg.startTime << ",\n";
        o << "        \"end_time\": " << r.cfg.endTime << ",\n";
        o << "        \"unit_time\": " << r.cfg.unitTime << "\n";
        o << "      }\n";
        o << "    }" << (i + 1 < s.samples.size() ? "," : "") << "\n";
    }
    o << "  ]\n";
    o << "}\n";
    return o.str();
}

inline void PrintTextSummary(const SurveySummary& s) {
    std::printf("=== Phase 3J B12 Authentic Animation Container Survey ===\n");
    std::printf("data/animation .bin=%zu  non-animation .bin=%zu  rcc .bin=%zu  indexed .cfg=%zu\n",
                s.totalAnimDirBin, s.totalNonAnimDirBin, s.totalRccBin, s.totalCfgIndexed);
    std::printf("data/animation B11 buckets: parsed=%zu unsupported-version=%zu "
                "invalid-header=%zu unrecognized-type=%zu parser-rejected=%zu\n",
                s.animDirParsed, s.animDirUnsupportedOuter, s.animDirInvalidHeader,
                s.animDirUnrecognizedType, s.animDirParserRejected);
    std::printf("AnimContainer 128B header padding: zero-padded=%zu nonzero-trailing=%zu\n",
                s.animContainerZeroPadded, s.animContainerDirtyPadded);
    std::printf("asset SHA-256 unchanged after inspection: %s\n",
                s.sampleHashesUnchangedAfterRun ? "YES" : "NO");

    std::printf("\n--- AnimContainer outer version x B11 bucket ---\n");
    for (const auto& kv : s.outerVersionByBucket) {
        std::printf("  %6zu x %s\n", kv.second, kv.first.c_str());
    }

    std::printf("\n--- Unmodified reader accepted (%zu) metrics ---\n", s.animDirParsed);
    for (const auto& kv : s.acceptedTrailingBytes) {
        std::printf("  trailing_bytes=%zu : %zu files\n", kv.first, kv.second);
    }
    for (const auto& kv : s.acceptedTrackVersions) {
        std::printf("  track_version=%s : %zu tracks\n", HexU32(kv.first).c_str(), kv.second);
    }
    std::printf("  accepted files with m_cMatrixKeys>0 affected by 68B-vs-80B bug: %zu\n",
                s.acceptedFilesWithMatrixKeysAffectedBy68ByteBug);

    std::printf("\n--- Unmodified reader parser-rejected (%zu) root-cause breakdown ---\n",
                s.animDirParserRejected);
    for (const auto& kv : s.rejectedReasonBreakdown) {
        std::printf("  %6zu x %s\n", kv.second, kv.first.c_str());
    }

    std::printf("\n--- Backup-engine SAnimationSaveLoad.cpp shadow verification ---\n");
    for (const auto& kv : s.sourceSpecOutcomeByOuter) {
        std::printf("  %6zu x %s\n", kv.second, kv.first.c_str());
    }
    std::printf("  0x0103 inter-list DWORD == (0x0103 + upBody + main): %zu / %zu\n",
                s.container0103GarbageFormulaExact, s.container0103Total);
    std::printf("  0x0104 non-empty list counts: list0(main)=%zu list1(upBody1)=%zu list2(upBody2)=%zu\n",
                s.container0104List0NonEmpty, s.container0104List1NonEmpty,
                s.container0104List2NonEmpty);
    std::printf("  files containing >=1 track with empty m_strBone: %zu\n",
                s.filesWithEmptyBoneNames);

    std::printf("\n--- Source-spec track versions by outer container version ---\n");
    for (const auto& kv : s.sourceSpecTrackVerByOuter) {
        std::printf("  %6zu tracks : %s\n", kv.second, kv.first.c_str());
    }

    std::printf("\n--- Source-spec keyframe channel presence (files) by outer version ---\n");
    for (const auto& kv : s.sourceSpecKeyChannelsByOuter) {
        std::printf("  %6zu files  : %s\n", kv.second, kv.first.c_str());
    }
}

}  // namespace EP7AnimInspect

int main(int argc, char** argv) {
    if (!EP7AnimInspect::Sha256::SelfTest()) {
        std::fprintf(stderr, "FATAL: SHA-256 self-test failed\n");
        return 2;
    }
    if (argc < 2) {
        std::fprintf(stderr,
                     "usage: %s <client_root> [--json <report.json>] [--samples-tsv <samples.tsv>]\n",
                     argv[0]);
        return 2;
    }

    const fs::path rootDir = argv[1];
    std::string jsonOutPath;
    std::string samplesTsvPath;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--json" && i + 1 < argc) {
            jsonOutPath = argv[++i];
        } else if (arg == "--samples-tsv" && i + 1 < argc) {
            samplesTsvPath = argv[++i];
        } else {
            std::fprintf(stderr, "unknown argument: %s\n", arg.c_str());
            return 2;
        }
    }

    if (!fs::exists(rootDir)) {
        std::fprintf(stderr, "root path does not exist: %s\n", rootDir.string().c_str());
        return 3;
    }

    const EP7AnimInspect::SurveySummary summary = EP7AnimInspect::RunDirectorySurvey(rootDir);
    EP7AnimInspect::PrintTextSummary(summary);

    if (!jsonOutPath.empty()) {
        std::ofstream jf(jsonOutPath, std::ios::binary);
        if (!jf) {
            std::fprintf(stderr, "failed to write JSON output: %s\n", jsonOutPath.c_str());
            return 2;
        }
        const std::string js = EP7AnimInspect::FormatJsonReport(summary);
        jf.write(js.data(), static_cast<std::streamsize>(js.size()));
    }

    if (!samplesTsvPath.empty()) {
        std::ofstream tf(samplesTsvPath, std::ios::binary);
        if (!tf) {
            std::fprintf(stderr, "failed to write TSV output: %s\n", samplesTsvPath.c_str());
            return 2;
        }
        for (const auto& r : summary.samples) {
            if (r.fromRcc) continue;
            tf << r.relPath << "\t" << r.sha256 << "\t" << r.byteSize << "\t"
               << EP7AnimInspect::HexU32(r.outerVersion) << "\t"
               << EP7AnimInspect::BinBucketName(r.bucket) << "\n";
        }
    }

    if (!summary.sampleHashesUnchangedAfterRun) {
        std::fprintf(stderr, "FATAL: sample SHA-256 changed after inspection!\n");
        return 4;
    }
    return 0;
}
