#include "RANMobileEngine.h"
#include "Logger.h"
#include "../Renderer/MobileRenderer.h"
#include "../Resource/ResourceManager.h"
#include "../Touch/TouchSystem.h"
#include "../Camera/MobileCamera.h"
#include "../Network/NetworkClient.h"
#include "../../Data/Loader/GameDataLoader.h"
#include "../../Data/Importer/EP7AssetPipeline.h"
#include "../Scene/EP7SceneRenderer.h"
#include "../../UI/Login/LoginScreen.h"
#include "../../UI/HUD/MobileHUD.h"

RANMobileEngine::RANMobileEngine() = default;
RANMobileEngine::~RANMobileEngine() {
    if (m_initialized) {
        Shutdown();
    }
}

bool RANMobileEngine::Initialize() {
    if (m_initialized) {
        return true;
    }

    RANMobile::Logger::Info("RANMobileEngine", "Initializing RANMobileEngine for RAN Online EP7 (FAM HAWAII)...");

    m_resourceManager = std::make_unique<RANMobile::ResourceManager>();
    m_renderer = std::make_unique<RANMobile::MobileRenderer>();
    m_touchSystem = std::make_unique<RANMobile::TouchSystem>();
    m_camera = std::make_unique<RANMobile::MobileCamera>();
    m_networkClient = std::make_unique<RANMobile::NetworkClient>();
    m_gameDataLoader = std::make_unique<RANMobile::GameDataLoader>();
    m_scene = std::make_unique<RANMobile::EP7SceneRenderer>();
    m_loginScreen = std::make_unique<LoginScreen>();
    m_mobileHUD = std::make_unique<RANMobile::MobileHUD>();

    if (!m_renderer->Initialize(1280, 720)) {
        RANMobile::Logger::Error("RANMobileEngine", "Failed to initialize MobileRenderer");
        return false;
    }
    m_renderSurfaceReady = m_renderer->HasGPUContext();
    m_touchSystem->SetViewport(m_renderer->GetWidth(), m_renderer->GetHeight());

    m_loginScreen->Initialize(m_networkClient.get());
    m_mobileHUD->Initialize(static_cast<float>(m_renderer->GetDesignWidth()),
                            static_cast<float>(m_renderer->GetDesignHeight()));
    m_initialized = true;
    RANMobile::Logger::Info("RANMobileEngine", "RANMobileEngine initialized successfully.");
    return true;
}

bool RANMobileEngine::LoadGameData(const std::string& path) {
    if (!m_initialized) {
        RANMobile::Logger::Error("RANMobileEngine", "Cannot LoadGameData before Initialize()");
        return false;
    }
    RANMobile::Logger::Info("RANMobileEngine", "Loading EP7 Game Data from: " + path);
    const bool validated = m_gameDataLoader->LoadFromDirectory(path);

    // Asset inspection is read-only; it never generates substitute assets.
    auto report = std::make_unique<RANMobile::AssetPipelineReport>(
        RANMobile::EP7AssetPipeline::ScanDirectory(path, m_resourceManager.get()));
    m_assetReport = std::move(report);
    return validated;
}

const RANMobile::AssetPipelineReport* RANMobileEngine::GetAssetPipelineReport() const {
    return m_assetReport.get();
}

bool RANMobileEngine::LoadScene(const std::string& root) {
    if (!m_initialized || !m_scene) {
        RANMobile::Logger::Error("RANMobileEngine", "Cannot LoadScene before Initialize()");
        return false;
    }
    return m_scene->Load(root, m_renderer.get(), m_resourceManager.get());
}

void RANMobileEngine::SetSafeAreaInsets(float left, float top, float right, float bottom) {
    if (m_renderer) m_renderer->SetSafeAreaInsets(left, top, right, bottom);
    if (m_loginScreen) m_loginScreen->SetSafeAreaInsets(left, top, right, bottom);
}

std::string RANMobileEngine::GetSceneStatus() const {
    return m_scene ? m_scene->GetStatus() : std::string("NO SCENE SYSTEM");
}

void RANMobileEngine::Start() {
    if (!m_initialized) {
        RANMobile::Logger::Error("RANMobileEngine", "Cannot Start() before Initialize()");
        return;
    }
    m_running = true;
    RANMobile::Logger::Info("RANMobileEngine", "Engine loop started.");
}

void RANMobileEngine::Update(float deltaTime) {
    if (!m_initialized || !m_running) {
        return;
    }
    m_totalTime += deltaTime;
    m_frameCount++;

    if (m_camera) {
        m_camera->Update(deltaTime);
    }
    if (m_loginScreen) {
        m_loginScreen->Update(deltaTime);
    }
    if (m_networkClient) {
        m_networkClient->Poll(deltaTime);
    }
}

void RANMobileEngine::Render() {
    if (!m_initialized || !m_running || !m_renderer || !m_renderSurfaceReady) {
        return;
    }
    if (!m_renderer->BeginFrame()) {
        return;
    }
    if (m_screenState == EngineScreenState::Login && m_loginScreen) {
        m_loginScreen->Draw(m_renderer.get());
    } else if (m_screenState == EngineScreenState::InGame && m_mobileHUD) {
        m_mobileHUD->Draw(m_renderer.get());
    } else if (m_screenState == EngineScreenState::ScenePreview && m_scene) {
        m_scene->Draw(m_renderer.get());
    }
    if (m_showDiagnostics) {
        m_renderer->DrawUIText(12.0f, 12.0f,
                               "EP7 DIAGNOSTICS | " + GetSceneStatus(), 0.62f,
                               0.65f, 0.82f, 0.92f, 0.75f);
    }
    m_renderer->EndFrame();
}

void RANMobileEngine::Resize(int width, int height) {
    if (m_renderer) {
        m_renderer->Resize(width, height);
        m_renderSurfaceReady = m_renderer->HasGPUContext();
    }
    if (m_touchSystem) {
        m_touchSystem->SetViewport(width, height);
    }
}

void RANMobileEngine::OnSurfaceCreated(int width, int height) {
    if (!m_initialized && !Initialize()) {
        return;
    }
    m_renderer->OnSurfaceCreated(width, height);
    m_renderSurfaceReady = m_renderer->HasGPUContext();
    m_touchSystem->SetViewport(width, height);
    m_mobileHUD->Initialize(static_cast<float>(m_renderer->GetDesignWidth()),
                            static_cast<float>(m_renderer->GetDesignHeight()));
    Start();
    RANMobile::Logger::Info("RANMobileEngine", "Surface created; renderer and input viewport updated");
}

void RANMobileEngine::OnSurfaceLost() {
    if (!m_initialized) return;
    m_renderSurfaceReady = false;
    m_running = false;
    if (m_renderer) m_renderer->OnSurfaceLost();
    if (m_touchSystem) m_touchSystem->Clear();
    if (m_loginScreen) m_loginScreen->CancelInteraction();
    RANMobile::Logger::Info("RANMobileEngine", "Surface lost; rendering paused, UI state preserved");
}

void RANMobileEngine::Pause() {
    m_running = false;
}

void RANMobileEngine::Resume() {
    if (m_initialized && m_renderSurfaceReady) {
        m_running = true;
    }
}

void RANMobileEngine::OnTouchDown(int pointerId, float x, float y) {
    if (m_touchSystem) m_touchSystem->OnTouchDown(pointerId, x, y);
    if (m_loginScreen) {
        m_loginScreen->HandleTouchDown(pointerId,
                                       m_touchSystem ? m_touchSystem->ToDesignX(x) : x,
                                       m_touchSystem ? m_touchSystem->ToDesignY(y) : y);
    }
}

void RANMobileEngine::OnTouchMove(int pointerId, float x, float y) {
    if (m_touchSystem) m_touchSystem->OnTouchMove(pointerId, x, y);
    if (m_loginScreen) {
        m_loginScreen->HandleTouchMove(pointerId,
                                       m_touchSystem ? m_touchSystem->ToDesignX(x) : x,
                                       m_touchSystem ? m_touchSystem->ToDesignY(y) : y);
    }
}

void RANMobileEngine::OnTouchUp(int pointerId, float x, float y) {
    if (m_touchSystem) m_touchSystem->OnTouchUp(pointerId, x, y);
    if (m_loginScreen) {
        m_loginScreen->HandleTouchUp(pointerId,
                                     m_touchSystem ? m_touchSystem->ToDesignX(x) : x,
                                     m_touchSystem ? m_touchSystem->ToDesignY(y) : y);
    }
}

void RANMobileEngine::OnTouchCancel(int pointerId, float x, float y) {
    if (m_touchSystem) m_touchSystem->OnTouchCancel(pointerId, x, y);
    if (m_loginScreen) m_loginScreen->HandleTouchCancel(pointerId);
}

void RANMobileEngine::OnTextInput(const std::string& utf8) {
    if (m_loginScreen) m_loginScreen->HandleTextInput(utf8);
}

void RANMobileEngine::OnBackspace() {
    if (m_loginScreen) m_loginScreen->HandleBackspace();
}

void RANMobileEngine::OnIMETextChanged(LoginFocus field, const std::string& utf8) {
    if (m_loginScreen) m_loginScreen->SetFieldText(field, utf8);
}

void RANMobileEngine::OnIMEFocusChanged(LoginFocus field) {
    if (m_loginScreen) m_loginScreen->SetFocus(field);
}

void RANMobileEngine::OnIMEActionDone() {
    if (m_loginScreen && m_loginScreen->GetFocus() != LoginFocus::None) {
        m_loginScreen->OnLoginPressed();
    }
}

void RANMobileEngine::SetKeyboardVisible(bool visible) {
    if (m_loginScreen) m_loginScreen->SetKeyboardVisible(visible);
}

void RANMobileEngine::Shutdown() {
    if (!m_initialized) {
        return;
    }
    RANMobile::Logger::Info("RANMobileEngine", "Shutting down RANMobileEngine...");
    m_running = false;
    m_renderSurfaceReady = false;
    if (m_networkClient) m_networkClient->Disconnect();
    if (m_scene) m_scene->Unload(m_renderer.get());
    if (m_renderer) m_renderer->Shutdown();
    if (m_resourceManager) m_resourceManager->UnloadAll();
    m_initialized = false;
}
