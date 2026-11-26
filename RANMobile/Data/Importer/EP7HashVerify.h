#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace RANMobile {

struct EP7ManifestEntry {
    std::string repoPath;
    std::string oidSha256;
    std::string actualSha256;
    uint64_t byteSize = 0;
    std::string blobSha;
    std::string note;
    std::string localPath;
};

struct EP7Manifest {
    bool loaded = false;
    std::string path;
    std::vector<EP7ManifestEntry> entries;
    std::string error;

    const EP7ManifestEntry* FindByPathOrName(const std::string& pathOrName) const;
};

struct EP7HashVerifyResult {
    bool valid = false;
    std::string actualSha256;
    std::string expectedSha256;
    uint64_t actualSize = 0;
    uint64_t expectedSize = 0;
    std::string matchedManifestPath;
    std::string error;
};

class EP7HashVerify {
public:
    static std::string Sha256Hex(const uint8_t* data, size_t size);
    static std::string Sha256Hex(const std::vector<uint8_t>& data);
    static bool IsHex64(const std::string& value);
    static std::string LowerAscii(const std::string& value);

    static EP7Manifest LoadManifest(const std::string& manifestPath);

    static EP7HashVerifyResult VerifyBytesAgainstExpected(
        const std::vector<uint8_t>& data,
        const std::string& expectedSha256,
        uint64_t expectedSize = 0);

    static EP7HashVerifyResult VerifyBytesAgainstManifest(
        const EP7Manifest& manifest,
        const std::string& pathOrName,
        const std::vector<uint8_t>& data);
};

} // namespace RANMobile
