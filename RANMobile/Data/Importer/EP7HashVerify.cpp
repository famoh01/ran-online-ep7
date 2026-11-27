#include "EP7HashVerify.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

namespace RANMobile {

namespace {

constexpr std::array<uint32_t, 64> kSha256K = {{
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
}};

inline uint32_t RotR(uint32_t v, uint32_t n) {
    return (v >> n) | (v << (32u - n));
}

void Sha256Transform(uint32_t state[8], const uint8_t block[64]) {
    uint32_t w[64]{};
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
               static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        const uint32_t s0 = RotR(w[i - 15], 7) ^ RotR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 = RotR(w[i - 2], 17) ^ RotR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; ++i) {
        const uint32_t S1 = RotR(e, 6) ^ RotR(e, 11) ^ RotR(e, 25);
        const uint32_t ch = (e & f) ^ ((~e) & g);
        const uint32_t temp1 = h + S1 + ch + kSha256K[i] + w[i];
        const uint32_t S0 = RotR(a, 2) ^ RotR(a, 13) ^ RotR(a, 22);
        const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temp2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

std::string TrimWhitespace(const std::string& value) {
    const size_t begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return std::string();
    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string BasenameOf(const std::string& path) {
    const size_t pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

std::vector<std::string> SplitTab(const std::string& line) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        const size_t tab = line.find('\t', start);
        if (tab == std::string::npos) {
            out.push_back(TrimWhitespace(line.substr(start)));
            break;
        }
        out.push_back(TrimWhitespace(line.substr(start, tab - start)));
        start = tab + 1;
    }
    return out;
}

bool ParseU64Strict(const std::string& value, uint64_t& out) {
    if (value.empty()) return false;
    for (unsigned char ch : value) {
        if (!std::isdigit(ch)) return false;
    }
    try {
        size_t idx = 0;
        const unsigned long long parsed = std::stoull(value, &idx, 10);
        if (idx != value.size()) return false;
        out = static_cast<uint64_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

std::string EP7HashVerify::LowerAscii(const std::string& value) {
    std::string out = value;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return out;
}

bool EP7HashVerify::IsHex64(const std::string& value) {
    if (value.size() != 64) return false;
    for (unsigned char ch : value) {
        if (!std::isxdigit(ch)) return false;
    }
    return true;
}

std::string EP7HashVerify::Sha256Hex(const uint8_t* data, size_t size) {
    if (size > 0 && !data) return std::string();
    uint32_t state[8] = {
        0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
        0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
    };

    uint8_t block[64]{};
    size_t offset = 0;
    while (size - offset >= 64) {
        Sha256Transform(state, data + offset);
        offset += 64;
    }

    const size_t rem = size - offset;
    if (rem > 0) std::memcpy(block, data + offset, rem);
    block[rem] = 0x80u;
    if (rem >= 56) {
        Sha256Transform(state, block);
        std::memset(block, 0, sizeof(block));
    }

    const uint64_t bitLen = static_cast<uint64_t>(size) * 8ULL;
    for (int i = 0; i < 8; ++i) {
        block[56 + i] = static_cast<uint8_t>((bitLen >> (56 - i * 8)) & 0xFFu);
    }
    Sha256Transform(state, block);

    static constexpr char kHex[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(64);
    for (int i = 0; i < 8; ++i) {
        for (int shift = 28; shift >= 0; shift -= 4) {
            hex.push_back(kHex[(state[i] >> shift) & 0xFu]);
        }
    }
    return hex;
}

std::string EP7HashVerify::Sha256Hex(const std::vector<uint8_t>& data) {
    return Sha256Hex(data.empty() ? nullptr : data.data(), data.size());
}

const EP7ManifestEntry* EP7Manifest::FindByPathOrName(const std::string& pathOrName) const {
    if (!loaded || pathOrName.empty()) return nullptr;
    const std::string targetLower = EP7HashVerify::LowerAscii(pathOrName);
    const std::string baseLower = EP7HashVerify::LowerAscii(BasenameOf(pathOrName));

    for (const EP7ManifestEntry& entry : entries) {
        if (entry.repoPath == pathOrName) return &entry;
    }
    for (const EP7ManifestEntry& entry : entries) {
        if (EP7HashVerify::LowerAscii(entry.repoPath) == targetLower) return &entry;
    }
    for (const EP7ManifestEntry& entry : entries) {
        if (BasenameOf(entry.repoPath) == BasenameOf(pathOrName)) return &entry;
    }
    for (const EP7ManifestEntry& entry : entries) {
        if (EP7HashVerify::LowerAscii(BasenameOf(entry.repoPath)) == baseLower) return &entry;
    }
    return nullptr;
}

EP7Manifest EP7HashVerify::LoadManifest(const std::string& manifestPath) {
    EP7Manifest manifest;
    manifest.path = manifestPath;

    std::ifstream in(manifestPath);
    if (!in) {
        manifest.error = "MANIFEST FILE UNREADABLE: " + manifestPath;
        return manifest;
    }

    std::string line;
    size_t lineNumber = 0;
    while (std::getline(in, line)) {
        ++lineNumber;
        const std::string trimmed = TrimWhitespace(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        const std::vector<std::string> cols = SplitTab(trimmed);
        if (lineNumber == 1 && cols.size() >= 4 && !IsHex64(cols[1])) {
            // Optional TSV header line (e.g. path\toid\tsha256\tsize...)
            continue;
        }
        if (cols.size() < 4) {
            manifest.error = "MALFORMED MANIFEST ROW AT LINE " + std::to_string(lineNumber);
            manifest.entries.clear();
            return manifest;
        }
        if (!IsHex64(cols[1]) || !IsHex64(cols[2])) {
            manifest.error = "INVALID SHA-256 HEX IN MANIFEST AT LINE " + std::to_string(lineNumber);
            manifest.entries.clear();
            return manifest;
        }
        const std::string oidLower = LowerAscii(cols[1]);
        const std::string shaLower = LowerAscii(cols[2]);
        if (oidLower != shaLower) {
            manifest.error = "MANIFEST OID/SHA-256 MISMATCH AT LINE " + std::to_string(lineNumber);
            manifest.entries.clear();
            return manifest;
        }
        uint64_t parsedSize = 0;
        if (!ParseU64Strict(cols[3], parsedSize)) {
            manifest.error = "INVALID BYTE SIZE IN MANIFEST AT LINE " + std::to_string(lineNumber);
            manifest.entries.clear();
            return manifest;
        }

        EP7ManifestEntry entry;
        entry.repoPath = cols[0];
        entry.oidSha256 = oidLower;
        entry.actualSha256 = shaLower;
        entry.byteSize = parsedSize;
        if (cols.size() > 4) entry.blobSha = LowerAscii(cols[4]);
        if (cols.size() > 5) entry.note = cols[5];
        if (cols.size() > 6) entry.localPath = cols[6];
        manifest.entries.push_back(std::move(entry));
    }

    if (manifest.entries.empty()) {
        manifest.error = "MANIFEST CONTAINS NO ENTRIES: " + manifestPath;
        return manifest;
    }

    manifest.loaded = true;
    return manifest;
}

EP7HashVerifyResult EP7HashVerify::VerifyBytesAgainstExpected(
    const std::vector<uint8_t>& data,
    const std::string& expectedSha256,
    uint64_t expectedSize) {
    EP7HashVerifyResult res;
    res.actualSize = static_cast<uint64_t>(data.size());
    res.expectedSize = expectedSize;
    res.actualSha256 = Sha256Hex(data);
    res.expectedSha256 = LowerAscii(TrimWhitespace(expectedSha256));

    if (!IsHex64(res.expectedSha256)) {
        res.error = "INVALID EXPECTED SHA-256 DIGEST";
        return res;
    }
    if (expectedSize > 0 && res.actualSize != expectedSize) {
        std::ostringstream oss;
        oss << "BYTE SIZE MISMATCH (actual=" << res.actualSize
            << " expected=" << expectedSize << ")";
        res.error = oss.str();
        return res;
    }
    if (res.actualSha256 != res.expectedSha256) {
        res.error = "SHA-256 MISMATCH (actual=" + res.actualSha256 +
                    " expected=" + res.expectedSha256 + ")";
        return res;
    }
    res.valid = true;
    return res;
}

EP7HashVerifyResult EP7HashVerify::VerifyBytesAgainstManifest(
    const EP7Manifest& manifest,
    const std::string& pathOrName,
    const std::vector<uint8_t>& data) {
    EP7HashVerifyResult res;
    res.actualSize = static_cast<uint64_t>(data.size());
    res.actualSha256 = Sha256Hex(data);

    if (!manifest.loaded) {
        res.error = manifest.error.empty() ? "MANIFEST NOT LOADED" : manifest.error;
        return res;
    }
    const EP7ManifestEntry* entry = manifest.FindByPathOrName(pathOrName);
    if (!entry) {
        res.error = "MANIFEST ENTRY NOT FOUND: " + pathOrName;
        return res;
    }
    res.matchedManifestPath = entry->repoPath;
    res.expectedSha256 = entry->oidSha256;
    res.expectedSize = entry->byteSize;

    if (res.actualSize != entry->byteSize) {
        std::ostringstream oss;
        oss << "MANIFEST SIZE MISMATCH for " << entry->repoPath
            << " (actual=" << res.actualSize << " expected=" << entry->byteSize << ")";
        res.error = oss.str();
        return res;
    }
    if (res.actualSha256 != entry->oidSha256) {
        res.error = "MANIFEST SHA-256 MISMATCH for " + entry->repoPath +
                    " (actual=" + res.actualSha256 + " expected=" + entry->oidSha256 + ")";
        return res;
    }
    res.valid = true;
    return res;
}

} // namespace RANMobile
