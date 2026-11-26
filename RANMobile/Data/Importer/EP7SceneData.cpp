#include "EP7SceneData.h"
#include "EP7HashVerify.h"
#include "EP7SerialFile.h"
#include "../../Engine/Core/Logger.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace RANMobile {

namespace {

std::string Trim(const std::string& input) {
    const size_t begin = input.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return std::string();
    const size_t end = input.find_last_not_of(" \t\r\n");
    return input.substr(begin, end - begin + 1);
}

std::string LowerAscii(const std::string& input) {
    return EP7HashVerify::LowerAscii(input);
}

std::string FormatHexU32(uint32_t v) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase;
    oss.width(4);
    oss.fill('0');
    oss << v;
    return oss.str();
}

const char* ValidationModeName(EP7ValidationMode mode) {
    return mode == EP7ValidationMode::StrictAuthentic ? "STRICT_AUTHENTIC" : "LEGACY_SOURCE_COMPAT";
}

bool IsPrintableAsciiString(const std::string& value) {
    for (unsigned char ch : value) {
        if (ch < 0x20u || ch > 0x7Eu) return false;
    }
    return true;
}

bool IsSafeAssetFilename(const std::string& value, bool allowEmpty) {
    if (value.empty()) return allowEmpty;
    if (!IsPrintableAsciiString(value)) return false;
    if (value.find('/') != std::string::npos ||
        value.find('\\') != std::string::npos ||
        value.find(':') != std::string::npos ||
        value.find("..") != std::string::npos) {
        return false;
    }
    return true;
}

bool HasExtensionCI(const std::string& value, const std::string& extLower) {
    if (value.size() <= extLower.size()) return false;
    const std::string tail = LowerAscii(value.substr(value.size() - extLower.size()));
    return tail == extLower;
}

bool IsValidDivision(uint32_t division) {
    // enginelib/G-Logic/GLQuadTree.h:7-17 (QUAD_DIV_01=2 .. QUAD_DIV_10=1024)
    return division >= 2u && division <= 1024u && ((division & (division - 1u)) == 0u);
}

bool IsValidBright(uint32_t bright) {
    // RanClientLib/G-Logic/GLCharDefine.h:698-705 (BRIGHT_LIGHT=0, BRIGHT_DARK=1, BRIGHT_BOTH=2)
    return bright <= 2u;
}

bool IsSupportedLegacyRecordVersion(uint32_t ver) {
    switch (ver) {
        case 0x0100:
        case 0x0101:
        case 0x0102:
        case 0x0103:
        case 0x0200:
        case 0x0201:
        case 0x0202:
        case 0x0203:
            return true;
        default:
            return false;
    }
}

std::vector<std::string> SplitIniValues(const std::string& line) {
    std::vector<std::string> out;
    std::string token;
    for (char ch : line) {
        if (ch == ',' || ch == '[' || ch == ']' || ch == '\t') {
            out.push_back(Trim(token));
            token.clear();
        } else {
            token.push_back(ch);
        }
    }
    out.push_back(Trim(token));
    return out;
}

bool ParseBool(const std::string& value) {
    return value == "1" || value == "true" || value == "TRUE";
}

bool ParseU32(const std::string& value, uint32_t& out) {
    try {
        const unsigned long parsed = std::stoul(value);
        out = static_cast<uint32_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<uint8_t> ReadFileBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return {};
    const std::streamsize size = in.tellg();
    if (size < 0) return {};
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (size > 0 && !in.read(reinterpret_cast<char*>(data.data()), size)) return {};
    return data;
}

fs::path FindFileCaseInsensitive(const fs::path& root, const std::string& fileName) {
    if (!IsSafeAssetFilename(fileName, false)) return {};
    const std::string targetLower = LowerAscii(fileName);
    std::vector<fs::path> ciMatches;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(root, ec), end; it != end && !ec; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        const std::string candidate = it->path().filename().string();
        if (candidate == fileName) {
            return it->path();
        }
        if (LowerAscii(candidate) == targetLower) {
            ciMatches.push_back(it->path());
        }
    }
    if (ciMatches.empty()) return {};
    std::sort(ciMatches.begin(), ciMatches.end());
    return ciMatches.front();
}

} // namespace

std::string EP7MapListReport::ToString() const {
    std::ostringstream oss;
    oss << "MAP LIST: " << (valid ? "VALID" : "INVALID")
        << " | Mode: " << ValidationModeName(validationMode)
        << " | Format: " << (format.empty() ? "UNKNOWN" : format) << "\n";
    if (fileVersion != 0) {
        oss << "FileVersion: " << FormatHexU32(fileVersion);
        if (recordVersion != 0) oss << " | RecordVersion: " << FormatHexU32(recordVersion);
        oss << "\n";
    }
    if (mapKeySize != 0) oss << "MapKeySize: " << mapKeySize << "\n";
    oss << "Records: " << totalRecords << " (Used: " << usedRecords << ")"
        << " | BytesConsumed: " << bytesConsumed << "/" << fileBytes << "\n";
    if (!sha256.empty()) oss << "SHA256: " << sha256 << "\n";
    for (const auto& e : errors) oss << "ERROR: " << e << "\n";
    return oss.str();
}

EP7MapListReport EP7BinaryMapListParser::Parse(const std::vector<uint8_t>& data,
                                               EP7ValidationMode mode) {
    EP7MapListReport report;
    report.validationMode = mode;
    report.fileBytes = data.size();
    report.sha256 = EP7HashVerify::Sha256Hex(data);

    if (data.empty()) {
        report.errors.push_back("EMPTY MAP LIST FILE");
        return report;
    }

    const EP7SerialFileView header = EP7SerialFile::ReadHeaderOnly(data);
    if (!header.valid) {
        report.bytesConsumed = std::min(data.size(), EP7SerialFile::kHeaderSize);
        report.errors.push_back(header.error);
        return report;
    }

    report.format = header.fileType;
    report.fileVersion = header.version;
    report.bytesConsumed = EP7SerialFile::kHeaderSize;

    if (header.fileType != kFileHead) {
        report.errors.push_back("INVALID MAP LIST FILE HEAD (expected " +
                                std::string(kFileHead) + ", got " + header.fileType + ")");
        return report;
    }

    EP7Serial::EP7ByteCryptMode cryptMode = EP7Serial::EP7ByteCryptMode::None;
    if (mode == EP7ValidationMode::StrictAuthentic) {
        if (header.version != kVersion) {
            report.errors.push_back("UNSUPPORTED GLMAPS_LIST VERSION (strict mode requires " +
                                    FormatHexU32(kVersion) + ", got " +
                                    FormatHexU32(header.version) + ")");
            return report;
        }
        cryptMode = EP7Serial::EP7ByteCryptMode::MapsList;
    } else {
        if (header.version == kVersion) {
            cryptMode = EP7Serial::EP7ByteCryptMode::MapsList;
        } else if (header.version == kVersionEncodeOld) {
            cryptMode = EP7Serial::EP7ByteCryptMode::Old;
        } else {
            report.errors.push_back("UNSUPPORTED GLMAPS_LIST VERSION (" +
                                    FormatHexU32(header.version) + ")");
            return report;
        }
    }

    const EP7SerialFileView view = EP7SerialFile::Read(data, cryptMode);
    if (!view.valid) {
        report.errors.push_back(view.error.empty() ? "FAILED TO DECODE GLMAPS_LIST PAYLOAD" : view.error);
        return report;
    }

    size_t offset = 0;
    uint32_t dwNum = 0;
    if (!EP7SerialFile::ReadU32(view.payload, offset, dwNum)) {
        report.errors.push_back("TRUNCATED GLMAPS_LIST RECORD COUNT");
        return report;
    }
    report.bytesConsumed = EP7SerialFile::kHeaderSize + offset;

    if (dwNum == 0u) {
        report.errors.push_back("GLMAPS_LIST CONTAINS ZERO MAP RECORDS");
        return report;
    }
    if (dwNum > kMapListMax) {
        report.errors.push_back("GLMAPS_LIST RECORD COUNT EXCEEDS MAPLIST_MAX (" +
                                std::to_string(dwNum) + " > " + std::to_string(kMapListMax) + ")");
        return report;
    }

    std::set<uint32_t> seenUsedIDs;
    std::set<std::string> seenUsedFilesLower;
    uint32_t firstRecVer = 0;
    bool uniformRecVer = true;

    for (uint32_t i = 0; i < dwNum; ++i) {
        uint32_t recVer = 0;
        if (!EP7SerialFile::ReadU32(view.payload, offset, recVer)) {
            report.bytesConsumed = EP7SerialFile::kHeaderSize + offset;
            report.errors.push_back("TRUNCATED SMAPNODE_DATA VERSION AT RECORD " + std::to_string(i));
            return report;
        }
        if (i == 0) {
            firstRecVer = recVer;
        } else if (recVer != firstRecVer) {
            uniformRecVer = false;
        }

        if (mode == EP7ValidationMode::StrictAuthentic) {
            if (recVer != kRecordVersion) {
                report.bytesConsumed = EP7SerialFile::kHeaderSize + offset;
                report.errors.push_back("UNSUPPORTED SMAPNODE_DATA VERSION AT RECORD " +
                                        std::to_string(i) + " (strict mode requires " +
                                        FormatHexU32(kRecordVersion) + ", got " +
                                        FormatHexU32(recVer) + ")");
                return report;
            }
        } else if (!IsSupportedLegacyRecordVersion(recVer)) {
            report.bytesConsumed = EP7SerialFile::kHeaderSize + offset;
            report.errors.push_back("UNSUPPORTED SMAPNODE_DATA VERSION AT RECORD " +
                                    std::to_string(i) + " (" + FormatHexU32(recVer) + ")");
            return report;
        }

        EP7MapListEntry entry;
        entry.index = i;
        entry.recordVersion = recVer;

        const auto failField = [&](const std::string& detail) -> EP7MapListReport {
            report.bytesConsumed = EP7SerialFile::kHeaderSize + offset;
            report.errors.push_back("RECORD " + std::to_string(i) + ": " + detail);
            return report;
        };

        std::string strErr;
        if (!EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.used)) {
            return failField("MALFORMED OR TRUNCATED bUsed BOOL");
        }
        if (!EP7SerialFile::ReadString(view.payload, offset, entry.file, 512, &strErr)) {
            return failField("strFile: " + strErr);
        }
        if (!EP7SerialFile::ReadU16(view.payload, offset, entry.mainID) ||
            !EP7SerialFile::ReadU16(view.payload, offset, entry.subID) ||
            !EP7SerialFile::ReadU32(view.payload, offset, entry.fieldSID)) {
            return failField("TRUNCATED sNativeID/dwFieldSID");
        }
        if (!EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.peaceZone) ||
            !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.commission) ||
            !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.pkZone) ||
            !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.freePK) ||
            !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.itemDrop) ||
            !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.move) ||
            !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.restart) ||
            !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.petActivity) ||
            !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.decExp) ||
            !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.vehicleActivity) ||
            !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.clubBattleZone)) {
            return failField("MALFORMED OR TRUNCATED CORE ZONE BOOL FLAGS");
        }
        if (!EP7SerialFile::ReadString(view.payload, offset, entry.mapName, 512, &strErr)) {
            return failField("strMapName: " + strErr);
        }
        if (!EP7SerialFile::ReadString(view.payload, offset, entry.bgm, 512, &strErr)) {
            return failField("strBGM: " + strErr);
        }
        if (!EP7SerialFile::ReadString(view.payload, offset, entry.loadingImage, 512, &strErr)) {
            return failField("strLoadingImageName: " + strErr);
        }
        if (!EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.instantMap) ||
            !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.qBoxEnable)) {
            return failField("MALFORMED OR TRUNCATED bInstantMap/bQBoxEnable");
        }
        if (recVer >= 0x0101u &&
            !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.lunchBoxForbid)) {
            return failField("MALFORMED OR TRUNCATED bLunchBoxForbid");
        }
        if (recVer >= 0x0102u &&
            !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.cpReset)) {
            return failField("MALFORMED OR TRUNCATED bCPReset");
        }
        if (recVer >= 0x0103u &&
            !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.pkMap)) {
            return failField("MALFORMED OR TRUNCATED bPKMap");
        }
        if (recVer >= 0x0200u) {
            if (!EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.uiMapSelect) ||
                !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.uiMapInfo) ||
                !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.clubPKRecord) ||
                !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.openPrivateMarket) ||
                !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.partySparring)) {
                return failField("MALFORMED OR TRUNCATED UI/MARKET/SPARRING FLAGS");
            }
        }
        if (recVer >= 0x0201u) {
            if (!EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.blockTaxi) ||
                !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.blockFriendCard) ||
                !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.blockRecall) ||
                !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.blockTeleport)) {
                return failField("MALFORMED OR TRUNCATED TELEPORT BLOCK FLAGS");
            }
        }
        if (recVer >= 0x0202u &&
            !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.disableSystemBuffs)) {
            return failField("MALFORMED OR TRUNCATED bDisableSystemBuffs");
        }
        if (recVer >= 0x0203u &&
            !EP7SerialFile::ReadBoolStrict(view.payload, offset, entry.blockHPPotion)) {
            return failField("MALFORMED OR TRUNCATED bBlockHPPotion");
        }

        // Source-verified field constraints (GLMapList.cpp:158-200, DxLandDef.h:14-16, GLMapNode.h:15)
        if (!IsSafeAssetFilename(entry.file, !entry.used)) {
            return failField("UNSAFE OR EMPTY strFile");
        }
        if (!IsPrintableAsciiString(entry.mapName)) {
            return failField("NON-ASCII OR CONTROL CHARACTER IN strMapName");
        }
        if (!IsSafeAssetFilename(entry.bgm, true)) {
            return failField("UNSAFE CHARACTER IN strBGM");
        }
        if (!IsSafeAssetFilename(entry.loadingImage, true)) {
            return failField("UNSAFE CHARACTER IN strLoadingImageName");
        }
        if (entry.mainID >= kMaxLandMid || entry.subID >= kMaxLandSid) {
            return failField("MAP NATIVE ID OUT OF BOUNDS (" + std::to_string(entry.mainID) +
                             "/" + std::to_string(entry.subID) + ", limit 0..255/0..255)");
        }
        if (entry.mapName.size() > kMapNameMax) {
            return failField("MAP NAME EXCEEDS MAP_NAME_MAX (16): " + entry.mapName);
        }
        if (mode == EP7ValidationMode::StrictAuthentic && !entry.file.empty() &&
            !HasExtensionCI(entry.file, ".lev")) {
            return failField("UNEXPECTED LEVEL EXTENSION IN STRICT MODE (expected .lev, got " +
                             entry.file + ")");
        }

        if (entry.used) {
            const uint32_t packedId = (static_cast<uint32_t>(entry.mainID) << 16) | entry.subID;
            if (!seenUsedIDs.insert(packedId).second) {
                return failField("DUPLICATE USED MAP NATIVE ID (" +
                                 std::to_string(entry.mainID) + "/" +
                                 std::to_string(entry.subID) + ")");
            }
            const std::string lowerFile = LowerAscii(entry.file);
            if (!seenUsedFilesLower.insert(lowerFile).second) {
                return failField("DUPLICATE USED MAP FILE (" + entry.file + ")");
            }
            ++report.usedRecords;
        }

        report.entries.push_back(std::move(entry));
    }

    report.totalRecords = static_cast<uint32_t>(report.entries.size());
    report.recordVersion = uniformRecVer ? firstRecVer : 0u;
    report.bytesConsumed = EP7SerialFile::kHeaderSize + offset;

    if (offset != view.payload.size()) {
        report.errors.push_back("TRAILING UNPARSED BYTES IN GLMAPS_LIST PAYLOAD (consumed " +
                                std::to_string(report.bytesConsumed) + " of " +
                                std::to_string(data.size()) + ")");
        return report;
    }

    if (report.usedRecords == 0u) {
        report.errors.push_back("GLMAPS_LIST CONTAINS NO USED MAP ENTRIES");
        return report;
    }

    report.valid = true;
    return report;
}

EP7MapListReport EP7MapListParser::Parse(const std::string& text) {
    EP7MapListReport report;
    report.validationMode = EP7ValidationMode::LegacySourceCompat;
    report.format = "INI_MAP_LIST";
    report.fileBytes = text.size();

    if (text.size() >= std::strlen(EP7BinaryMapListParser::kFileHead) &&
        text.compare(0, std::strlen(EP7BinaryMapListParser::kFileHead),
                     EP7BinaryMapListParser::kFileHead) == 0) {
        report.errors.push_back("BINARY GLMAPS_LIST INPUT PASSED TO LEGACY INI PARSER");
        return report;
    }

    std::istringstream in(text);
    std::string line;
    std::string section;
    uint32_t mapIndex = 0;

    while (std::getline(in, line)) {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == ';') continue;
        if (trimmed.front() == '[' && trimmed.back() == ']') {
            section = Trim(trimmed.substr(1, trimmed.size() - 2));
            continue;
        }
        const size_t eq = trimmed.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = Trim(trimmed.substr(0, eq));
        const std::string value = trimmed.substr(eq + 1);

        if (section == "MAP_LIST_INFO" && key == "MapKeySize") {
            if (!ParseU32(Trim(value), report.mapKeySize)) report.errors.push_back("Invalid MapKeySize");
            continue;
        }
        if (section != "MAP_LIST" || key != "MAP") continue;
        if (report.mapKeySize == 0) {
            report.errors.push_back("MAP encountered before MapKeySize");
            continue;
        }
        uint32_t tmp = 0, tmp2 = 0, tmp3 = 0;
        const std::vector<std::string> v = SplitIniValues(value);
        if (v.size() < report.mapKeySize) {
            report.errors.push_back("MAP row " + std::to_string(mapIndex) + " has " +
                                    std::to_string(v.size()) + " fields, expected " +
                                    std::to_string(report.mapKeySize));
            ++mapIndex;
            continue;
        }
        EP7MapListEntry entry;
        entry.index = mapIndex++;
        entry.used = ParseBool(v[0]);
        if (!entry.used) continue;
        entry.file = v[1];
        if (!ParseU32(v[2], tmp) || !ParseU32(v[3], tmp2) || !ParseU32(v[4], tmp3)) {
            report.errors.push_back("Non-numeric ID in MAP row " + std::to_string(entry.index));
            continue;
        }
        entry.mainID = static_cast<uint16_t>(tmp);
        entry.subID = static_cast<uint16_t>(tmp2);
        entry.fieldSID = tmp3;
        entry.mapName = v[16];
        entry.bgm = v[17];
        entry.loadingImage = v[18];
        entry.instantMap = v.size() > 19 && ParseBool(v[19]);
        report.entries.push_back(entry);
    }
    report.totalRecords = mapIndex;
    report.usedRecords = static_cast<uint32_t>(report.entries.size());
    report.bytesConsumed = text.size();
    report.valid = !report.entries.empty() && report.errors.empty();
    return report;
}

EP7LevelHeader EP7LevelHeaderParser::Parse(const std::vector<uint8_t>& data,
                                           EP7ValidationMode mode) {
    EP7LevelHeader out;
    out.validationMode = mode;
    out.fileBytes = data.size();
    out.sha256 = EP7HashVerify::Sha256Hex(data);

    if (data.empty()) {
        out.error = "EMPTY LEVEL FILE";
        return out;
    }

    const EP7SerialFileView header = EP7SerialFile::ReadHeaderOnly(data);
    if (!header.valid) {
        out.bytesConsumed = std::min(data.size(), EP7SerialFile::kHeaderSize);
        out.error = header.error;
        return out;
    }
    out.fileType = header.fileType;
    out.fileVersion = header.version;
    out.bytesConsumed = EP7SerialFile::kHeaderSize;

    EP7Serial::EP7ByteCryptMode cryptMode = EP7Serial::EP7ByteCryptMode::None;
    if (mode == EP7ValidationMode::StrictAuthentic) {
        if (out.fileType != kAuthenticFileExt) {
            out.error = "UNSUPPORTED LEVEL FILE TYPE IN STRICT MODE (expected " +
                        std::string(kAuthenticFileExt) + ", got " + out.fileType + ")";
            return out;
        }
        if (out.fileVersion != kAuthenticFileVersion) {
            out.error = "UNSUPPORTED GLLEVEL/GLMAP VERSION IN STRICT MODE (expected " +
                        FormatHexU32(kAuthenticFileVersion) + ", got " +
                        FormatHexU32(out.fileVersion) + ")";
            return out;
        }
        cryptMode = EP7Serial::EP7ByteCryptMode::LevelNew;
    } else {
        if (out.fileType == kAuthenticFileExt && out.fileVersion == kAuthenticFileVersion) {
            cryptMode = EP7Serial::EP7ByteCryptMode::LevelNew;
        } else if (out.fileType == kLegacyFileType && out.fileVersion == kLegacyFileVersion) {
            cryptMode = EP7Serial::EP7ByteCryptMode::Old;
        } else {
            out.error = "UNSUPPORTED LEVEL FILE TYPE OR VERSION (" + out.fileType + " " +
                        FormatHexU32(out.fileVersion) + ")";
            return out;
        }
    }

    const EP7SerialFileView file = EP7SerialFile::Read(data, cryptMode);
    if (!file.valid) {
        out.error = file.error.empty() ? "FAILED TO DECODE LEVEL PAYLOAD" : file.error;
        return out;
    }

    size_t pos = 0;
    uint32_t headVersion = 0;
    uint32_t blockSize = 0;
    if (!EP7SerialFile::ReadU32(file.payload, pos, headVersion) ||
        !EP7SerialFile::ReadU32(file.payload, pos, blockSize)) {
        out.bytesConsumed = EP7SerialFile::kHeaderSize + pos;
        out.error = "TRUNCATED SLEVEL_HEAD HEADER";
        return out;
    }
    out.headVersion = headVersion;
    out.headBlockSize = blockSize;

    if (mode == EP7ValidationMode::StrictAuthentic) {
        if (headVersion != kAuthenticHeadVersion) {
            out.bytesConsumed = EP7SerialFile::kHeaderSize + pos;
            out.error = "UNSUPPORTED SLEVEL_HEAD VERSION IN STRICT MODE (expected " +
                        FormatHexU32(kAuthenticHeadVersion) + ", got " +
                        FormatHexU32(headVersion) + ")";
            return out;
        }
    } else if (headVersion != kAuthenticHeadVersion && headVersion != kLegacyHeadVersion) {
        out.bytesConsumed = EP7SerialFile::kHeaderSize + pos;
        out.error = "UNSUPPORTED SLEVEL_HEAD VERSION (" + FormatHexU32(headVersion) + ")";
        return out;
    }

    if (blockSize > file.payload.size() - pos) {
        out.bytesConsumed = EP7SerialFile::kHeaderSize + pos;
        out.error = "TRUNCATED SLEVEL_HEAD BLOCK (declared " + std::to_string(blockSize) +
                    " bytes, remaining " + std::to_string(file.payload.size() - pos) + ")";
        return out;
    }

    const size_t blockStart = pos;
    std::string strErr;
    if (headVersion == kAuthenticHeadVersion) {
        // SLEVEL_HEAD::LOAD_0102 (GLLevelHead.cpp:50-63):
        //   m_strMapName, m_strWldFile, m_eDivision, m_emBright
        if (!EP7SerialFile::ReadString(file.payload, pos, out.mapName, 512, &strErr)) {
            out.bytesConsumed = EP7SerialFile::kHeaderSize + pos;
            out.error = "MALFORMED SLEVEL_HEAD m_strMapName: " + strErr;
            return out;
        }
        if (!EP7SerialFile::ReadString(file.payload, pos, out.wldFile, 512, &strErr)) {
            out.bytesConsumed = EP7SerialFile::kHeaderSize + pos;
            out.error = "MALFORMED SLEVEL_HEAD m_strWldFile: " + strErr;
            return out;
        }
        if (!EP7SerialFile::ReadU32(file.payload, pos, out.division) ||
            !EP7SerialFile::ReadU32(file.payload, pos, out.bright)) {
            out.bytesConsumed = EP7SerialFile::kHeaderSize + pos;
            out.error = "TRUNCATED SLEVEL_HEAD m_eDivision/m_emBright";
            return out;
        }
    } else {
        // SLEVEL_HEAD::LOAD_0101 (GLLevelHead.cpp:32-47):
        //   m_strWldFile, m_strMapName, m_emBright, m_eDivision
        if (!EP7SerialFile::ReadString(file.payload, pos, out.wldFile, 512, &strErr)) {
            out.bytesConsumed = EP7SerialFile::kHeaderSize + pos;
            out.error = "MALFORMED SLEVEL_HEAD m_strWldFile: " + strErr;
            return out;
        }
        if (!EP7SerialFile::ReadString(file.payload, pos, out.mapName, 512, &strErr)) {
            out.bytesConsumed = EP7SerialFile::kHeaderSize + pos;
            out.error = "MALFORMED SLEVEL_HEAD m_strMapName: " + strErr;
            return out;
        }
        if (!EP7SerialFile::ReadU32(file.payload, pos, out.bright) ||
            !EP7SerialFile::ReadU32(file.payload, pos, out.division)) {
            out.bytesConsumed = EP7SerialFile::kHeaderSize + pos;
            out.error = "TRUNCATED SLEVEL_HEAD m_emBright/m_eDivision";
            return out;
        }
    }

    const size_t consumedBlock = pos - blockStart;
    out.headBlockBytesConsumed = consumedBlock;
    out.bytesConsumed = EP7SerialFile::kHeaderSize + pos;
    out.remainingPayloadBytes = file.payload.size() - pos;

    if (consumedBlock != blockSize) {
        out.error = "SLEVEL_HEAD SIZE MISMATCH (consumed " + std::to_string(consumedBlock) +
                    ", declared " + std::to_string(blockSize) + ")";
        return out;
    }

    if (!IsPrintableAsciiString(out.mapName) || out.mapName.size() > 128u) {
        out.error = "INVALID SLEVEL_HEAD m_strMapName";
        return out;
    }
    if (!IsSafeAssetFilename(out.wldFile, false)) {
        out.error = "EMPTY OR UNSAFE SLEVEL_HEAD m_strWldFile";
        return out;
    }
    if (mode == EP7ValidationMode::StrictAuthentic) {
        if (!HasExtensionCI(out.wldFile, ".wld")) {
            out.error = "INVALID WLD REFERENCE EXTENSION (expected .wld, got " + out.wldFile + ")";
            return out;
        }
        if (!IsValidDivision(out.division)) {
            out.error = "OUT-OF-BOUNDS SLEVEL_HEAD m_eDivision (" +
                        std::to_string(out.division) + ", expected power of 2 in 2..1024)";
            return out;
        }
        if (!IsValidBright(out.bright)) {
            out.error = "OUT-OF-BOUNDS SLEVEL_HEAD m_emBright (" +
                        std::to_string(out.bright) + ", expected 0..2)";
            return out;
        }
    }

    out.valid = true;
    return out;
}

std::string EP7SceneIndex::ToString() const {
    std::ostringstream oss;
    oss << "SCENE INDEX root=" << sourceRoot
        << " mode=" << ValidationModeName(validationMode)
        << " valid=" << (valid ? "YES" : "NO")
        << " format=" << (mapListFormat.empty() ? "UNKNOWN" : mapListFormat)
        << " ver=" << FormatHexU32(mapListFileVersion)
        << " records=" << totalMapRecords
        << " used=" << usedMapRecords
        << " mapListBytes=" << mapListBytesConsumed
        << " resolvedLevels=" << resolvedLevelHeaders
        << " missingLevels=" << missingLevelFiles
        << " invalidLevels=" << invalidLevelHeaders << "\n";
    for (const auto& s : scenes) {
        if (!s.glmapFound && s.error == "LEV/GLMAP FILE NOT FOUND" && scenes.size() > 16) {
            continue;
        }
        oss << "  [" << s.mainID << "/" << s.subID << "] " << s.mapName
            << " | lev=" << s.glmapFile
            << " | wld=" << s.wldFile
            << " | loading=" << s.loadingImage << "\n";
        if (!s.error.empty()) oss << "    ERROR: " << s.error << "\n";
    }
    for (const auto& d : diagnostics) oss << "DIAGNOSTIC: " << d << "\n";
    return oss.str();
}

EP7SceneIndex EP7SceneIndexBuilder::Build(const std::string& root, EP7ValidationMode mode) {
    EP7SceneBuildOptions options;
    options.validationMode = mode;
    return Build(root, options);
}

EP7SceneIndex EP7SceneIndexBuilder::Build(const std::string& root,
                                          const EP7SceneBuildOptions& options) {
    EP7SceneIndex index;
    index.sourceRoot = root;
    index.validationMode = options.validationMode;

    std::error_code ec;
    const fs::path base(root);
    if (!fs::exists(base, ec) || !fs::is_directory(base, ec)) {
        index.diagnostics.push_back("ROOT NOT FOUND: " + root);
        return index;
    }

    EP7Manifest manifest;
    const bool useManifest = !options.manifestPath.empty();
    if (useManifest) {
        manifest = EP7HashVerify::LoadManifest(options.manifestPath);
        if (!manifest.loaded) {
            index.diagnostics.push_back(manifest.error.empty() ? "MANIFEST LOAD FAILED" : manifest.error);
            return index;
        }
    }

    const fs::path manifestPath = FindFileCaseInsensitive(base, "mapslist.mst");
    if (manifestPath.empty()) {
        index.diagnostics.push_back("mapslist.mst NOT FOUND");
        return index;
    }

    const std::vector<uint8_t> mapListBytes = ReadFileBytes(manifestPath);
    if (mapListBytes.empty()) {
        index.diagnostics.push_back("mapslist.mst UNREADABLE OR EMPTY");
        return index;
    }
    index.mapListSha256 = EP7HashVerify::Sha256Hex(mapListBytes);

    size_t manifestVerificationFailures = 0;
    if (useManifest) {
        const EP7HashVerifyResult hv = EP7HashVerify::VerifyBytesAgainstManifest(
            manifest, "mapslist.mst", mapListBytes);
        if (!hv.valid) {
            index.diagnostics.push_back("MANIFEST VERIFICATION FAILED FOR mapslist.mst: " + hv.error);
            return index;
        }
    }

    EP7MapListReport list;
    if (options.validationMode == EP7ValidationMode::StrictAuthentic) {
        list = EP7BinaryMapListParser::Parse(mapListBytes, EP7ValidationMode::StrictAuthentic);
    } else {
        const std::string text(reinterpret_cast<const char*>(mapListBytes.data()), mapListBytes.size());
        list = EP7MapListParser::Parse(text);
    }

    index.mapListFormat = list.format;
    index.mapListFileVersion = list.fileVersion;
    index.totalMapRecords = list.totalRecords;
    index.usedMapRecords = list.usedRecords;
    index.mapListBytesConsumed = list.bytesConsumed;

    if (!list.valid) {
        index.diagnostics.push_back("mapslist.mst VALIDATION FAILED (" + list.format + ")");
        for (const auto& e : list.errors) index.diagnostics.push_back(e);
        return index;
    }

    for (const EP7MapListEntry& entry : list.entries) {
        if (!entry.used) continue;

        EP7SceneEntry scene;
        scene.used = entry.used;
        scene.glmapFile = entry.file;
        scene.mapName = entry.mapName;
        scene.loadingImage = entry.loadingImage;
        scene.mainID = entry.mainID;
        scene.subID = entry.subID;

        if (!entry.loadingImage.empty()) {
            const fs::path imgPath = FindFileCaseInsensitive(base, entry.loadingImage);
            if (!imgPath.empty()) {
                scene.loadingImageFound = true;
                if (useManifest && manifest.FindByPathOrName(entry.loadingImage) != nullptr) {
                    const std::vector<uint8_t> imgBytes = ReadFileBytes(imgPath);
                    const EP7HashVerifyResult hv = EP7HashVerify::VerifyBytesAgainstManifest(
                        manifest, imgPath.filename().string(), imgBytes);
                    if (!hv.valid) {
                        ++manifestVerificationFailures;
                        scene.error = "LOADING IMAGE MANIFEST FAILURE: " + hv.error;
                        index.diagnostics.push_back(scene.error);
                    }
                }
            }
        }

        const fs::path glmapPath = FindFileCaseInsensitive(base, entry.file);
        if (glmapPath.empty()) {
            ++index.missingLevelFiles;
            scene.error = "LEV/GLMAP FILE NOT FOUND";
        } else {
            scene.glmapFound = true;
            const std::vector<uint8_t> levBytes = ReadFileBytes(glmapPath);
            if (useManifest) {
                const EP7HashVerifyResult hv = EP7HashVerify::VerifyBytesAgainstManifest(
                    manifest, glmapPath.filename().string(), levBytes);
                if (!hv.valid) {
                    ++manifestVerificationFailures;
                    ++index.invalidLevelHeaders;
                    scene.error = "LEV MANIFEST VERIFICATION FAILED: " + hv.error;
                    index.diagnostics.push_back(scene.error);
                    index.scenes.push_back(std::move(scene));
                    continue;
                }
            }

            const EP7LevelHeader header = EP7LevelHeaderParser::Parse(levBytes, options.validationMode);
            scene.levelBytesConsumed = header.bytesConsumed;
            scene.levelSha256 = header.sha256;
            if (!header.valid) {
                ++index.invalidLevelHeaders;
                scene.error = "GLMAP/LEV HEADER: " + header.error;
                index.diagnostics.push_back(entry.file + ": " + scene.error);
            } else {
                scene.glmapValid = true;
                scene.wldFile = header.wldFile;
                scene.division = header.division;
                scene.bright = header.bright;
                if (scene.mapName.empty() && !header.mapName.empty()) {
                    scene.mapName = header.mapName;
                }
                ++index.resolvedLevelHeaders;

                const fs::path wldPath = FindFileCaseInsensitive(base, header.wldFile);
                if (!wldPath.empty()) {
                    scene.wldFound = true;
                    ++index.resolvedWldFiles;
                    if (useManifest && manifest.FindByPathOrName(header.wldFile) != nullptr) {
                        const std::vector<uint8_t> wldBytes = ReadFileBytes(wldPath);
                        const EP7HashVerifyResult hv = EP7HashVerify::VerifyBytesAgainstManifest(
                            manifest, wldPath.filename().string(), wldBytes);
                        if (!hv.valid) {
                            ++manifestVerificationFailures;
                            scene.error = "WLD MANIFEST VERIFICATION FAILED: " + hv.error;
                            index.diagnostics.push_back(scene.error);
                        }
                    }
                } else {
                    ++index.missingWldFiles;
                }
            }
        }
        index.scenes.push_back(std::move(scene));
    }

    index.manifestVerified = useManifest && manifestVerificationFailures == 0;

    bool ok = (index.resolvedLevelHeaders > 0) &&
              (index.invalidLevelHeaders == 0) &&
              (manifestVerificationFailures == 0);
    if (options.requireAllReferencedLevels && index.missingLevelFiles > 0) {
        ok = false;
        index.diagnostics.push_back("MISSING REFERENCED LEVEL FILES IN STRICT REQUIRE-ALL MODE (" +
                                    std::to_string(index.missingLevelFiles) + ")");
    }
    if (options.requireReferencedWlds && index.missingWldFiles > 0) {
        ok = false;
        index.diagnostics.push_back("MISSING REFERENCED WLD FILES (" +
                                    std::to_string(index.missingWldFiles) + ")");
    }
    if (index.resolvedLevelHeaders == 0) {
        index.diagnostics.push_back("NO VALID GLLEVEL HEADERS RESOLVED");
    }

    index.valid = ok;
    Logger::Info("EP7SceneIndex", index.ToString());
    return index;
}

} // namespace RANMobile
