#include "ResourceManager.h"

namespace RANMobile {

bool ResourceManager::RegisterResource(const std::string& id, const std::string& type, const std::string& path, size_t sizeBytes) {
    if (id.empty()) return false;
    ResourceEntry entry;
    entry.id = id;
    entry.type = type;
    entry.path = path;
    entry.sizeBytes = sizeBytes;
    entry.loaded = true;

    auto it = m_resources.find(id);
    if (it != m_resources.end()) {
        m_totalMemoryBytes -= it->second.sizeBytes;
    }
    m_resources[id] = entry;
    m_totalMemoryBytes += sizeBytes;
    return true;
}

bool ResourceManager::HasResource(const std::string& id) const {
    return m_resources.find(id) != m_resources.end();
}

const ResourceEntry* ResourceManager::GetResource(const std::string& id) const {
    auto it = m_resources.find(id);
    if (it == m_resources.end()) return nullptr;
    return &it->second;
}

void ResourceManager::UnloadAll() {
    m_resources.clear();
    m_totalMemoryBytes = 0;
}

} // namespace RANMobile
