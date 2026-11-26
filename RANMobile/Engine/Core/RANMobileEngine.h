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
    struct AssetPipelineReport;
}
class LoginScreen;
namespace RANMobile { class MobileHUD; }

enum class EngineScreenState {
    Login,
    CharacterSelect,
    InGame
};

class RANMobileEngine {
public:
    RANMobileEngine();
    ~RANMobileEngine();

    bool Initialize();
    bool LoadGameData(const std::string& path);
    const RANMobile::AssetPipelineReport* GetAssetPipelineReport() const;

    void Start();
    void Update(float deltaTime);
    void Render();
    void Resize(int width, int height);

    // Android surface lifecycle: rendering is separated from UI state so a
    // destroyed EGL surface does not need a full engine restart.
    void OnSurfaceCreated(int width, int height);
    void OnSurfaceLost();

    void Pause();
    void Resume();
    void Shutdown();

    void OnTouchDown(int pointerId, float x, float y);
    void OnTouchMove(int pointerId, float x, float y);
    void OnTouchUp(int pointerId, float x, float y);
    void OnTouchCancel(int pointerId, float x, float y);
    void OnTextInput(const std::string& utf8);
    void OnBackspace();

    LoginScreen* GetLoginScreen() const { return m_loginScreen.get(); }
    RANMobile::MobileHUD* GetMobileHUD() const { return m_mobileHUD.get(); }
    void SetScreenState(EngineScreenState state) { m_screenState = state; }
    EngineScreenState GetScreenState() const { return m_screenState; }

    bool IsInitialized() const { return m_initialized; }
    bool IsRunning() const { return m_running; }
    bool HasRenderSurface() const { return m_renderSurfaceReady; }

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
    bool m_renderSurfaceReady = false;
    float m_totalTime = 0.0f;
    uint64_t m_frameCount = 0;

    std::unique_ptr<RANMobile::MobileRenderer> m_renderer;
    std::unique_ptr<RANMobile::ResourceManager> m_resourceManager;
    std::unique_ptr<RANMobile::TouchSystem> m_touchSystem;
    std::unique_ptr<RANMobile::MobileCamera> m_camera;
    std::unique_ptr<RANMobile::NetworkClient> m_networkClient;
    std::unique_ptr<RANMobile::GameDataLoader> m_gameDataLoader;
    std::unique_ptr<LoginScreen> m_loginScreen;
    std::unique_ptr<RANMobile::MobileHUD> m_mobileHUD;
    std::unique_ptr<RANMobile::AssetPipelineReport> m_assetReport;
    EngineScreenState m_screenState = EngineScreenState::Login;
};
