#include "EP7CharacterFile.h"

#include <cstring>

namespace RANMobile {
namespace {

constexpr uint32_t kMaxAnimNames = 4096;
constexpr uint32_t kMaxPieceName = 1024;

bool ReadPieceSlot(ep7byte::Stream& s, std::string& out) {
    bool present = false;
    if (!s.ReadBool32(present)) return false;
    if (!present) {
        out.clear();
        return true;
    }
    return s.ReadString(out, kMaxPieceName);
}

}  // namespace

EP7CharacterFileInfo EP7CharacterFileReader::Parse(const std::vector<uint8_t>& bytes, bool compByteEncoded) {
    ep7byte::Stream stream;
    stream.Attach(bytes, compByteEncoded);

    // DxSkinCharData::LoadFile goes through GLOGIC::openfile_basestream ->
    // CSerialFile/CSerialMemory::OpenFile -> ReadFileType(), which consumes the 128-byte
    // file-type block and the version DWORD before any payload read.
    if (bytes.size() >= ep7byte::Stream::kHeaderSize) {
        std::string fileType;
        uint32_t fileId = 0;
        stream.ReadFileType(fileType, fileId);
    }
    return ParseStream(stream);
}

EP7CharacterFileInfo EP7CharacterFileReader::ParseStream(ep7byte::Stream& s) {
    EP7CharacterFileInfo info;

    if (!s.IsAttached()) {
        info.error = "empty .chf buffer";
        return info;
    }

    uint32_t version = 0;
    if (!s.ReadU32(version)) {
        info.error = "truncated .chf version";
        return info;
    }
    info.version = version;

    if (version == EP7CharacterFileInfo::kVersion101 || version == EP7CharacterFileInfo::kVersion102) {
        if (!s.ReadF32(info.scale)) {
            info.error = "truncated .chf scale";
            return info;
        }

        uint32_t pieceCount = 0;
        if (!s.ReadU32(pieceCount)) {
            info.error = "truncated .chf piece count";
            return info;
        }
        // Original: `if (dwPieceSize > PIECE_SIZE) return FALSE;`
        if (pieceCount > kEP7PieceSize) {
            info.error = ".chf piece count exceeds PIECE_SIZE (13)";
            return info;
        }
        for (uint32_t i = 0; i < pieceCount; ++i) {
            if (!ReadPieceSlot(s, info.pieces[i])) {
                info.error = "invalid .chf piece entry";
                return info;
            }
        }

        if (!s.ReadString(info.skeleton, kMaxPieceName)) {
            info.error = "invalid .chf skeleton name";
            return info;
        }

        uint32_t animCount = 0;
        if (!s.ReadU32(animCount)) {
            info.error = "truncated .chf animation count";
            return info;
        }
        if (animCount > kMaxAnimNames) {
            info.error = ".chf animation count exceeds sanity limit";
            return info;
        }
        info.animations.reserve(animCount);
        for (uint32_t i = 0; i < animCount; ++i) {
            std::string name;
            if (!s.ReadString(name, kMaxPieceName)) {
                info.error = "invalid .chf animation name";
                return info;
            }
            info.animations.push_back(std::move(name));
        }

        bool hasBox = false;
        if (!s.ReadBool32(hasBox)) {
            info.error = "truncated .chf bounding box flag";
            return info;
        }
        info.hasBoundingBox = hasBox;
        if (hasBox) {
            if (!s.ReadF32(info.boundsMax[0]) || !s.ReadF32(info.boundsMax[1]) || !s.ReadF32(info.boundsMax[2]) ||
                !s.ReadF32(info.boundsMin[0]) || !s.ReadF32(info.boundsMin[1]) || !s.ReadF32(info.boundsMin[2])) {
                info.error = "truncated .chf bounding box";
                return info;
            }
        }

        // Original LOAD_Ver101 returns TRUE early when the world-obj flag is absent.
        bool hasWorld = false;
        if (!s.ReadBool32(hasWorld)) {
            info.valid = true;  // the file simply ends here: this is legal, not corrupt
            return info;
        }
        info.hasWorldObj = hasWorld;
        if (hasWorld) {
            if (!s.ReadBool32(info.worldObj)) {
                info.valid = true;
                return info;
            }
        }

        bool hasHeightRadius = false;
        if (!s.ReadBool32(hasHeightRadius)) {
            info.valid = true;
            return info;
        }
        info.hasHeightRadius = hasHeightRadius;
        if (hasHeightRadius) {
            if (s.ReadF32(info.height) && s.ReadF32(info.radius)) {
                // values read; nothing further to validate
            } else {
                info.valid = true;
                return info;
            }
        }

        // Trailing flag written as FALSE by SaveFile; absence is tolerated.
        bool trailer = false;
        (void)s.ReadBool32(trailer);

        info.valid = true;
        return info;
    }

    if (version == EP7CharacterFileInfo::kVersion100) {
        // LOAD_Ver100: no scale field.
        uint32_t pieceCount = 0;
        if (!s.ReadU32(pieceCount)) {
            info.error = "truncated .chf piece count";
            return info;
        }
        if (pieceCount > kEP7PieceSize) {
            info.error = ".chf piece count exceeds PIECE_SIZE (13)";
            return info;
        }
        for (uint32_t i = 0; i < pieceCount; ++i) {
            if (!ReadPieceSlot(s, info.pieces[i])) {
                info.error = "invalid .chf piece entry (v0x0100)";
                return info;
            }
        }

        // Fixed 260-byte skeleton buffer.
        if (!s.ReadFixedString(kEP7MaxPath, info.skeleton)) {
            info.error = "truncated .chf skeleton buffer (v0x0100)";
            return info;
        }

        uint32_t animCount = 0;
        if (!s.ReadU32(animCount)) {
            info.error = "truncated .chf animation count (v0x0100)";
            return info;
        }
        if (animCount > kMaxAnimNames) {
            info.error = ".chf animation count exceeds sanity limit";
            return info;
        }
        info.animations.reserve(animCount);
        for (uint32_t i = 0; i < animCount; ++i) {
            std::string name;
            if (!s.ReadFixedString(kEP7AnimNameSize, name)) {
                info.error = "truncated .chf animation name buffer (v0x0100)";
                return info;
            }
            info.animations.push_back(std::move(name));
        }

        bool hasBox = false;
        if (!s.ReadBool32(hasBox)) {
            info.error = "truncated .chf bounding box flag (v0x0100)";
            return info;
        }
        info.hasBoundingBox = hasBox;
        if (hasBox) {
            if (!s.ReadF32(info.boundsMax[0]) || !s.ReadF32(info.boundsMax[1]) || !s.ReadF32(info.boundsMax[2]) ||
                !s.ReadF32(info.boundsMin[0]) || !s.ReadF32(info.boundsMin[1]) || !s.ReadF32(info.boundsMin[2])) {
                info.error = "truncated .chf bounding box (v0x0100)";
                return info;
            }
        }

        info.scale = 1.0f;  // not stored in 0x0100
        info.valid = true;
        return info;
    }

    info.error = "unsupported .chf version (expected 0x0100..0x0102)";
    return info;
}

}  // namespace RANMobile
