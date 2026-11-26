#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace RANMobile {

// Minimal reader for the original CSerialFile container:
//   char fileType[128] + DWORD version + payload
// Payload bytes are decoded with compbyte::decode() for versions >= 0x0100,
// matching GLLevelFile::LoadFile (RanClientLib/G-Logic/GLLevelFile.cpp:1359-1364)
// and CSerialFile::read (enginelib/Common/SerialFile.cpp:224-242).
struct EP7SerialFileView {
    bool valid = false;
    std::string fileType;
    uint32_t version = 0;
    std::vector<uint8_t> payload;
    std::string error;
};

class EP7SerialFile {
public:
    static constexpr size_t kFileTypeSize = 128;
    static constexpr uint32_t kVersionBeforeEncode = 0x0100;

    static EP7SerialFileView Read(const std::vector<uint8_t>& data);
    static bool IsEncodedVersion(uint32_t version) { return version >= kVersionBeforeEncode; }

    // Reads a CSerialFile std::string: DWORD length including NUL, then bytes.
    static bool ReadString(const std::vector<uint8_t>& payload, size_t& offset,
                           std::string& out, size_t maxLength = 4096);
    static bool ReadU32(const std::vector<uint8_t>& payload, size_t& offset, uint32_t& out);
};

} // namespace RANMobile
