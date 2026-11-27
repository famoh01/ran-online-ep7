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
    class EP7CharacterScene;
}
class LoginScreen;
namespace RANMobile { class MobileHUD; }

enum class EngineScreenState {
    Login,
    CharacterSelect,
    InGame,
    CharacterPreview  // B17: authentic character/animation preview (engine diagnostic path)
};

class RANMobileEngine {
public:
    RANMobileEngine();
    ~RANMobileEngine();

    bool Initialize();
    bool LoadGameData(const std::string& path);

    void Start();
    void Update(float deltaTime);
    void Render();

    // B19 GPU-resource lifecycle: called by the Android bridge when the EGL context is
    // destroyed (window loss/rotation).  Releases every GL object (renderer + scene
    // caches) and resets cached handles so the next window rebuilds cleanly instead
    // of drawing with stale handles.  Game/login state is preserved.
    void OnGpuContextLost();

    // B19: re-creates the renderer's GL objects after OnGpuContextLost (the engine
    // itself stays initialized).  Returns false when the renderer cannot be rebuilt.
    bool EnsureGpuContext(int width, int height);
    void Resize(int width, int height);
    void Pause();
    void Resume();
    void Shutdown();

    void OnTouchDown(int pointerId, float x, float y);
    void OnTouchMove(int pointerId, float x, float y);
    void OnTouchUp(int pointerId, float x, float y);
    void OnTextInput(const std::string& utf8);
    void OnBackspace();

    LoginScreen* GetLoginScreen() const { return m_loginScreen.get(); }
    RANMobile::MobileHUD* GetMobileHUD() const { return m_mobileHUD.get(); }
    void SetScreenState(EngineScreenState state) { m_screenState = state; }
    EngineScreenState GetScreenState() const { return m_screenState; }

    // B17: authentic character/animation runtime. The engine owns the scene; assets
    // are registered through EP7CharacterScene (EP7CharacterLoader with an asset
    // resolver — pack archive, Android assets, or test fixtures). The scene updates
    // and renders inside the CharacterPreview screen state with the shared camera.
    RANMobile::EP7CharacterScene* GetCharacterScene() const { return m_characterScene.get(); }

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
    std::unique_ptr<LoginScreen> m_loginScreen;
    std::unique_ptr<RANMobile::MobileHUD> m_mobileHUD;
    std::unique_ptr<RANMobile::EP7CharacterScene> m_characterScene;
    EngineScreenState m_screenState = EngineScreenState::Login;
};
