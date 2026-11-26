#include "EP7SerialFile.h"
#include "EP7SerialCodec.h"

#include <cstring>

namespace RANMobile {

EP7SerialFileView EP7SerialFile::Read(const std::vector<uint8_t>& data) {
    EP7SerialFileView view;
    if (data.size() < kFileTypeSize + 4) {
        view.error = "TRUNCATED SERIAL FILE HEADER";
        return view;
    }
    char type[kFileTypeSize + 1]{};
    std::memcpy(type, data.data(), kFileTypeSize);
    view.fileType = std::string(type);
    std::memcpy(&view.version, data.data() + kFileTypeSize, 4);

    view.payload.assign(data.begin() + kFileTypeSize + 4, data.end());
    if (IsEncodedVersion(view.version)) EP7Serial::DecodeInPlace(view.payload.data(), view.payload.size());
    view.valid = !view.fileType.empty();
    return view;
}

bool EP7SerialFile::ReadU32(const std::vector<uint8_t>& payload, size_t& offset, uint32_t& out) {
    if (offset > payload.size() || payload.size() - offset < 4) return false;
    std::memcpy(&out, payload.data() + offset, 4);
    offset += 4;
    return true;
}

bool EP7SerialFile::ReadString(const std::vector<uint8_t>& payload, size_t& offset,
                               std::string& out, size_t maxLength) {
    uint32_t length = 0;
    if (!ReadU32(payload, offset, length)) return false;
    if (length < 1 || length > maxLength || length > payload.size() - offset) return false;
    // Original CSerialFile writes length()+1 including the terminating NUL.
    out.assign(reinterpret_cast<const char*>(payload.data() + offset), length - 1);
    offset += length;
    return true;
}

} // namespace RANMobile
