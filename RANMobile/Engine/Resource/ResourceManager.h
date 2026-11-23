#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace RANMobile {

struct ResourceEntry {
    std::string id;
    std::string type;
    std::string path;
    size_t sizeBytes = 0;
    bool loaded = false;
};

class ResourceManager {
public:
    bool RegisterResource(const std::string& id, const std::string& type, const std::string& path, size_t sizeBytes);
    bool HasResource(const std::string& id) const;
    const ResourceEntry* GetResource(const std::string& id) const;
    void UnloadAll();
    size_t GetTotalMemoryBytes() const { return m_totalMemoryBytes; }
    size_t GetResourceCount() const { return m_resources.size(); }

private:
    std::unordered_map<std::string, ResourceEntry> m_resources;
    size_t m_totalMemoryBytes = 0;
};

} // namespace RANMobile
