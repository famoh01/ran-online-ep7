#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace RANMobile {

// Strict no-fallback validation mode is the default:
//   - StrictAuthentic: accepts only binary GLMAPS_LIST 0x0200 (SMAPNODE_DATA 0x0203)
//     and glmap 0x0200 (SLEVEL_HEAD 0x0102) verified against Phase 3G sources/assets.
//     Plain-text INI mapslist, older GLLEVEL 0x0104, placeholders, and silent
//     downgrades are strictly rejected.
//   - LegacySourceCompat: explicit opt-in only for older source-derived plain-text
//     INI mapslist and GLLEVEL 0x0104 / SLEVEL_HEAD 0x0101 test fixtures.
enum class EP7ValidationMode {
    StrictAuthentic = 0,
    LegacySourceCompat = 1
};

struct EP7MapListEntry {
    uint32_t index = 0;
    uint32_t recordVersion = 0;
    bool used = false;
    std::string file;         // .lev file name (GLMapNode.h:22, GLMapList.cpp:159)
    uint16_t mainID = 0;
    uint16_t subID = 0;
    uint32_t fieldSID = 0;

    bool peaceZone = false;
    bool commission = false;
    bool pkZone = false;
    bool freePK = false;
    bool itemDrop = false;
    bool move = false;
    bool restart = false;
    bool petActivity = false;
    bool decExp = false;
    bool vehicleActivity = false;
    bool clubBattleZone = false;

    std::string mapName;
    std::string bgm;
    std::string loadingImage;

    bool instantMap = false;
    bool qBoxEnable = false;
    bool lunchBoxForbid = false;
    bool cpReset = false;
    bool pkMap = false;
    bool uiMapSelect = false;
    bool uiMapInfo = false;
    bool clubPKRecord = false;
    bool openPrivateMarket = false;
    bool partySparring = false;
    bool blockTaxi = false;
    bool blockFriendCard = false;
    bool blockRecall = false;
    bool blockTeleport = false;
    bool disableSystemBuffs = false;
    bool blockHPPotion = false;
};

struct EP7MapListReport {
    bool valid = false;
    EP7ValidationMode validationMode = EP7ValidationMode::StrictAuthentic;
    std::string format;
    uint32_t fileVersion = 0;
    uint32_t recordVersion = 0;
    uint32_t mapKeySize = 0;       // Legacy INI only
    uint32_t totalRecords = 0;
    uint32_t usedRecords = 0;
    size_t fileBytes = 0;
    size_t bytesConsumed = 0;
    std::string sha256;
    std::vector<EP7MapListEntry> entries;
    std::vector<std::string> errors;
    std::string ToString() const;
};

// Parses authentic binary GLMAPS_LIST mapslist.mst using:
//   - GLMapList::LoadMapsListFile (GLMapList.cpp:104-215, GLMapList.h:18-24)
//   - SMAPNODE_DATA::LOAD (GLMapNode.cpp:128-445, GLMapNode.h:18-100)
//   - BYTECRYPT::byte_decode(EMBYTECRYPT_MAPSLIST) (ByteCrypt.cpp:47, 108-127)
class EP7BinaryMapListParser {
public:
    static constexpr const char* kFileHead = "GLMAPS_LIST";
    static constexpr uint32_t kVersion = 0x0200;
    static constexpr uint32_t kVersionEncodeOld = 0x0100;
    static constexpr uint32_t kRecordVersion = 0x0203;
    static constexpr uint32_t kMapListMax = 1000;
    static constexpr uint16_t kMaxLandMid = 256; // enginelib/DxOctree/DxLandDef.h:16
    static constexpr uint16_t kMaxLandSid = 256; // enginelib/DxOctree/DxLandDef.h:14
    static constexpr size_t kMapNameMax = 16;    // GLMapNode.h:15

    static EP7MapListReport Parse(
        const std::vector<uint8_t>& data,
        EP7ValidationMode mode = EP7ValidationMode::StrictAuthentic);
};

// Parses legacy plain-text INI mapslist.mst (GLMapList::LoadMapList_OLD).
// Never invoked in StrictAuthentic mode.
class EP7MapListParser {
public:
    static EP7MapListReport Parse(const std::string& text);
};

struct EP7LevelHeader {
    bool valid = false;
    EP7ValidationMode validationMode = EP7ValidationMode::StrictAuthentic;
    std::string fileType;
    uint32_t fileVersion = 0;
    uint32_t headVersion = 0;
    uint32_t headBlockSize = 0;
    size_t fileBytes = 0;
    size_t headBlockBytesConsumed = 0;
    size_t bytesConsumed = 0;
    size_t remainingPayloadBytes = 0;
    std::string sha256;
    std::string wldFile;
    std::string mapName;
    uint32_t bright = 0;
    uint32_t division = 0;
    std::string error;
};

// Parses CSerialFile header + SLEVEL_HEAD from authentic .Lev ("glmap" 0x0200,
// EMBYTECRYPT_LEVEL_NEW, SLEVEL_HEAD 0x0102) or explicit legacy "GLLEVEL" 0x0104 / 0x0101.
// Traced to:
//   - GLLevelFileSaveLoad.cpp:175-238 (GLLevelFile::LoadFile)
//   - GLLevelHead.cpp:8-64 (SLEVEL_HEAD::LOAD, LOAD_0101, LOAD_0102)
// Full .wld geometry and LOAD_200 subsections beyond SLEVEL_HEAD are NOT parsed here.
class EP7LevelHeaderParser {
public:
    static constexpr const char* kAuthenticFileExt = "glmap";
    static constexpr uint32_t kAuthenticFileVersion = 0x0200;
    static constexpr uint32_t kAuthenticHeadVersion = 0x0102;
    static constexpr const char* kLegacyFileType = "GLLEVEL";
    static constexpr uint32_t kLegacyFileVersion = 0x0104;
    static constexpr uint32_t kLegacyHeadVersion = 0x0101;

    static EP7LevelHeader Parse(
        const std::vector<uint8_t>& data,
        EP7ValidationMode mode = EP7ValidationMode::StrictAuthentic);
};

struct EP7SceneEntry {
    std::string mapName;
    std::string glmapFile;
    std::string wldFile;
    std::string loadingImage;
    uint16_t mainID = 0;
    uint16_t subID = 0;
    bool used = true;
    bool glmapFound = false;
    bool glmapValid = false;
    bool wldFound = false;
    bool loadingImageFound = false;
    uint32_t division = 0;
    uint32_t bright = 0;
    size_t levelBytesConsumed = 0;
    std::string levelSha256;
    std::string error;
};

struct EP7SceneBuildOptions {
    EP7ValidationMode validationMode = EP7ValidationMode::StrictAuthentic;
    bool requireAllReferencedLevels = false;
    bool requireReferencedWlds = false;
    std::string manifestPath; // Optional Phase 3G retrieval_manifest.tsv SHA-256 enforcement
};

struct EP7SceneIndex {
    bool valid = false;
    EP7ValidationMode validationMode = EP7ValidationMode::StrictAuthentic;
    std::string sourceRoot;
    std::string mapListFormat;
    uint32_t mapListFileVersion = 0;
    uint32_t totalMapRecords = 0;
    uint32_t usedMapRecords = 0;
    size_t mapListBytesConsumed = 0;
    size_t resolvedLevelHeaders = 0;
    size_t missingLevelFiles = 0;
    size_t invalidLevelHeaders = 0;
    size_t resolvedWldFiles = 0;
    size_t missingWldFiles = 0;
    bool manifestVerified = false;
    std::string mapListSha256;
    std::vector<EP7SceneEntry> scenes;
    std::vector<std::string> diagnostics;
    std::string ToString() const;
};

class EP7SceneIndexBuilder {
public:
    static EP7SceneIndex Build(
        const std::string& root,
        EP7ValidationMode mode = EP7ValidationMode::StrictAuthentic);
    static EP7SceneIndex Build(
        const std::string& root,
        const EP7SceneBuildOptions& options);
};

} // namespace RANMobile
