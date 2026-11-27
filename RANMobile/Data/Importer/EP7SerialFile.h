#pragma once

#include "EP7SerialCodec.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace RANMobile {

// Reader for the original CSerialFile container:
//   char m_szFileType[128] + DWORD m_FileID (version) + payload
// Traced directly to:
//   - Lib_Engine/Common/basestream.h (FILETYPESIZE = 128)
//   - Lib_Engine/Common/SerialFile.cpp:48-57 (ReadFileType)
//   - Lib_Engine/Common/SerialFile.cpp:259-277 (read / BYTECRYPT::byte_decode)
//   - Lib_Engine/Common/SerialFile.cpp:520-533 (operator>> std::string)
struct EP7SerialFileView {
    bool valid = false;
    std::string fileType;
    uint32_t version = 0;
    EP7Serial::EP7ByteCryptMode cryptMode = EP7Serial::EP7ByteCryptMode::None;
    std::vector<uint8_t> payload;
    size_t totalBytes = 0;
    std::string error;
};

class EP7SerialFile {
public:
    static constexpr size_t kFileTypeSize = 128;
    static constexpr size_t kHeaderSize = kFileTypeSize + 4;
    static constexpr uint32_t kVersionBeforeEncode = 0x0100;

    static EP7SerialFileView ReadHeaderOnly(const std::vector<uint8_t>& data);
    static EP7SerialFileView Read(const std::vector<uint8_t>& data,
                                  EP7Serial::EP7ByteCryptMode cryptMode);
    static EP7SerialFileView Read(const std::vector<uint8_t>& data);

    static bool IsEncodedVersion(uint32_t version) { return version >= kVersionBeforeEncode; }

    static bool ReadU8(const std::vector<uint8_t>& payload, size_t& offset, uint8_t& out);
    static bool ReadU16(const std::vector<uint8_t>& payload, size_t& offset, uint16_t& out);
    static bool ReadU32(const std::vector<uint8_t>& payload, size_t& offset, uint32_t& out);
    static bool ReadBoolStrict(const std::vector<uint8_t>& payload, size_t& offset, bool& out);

    // Reads a CSerialFile std::string: DWORD (length + 1), chars, terminating NUL.
    static bool ReadString(const std::vector<uint8_t>& payload, size_t& offset,
                           std::string& out, size_t maxLength = 4096,
                           std::string* errorOut = nullptr);
};

} // namespace RANMobile
