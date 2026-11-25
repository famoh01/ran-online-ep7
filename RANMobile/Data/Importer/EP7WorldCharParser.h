#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace RANMobile {

class BinaryReader {
public:
    explicit BinaryReader(const std::vector<uint8_t>& buf) : m_buf(buf), m_pos(0) {}

    bool CanRead(size_t bytes) const {
        return bytes <= (m_buf.size() - m_pos);
    }
    size_t Offset() const { return m_pos; }
    size_t Remaining() const { return m_buf.size() - m_pos; }

    bool Skip(size_t bytes) {
        if (!CanRead(bytes)) return false;
        m_pos += bytes;
        return true;
    }

    bool ReadU32(uint32_t& out);
    bool ReadBytes(void* dst, size_t bytes);
    bool ReadLengthPrefixedString(std::string& out, size_t maxLen = 4096);

private:
    const std::vector<uint8_t>& m_buf;
    size_t m_pos;
};

struct EP7WldHeader {
    std::string fileType;
    uint32_t version = 0;
    bool valid = false;
    std::string error;
};

struct EP7ChfHeader {
    uint32_t version = 0;
    bool valid = false;
    std::string error;
};

struct EP7CpsHeader {
    uint32_t version = 0;
    bool valid = false;
    std::string error;
};

struct EP7MeshGeometry {
    std::vector<float> vertices;
    std::vector<uint16_t> indices;
    bool valid = false;
};

class EP7WorldCharParser {
public:
    static constexpr uint32_t WLD_MIN_VERSION = 0x0108;
    static constexpr uint32_t WLD_MAX_VERSION = 0x0114; // DxLandMan::VERSION
    static constexpr uint32_t CPS_VERSION     = 0x00000107; // DxSkinPiece::VERSION

    static EP7WldHeader ParseWldHeader(const std::vector<uint8_t>& data);
    static EP7ChfHeader ParseChfHeader(const std::vector<uint8_t>& data);
    static EP7CpsHeader ParseCpsHeader(const std::vector<uint8_t>& data);
    static EP7MeshGeometry ExtractWldOctreeMesh(const std::vector<uint8_t>& bytes);
};

} // namespace RANMobile
