#include "RANMobileEngine.h"
#include "Logger.h"
#include "../Renderer/MobileRenderer.h"
#include "../Resource/ResourceManager.h"
#include "../Touch/TouchSystem.h"
#include "../Camera/MobileCamera.h"
#include "../Network/NetworkClient.h"
#include "../../Data/Loader/GameDataLoader.h"
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
    m_loginScreen = std::make_unique<LoginScreen>();
    m_mobileHUD = std::make_unique<RANMobile::MobileHUD>();

    if (!m_renderer->Initialize(1280, 720)) {
        RANMobile::Logger::Error("RANMobileEngine", "Failed to initialize MobileRenderer");
        return false;
    }

    m_loginScreen->Initialize(m_networkClient.get());
    m_mobileHUD->Initialize(1280.0f, 720.0f);
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
    return m_gameDataLoader->LoadFromDirectory(path);
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
    if (!m_initialized || !m_running || !m_renderer) {
        return;
    }
    m_renderer->BeginFrame();
    if (m_screenState == EngineScreenState::Login && m_loginScreen) {
        m_loginScreen->Draw(m_renderer.get());
    } else if (m_screenState == EngineScreenState::InGame && m_mobileHUD) {
        m_mobileHUD->Draw(m_renderer.get());
    }
    m_renderer->EndFrame();
}

void RANMobileEngine::Resize(int width, int height) {
    if (m_renderer) {
        m_renderer->Resize(width, height);
    }
}

void RANMobileEngine::Pause() {
    m_running = false;
}

void RANMobileEngine::Resume() {
    if (m_initialized) {
        m_running = true;
    }
}

void RANMobileEngine::OnTouchDown(int pointerId, float x, float y) {
    if (m_touchSystem) m_touchSystem->OnTouchDown(pointerId, x, y);
    if (m_loginScreen) m_loginScreen->HandleTouchDown(pointerId, x, y);
}

void RANMobileEngine::OnTouchMove(int pointerId, float x, float y) {
    if (m_touchSystem) m_touchSystem->OnTouchMove(pointerId, x, y);
    if (m_loginScreen) m_loginScreen->HandleTouchMove(pointerId, x, y);
}

void RANMobileEngine::OnTouchUp(int pointerId, float x, float y) {
    if (m_touchSystem) m_touchSystem->OnTouchUp(pointerId, x, y);
    if (m_loginScreen) m_loginScreen->HandleTouchUp(pointerId, x, y);
}

void RANMobileEngine::OnTextInput(const std::string& utf8) {
    if (m_loginScreen) m_loginScreen->HandleTextInput(utf8);
}

void RANMobileEngine::OnBackspace() {
    if (m_loginScreen) m_loginScreen->HandleBackspace();
}

void RANMobileEngine::Shutdown() {
    if (!m_initialized) {
        return;
    }
    RANMobile::Logger::Info("RANMobileEngine", "Shutting down RANMobileEngine...");
    m_running = false;
    if (m_networkClient) m_networkClient->Disconnect();
    if (m_renderer) m_renderer->Shutdown();
    if (m_resourceManager) m_resourceManager->UnloadAll();
    m_initialized = false;
}
