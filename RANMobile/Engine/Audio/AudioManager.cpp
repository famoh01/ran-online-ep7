#include "AudioManager.h"
#include "../Core/Logger.h"
#include <algorithm>

namespace RANMobile {

bool AudioManager::Initialize() {
    m_initialized = true;
    Logger::Info("AudioManager", "Initialized OpenSL ES / Portable AudioManager");
    return true;
}

void AudioManager::Shutdown() {
    m_initialized = false;
}

void AudioManager::PlayBGM(const std::string& filePath, bool loop) {
    (void)loop;
    if (!m_initialized || filePath.empty()) return;
}

void AudioManager::PlaySFX(const std::string& filePath) {
    if (!m_initialized || filePath.empty()) return;
}

void AudioManager::SetMasterVolume(float volume) {
    m_masterVolume = std::clamp(volume, 0.0f, 1.0f);
}

} // namespace RANMobile
