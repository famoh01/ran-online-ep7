#include "EP7SerialFile.h"

#include <cstring>

namespace RANMobile {

namespace {

bool IsPrintableAscii(unsigned char ch) {
    return ch >= 0x20u && ch <= 0x7Eu;
}

} // namespace

EP7SerialFileView EP7SerialFile::ReadHeaderOnly(const std::vector<uint8_t>& data) {
    EP7SerialFileView view;
    view.totalBytes = data.size();
    if (data.size() < kHeaderSize) {
        view.error = "TRUNCATED SERIAL FILE HEADER (expected >= 132 bytes, got " +
                     std::to_string(data.size()) + ")";
        return view;
    }

    const void* nulPos = std::memchr(data.data(), '\0', kFileTypeSize);
    if (!nulPos) {
        view.error = "UNTERMINATED SERIAL FILE TYPE IN 128-BYTE HEADER";
        return view;
    }

    const size_t typeLen = static_cast<size_t>(
        static_cast<const uint8_t*>(nulPos) - data.data());
    if (typeLen == 0) {
        view.error = "EMPTY SERIAL FILE TYPE";
        return view;
    }

    for (size_t i = 0; i < typeLen; ++i) {
        if (!IsPrintableAscii(data[i])) {
            view.error = "INVALID NON-ASCII BYTE IN SERIAL FILE TYPE";
            return view;
        }
    }

    view.fileType.assign(reinterpret_cast<const char*>(data.data()), typeLen);
    view.version = static_cast<uint32_t>(data[kFileTypeSize]) |
                   (static_cast<uint32_t>(data[kFileTypeSize + 1]) << 8) |
                   (static_cast<uint32_t>(data[kFileTypeSize + 2]) << 16) |
                   (static_cast<uint32_t>(data[kFileTypeSize + 3]) << 24);
    view.valid = true;
    return view;
}

EP7SerialFileView EP7SerialFile::Read(const std::vector<uint8_t>& data,
                                      EP7Serial::EP7ByteCryptMode cryptMode) {
    EP7SerialFileView view = ReadHeaderOnly(data);
    if (!view.valid) {
        return view;
    }

    view.cryptMode = cryptMode;
    view.payload.assign(data.begin() + static_cast<std::ptrdiff_t>(kHeaderSize), data.end());
    if (!view.payload.empty() && cryptMode != EP7Serial::EP7ByteCryptMode::None) {
        EP7Serial::DecodeInPlace(view.payload.data(), view.payload.size(), cryptMode);
    }
    return view;
}

EP7SerialFileView EP7SerialFile::Read(const std::vector<uint8_t>& data) {
    EP7SerialFileView header = ReadHeaderOnly(data);
    if (!header.valid) {
        return header;
    }
    const EP7Serial::EP7ByteCryptMode mode = IsEncodedVersion(header.version)
        ? EP7Serial::EP7ByteCryptMode::Old
        : EP7Serial::EP7ByteCryptMode::None;
    return Read(data, mode);
}

bool EP7SerialFile::ReadU8(const std::vector<uint8_t>& payload, size_t& offset, uint8_t& out) {
    if (offset >= payload.size()) return false;
    out = payload[offset++];
    return true;
}

bool EP7SerialFile::ReadU16(const std::vector<uint8_t>& payload, size_t& offset, uint16_t& out) {
    if (offset > payload.size() || payload.size() - offset < 2) return false;
    out = static_cast<uint16_t>(payload[offset]) |
          static_cast<uint16_t>(static_cast<uint16_t>(payload[offset + 1]) << 8);
    offset += 2;
    return true;
}

bool EP7SerialFile::ReadU32(const std::vector<uint8_t>& payload, size_t& offset, uint32_t& out) {
    if (offset > payload.size() || payload.size() - offset < 4) return false;
    out = static_cast<uint32_t>(payload[offset]) |
          (static_cast<uint32_t>(payload[offset + 1]) << 8) |
          (static_cast<uint32_t>(payload[offset + 2]) << 16) |
          (static_cast<uint32_t>(payload[offset + 3]) << 24);
    offset += 4;
    return true;
}

bool EP7SerialFile::ReadBoolStrict(const std::vector<uint8_t>& payload, size_t& offset, bool& out) {
    uint8_t raw = 0;
    if (!ReadU8(payload, offset, raw)) return false;
    if (raw > 1u) return false;
    out = (raw != 0u);
    return true;
}

bool EP7SerialFile::ReadString(const std::vector<uint8_t>& payload, size_t& offset,
                               std::string& out, size_t maxLength,
                               std::string* errorOut) {
    uint32_t length = 0;
    if (!ReadU32(payload, offset, length)) {
        if (errorOut) *errorOut = "TRUNCATED STRING LENGTH PREFIX";
        return false;
    }
    if (length < 1u) {
        if (errorOut) *errorOut = "MALFORMED STRING LENGTH (0, missing NUL terminator)";
        return false;
    }
    if (length > maxLength) {
        if (errorOut) {
            *errorOut = "STRING LENGTH EXCEEDS LIMIT (" + std::to_string(length) +
                        " > " + std::to_string(maxLength) + ")";
        }
        return false;
    }
    if (offset > payload.size() || length > payload.size() - offset) {
        if (errorOut) {
            *errorOut = "TRUNCATED STRING PAYLOAD (need " + std::to_string(length) +
                        " bytes, remaining " +
                        std::to_string(offset <= payload.size() ? payload.size() - offset : 0) + ")";
        }
        return false;
    }
    if (payload[offset + length - 1] != 0u) {
        if (errorOut) *errorOut = "STRING MISSING TERMINATING NUL";
        return false;
    }
    for (size_t i = 0; i + 1 < length; ++i) {
        if (payload[offset + i] == 0u) {
            if (errorOut) *errorOut = "EMBEDDED NUL INSIDE DECLARED STRING";
            return false;
        }
    }
    out.assign(reinterpret_cast<const char*>(payload.data() + offset), length - 1);
    offset += length;
    return true;
}

} // namespace RANMobile
