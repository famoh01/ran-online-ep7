#include "EP7WorldCharParser.h"
#include <cstring>

namespace RANMobile {

namespace {
inline uint32_t ReadU32LE(const uint8_t* p) {
    uint32_t v = 0;
    std::memcpy(&v, p, 4);
    return v;
}
} // namespace

EP7WldHeader EP7WorldCharParser::ParseWldHeader(const std::vector<uint8_t>& data) {
    EP7WldHeader hdr;
    // CSerialFile::GetFileType reads FILETYPESIZE (128 bytes) + DWORD dwVersion (4 bytes)
    if (data.size() < 132) return hdr;
    char buf[129]{};
    std::memcpy(buf, data.data(), 128);
    buf[128] = '\0';
    hdr.fileType = std::string(buf);
    hdr.version = ReadU32LE(data.data() + 128);
    hdr.valid = (!hdr.fileType.empty() && hdr.version >= WLD_MIN_VERSION && hdr.version <= WLD_MAX_VERSION);
    return hdr;
}

EP7ChfHeader EP7WorldCharParser::ParseChfHeader(const std::vector<uint8_t>& data) {
    EP7ChfHeader hdr;
    if (data.size() < 4) return hdr;
    hdr.version = ReadU32LE(data.data());
    hdr.valid = (hdr.version == 0x0100 || hdr.version == 0x0101 || hdr.version == 0x0102 || hdr.version == 0x0103);
    return hdr;
}

EP7CpsHeader EP7WorldCharParser::ParseCpsHeader(const std::vector<uint8_t>& data) {
    EP7CpsHeader hdr;
    if (data.size() < 4) return hdr;
    hdr.version = ReadU32LE(data.data());
    hdr.valid = (hdr.version > 0 && hdr.version <= CPS_VERSION);
    return hdr;
}

} // namespace RANMobile
