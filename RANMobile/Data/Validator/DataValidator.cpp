#include "DataValidator.h"
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace RANMobile {

std::string DataValidationReport::ToString() const {
    std::ostringstream oss;
    oss << "DATA VALIDATION REPORT\n\n";
    oss << "Map: " << (mapFound ? "FOUND" : "NOT FOUND") << "\n";
    oss << "Monster: " << (monsterFound ? "FOUND" : "NOT FOUND") << "\n";
    oss << "NPC: " << (npcFound ? "FOUND" : "NOT FOUND") << "\n";
    oss << "Item: " << (itemFound ? "FOUND" : "NOT FOUND") << "\n";
    oss << "Skill: " << (skillFound ? "FOUND" : "NOT FOUND") << "\n";
    oss << "Quest: " << (questFound ? "FOUND" : "NOT FOUND") << "\n";
    oss << "Model: " << (modelFound ? "FOUND" : "NOT FOUND") << "\n";
    oss << "Texture: " << (textureFound ? "FOUND" : "NOT FOUND") << "\n";
    oss << "Animation: " << (animationFound ? "FOUND" : "NOT FOUND") << "\n";

    for (const auto& err : errors) {
        oss << "\nDATA ERROR\n";
        oss << "Resource ID: " << err.resourceID << "\n";
        oss << "Resource Type: " << err.resourceType << "\n";
        oss << "File: " << err.file << "\n";
        oss << "Status: " << err.status << "\n";
    }
    return oss.str();
}

static bool CheckFileExists(const fs::path& base, const std::string& rel) {
    std::error_code ec;
    return fs::exists(base / rel, ec);
}

DataValidationReport DataValidator::ValidateDirectory(const std::string& dataDir) {
    DataValidationReport report;
    fs::path base(dataDir);

    report.mapFound = CheckFileExists(base, "mapslist.mst") || CheckFileExists(base, "map");
    if (!report.mapFound) report.errors.push_back({"N/A", "Map", "mapslist.mst", "NOT FOUND"});

    report.monsterFound = CheckFileExists(base, "crow.mnsf") || CheckFileExists(base, "glogic/crow.mnsf");
    if (!report.monsterFound) report.errors.push_back({"N/A", "Monster", "crow.mnsf", "NOT FOUND"});

    report.npcFound = CheckFileExists(base, "npctalk") || report.monsterFound;
    if (!report.npcFound) report.errors.push_back({"N/A", "NPC", "npctalk/", "NOT FOUND"});

    report.itemFound = CheckFileExists(base, "item.isf") || CheckFileExists(base, "glogic/item.isf");
    if (!report.itemFound) report.errors.push_back({"N/A", "Item", "item.isf", "NOT FOUND"});

    report.skillFound = CheckFileExists(base, "skill.ssf") || CheckFileExists(base, "glogic/skill.ssf");
    if (!report.skillFound) report.errors.push_back({"N/A", "Skill", "skill.ssf", "NOT FOUND"});

    report.questFound = CheckFileExists(base, "quest.lst") || CheckFileExists(base, "quest");
    if (!report.questFound) report.errors.push_back({"N/A", "Quest", "quest.lst", "NOT FOUND"});

    report.modelFound = CheckFileExists(base, "skinobject") || CheckFileExists(base, "object");
    if (!report.modelFound) report.errors.push_back({"N/A", "Model", "skinobject/", "NOT FOUND"});

    report.textureFound = CheckFileExists(base, "texture");
    if (!report.textureFound) report.errors.push_back({"N/A", "Texture", "texture/", "NOT FOUND"});

    report.animationFound = CheckFileExists(base, "animation");
    if (!report.animationFound) report.errors.push_back({"N/A", "Animation", "animation/", "NOT FOUND"});

    return report;
}

} // namespace RANMobile
