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

// Phase 3J B20: 0x0103..0x0108 mobile-exported SkinCharData loader.
//
// Authentic evidence (EP7 release pack skinobject.rcc; PHASE3J.md §16.9):
// CI census of its 243 .chf — 0x0104 x33, 0x0106 x83, 0x0107 x81, 0x0108 x3,
// 0x0200 x43 — plus byte-exact tail dumps of the four smallest examples
// (NPC_ark 0x0104/309 B, event_Athena 0x0106/311 B, npc_police_1 0x0107/307 B,
// NPC_hangar 0x0108/319 B) reconstructed against this layout with exact total
// size matches.  See EP7CharacterFile.h for the field-by-field layout and the
// evidence trail.
bool LoadVer0103Plus(ep7byte::Stream& s, EP7CharacterFileInfo& info) {
    // Piece slots: same flag + optional length-prefixed name encoding as
    // 0x0101/0x0102 (proven byte-exact by the payload-head dumps: the version
    // DWORD is followed directly by the piece count — no leading scale).
    uint32_t pieceCount = 0;
    if (!s.ReadU32(pieceCount)) {
        info.error = "truncated .chf piece count";
        return false;
    }
    if (pieceCount > kEP7PieceSize) {
        info.error = ".chf piece count exceeds PIECE_SIZE (13)";
        return false;
    }
    for (uint32_t i = 0; i < pieceCount; ++i) {
        if (!ReadPieceSlot(s, info.pieces[i])) {
            info.error = "invalid .chf piece entry";
            return false;
        }
    }

    if (!s.ReadString(info.skeleton, kMaxPieceName)) {
        info.error = "invalid .chf skeleton name";
        return false;
    }

    uint32_t animCount = 0;
    if (!s.ReadU32(animCount)) {
        info.error = "truncated .chf animation count";
        return false;
    }
    if (animCount > kMaxAnimNames) {
        info.error = ".chf animation count exceeds sanity limit";
        return false;
    }
    info.animations.reserve(animCount);
    for (uint32_t i = 0; i < animCount; ++i) {
        std::string name;
        if (!s.ReadString(name, kMaxPieceName)) {
            info.error = "invalid .chf animation name";
            return false;
        }
        info.animations.push_back(std::move(name));
    }

    // Trailer block.  Its TOTAL byte size selects one of the three variants
    // observed in the authentic dumps (60 B = 0x0104 sample, 64 B =
    // 0x0106/0x0107 samples, 72 B = 0x0108 sample); see EP7CharacterFile.h.
    const size_t rem = s.Remaining();
    uint32_t flagA = 0, flagB = 0;
    float height = 0.0f, radius = 0.0f;
    if (rem == 60) {
        // Single DWORD (1) between scale and height in the 0x0104 sample.
        if (!s.ReadF32(info.scale) || !s.ReadU32(flagA) ||
            !s.ReadF32(height) || !s.ReadF32(radius)) {
            info.error = "truncated .chf trailer block";
            return false;
        }
    } else if (rem == 64 || rem == 72) {
        if (!s.ReadF32(info.scale) || !s.ReadU32(flagA) || !s.ReadU32(flagB) ||
            !s.ReadF32(height) || !s.ReadF32(radius)) {
            info.error = "truncated .chf trailer block";
            return false;
        }
    } else {
        info.error = "unsupported .chf trailer block size (" + std::to_string(rem) + " bytes)";
        return false;
    }
    if (flagA > 1 || flagB > 1) {
        info.error = ".chf trailer flag out of range";
        return false;
    }

    // Bounding box: DWORD 0, DWORD 1, vMax, vMin (all three variants).
    uint32_t boxFlag0 = 0, boxFlag1 = 0;
    if (!s.ReadU32(boxFlag0) || !s.ReadU32(boxFlag1) ||
        !s.ReadF32(info.boundsMax[0]) || !s.ReadF32(info.boundsMax[1]) || !s.ReadF32(info.boundsMax[2]) ||
        !s.ReadF32(info.boundsMin[0]) || !s.ReadF32(info.boundsMin[1]) || !s.ReadF32(info.boundsMin[2])) {
        info.error = "truncated .chf bounding box";
        return false;
    }
    if (boxFlag0 > 1 || boxFlag1 > 1) {
        info.error = ".chf bbox flag out of range";
        return false;
    }
    info.hasBoundingBox = true;
    info.hasHeightRadius = true;
    info.height = height;
    info.radius = radius;

    // The 0x0108 sample carries two extra booleans immediately before the
    // final three-DWORD run.
    if (rem == 72) {
        uint32_t extra0 = 0, extra1 = 0;
        if (!s.ReadU32(extra0) || !s.ReadU32(extra1)) {
            info.error = "truncated .chf trailer block (extra flags)";
            return false;
        }
        if (extra0 > 1 || extra1 > 1) {
            info.error = ".chf trailer extra flag out of range";
            return false;
        }
    }

    // Final run (1, 0, 0) in every authentic sample; read in the 0x0102 field
    // order (hasWorldObj, worldObj, end flag).
    uint32_t worldFlag = 0, worldObjFlag = 0, endFlag = 0;
    if (!s.ReadU32(worldFlag) || !s.ReadU32(worldObjFlag) || !s.ReadU32(endFlag)) {
        info.error = "truncated .chf trailer flags";
        return false;
    }
    if (worldFlag > 1 || worldObjFlag > 1 || endFlag > 1) {
        info.error = ".chf world flag out of range";
        return false;
    }
    info.hasWorldObj = worldFlag != 0;
    info.worldObj = info.hasWorldObj && worldObjFlag != 0;

    return true;
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

    // Phase 3J B20: mobile-exported SkinCharData versions.  0x0104/0x0106/
    // 0x0107/0x0108 are attested by authentic pack files; 0x0103 is admitted
    // with the same size-adaptive layout (see EP7CharacterFile.h); 0x0105 has
    // no authentic sample and no known layout and stays unsupported.
    if (version >= EP7CharacterFileInfo::kVersion103 && version <= EP7CharacterFileInfo::kVersion108 &&
        version != 0x0105) {
        if (!LoadVer0103Plus(s, info)) {
            return info;
        }
        info.valid = true;
        return info;
    }

    info.error = "unsupported .chf version (expected 0x0100..0x0104 or 0x0106..0x0108)";
    return info;
}

}  // namespace RANMobile
