//
// EP7ByteFormat.h — Authentic RAN Online EP7 serialized-byte primitives (Phase 3J)
//
// This header ports the *exact* byte layout produced by the original PC engine
// serialization classes, so that mobile can read the original binary asset files.
//
// Source evidence (all in this repository, original PC source):
//   enginelib/Common/basestream.h            — FILETYPESIZE 128, EMSFILE_BLOCK, offset semantics
//   enginelib/Common/SerialFile.h/.cpp       — CSerialFile: operator<< / operator>> / std::string / vector
//   enginelib/Common/SerialMemory.h/.cpp     — CSerialMemory: header read + bounds behaviour
//   enginelib/Common/CompByte.cpp            — compbyte::encode/decode 256-byte permutation
//   enginelib/Common/SFileSystem.cpp         — pack/ZIP container used when bENGLIB_ZIPFILE is set
//   enginelib/G-Logic/GLogic.cpp             — GLOGIC::openfile_basestream (CSerialMemory vs CSerialFile)
//
// Verified facts that this header encodes as constants:
//   * `CSerialFile::ReadFileType()` reads FILETYPESIZE (128) bytes of file type, then a
//     DWORD file id (the asset "version").  `OpenFile()` then sets m_DefaultOffSet to the
//     current file offset, and `GetfTell()/SetOffSet()` are relative to that offset.
//     Consequence: every asset's version DWORD lives at absolute file offset 128.
//   * `CSerialMemory` performs the same `ReadFileType()` on the *decompressed* pack entry.
//   * `operator<<(const std::string&)` writes `DWORD(length + 1)` followed by the bytes.
//     (SerialFile.cpp, operator<<(const std::string &str)).
//   * Integer/float reads are raw memcpy of the native little-endian scalar.
//   * When `m_bencode` is true, *every* read/write block is passed through
//     compbyte::decode/encode (a fixed 256-entry byte permutation, not a cipher).
//
// No fabricated structures: the helpers below decode only what the original
// code demonstrably writes/reads.
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace RANMobile {
namespace ep7byte {

// ---------------------------------------------------------------------------
// ByteOrder / scalar readers
// ---------------------------------------------------------------------------

// RAN EP7 (Windows x86 / DirectX 9) data is little-endian. Android ARM64 is
// little-endian too, but we keep an explicit implementation so behaviour is
// identical in host unit tests on any platform.
inline uint16_t ReadU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8);
}

inline uint32_t ReadU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

inline int32_t ReadI32LE(const uint8_t* p) {
    return static_cast<int32_t>(ReadU32LE(p));
}

inline uint64_t ReadU64LE(const uint8_t* p) {
    return static_cast<uint64_t>(ReadU32LE(p)) |
           (static_cast<uint64_t>(ReadU32LE(p + 4)) << 32);
}

inline float ReadF32LE(const uint8_t* p) {
    const uint32_t bits = ReadU32LE(p);
    float f = 0.0f;
    static_assert(sizeof(float) == 4, "float must be 32-bit");
    std::memcpy(&f, &bits, sizeof(float));
    return f;
}

// ---------------------------------------------------------------------------
// compbyte — authentic byte permutation (enginelib/Common/CompByte.cpp)
// ---------------------------------------------------------------------------

// The exact ARRAY[256] table from CompByte.cpp (extracted programmatically and
// asserted to be a permutation of 0..255). encode: out[i] = ARRAY[in[i]].
inline const uint8_t* CompByteEncodeTable() {
    static const uint8_t kTable[256] = {
        0xCD, 0xF0, 0xA2, 0xE2, 0xED, 0x88, 0xE3, 0x98, 0xBC, 0x66, 0xC1, 0x7C,
        0xF4, 0xDB, 0x47, 0x96, 0xF6, 0x6C, 0x5E, 0x11, 0xAF, 0x2F, 0x40, 0x42,
        0x41, 0x07, 0xDE, 0x4C, 0x8A, 0x63, 0x4D, 0x51, 0xC0, 0x9B, 0x38, 0x27,
        0x19, 0x03, 0x97, 0x65, 0x3D, 0x44, 0xAC, 0xA7, 0x18, 0xA0, 0x61, 0x13,
        0xB3, 0xB4, 0xC6, 0x21, 0x15, 0xE0, 0xC5, 0x0F, 0x78, 0xC4, 0xEF, 0x2C,
        0x53, 0x26, 0x2E, 0x67, 0x54, 0x5F, 0xD5, 0xC8, 0xAA, 0x17, 0x46, 0x95,
        0xA3, 0x94, 0xFB, 0xBA, 0xD3, 0xBD, 0x64, 0x2A, 0xBF, 0x34, 0x48, 0x35,
        0x43, 0xD7, 0xF5, 0xCF, 0x90, 0x92, 0x2D, 0xB5, 0x5D, 0x93, 0x99, 0x50,
        0x74, 0x72, 0x31, 0x04, 0x58, 0x10, 0x5A, 0x7F, 0xFF, 0xCA, 0x55, 0x37,
        0xB2, 0xDD, 0xE5, 0x0A, 0x0D, 0x69, 0xB1, 0x3A, 0x00, 0x3C, 0xEA, 0x22,
        0x32, 0x8D, 0xF2, 0x9C, 0x86, 0x1C, 0xB0, 0x76, 0x30, 0x01, 0xD2, 0x06,
        0xBB, 0x77, 0xF3, 0x80, 0xE8, 0xA6, 0x05, 0xEC, 0x89, 0x49, 0xFD, 0xD9,
        0xD6, 0xD4, 0x45, 0x6F, 0x4F, 0xB8, 0x33, 0x57, 0xD8, 0x87, 0xA8, 0x9E,
        0xF9, 0x5C, 0x23, 0xB6, 0x6B, 0xEB, 0x7E, 0x1F, 0x02, 0xFE, 0x85, 0xE9,
        0x12, 0xC9, 0xAE, 0x08, 0x9F, 0x52, 0x25, 0x71, 0x09, 0x3F, 0x29, 0x68,
        0x3B, 0x1A, 0xE7, 0x91, 0x59, 0x7A, 0x6E, 0x8E, 0x56, 0xA4, 0x1D, 0x1E,
        0xA9, 0x9A, 0xDF, 0x70, 0x8C, 0x4E, 0x4B, 0xDC, 0xEE, 0x36, 0x8F, 0xC3,
        0x83, 0x82, 0xE4, 0x8B, 0x79, 0x6D, 0x0C, 0xA1, 0x7D, 0x39, 0x4A, 0xBE,
        0xFA, 0xAB, 0xD1, 0xC7, 0x28, 0x7B, 0xCC, 0xF7, 0xB7, 0xAD, 0x62, 0xB9,
        0xD0, 0xF8, 0xF1, 0x0E, 0x1B, 0xCB, 0xDA, 0xE6, 0x2B, 0x60, 0x16, 0xC2,
        0x81, 0x0B, 0x73, 0xA5, 0x20, 0x84, 0x5B, 0x24, 0x9D, 0x75, 0xE1, 0xCE,
        0x14, 0x6A, 0x3E, 0xFC,
    };
    return kTable;
}

// compbyte::decode — inverse permutation (DEARRAY), built lazily as in CompByte.cpp.
inline const uint8_t* CompByteDecodeTable() {
    static uint8_t kInverse[256];
    static bool built = false;
    if (!built) {
        const uint8_t* enc = CompByteEncodeTable();
        for (int i = 0; i < 256; ++i) {
            kInverse[enc[i]] = static_cast<uint8_t>(i);
        }
        built = true;
    }
    return kInverse;
}

inline void CompByteEncode(uint8_t* buffer, size_t size) {
    const uint8_t* table = CompByteEncodeTable();
    for (size_t i = 0; i < size; ++i) {
        buffer[i] = table[buffer[i]];
    }
}

inline void CompByteDecode(uint8_t* buffer, size_t size) {
    const uint8_t* table = CompByteDecodeTable();
    for (size_t i = 0; i < size; ++i) {
        buffer[i] = table[buffer[i]];
    }
}

// ---------------------------------------------------------------------------
// basestream-compatible cursor
// ---------------------------------------------------------------------------

// Mirrors basestream semantics used by every EP7 loader:
//   * absolute offset starts at 0,
//   * ReadFileType() consumes 128 type bytes + 4 version bytes and then defines
//     offset 0 ("m_DefaultOffSet") at the first *payload* byte,
//   * GetfTell()/SetOffSet() are relative to that payload start,
//   * read() past the end does NOT advance and does not write (CSerialMemory::read
//     returns 0 when (m_dwOffSet + dwSize) > m_nBufSize).
//
// Unlike the original, every read is bounds-checked and reports failure; the
// original client silently poisoned values on short reads (read() returned
// TRUE regardless), which we must not reproduce.
class Stream {
public:
    static constexpr size_t kFileTypeSize = 128;  // basestream.h FILETYPESIZE
    static constexpr size_t kHeaderSize = kFileTypeSize + sizeof(uint32_t);

    Stream() = default;

    // Attach to raw bytes. When `encoded` is true the payload is compbyte-decoded
    // block-by-block exactly as CSerialFile/CSerialMemory do (m_bencode).
    void Attach(std::vector<uint8_t> bytes, bool encoded) {
        m_bytes = std::move(bytes);
        m_encoded = encoded;
        m_pos = 0;
        m_ok = true;
        m_error.clear();
    }

    bool IsAttached() const { return !m_bytes.empty(); }
    bool Ok() const { return m_ok; }
    const std::string& Error() const { return m_error; }
    size_t Size() const { return m_bytes.size(); }
    size_t Position() const { return m_pos; }

    size_t Remaining() const { return m_pos <= m_bytes.size() ? (m_bytes.size() - m_pos) : 0; }

    void Fail(const std::string& why) {
        if (m_ok) {
            m_ok = false;
            m_error = why;
        }
    }

    // ReadFileType(). Verifies the 128-byte type block, reads the version DWORD.
    bool ReadFileType(std::string& outType, uint32_t& outVersion) {
        if (m_bytes.size() < kHeaderSize) {
            Fail("file shorter than the 132-byte EP7 header");
            return false;
        }
        // The type block is written by WriteFileType() through basestream::write,
        // so it is compbyte-encoded too when m_bencode is set.
        uint8_t typeBlock[kFileTypeSize];
        std::memcpy(typeBlock, m_bytes.data(), kFileTypeSize);
        if (m_encoded) CompByteDecode(typeBlock, kFileTypeSize);

        size_t len = 0;
        while (len < kFileTypeSize && typeBlock[len] != 0) ++len;
        outType.assign(reinterpret_cast<const char*>(typeBlock), len);

        uint8_t verBytes[4];
        std::memcpy(verBytes, m_bytes.data() + kFileTypeSize, 4);
        if (m_encoded) CompByteDecode(verBytes, 4);
        outVersion = ReadU32LE(verBytes);

        m_pos = kHeaderSize;  // payload offset 0
        return true;
    }

    // Raw primitive reads (basestream::operator>>).
    bool ReadU8(uint8_t& out) {
        if (!Ensure(1)) return false;
        out = DecodedByteAt(m_pos);
        m_pos += 1;
        return true;
    }
    bool ReadI8(int8_t& out) {
        uint8_t v = 0;
        if (!ReadU8(v)) return false;
        out = static_cast<int8_t>(v);
        return true;
    }
    bool ReadU16(uint16_t& out) {
        uint8_t b[2];
        if (!ReadRaw(b, 2)) return false;
        out = ReadU16LE(b);
        return true;
    }
    bool ReadU32(uint32_t& out) {
        uint8_t b[4];
        if (!ReadRaw(b, 4)) return false;
        out = ReadU32LE(b);
        return true;
    }
    bool ReadI32(int32_t& out) {
        uint8_t b[4];
        if (!ReadRaw(b, 4)) return false;
        out = ReadI32LE(b);
        return true;
    }
    bool ReadF32(float& out) {
        uint8_t b[4];
        if (!ReadRaw(b, 4)) return false;
        out = ReadF32LE(b);
        return true;
    }
    // WIN32 BOOL is a 4-byte int; the original code does `SFile >> bExist;` with BOOL.
    bool ReadBool32(bool& out) {
        int32_t v = 0;
        if (!ReadI32(v)) return false;
        out = (v != 0);
        return true;
    }

    // basestream::operator>>(std::string&): DWORD length (including terminator) then bytes.
    // The original stores a char buffer; when the terminator is present we drop it.
    bool ReadString(std::string& out, uint32_t maxLen = 4096) {
        uint32_t len = 0;
        if (!ReadU32(len)) return false;
        if (len == 0) {
            out.clear();
            return true;
        }
        if (len > maxLen) {
            Fail("length-prefixed string exceeds sanity limit");
            return false;
        }
        return ReadFixedString(len, out);
    }

    // Reads `len` bytes as a char buffer (the original code allocates `new char[len]`
    // and copies len bytes; most call sites include the terminator in len).
    bool ReadFixedString(uint32_t len, std::string& out) {
        if (len > Remaining()) {
            Fail("string runs past end of buffer");
            return false;
        }
        std::vector<uint8_t> tmp(len);
        if (!ReadRaw(tmp.data(), len)) return false;
        size_t n = len;
        while (n > 0 && tmp[n - 1] == 0) --n;  // strip trailing NUL(s)
        out.assign(reinterpret_cast<const char*>(tmp.data()), n);
        return true;
    }

    // Bounds-checked block read.
    bool ReadRaw(void* dst, size_t bytes) {
        if (!Ensure(bytes)) return false;
        std::memcpy(dst, m_bytes.data() + m_pos, bytes);
        if (m_encoded) CompByteDecode(static_cast<uint8_t*>(dst), bytes);
        m_pos += bytes;
        return true;
    }

    // Skip without decoding (used when a section must be preserved but not parsed).
    bool Skip(size_t bytes) {
        if (!Ensure(bytes)) return false;
        m_pos += bytes;
        return true;
    }

    size_t Tell() const { return m_pos >= kHeaderSize ? (m_pos - kHeaderSize) : 0; }

    bool Seek(size_t payloadOffset) {
        const size_t abs = kHeaderSize + payloadOffset;
        if (abs > m_bytes.size()) {
            Fail("seek past end of buffer");
            return false;
        }
        m_pos = abs;
        return true;
    }

    // Detect a truncated/oversized declared element count without multiplying into overflow.
    bool CanReadElements(size_t count, size_t elementSize) const {
        if (elementSize == 0) return false;
        if (count > Remaining() / elementSize) return false;
        return true;
    }

private:
    bool Ensure(size_t bytes) {
        if (!m_ok) return false;
        if (bytes > Remaining()) {
            Fail("read past end of buffer");
            return false;
        }
        return true;
    }

    uint8_t DecodedByteAt(size_t index) const {
        const uint8_t raw = m_bytes[index];
        return m_encoded ? CompByteDecodeTable()[raw] : raw;
    }

    std::vector<uint8_t> m_bytes;
    size_t m_pos = 0;
    bool m_encoded = false;
    bool m_ok = true;
    std::string m_error;
};

// ---------------------------------------------------------------------------
// Pack-container access
// ---------------------------------------------------------------------------

// The original client reads assets out of `data.rcc`-style pack files
// (enginelib/Common/SFileSystem.cpp, CUnzipper) through CSerialMemory.
// A pack entry is the same basestream byte layout, optionally compbyte-encoded.
//
// In production the caller supplies the already-decompressed entry bytes
// (the Android build extracts pack entries, see EP7PackArchive). For host tests
// we also accept an uncompressed "stored" ZIP entry.
struct PackEntry {
    std::string name;
    std::vector<uint8_t> payload;  // decompressed bytes, exactly as CSerialMemory sees them
};

}  // namespace ep7byte
}  // namespace RANMobile
