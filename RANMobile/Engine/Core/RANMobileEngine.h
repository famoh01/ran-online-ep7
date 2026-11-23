#pragma once

#include <string>
#include <memory>
#include <cstdint>

namespace RANMobile {
    class MobileRenderer;
    class ResourceManager;
    class TouchSystem;
    class MobileCamera;
    class NetworkClient;
    class GameDataLoader;
}

class RANMobileEngine {
public:
    RANMobileEngine();
    ~RANMobileEngine();

    bool Initialize();
    bool LoadGameData(const std::string& path);

    void Start();
    void Update(float deltaTime);
    void Render();
    void Shutdown();

    bool IsInitialized() const { return m_initialized; }
    bool IsRunning() const { return m_running; }

    float GetTotalTime() const { return m_totalTime; }
    uint64_t GetFrameCount() const { return m_frameCount; }

    RANMobile::MobileRenderer* GetRenderer() const { return m_renderer.get(); }
    RANMobile::ResourceManager* GetResourceManager() const { return m_resourceManager.get(); }
    RANMobile::TouchSystem* GetTouchSystem() const { return m_touchSystem.get(); }
    RANMobile::MobileCamera* GetCamera() const { return m_camera.get(); }
    RANMobile::NetworkClient* GetNetworkClient() const { return m_networkClient.get(); }
    RANMobile::GameDataLoader* GetGameDataLoader() const { return m_gameDataLoader.get(); }

private:
    bool m_initialized = false;
    bool m_running = false;
    float m_totalTime = 0.0f;
    uint64_t m_frameCount = 0;

    std::unique_ptr<RANMobile::MobileRenderer> m_renderer;
    std::unique_ptr<RANMobile::ResourceManager> m_resourceManager;
    std::unique_ptr<RANMobile::TouchSystem> m_touchSystem;
    std::unique_ptr<RANMobile::MobileCamera> m_camera;
    std::unique_ptr<RANMobile::NetworkClient> m_networkClient;
    std::unique_ptr<RANMobile::GameDataLoader> m_gameDataLoader;
};
