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
//      invalid-header, unrecognized-type, parser-rejected).
//   3. Traces 0x0100..0x0102 track streams step-by-step to pinpoint exact rejection
//      sites (list, track index, byte offset, 32-bit track version word, surrounding
//      bytes, trailing bytes on accepted files, and read-only SAnimation::Load_0100
//      compatibility check from enginelib/Meshs/SAnimation.cpp:1418-1462).
//   4. Evaluates read-only payload-decoding probes (raw, compbyte, and verified
//      backup-repo ByteCrypt substitution tables) on unsupported outer versions
//      (0x0103, 0x0104, 0x0200, ...) and correlates .bin versions against companion
//      .cfg versions from loose files and Animation.rcc.
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
// Used strictly as read-only diagnostic probes to identify payload encodings.
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
                if (c < 0x20) {
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
    uint32_t d0 = 0;  // payload[0..3]
    uint32_t d1 = 0;  // payload[4..7]
    uint32_t d2 = 0;  // payload[8..11]
    uint32_t d3 = 0;  // payload[12..15]
    bool looksLikeTrackList = false;
    std::string decodedPrefixHex;
    std::string decodedAscii;
};

struct BinInspectionRecord {
    std::string relPath;
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

    // Step-by-step trace on 0x0100..0x0102
    uint32_t upBodyDeclared = 0;
    uint32_t mainDeclared = 0;
    uint32_t upBodyParsed = 0;
    uint32_t mainParsed = 0;
    size_t bytesConsumed = 0;
    size_t trailingBytes = 0;
    std::map<uint32_t, uint32_t> trackVersionsSeen;
    uint32_t tracksWithPos = 0;
    uint32_t tracksWithRot = 0;
    uint32_t tracksWithScale = 0;
    uint32_t tracksWithQuatPos = 0;
    uint32_t tracksWithMatrix = 0;

    // Rejection pinpoint (when bucket == ParserRejected)
    std::string failList;        // "upBody" or "main"
    uint32_t failTrackIndex = 0;
    size_t failOffset = 0;
    uint32_t failTrackVersion = 0;
    uint32_t failNextDword = 0;
    std::string failWindowHex;
    std::string failAsciiPreview;

    // Read-only SAnimation::Load_0100 shadow test (enginelib/Meshs/SAnimation.cpp:1418-1462)
    bool load0100ShadowTested = false;
    bool load0100ShadowValidToEof = false;
    std::string load0100ShadowNote;

    // Hex slices for reproducible format analysis
    std::string headerPrefix16Hex;
    std::string headerTail16Hex;     // [116..132)
    std::string payloadPrefix64Hex;  // [132..196)

    // Read-only payload probes (populated for samples & unsupported versions)
    std::string matchingCipherName;
    std::vector<CipherProbeMatch> cipherProbes;

    CompanionCfgMeta cfg;
};

// ---------------------------------------------------------------------------
// Step-by-step read-only shadow walker for 0x0100..0x0102 containers.
// Mirrors EP7AnimationReader::ReadBoneTrack + optionally tests SAnimation::Load_0100
// (enginelib/Meshs/SAnimation.cpp:1418-1462) when allowTrack0100Shadow == true.
// ---------------------------------------------------------------------------
inline bool ShadowSkipBoneTrack(RANMobile::ep7byte::Stream& s, bool allowTrack0100Shadow,
                                BinInspectionRecord* rec, const char* listName,
                                uint32_t trackIndex) {
    static constexpr uint32_t kMaxKeys = 262144;
    const size_t trackStart = s.Position();
    uint32_t version = 0;
    if (!s.ReadU32(version)) {
        if (rec && !allowTrack0100Shadow) {
            rec->failList = listName;
            rec->failTrackIndex = trackIndex;
            rec->failOffset = trackStart;
        }
        return false;
    }

    auto recordKeyUsage = [&](uint32_t nPos, uint32_t nRot, uint32_t nScale,
                              uint32_t nQuat, uint32_t nMat) {
        if (!rec || allowTrack0100Shadow) return;
        rec->trackVersionsSeen[version]++;
        if (nPos > 0) rec->tracksWithPos++;
        if (nRot > 0) rec->tracksWithRot++;
        if (nScale > 0) rec->tracksWithScale++;
        if (nQuat > 0) rec->tracksWithQuatPos++;
        if (nMat > 0) rec->tracksWithMatrix++;
    };

    if (version == 0x0102u) {
        std::string bone;
        uint32_t nRot = 0, nPos = 0, g1 = 0, nQuat = 0, g2 = 0, nMat = 0, nScale = 0;
        if (!s.ReadString(bone)) return false;
        if (!s.ReadU32(nRot) || nRot > kMaxKeys || !s.Skip(static_cast<size_t>(nRot) * 20u)) return false;
        if (!s.ReadU32(nPos) || nPos > kMaxKeys || !s.Skip(static_cast<size_t>(nPos) * 16u)) return false;
        if (!s.ReadU32(g1)) return false;
        if (!s.ReadU32(nQuat) || nQuat > kMaxKeys || !s.Skip(static_cast<size_t>(nQuat) * 24u)) return false;
        if (!s.ReadU32(g2)) return false;
        if (!s.ReadU32(nMat) || nMat > kMaxKeys || !s.Skip(static_cast<size_t>(nMat) * 80u)) return false;
        if (!s.ReadU32(nScale) || nScale > kMaxKeys || !s.Skip(static_cast<size_t>(nScale) * 16u)) return false;
        recordKeyUsage(nPos, nRot, nScale, nQuat, nMat);
        return true;
    }

    if (version == 0x0101u) {
        std::string bone;
        uint32_t nPos = 0, nRot = 0, nScale = 0, nQuat = 0, nMat = 0;
        if (!s.ReadU32(nPos) || nPos > kMaxKeys || !s.Skip(static_cast<size_t>(nPos) * 16u)) return false;
        if (!s.ReadU32(nRot) || nRot > kMaxKeys || !s.Skip(static_cast<size_t>(nRot) * 20u)) return false;
        if (!s.ReadU32(nScale) || nScale > kMaxKeys || !s.Skip(static_cast<size_t>(nScale) * 16u)) return false;
        if (!s.ReadU32(nQuat) || nQuat > kMaxKeys || !s.Skip(static_cast<size_t>(nQuat) * 24u)) return false;
        if (!s.ReadU32(nMat) || nMat > kMaxKeys || !s.Skip(static_cast<size_t>(nMat) * 80u)) return false;
        if (!s.ReadString(bone)) return false;
        recordKeyUsage(nPos, nRot, nScale, nQuat, nMat);
        return true;
    }

    if (allowTrack0100Shadow && version == 0x0100u) {
        // SAnimation::Load_0100 (enginelib/Meshs/SAnimation.cpp:1418-1462):
        // PositionKeys (16B), RotateKeys (20B), ScaleKeys (16B), MatrixKeys (80B), m_strBone.
        // No QuatPosKeys and no garbage DWORDs.
        std::string bone;
        uint32_t nPos = 0, nRot = 0, nScale = 0, nMat = 0;
        if (!s.ReadU32(nPos) || nPos > kMaxKeys || !s.Skip(static_cast<size_t>(nPos) * 16u)) return false;
        if (!s.ReadU32(nRot) || nRot > kMaxKeys || !s.Skip(static_cast<size_t>(nRot) * 20u)) return false;
        if (!s.ReadU32(nScale) || nScale > kMaxKeys || !s.Skip(static_cast<size_t>(nScale) * 16u)) return false;
        if (!s.ReadU32(nMat) || nMat > kMaxKeys || !s.Skip(static_cast<size_t>(nMat) * 80u)) return false;
        if (!s.ReadString(bone)) return false;
        return !bone.empty();
    }

    if (rec && !allowTrack0100Shadow) {
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

inline bool ShadowWalkContainer(const std::vector<uint8_t>& bytes, uint32_t outerVersion,
                                bool allowTrack0100Shadow, BinInspectionRecord* rec,
                                size_t* outConsumed) {
    static constexpr uint32_t kMaxTracks = 512;
    RANMobile::ep7byte::Stream s;
    s.Attach(bytes, false);
    std::string hdrType;
    uint32_t hdrVer = 0;
    if (!s.ReadFileType(hdrType, hdrVer)) return false;

    auto walkList = [&](const char* name, uint32_t* declaredOut, uint32_t* parsedOut) -> bool {
        const size_t cntPos = s.Position();
        uint32_t count = 0;
        if (!s.ReadU32(count) || count > kMaxTracks) {
            if (rec && !allowTrack0100Shadow) {
                rec->failList = name;
                rec->failOffset = cntPos;
                rec->failTrackVersion = count;
            }
            return false;
        }
        if (declaredOut && !allowTrack0100Shadow) *declaredOut = count;
        for (uint32_t i = 0; i < count; ++i) {
            if (!ShadowSkipBoneTrack(s, allowTrack0100Shadow, rec, name, i)) return false;
            if (parsedOut && !allowTrack0100Shadow) *parsedOut = i + 1;
        }
        return true;
    };

    bool ok = false;
    if (outerVersion == 0x0102u) {
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
    if (ok && outConsumed) *outConsumed = s.Position();
    return ok;
}

// Evaluates whether a decoded post-header payload starts with a plausible track list or
// block-sized header without modifying anything.
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
        // Heuristic indicator ONLY for diagnostic reporting:
        // Case A: d0 == 0 (empty first list) and d1 <= 256 (second list count)
        // Case B: 1 <= d0 <= 256 (list count) and 0x0100 <= d1 <= 0x0205 (track version)
        const bool caseEmptyFirst =
            (m.d0 == 0 && m.d1 <= 256u &&
             (m.d1 == 0 || (m.d2 >= 0x0100u && m.d2 <= 0x0205u)));
        const bool caseNonEmptyFirst =
            (m.d0 >= 1u && m.d0 <= 256u && m.d1 >= 0x0100u && m.d1 <= 0x0205u);
        m.looksLikeTrackList = caseEmptyFirst || caseNonEmptyFirst;
    }
    return m;
}

inline BinInspectionRecord InspectSingleBin(const std::string& relPath,
                                            const std::vector<uint8_t>& bytes,
                                            bool fromRcc,
                                            bool populateDetailedProbes) {
    BinInspectionRecord r;
    r.relPath = relPath;
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

    // Invoke UNMODIFIED production EP7AnimationReader::ParseContainer.
    const RANMobile::EP7AnimClip clip =
        RANMobile::EP7AnimationReader::ParseContainer(bytes, true, false);
    r.readerValid = clip.valid;
    r.readerError = clip.error;

    const bool outerSupported =
        (r.outerVersion == 0x0100u || r.outerVersion == 0x0101u || r.outerVersion == 0x0102u);

    if (!outerSupported) {
        r.bucket = BinBucket::UnsupportedOuterVersion;
    } else if (clip.valid) {
        r.bucket = BinBucket::Parsed;
    } else {
        r.bucket = BinBucket::ParserRejected;
    }

    if (outerSupported) {
        size_t consumed = 0;
        const bool shadowOk = ShadowWalkContainer(bytes, r.outerVersion, false, &r, &consumed);
        if (shadowOk) {
            r.bytesConsumed = consumed;
            r.trailingBytes = bytes.size() - consumed;
        } else if (r.bucket == BinBucket::ParserRejected) {
            const size_t winStart = (r.failOffset >= 16) ? (r.failOffset - 16) : 0;
            r.failWindowHex = HexSlice(bytes, winStart, 80);
            if (r.failOffset < bytes.size()) {
                const size_t avail = std::min<size_t>(48u, bytes.size() - r.failOffset);
                r.failAsciiPreview = AsciiPreview(bytes.data() + r.failOffset, avail);
            }
            if (r.failTrackVersion == 0x0100u) {
                r.load0100ShadowTested = true;
                size_t end0100 = 0;
                if (ShadowWalkContainer(bytes, r.outerVersion, true, nullptr, &end0100)) {
                    r.load0100ShadowValidToEof = (end0100 == bytes.size());
                    r.load0100ShadowNote =
                        (end0100 == bytes.size())
                            ? "SAnimation::Load_0100 parses cleanly to exact EOF"
                            : "SAnimation::Load_0100 parses tracks with trailing bytes";
                } else {
                    r.load0100ShadowNote = "SAnimation::Load_0100 failed mid-stream";
                }
            }
        }
    }

    // Evaluate payload cipher probes on unsupported outer versions (or when requested).
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
    size_t totalRccBin = 0;
    size_t totalCfgIndexed = 0;

    size_t countParsed = 0;
    size_t countUnsupportedOuter = 0;
    size_t countInvalidHeader = 0;
    size_t countUnrecognizedType = 0;
    size_t countParserRejected = 0;

    size_t zeroPaddedHeaders = 0;
    size_t dirtyPaddedHeaders = 0;

    std::map<uint32_t, size_t> outerVersionCounts;
    std::map<std::string, size_t> outerVersionByBucket;
    std::map<std::string, size_t> binVsCfgVersion;

    // Accepted container metrics
    std::map<size_t, size_t> acceptedTrailingBytes;
    std::map<uint32_t, size_t> acceptedTrackVersions;
    size_t acceptedTracksPos = 0;
    size_t acceptedTracksRot = 0;
    size_t acceptedTracksScale = 0;
    size_t acceptedTracksQuatPos = 0;
    size_t acceptedTracksMatrix = 0;

    // 1,209 parser-rejected breakdown
    std::map<std::string, size_t> rejectedReasonBreakdown;
    size_t rejectedLoad0100Tested = 0;
    size_t rejectedLoad0100ExactEof = 0;

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

    // Index companion .cfg files from .rcc packs first, then loose .cfg files.
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

        const auto cfgIt = cfgByStem.find(StemLower(relPath));
        if (cfgIt != cfgByStem.end()) rec.cfg = cfgIt->second;

        if (rec.hasFullHeader) {
            if (rec.zeroPaddedHeader) sum.zeroPaddedHeaders++;
            else sum.dirtyPaddedHeaders++;
            sum.outerVersionCounts[rec.outerVersion]++;
        }

        switch (rec.bucket) {
            case BinBucket::Parsed: sum.countParsed++; break;
            case BinBucket::UnsupportedOuterVersion: sum.countUnsupportedOuter++; break;
            case BinBucket::InvalidHeader: sum.countInvalidHeader++; break;
            case BinBucket::UnrecognizedType: sum.countUnrecognizedType++; break;
            case BinBucket::ParserRejected: sum.countParserRejected++; break;
        }

        const std::string verStr = HexU32(rec.outerVersion);
        const std::string bucketKey = verStr + ":" + BinBucketName(rec.bucket);
        sum.outerVersionByBucket[bucketKey]++;

        const std::string cfgVerStr =
            rec.cfg.found ? HexU32(rec.cfg.rawVersion) : std::string("no-cfg");
        sum.binVsCfgVersion[verStr + " -> cfg " + cfgVerStr]++;

        std::string sampleSubKey = bucketKey;
        if (rec.bucket == BinBucket::Parsed) {
            sum.acceptedTrailingBytes[rec.trailingBytes]++;
            for (const auto& kv : rec.trackVersionsSeen) {
                sum.acceptedTrackVersions[kv.first] += kv.second;
            }
            sum.acceptedTracksPos += rec.tracksWithPos;
            sum.acceptedTracksRot += rec.tracksWithRot;
            sum.acceptedTracksScale += rec.tracksWithScale;
            sum.acceptedTracksQuatPos += rec.tracksWithQuatPos;
            sum.acceptedTracksMatrix += rec.tracksWithMatrix;
        } else if (rec.bucket == BinBucket::ParserRejected) {
            const std::string idxTag = (rec.failTrackIndex == 0) ? "idx=0" : "idx>0";
            const std::string reasonKey = verStr + " | list=" + rec.failList + " | " + idxTag +
                                          " | trackVer=" + HexU32(rec.failTrackVersion) +
                                          " | err=" + rec.readerError;
            sum.rejectedReasonBreakdown[reasonKey]++;
            sampleSubKey = reasonKey;
            if (rec.load0100ShadowTested) {
                sum.rejectedLoad0100Tested++;
                if (rec.load0100ShadowValidToEof) sum.rejectedLoad0100ExactEof++;
            }
        } else if (rec.bucket == BinBucket::UnsupportedOuterVersion) {
            const std::string hit =
                rec.matchingCipherName.empty() ? "none" : rec.matchingCipherName;
            sum.unsupportedCipherHits[verStr + " | probe=" + hit]++;
            sampleSubKey = verStr + "|probe=" + hit;
        }

        // Track smallest file per sub-bucket so CI can inspect compact authentic samples too.
        auto& sm = smallestPerBucket[sampleSubKey];
        if (bytes.size() < sm.byteSize) {
            sm.byteSize = bytes.size();
            sm.relPath = relPath;
            sm.fromRcc = fromRcc;
            if (diskPath) sm.fullPath = *diskPath;
            else sm.rccBytes = bytes;
        }

        const std::string baseLower = LowerAscii(fs::path(relPath).filename().string());
        const bool isNamedB11 = (kB11NamedSamples.count(baseLower) > 0);
        if (isNamedB11 || bucketSampleCounts[sampleSubKey] < maxSamplesPerBucket) {
            if (!isNamedB11) bucketSampleCounts[sampleSubKey]++;
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

    // Also include the smallest authentic file in each sub-bucket if not already sampled.
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

    // Prove zero mutation of sampled loose files on disk.
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
    o << "  \"total_rcc_bin\": " << s.totalRccBin << ",\n";
    o << "  \"total_cfg_indexed\": " << s.totalCfgIndexed << ",\n";
    o << "  \"b11_buckets\": {\n";
    o << "    \"parsed\": " << s.countParsed << ",\n";
    o << "    \"unsupported_version\": " << s.countUnsupportedOuter << ",\n";
    o << "    \"invalid_header\": " << s.countInvalidHeader << ",\n";
    o << "    \"unrecognized_type\": " << s.countUnrecognizedType << ",\n";
    o << "    \"parser_rejected\": " << s.countParserRejected << "\n";
    o << "  },\n";
    o << "  \"header_padding\": {\n";
    o << "    \"zero_padded\": " << s.zeroPaddedHeaders << ",\n";
    o << "    \"nonzero_trailing_bytes\": " << s.dirtyPaddedHeaders << "\n";
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
    for (const auto& kv : s.outerVersionCounts) verStrMap[HexU32(kv.first)] = kv.second;
    emitMapStr("outer_version_counts", verStrMap, true);
    emitMapStr("outer_version_by_bucket", s.outerVersionByBucket, true);
    emitMapStr("bin_vs_cfg_version", s.binVsCfgVersion, true);

    std::map<std::string, size_t> trailMap;
    for (const auto& kv : s.acceptedTrailingBytes) trailMap[std::to_string(kv.first)] = kv.second;
    emitMapStr("accepted_trailing_bytes", trailMap, true);

    std::map<std::string, size_t> accTrkVer;
    for (const auto& kv : s.acceptedTrackVersions) accTrkVer[HexU32(kv.first)] = kv.second;
    emitMapStr("accepted_track_versions", accTrkVer, true);

    o << "  \"accepted_key_channels\": {\n";
    o << "    \"tracks_with_pos\": " << s.acceptedTracksPos << ",\n";
    o << "    \"tracks_with_rot\": " << s.acceptedTracksRot << ",\n";
    o << "    \"tracks_with_scale\": " << s.acceptedTracksScale << ",\n";
    o << "    \"tracks_with_quatpos\": " << s.acceptedTracksQuatPos << ",\n";
    o << "    \"tracks_with_matrix\": " << s.acceptedTracksMatrix << "\n";
    o << "  },\n";

    emitMapStr("rejected_reason_breakdown", s.rejectedReasonBreakdown, true);
    o << "  \"rejected_load0100_shadow\": {\n";
    o << "    \"tested\": " << s.rejectedLoad0100Tested << ",\n";
    o << "    \"valid_to_exact_eof\": " << s.rejectedLoad0100ExactEof << "\n";
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
        o << "      \"upbody_declared\": " << r.upBodyDeclared << ",\n";
        o << "      \"main_declared\": " << r.mainDeclared << ",\n";
        o << "      \"bytes_consumed\": " << r.bytesConsumed << ",\n";
        o << "      \"trailing_bytes\": " << r.trailingBytes << ",\n";
        o << "      \"fail_list\": \"" << JsonEscape(r.failList) << "\",\n";
        o << "      \"fail_track_index\": " << r.failTrackIndex << ",\n";
        o << "      \"fail_offset\": " << r.failOffset << ",\n";
        o << "      \"fail_track_version\": \"" << HexU32(r.failTrackVersion) << "\",\n";
        o << "      \"fail_next_dword\": \"" << HexU32(r.failNextDword) << "\",\n";
        o << "      \"fail_window_hex\": \"" << r.failWindowHex << "\",\n";
        o << "      \"fail_ascii_preview\": \"" << JsonEscape(r.failAsciiPreview) << "\",\n";
        o << "      \"load0100_shadow_tested\": "
          << (r.load0100ShadowTested ? "true" : "false") << ",\n";
        o << "      \"load0100_shadow_valid_to_eof\": "
          << (r.load0100ShadowValidToEof ? "true" : "false") << ",\n";
        o << "      \"load0100_shadow_note\": \"" << JsonEscape(r.load0100ShadowNote) << "\",\n";
        o << "      \"header_prefix16_hex\": \"" << r.headerPrefix16Hex << "\",\n";
        o << "      \"header_tail16_hex\": \"" << r.headerTail16Hex << "\",\n";
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
        o << "      },\n";
        o << "      \"cipher_probes\": [\n";
        for (size_t j = 0; j < r.cipherProbes.size(); ++j) {
            const auto& cp = r.cipherProbes[j];
            o << "        {\"cipher\": \"" << ProbeCipherName(cp.cipher)
              << "\", \"d0\": \"" << HexU32(cp.d0)
              << "\", \"d1\": \"" << HexU32(cp.d1)
              << "\", \"d2\": \"" << HexU32(cp.d2)
              << "\", \"d3\": \"" << HexU32(cp.d3)
              << "\", \"track_like\": " << (cp.looksLikeTrackList ? "true" : "false")
              << ", \"hex32\": \"" << cp.decodedPrefixHex
              << "\", \"ascii\": \"" << JsonEscape(cp.decodedAscii) << "\"}"
              << (j + 1 < r.cipherProbes.size() ? "," : "") << "\n";
        }
        o << "      ]\n";
        o << "    }" << (i + 1 < s.samples.size() ? "," : "") << "\n";
    }
    o << "  ]\n";
    o << "}\n";
    return o.str();
}

inline void PrintTextSummary(const SurveySummary& s) {
    std::printf("=== Phase 3J B12 Authentic Animation Container Survey ===\n");
    std::printf("loose .bin=%zu  rcc .bin=%zu  indexed .cfg=%zu\n",
                s.totalLooseBin, s.totalRccBin, s.totalCfgIndexed);
    std::printf("B11 buckets: parsed=%zu unsupported-version=%zu invalid-header=%zu "
                "unrecognized-type=%zu parser-rejected=%zu\n",
                s.countParsed, s.countUnsupportedOuter, s.countInvalidHeader,
                s.countUnrecognizedType, s.countParserRejected);
    std::printf("header 128B padding: zero-padded=%zu nonzero-trailing=%zu\n",
                s.zeroPaddedHeaders, s.dirtyPaddedHeaders);
    std::printf("asset SHA-256 unchanged after inspection: %s\n",
                s.sampleHashesUnchangedAfterRun ? "YES" : "NO");

    std::printf("\n--- Outer version x bucket ---\n");
    for (const auto& kv : s.outerVersionByBucket) {
        std::printf("  %6zu x %s\n", kv.second, kv.first.c_str());
    }

    std::printf("\n--- Outer .bin version -> companion .cfg version ---\n");
    for (const auto& kv : s.binVsCfgVersion) {
        std::printf("  %6zu x %s\n", kv.second, kv.first.c_str());
    }

    std::printf("\n--- Accepted (.bin parsed=%zu) metrics ---\n", s.countParsed);
    for (const auto& kv : s.acceptedTrailingBytes) {
        std::printf("  trailing_bytes=%zu : %zu files\n", kv.first, kv.second);
    }
    for (const auto& kv : s.acceptedTrackVersions) {
        std::printf("  track_version=%s : %zu tracks\n", HexU32(kv.first).c_str(), kv.second);
    }
    std::printf("  channels: pos=%zu rot=%zu scale=%zu quatpos=%zu matrix=%zu\n",
                s.acceptedTracksPos, s.acceptedTracksRot, s.acceptedTracksScale,
                s.acceptedTracksQuatPos, s.acceptedTracksMatrix);

    std::printf("\n--- Parser-rejected (%zu) root-cause breakdown ---\n", s.countParserRejected);
    for (const auto& kv : s.rejectedReasonBreakdown) {
        std::printf("  %6zu x %s\n", kv.second, kv.first.c_str());
    }
    std::printf("  SAnimation::Load_0100 shadow test: tested=%zu valid_to_exact_eof=%zu\n",
                s.rejectedLoad0100Tested, s.rejectedLoad0100ExactEof);

    std::printf("\n--- Unsupported outer version (%zu) cipher probe hits ---\n",
                s.countUnsupportedOuter);
    for (const auto& kv : s.unsupportedCipherHits) {
        std::printf("  %6zu x %s\n", kv.second, kv.first.c_str());
    }

    std::printf("\n--- Representative authentic samples (%zu) ---\n", s.samples.size());
    for (const auto& r : s.samples) {
        std::printf("  [%s] %s (%zu B, sha256=%s, outer=%s, cfg=%s)\n",
                    BinBucketName(r.bucket), r.relPath.c_str(), r.byteSize,
                    r.sha256.c_str(), HexU32(r.outerVersion).c_str(),
                    r.cfg.found ? HexU32(r.cfg.rawVersion).c_str() : "none");
        if (r.bucket == BinBucket::ParserRejected) {
            std::printf("      fail: list=%s idx=%u/%u off=%zu trackVer=%s next=%s load0100Eof=%s\n",
                        r.failList.c_str(), r.failTrackIndex,
                        (r.failList == "upBody" ? r.upBodyDeclared : r.mainDeclared),
                        r.failOffset, HexU32(r.failTrackVersion).c_str(),
                        HexU32(r.failNextDword).c_str(),
                        r.load0100ShadowValidToEof ? "YES" : "NO");
            std::printf("      winHex=%s\n", r.failWindowHex.c_str());
        } else if (r.bucket == BinBucket::UnsupportedOuterVersion) {
            std::printf("      payload64=%s probeMatch=%s\n",
                        r.payloadPrefix64Hex.c_str(),
                        r.matchingCipherName.empty() ? "none" : r.matchingCipherName.c_str());
        }
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
