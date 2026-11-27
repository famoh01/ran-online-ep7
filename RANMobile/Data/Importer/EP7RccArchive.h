//
// EP7RccArchive.h — minimal, read-only ZIP reader for EP7 `.rcc` packs
//
// WHY THIS EXISTS
// ---------------
// The EP7 client does not ship character assets as loose files. `GLOGIC::InitFile`
// (enginelib/G-Logic/GLogic.cpp:40-62) points the loaders at two ZIP-based packs:
//
//     DxSkinObject::GetPath()   + "SkinObject.rcc"    -> .chf / .cps / skeleton .x / mesh .x
//     DxSkinAniMan::GetPath()   + "Animation.rcc"     -> .bin animation containers / .cfg
//
// and every asset load goes through `GLOGIC::openfile_basestream(bENGLIB_ZIPFILE, ...)` ->
// `CUnzipper::UnzipToMemory` (enginelib/Common/Unzipper.cpp:569), which is a stock minizip
// reader. Entry names are looked up case-insensitively as `zipName + entryName`
// (`CUnzipper::LOADFILE_RCC`, `GOTOFILEPOS`), and encrypted entries use the ZipCrypto
// password "tkqwlf" (`unzOpenCurrentFilePassword(m_uzFile, "tkqwlf")`).
//
// A directory walk of an extracted client therefore finds *no* character assets: the data is
// inside the two .rcc archives. This reader exists so verification (and later the runtime
// asset resolver) can reach the real bytes.
//
// SCOPE — deliberately small and honest
// -------------------------------------
//  * reads the ZIP central directory (End-Of-Central-Directory → CD entries),
//  * supports stored (method 0) and deflate (method 8) entries, no zip64,
//  * supports traditional ZipCrypto when the general-purpose bit 0 is set, using the same
//    password the client uses,
//  * CRC-32 is verified on every entry; a mismatch is an error, never silent,
//  * every offset/length is bounds-checked against the archive buffer.
//
// It is NOT a general ZIP library: no multi-disk, no zip64, no AES, no writing.
//
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace RANMobile {

struct EP7RccEntry {
    std::string name;         // entry path as stored in the archive
    uint32_t compressedSize = 0;
    uint32_t uncompressedSize = 0;
    uint32_t localHeaderOffset = 0;
    uint16_t method = 0;      // 0 = stored, 8 = deflate
    uint16_t flags = 0;
    uint32_t crc32 = 0;

    bool encrypted() const { return (flags & 0x0001u) != 0; }
};

class EP7RccArchive {
public:
    static constexpr const char* kZipPassword = "tkqwlf";  // CUnzipper::UnzipToMemory
    static constexpr uint32_t kMaxEntries = 200000;
    static constexpr uint32_t kMaxEntrySize = 256u * 1024u * 1024u;

    // Parses the central directory. Returns false with `error()` set on malformed input.
    bool Open(const std::vector<uint8_t>& archiveBytes);

    bool IsOpen() const { return m_open; }
    const std::string& Error() const { return m_error; }
    const std::vector<EP7RccEntry>& Entries() const { return m_entries; }
    size_t EntryCount() const { return m_entries.size(); }

    // Case-insensitive lookup (the client lowercases both sides, CUnzipper::GOTOFILEPOS).
    const EP7RccEntry* Find(const std::string& name) const;

    // Extracts (and decrypts/inflates) one entry. Verifies size and CRC-32.
    bool Extract(const EP7RccEntry& entry, std::vector<uint8_t>& out) const;

    // Convenience: Find() + Extract().
    bool ExtractByName(const std::string& name, std::vector<uint8_t>& out) const;

private:
    std::vector<uint8_t> m_bytes;
    std::vector<EP7RccEntry> m_entries;
    bool m_open = false;
    std::string m_error;
};

}  // namespace RANMobile
