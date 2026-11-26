#include "EP7AssetPipeline.h"
#include "EP7AssetImporter.h"
#include "../../Engine/Resource/ResourceManager.h"
#include "../../Engine/Core/Logger.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace RANMobile {

namespace {

struct FormatRule {
    const char* extension;
    const char* category;
};

// Extensions are taken from SOURCE_ASSET_INVENTORY.md and the original loaders.
constexpr FormatRule kSupportedFormats[] = {
    {".mst", "Map list"},        {".wld", "Map"},          {".lev", "Map"},
    {".mnsf", "Monster"},        {".ntk", "NPC talk"},     {".isf", "Item"},
    {".ssf", "Skill"},           {".qst", "Quest"},        {".lst", "Quest list"},
    {".x", "Model"},             {".mxf", "Model"},        {".cps", "Character"},
    {".chf", "Character"},       {".dds", "Texture"},      {".mtf", "Texture"},
    {".tga", "Texture"},         {".bin", "Animation"},    {".egp", "Effect"},
    {".wav", "Sound"},           {".ogg", "Sound"}
};

const FormatRule* FindRule(const std::string& extension) {
    for (const FormatRule& rule : kSupportedFormats) {
        if (extension == rule.extension) return &rule;
    }
    return nullptr;
}

bool HasDDSMagic(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    char magic[4] = {0, 0, 0, 0};
    in.read(magic, 4);
    return magic[0] == 'D' && magic[1] == 'D' && magic[2] == 'S' && magic[3] == ' ';
}

std::string LowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

} // namespace

std::string AssetPipelineReport::ToString() const {
    std::ostringstream oss;
    oss << "ASSET PIPELINE REPORT\n";
    oss << "Root: " << root << "\n";
    oss << "Root exists: " << (rootExists ? "YES" : "NO") << "\n";
    oss << "Files scanned: " << filesScanned << "\n";
    oss << "Accepted: " << accepted << " (" << bytes << " bytes)\n";
    oss << "Rejected: " << rejected << "\n";
    for (const auto& entry : acceptedByFormat) {
        oss << "  " << entry.first << ": " << entry.second << "\n";
    }
    for (const auto& diag : diagnostics) oss << "DIAGNOSTIC: " << diag << "\n";
    return oss.str();
}

AssetPipelineItem EP7AssetPipeline::InspectFile(const std::string& root, const std::string& fullPath) {
    AssetPipelineItem item;
    std::error_code ec;
    fs::path file(fullPath);
    fs::path base(root);
    fs::path rel = file.lexically_relative(base);
    item.relativePath = rel.empty() ? file.filename().string() : rel.generic_string();

    const ImportedAssetInfo imported = EP7AssetImporter::InspectAsset(item.relativePath, fullPath);
    item.format = LowerCopy(imported.format);
    if (!imported.found) {
        item.reason = "FILE NOT FOUND";
        return item;
    }
    item.sizeBytes = static_cast<size_t>(fs::file_size(file, ec));
    if (ec || item.sizeBytes == 0) {
        item.reason = "EMPTY OR UNREADABLE FILE";
        return item;
    }
    const FormatRule* rule = FindRule(item.format);
    if (!rule) {
        item.reason = "FORMAT NOT IN SOURCE INVENTORY";
        return item;
    }
    item.category = rule->category;
    if (item.format == ".dds" && !HasDDSMagic(file)) {
        item.reason = "INVALID DDS HEADER";
        return item;
    }
    item.accepted = true;
    item.reason = "OK";
    return item;
}

AssetPipelineReport EP7AssetPipeline::ScanDirectory(const std::string& root,
                                                    ResourceManager* resources) {
    AssetPipelineReport report;
    report.root = root;
    std::error_code ec;
    fs::path base(root);
    if (!fs::exists(base, ec) || !fs::is_directory(base, ec)) {
        report.diagnostics.push_back("ROOT NOT FOUND: " + root);
        Logger::Warn("EP7AssetPipeline", report.diagnostics.back());
        return report;
    }
    report.rootExists = true;

    for (fs::recursive_directory_iterator it(base, ec), end; it != end && !ec; it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        if (report.filesScanned >= 20000) {
            report.diagnostics.push_back("SCAN LIMIT REACHED (20000 files)");
            break;
        }
        ++report.filesScanned;
        AssetPipelineItem item = InspectFile(root, it->path().string());
        if (item.accepted) {
            ++report.accepted;
            report.bytes += item.sizeBytes;
            ++report.acceptedByFormat[item.format];
            if (resources) {
                const std::string id = item.relativePath;
                resources->RegisterResource(id, item.category, it->path().string(), item.sizeBytes);
            }
        } else {
            ++report.rejected;
            if (report.diagnostics.size() < 64) {
                report.diagnostics.push_back(item.relativePath + ": " + item.reason);
            }
        }
        if (report.items.size() < 256) report.items.push_back(item);
    }
    if (ec) report.diagnostics.push_back(std::string("FILESystem ERROR: ") + ec.message());

    Logger::Info("EP7AssetPipeline", report.ToString());
    return report;
}

} // namespace RANMobile
