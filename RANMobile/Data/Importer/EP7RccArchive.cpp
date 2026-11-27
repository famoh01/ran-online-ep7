#include "EP7RccArchive.h"

#include <cstring>

#include "EP7ByteFormat.h"

namespace RANMobile {
namespace {

constexpr uint32_t kEocdSignature = 0x06054b50u;   // 'PK\5\6'
constexpr uint32_t kCdSignature = 0x02014b50u;     // 'PK\1\2'
constexpr uint32_t kLocalSignature = 0x04034b50u;  // 'PK\3\4'
constexpr size_t kEocdSize = 22;
constexpr size_t kCdFixedSize = 46;
constexpr size_t kLocalFixedSize = 30;

uint16_t ReadU16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t ReadU32(const uint8_t* p) { return ep7byte::ReadU32LE(p); }

std::string ToLowerAscii(const std::string& value) {
    std::string out = value;
    for (auto& ch : out) {
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
        // The client lowercases with std::tolower in the C locale; path separators are not
        // normalised by the original, so we keep them verbatim.
    }
    return out;
}

// ---- CRC-32 (IEEE 802.3, reflected) ---------------------------------------
struct Crc32Table {
    uint32_t table[256];
    Crc32Table() {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
    }
};

uint32_t Crc32(const uint8_t* data, size_t size) {
    static const Crc32Table kTable;
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i) crc = kTable.table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

// ---- inflate (deflate, RFC 1951) ------------------------------------------
// A compact canonical-Huffman inflater. Every read is bounds-checked; a malformed stream
// fails instead of producing garbage.
class Inflater {
public:
    Inflater(const uint8_t* data, size_t size) : m_data(data), m_size(size) {}

    bool Run(std::vector<uint8_t>& out, uint32_t expectedSize) {
        if (expectedSize > EP7RccArchive::kMaxEntrySize) return false;
        out.clear();
        out.reserve(expectedSize);

        int finalBlock = 0;
        while (!finalBlock) {
            if (!ReadBits(1, finalBlock)) return false;
            int type = 0;
            if (!ReadBits(2, type)) return false;

            if (type == 0) {
                if (!StoredBlock(out)) return false;
            } else if (type == 1) {
                if (!FixedHuffmanBlock(out)) return false;
            } else if (type == 2) {
                if (!DynamicHuffmanBlock(out)) return false;
            } else {
                return false;  // reserved
            }
            if (out.size() > EP7RccArchive::kMaxEntrySize) return false;
        }
        return out.size() == expectedSize;
    }

private:
    struct Huffman {
        // counts[len], symbols sorted by code
        uint16_t counts[16] = {0};
        uint16_t symbols[288] = {0};
        uint16_t symbolCount = 0;
    };

    bool ReadBits(int count, int& value) {
        value = 0;
        for (int i = 0; i < count; ++i) {
            if (m_bitPos >= 8) {
                if (m_pos >= m_size) return false;
                m_bitBuf = m_data[m_pos++];
                m_bitPos = 0;
            }
            value |= ((m_bitBuf >> m_bitPos) & 1) << i;
            ++m_bitPos;
        }
        return true;
    }

    static void BuildHuffman(Huffman& h, const uint16_t* lengths, int n) {
        for (int i = 0; i < 16; ++i) h.counts[i] = 0;
        for (int i = 0; i < n; ++i) h.counts[lengths[i]]++;
        h.counts[0] = 0;

        uint16_t offsets[16];
        offsets[0] = 0;
        for (int i = 1; i < 16; ++i) offsets[i] = static_cast<uint16_t>(offsets[i - 1] + h.counts[i - 1]);

        h.symbolCount = 0;
        for (int i = 0; i < n; ++i) {
            if (lengths[i] != 0) h.symbols[offsets[lengths[i]]++] = static_cast<uint16_t>(i);
        }
        for (int i = 0; i < 16; ++i) h.symbolCount = static_cast<uint16_t>(h.symbolCount + h.counts[i]);
    }

    bool Decode(const Huffman& h, int& symbol) {
        int code = 0;
        int first = 0;
        int index = 0;
        for (int len = 1; len < 16; ++len) {
            int bit = 0;
            if (!ReadBits(1, bit)) return false;
            code |= bit;
            const int count = h.counts[len];
            if (code - first < count) {
                if (index + (code - first) >= h.symbolCount) return false;
                symbol = h.symbols[index + (code - first)];
                return true;
            }
            index += count;
            first = (first + count) << 1;
            code <<= 1;
        }
        return false;
    }

    bool StoredBlock(std::vector<uint8_t>& out) {
        // discard remaining bits in the current byte
        m_bitPos = 8;
        if (m_pos + 4 > m_size) return false;
        const uint16_t len = ReadU16(m_data + m_pos);
        const uint16_t nlen = ReadU16(m_data + m_pos + 2);
        m_pos += 4;
        if (static_cast<uint16_t>(~len & 0xFFFFu) != nlen) return false;
        if (m_pos + len > m_size) return false;
        out.insert(out.end(), m_data + m_pos, m_data + m_pos + len);
        m_pos += len;
        return true;
    }

    bool InflateCodes(const Huffman& lit, const Huffman& dist, std::vector<uint8_t>& out) {
        static const uint16_t kLengthBase[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,
                                                 17, 19, 23, 27, 31, 35, 43, 51,  59,  67,  83,
                                                 99, 115, 131, 163, 195, 227, 258};
        static const uint16_t kLengthExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                                  2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
        static const uint16_t kDistBase[30] = {1,    2,    3,    4,    5,    7,     9,     13,
                                               17,   25,   33,   49,   65,   97,    129,   193,
                                               257,  385,  513,  769,  1025, 1537,  2049,  3073,
                                               4097, 6145, 8193, 12289, 16385, 24577};
        static const uint16_t kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                                6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

        for (;;) {
            int symbol = 0;
            if (!Decode(lit, symbol)) return false;

            if (symbol < 256) {
                out.push_back(static_cast<uint8_t>(symbol));
            } else if (symbol == 256) {
                return true;
            } else {
                symbol -= 257;
                if (symbol >= 29) return false;
                int extra = 0;
                if (!ReadBits(kLengthExtra[symbol], extra)) return false;
                const size_t length = kLengthBase[symbol] + static_cast<size_t>(extra);

                int distSymbol = 0;
                if (!Decode(dist, distSymbol)) return false;
                if (distSymbol >= 30) return false;
                int distExtra = 0;
                if (!ReadBits(kDistExtra[distSymbol], distExtra)) return false;
                const size_t distance = kDistBase[distSymbol] + static_cast<size_t>(distExtra);

                if (distance == 0 || distance > out.size()) return false;
                const size_t start = out.size() - distance;
                for (size_t i = 0; i < length; ++i) out.push_back(out[start + i]);
            }
            if (out.size() > EP7RccArchive::kMaxEntrySize) return false;
        }
    }

    bool FixedHuffmanBlock(std::vector<uint8_t>& out) {
        uint16_t litLengths[288];
        for (int i = 0; i < 144; ++i) litLengths[i] = 8;
        for (int i = 144; i < 256; ++i) litLengths[i] = 9;
        for (int i = 256; i < 280; ++i) litLengths[i] = 7;
        for (int i = 280; i < 288; ++i) litLengths[i] = 8;

        uint16_t distLengths[30];
        for (int i = 0; i < 30; ++i) distLengths[i] = 5;

        Huffman lit;
        Huffman dist;
        BuildHuffman(lit, litLengths, 288);
        BuildHuffman(dist, distLengths, 30);
        return InflateCodes(lit, dist, out);
    }

    bool DynamicHuffmanBlock(std::vector<uint8_t>& out) {
        static const uint8_t kOrder[19] = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                           11, 4,  12, 3, 13, 2, 14, 1, 15};

        int hlit = 0, hdist = 0, hclen = 0;
        if (!ReadBits(5, hlit) || !ReadBits(5, hdist) || !ReadBits(4, hclen)) return false;
        hlit += 257;
        hdist += 1;
        hclen += 4;
        if (hlit > 286 || hdist > 30) return false;

        uint16_t codeLengths[19] = {0};
        for (int i = 0; i < hclen; ++i) {
            int v = 0;
            if (!ReadBits(3, v)) return false;
            codeLengths[kOrder[i]] = static_cast<uint16_t>(v);
        }
        Huffman codeHuffman;
        BuildHuffman(codeHuffman, codeLengths, 19);

        uint16_t lengths[288 + 30] = {0};
        const int total = hlit + hdist;
        int index = 0;
        while (index < total) {
            int symbol = 0;
            if (!Decode(codeHuffman, symbol)) return false;
            if (symbol < 16) {
                lengths[index++] = static_cast<uint16_t>(symbol);
                continue;
            }
            int repeat = 0;
            uint16_t value = 0;
            if (symbol == 16) {
                if (index == 0) return false;
                value = lengths[index - 1];
                int extra = 0;
                if (!ReadBits(2, extra)) return false;
                repeat = 3 + extra;
            } else if (symbol == 17) {
                int extra = 0;
                if (!ReadBits(3, extra)) return false;
                repeat = 3 + extra;
            } else if (symbol == 18) {
                int extra = 0;
                if (!ReadBits(7, extra)) return false;
                repeat = 11 + extra;
            } else {
                return false;
            }
            if (index + repeat > total) return false;
            for (int i = 0; i < repeat; ++i) lengths[index++] = value;
        }
        if (lengths[256] == 0) return false;  // missing end-of-block code

        Huffman lit;
        Huffman dist;
        BuildHuffman(lit, lengths, hlit);
        BuildHuffman(dist, lengths + hlit, hdist);
        return InflateCodes(lit, dist, out);
    }

    const uint8_t* m_data = nullptr;
    size_t m_size = 0;
    size_t m_pos = 0;
    uint8_t m_bitBuf = 0;
    int m_bitPos = 8;
};

// ---- traditional PKWARE (ZipCrypto) decryption ----------------------------
// CUnzipper::UnzipToMemory opens encrypted entries with the password "tkqwlf".
class ZipCrypto {
public:
    explicit ZipCrypto(const char* password) {
        for (const char* p = password; *p; ++p) Update(static_cast<uint8_t>(*p));
    }

    uint8_t DecryptByte(uint8_t cipher) {
        const uint8_t plain = static_cast<uint8_t>(cipher ^ (m_keys[2] >> 8));
        Update(plain);
        return plain;
    }

private:
    void Update(uint8_t plain) {
        m_keys[0] = Crc32Update(m_keys[0], plain);
        m_keys[1] = (m_keys[1] + (m_keys[0] & 0xFFu)) * 134775813u + 1u;
        m_keys[2] = Crc32Update(m_keys[2], static_cast<uint8_t>(m_keys[1] >> 24));
    }

    static uint32_t Crc32Update(uint32_t crc, uint8_t value) {
        static const Crc32Table kTable;
        return kTable.table[(crc ^ value) & 0xFFu] ^ (crc >> 8);
    }

    uint32_t m_keys[3] = {0x12345678u, 0x23456789u, 0x34567890u};
};

}  // namespace

// ---------------------------------------------------------------------------
// EP7RccArchive
// ---------------------------------------------------------------------------

bool EP7RccArchive::Open(const std::vector<uint8_t>& archiveBytes) {
    m_bytes = archiveBytes;
    m_entries.clear();
    m_open = false;
    m_error.clear();

    if (m_bytes.size() < kEocdSize) {
        m_error = "archive shorter than the end-of-central-directory record";
        return false;
    }

    // Locate the EOCD by scanning backwards (a ZIP comment may follow it).
    size_t eocd = SIZE_MAX;
    const size_t scanLimit = m_bytes.size() > 66000 ? m_bytes.size() - 66000 : 0;
    for (size_t i = m_bytes.size() - kEocdSize + 1; i-- > scanLimit;) {
        if (ReadU32(m_bytes.data() + i) == kEocdSignature) {
            eocd = i;
            break;
        }
    }
    if (eocd == SIZE_MAX) {
        m_error = "no end-of-central-directory record (not a ZIP/rcc archive)";
        return false;
    }

    const uint16_t entryCount = ReadU16(m_bytes.data() + eocd + 10);
    const uint32_t cdSize = ReadU32(m_bytes.data() + eocd + 12);
    const uint32_t cdOffset = ReadU32(m_bytes.data() + eocd + 16);

    if (entryCount > kMaxEntries) {
        m_error = "central directory entry count exceeds sanity limit";
        return false;
    }
    if (cdOffset > m_bytes.size() || cdSize > m_bytes.size() || cdOffset + cdSize > m_bytes.size()) {
        m_error = "central directory lies outside the archive";
        return false;
    }

    size_t pos = cdOffset;
    for (uint16_t i = 0; i < entryCount; ++i) {
        if (pos + kCdFixedSize > m_bytes.size()) {
            m_error = "truncated central directory entry";
            return false;
        }
        const uint8_t* p = m_bytes.data() + pos;
        if (ReadU32(p) != kCdSignature) {
            m_error = "bad central directory signature";
            return false;
        }
        EP7RccEntry entry;
        entry.flags = ReadU16(p + 8);
        entry.method = ReadU16(p + 10);
        entry.crc32 = ReadU32(p + 16);
        entry.compressedSize = ReadU32(p + 20);
        entry.uncompressedSize = ReadU32(p + 24);
        const uint16_t nameLen = ReadU16(p + 28);
        const uint16_t extraLen = ReadU16(p + 30);
        const uint16_t commentLen = ReadU16(p + 32);
        entry.localHeaderOffset = ReadU32(p + 42);

        if (pos + kCdFixedSize + nameLen > m_bytes.size()) {
            m_error = "central directory entry name runs past the archive";
            return false;
        }
        entry.name.assign(reinterpret_cast<const char*>(p + kCdFixedSize), nameLen);

        if (entry.uncompressedSize > kMaxEntrySize) {
            m_error = "declared entry size exceeds sanity limit";
            return false;
        }
        if (entry.localHeaderOffset + kLocalFixedSize > m_bytes.size()) {
            m_error = "entry local header lies outside the archive";
            return false;
        }

        m_entries.push_back(std::move(entry));
        pos += kCdFixedSize + nameLen + extraLen + commentLen;
    }

    m_open = true;
    return true;
}

const EP7RccEntry* EP7RccArchive::Find(const std::string& name) const {
    const std::string wanted = ToLowerAscii(name);
    for (const auto& entry : m_entries) {
        if (ToLowerAscii(entry.name) == wanted) return &entry;
    }
    // The client also matches `zipName + entryName`, so tolerate a leading "./".
    if (wanted.size() > 2 && wanted[0] == '.' && wanted[1] == '/') {
        return Find(wanted.substr(2));
    }
    return nullptr;
}

bool EP7RccArchive::Extract(const EP7RccEntry& entry, std::vector<uint8_t>& out) const {
    out.clear();
    if (!m_open) return false;

    const uint8_t* local = m_bytes.data() + entry.localHeaderOffset;
    if (ReadU32(local) != kLocalSignature) return false;

    const uint16_t nameLen = ReadU16(local + 26);
    const uint16_t extraLen = ReadU16(local + 28);
    size_t dataOffset = static_cast<size_t>(entry.localHeaderOffset) + kLocalFixedSize + nameLen + extraLen;
    if (dataOffset > m_bytes.size()) return false;
    if (entry.compressedSize > m_bytes.size() - dataOffset) return false;

    std::vector<uint8_t> raw(m_bytes.begin() + static_cast<long>(dataOffset),
                             m_bytes.begin() + static_cast<long>(dataOffset + entry.compressedSize));

    if (entry.encrypted()) {
        // 12-byte encryption header precedes the data; the last byte is a check byte.
        if (raw.size() < 12) return false;
        ZipCrypto crypto(kZipPassword);
        uint8_t check = 0;
        for (size_t i = 0; i < 12; ++i) check = crypto.DecryptByte(raw[i]);
        // Traditional PKWARE: the check byte is the HIGH byte of the entry's DOS mod time,
        // which the reader must take from the local header (the central directory copy may
        // differ in foreign archives).
        const uint16_t localModTime = ReadU16(local + 10);
        if (check != static_cast<uint8_t>(localModTime >> 8)) return false;
        for (size_t i = 12; i < raw.size(); ++i) raw[i] = crypto.DecryptByte(raw[i]);
        raw.erase(raw.begin(), raw.begin() + 12);
    }

    if (entry.method == 0) {
        out = std::move(raw);
    } else if (entry.method == 8) {
        Inflater inflater(raw.data(), raw.size());
        if (!inflater.Run(out, entry.uncompressedSize)) return false;
    } else {
        return false;  // unsupported compression method
    }

    if (out.size() != entry.uncompressedSize) return false;
    if (Crc32(out.data(), out.size()) != entry.crc32) return false;
    return true;
}

bool EP7RccArchive::ExtractByName(const std::string& name, std::vector<uint8_t>& out) const {
    const EP7RccEntry* entry = Find(name);
    if (!entry) return false;
    return Extract(*entry, out);
}

}  // namespace RANMobile
