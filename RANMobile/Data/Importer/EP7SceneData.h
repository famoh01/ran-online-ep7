#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace RANMobile {

struct EP7MapListEntry {
    uint32_t index = 0;
    bool used = false;
    std::string file;         // .glmap file name (GLMapList.cpp:110)
    uint16_t mainID = 0;
    uint16_t subID = 0;
    uint32_t fieldSID = 0;
    std::string mapName;
    std::string bgm;
    std::string loadingImage;
    bool instantMap = false;
};

struct EP7MapListReport {
    bool valid = false;
    uint32_t mapKeySize = 0;
    std::vector<EP7MapListEntry> entries;
    std::vector<std::string> errors;
    std::string ToString() const;
};

// Parses mapslist.mst using the field order in GLMapList::LoadMapList
// (RanClientLib/G-Logic/GLMapList.cpp:96-162) and the separators registered
// there (",[]\t"). Encoded/encrypted variants are rejected, not guessed.
class EP7MapListParser {
public:
    static EP7MapListReport Parse(const std::string& text);
};

struct EP7LevelHeader {
    bool valid = false;
    std::string fileType;
    uint32_t fileVersion = 0;
    uint32_t headVersion = 0;
    std::string wldFile;
    std::string mapName;
    uint32_t bright = 0;
    uint32_t division = 0;
    std::string error;
};

// Parses only SLEVEL_HEAD from a .glmap file (GLLevelFile.cpp:629-656, 1337-1392).
// Full map geometry resides in DxLandMan/.wld and is NOT parsed here.
class EP7LevelHeaderParser {
public:
    static EP7LevelHeader Parse(const std::vector<uint8_t>& data);
};

struct EP7SceneEntry {
    std::string mapName;
    std::string glmapFile;
    std::string wldFile;
    std::string loadingImage;
    uint16_t mainID = 0;
    uint16_t subID = 0;
    std::string error;
};

struct EP7SceneIndex {
    bool valid = false;
    std::string sourceRoot;
    std::vector<EP7SceneEntry> scenes;
    std::vector<std::string> diagnostics;
    std::string ToString() const;
};

class EP7SceneIndexBuilder {
public:
    // Reads mapslist.mst and each referenced .glmap header from root.
    static EP7SceneIndex Build(const std::string& root);
};

} // namespace RANMobile
