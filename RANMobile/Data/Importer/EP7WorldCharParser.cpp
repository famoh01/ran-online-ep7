#include "EP7WorldCharParser.h"
#include <cstring>

namespace RANMobile {

bool BinaryReader::ReadU32(uint32_t& out) {
    if (!CanRead(4)) return false;
    std::memcpy(&out, m_buf.data() + m_pos, 4);
    m_pos += 4;
    return true;
}

bool BinaryReader::ReadBytes(void* dst, size_t bytes) {
    if (!CanRead(bytes)) return false;
    if (bytes > 0 && dst) {
        std::memcpy(dst, m_buf.data() + m_pos, bytes);
    }
    m_pos += bytes;
    return true;
}

bool BinaryReader::ReadLengthPrefixedString(std::string& out, size_t maxLen) {
    uint32_t len = 0;
    if (!ReadU32(len)) return false;
    if (len > maxLen || !CanRead(len)) return false;
    out.assign(reinterpret_cast<const char*>(m_buf.data() + m_pos), len);
    m_pos += len;
    return true;
}

EP7WldHeader EP7WorldCharParser::ParseWldHeader(const std::vector<uint8_t>& data) {
    EP7WldHeader hdr;
    BinaryReader reader(data);
    if (!reader.CanRead(132)) {
        hdr.error = "Truncated .wld header (< 132 bytes)";
        return hdr;
    }
    char buf[129]{};
    reader.ReadBytes(buf, 128);
    buf[128] = '\0';
    hdr.fileType = std::string(buf);
    if (hdr.fileType.empty()) {
        hdr.error = "Invalid .wld fileType (empty)";
        return hdr;
    }
    reader.ReadU32(hdr.version);
    if (hdr.version < WLD_MIN_VERSION || hdr.version > WLD_MAX_VERSION) {
        hdr.error = "Unsupported .wld version";
        return hdr;
    }
    hdr.valid = true;
    return hdr;
}

EP7ChfHeader EP7WorldCharParser::ParseChfHeader(const std::vector<uint8_t>& data) {
    EP7ChfHeader hdr;
    BinaryReader reader(data);
    if (!reader.ReadU32(hdr.version)) {
        hdr.error = "Truncated .chf header (< 4 bytes)";
        return hdr;
    }
    if (hdr.version != 0x0100 && hdr.version != 0x0101 && hdr.version != 0x0102 && hdr.version != 0x0103) {
        hdr.error = "Unsupported .chf version";
        return hdr;
    }
    hdr.valid = true;
    return hdr;
}

EP7CpsHeader EP7WorldCharParser::ParseCpsHeader(const std::vector<uint8_t>& data) {
    EP7CpsHeader hdr;
    BinaryReader reader(data);
    if (!reader.ReadU32(hdr.version)) {
        hdr.error = "Truncated .cps header (< 4 bytes)";
        return hdr;
    }
    if (hdr.version == 0 || hdr.version > CPS_VERSION) {
        hdr.error = "Unsupported .cps version";
        return hdr;
    }
    hdr.valid = true;
    return hdr;
}

} // namespace RANMobile
