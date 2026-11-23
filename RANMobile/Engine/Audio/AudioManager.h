#pragma once

#include <string>

namespace RANMobile {

class AudioManager {
public:
    bool Initialize();
    void Shutdown();
    void PlayBGM(const std::string& filePath, bool loop = true);
    void PlaySFX(const std::string& filePath);
    void SetMasterVolume(float volume);
    float GetMasterVolume() const { return m_masterVolume; }
    bool IsInitialized() const { return m_initialized; }

private:
    bool m_initialized = false;
    float m_masterVolume = 1.0f;
};

} // namespace RANMobile
