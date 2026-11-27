#include "RANMobileEngine.h"
#include "Logger.h"
#include "../Renderer/MobileRenderer.h"
#include "../Resource/ResourceManager.h"
#include "../Touch/TouchSystem.h"
#include "../Camera/MobileCamera.h"
#include "../Network/NetworkClient.h"
#include "../../Data/Loader/GameDataLoader.h"
#include "../../Data/Importer/EP7AssetPipeline.h"
#include "../../Client/Gameplay/CharacterController.h"
#include "../Scene/EP7SceneRenderer.h"
#include "../../UI/Login/LoginScreen.h"
#include "../../UI/HUD/MobileHUD.h"

#include <algorithm>
#include <cmath>

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
    m_character = std::make_unique<RANMobile::CharacterController>();
    m_scene = std::make_unique<RANMobile::EP7SceneRenderer>();
    m_loginScreen = std::make_unique<LoginScreen>();
    m_mobileHUD = std::make_unique<RANMobile::MobileHUD>();

    if (!m_renderer->Initialize(1280, 720)) {
        RANMobile::Logger::Error("RANMobileEngine", "Failed to initialize MobileRenderer");
        return false;
    }
    m_renderSurfaceReady = m_renderer->HasGPUContext();
    m_touchSystem->SetViewport(m_renderer->GetWidth(), m_renderer->GetHeight());
    m_camera->SetViewportAspect(m_renderer->GetDesignWidth() /
                                std::max(1.0f, m_renderer->GetDesignHeight()));

    m_loginScreen->Initialize(m_networkClient.get());
    m_mobileHUD->Initialize(static_cast<int>(std::lround(m_renderer->GetDesignWidth())),
                            static_cast<int>(std::lround(m_renderer->GetDesignHeight())));
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

    auto report = std::make_unique<RANMobile::AssetPipelineReport>(
        RANMobile::EP7AssetPipeline::ScanDirectory(path, m_resourceManager.get()));
    m_assetReport = std::move(report);
    return validated;
}

const RANMobile::AssetPipelineReport* RANMobileEngine::GetAssetPipelineReport() const {
    return m_assetReport.get();
}

void RANMobileEngine::SyncCameraOnWorldLoad() {
    if (m_scene && m_scene->HasWorld3D() && m_camera) {
        m_scene->ConfigureDefaultCamera(m_camera.get());
        if (m_character) {
            m_character->SetPosition(m_camera->GetTargetX(),
                                     m_camera->GetTargetY(),
                                     m_camera->GetTargetZ());
        }
    }
}

bool RANMobileEngine::LoadScene(const std::string& root) {
    return LoadScene(root, RANMobile::EP7ValidationMode::StrictAuthentic);
}

bool RANMobileEngine::LoadScene(const std::string& root, RANMobile::EP7ValidationMode mode) {
    if (!m_initialized || !m_scene) {
        RANMobile::Logger::Error("RANMobileEngine", "Cannot LoadScene before Initialize()");
        return false;
    }
    const bool ok = m_scene->Load(root, m_renderer.get(), m_resourceManager.get(), mode);
    if (ok) SyncCameraOnWorldLoad();
    return ok;
}

bool RANMobileEngine::LoadSceneWithManifest(const std::string& root, const std::string& manifestPath) {
    if (!m_initialized || !m_scene) {
        RANMobile::Logger::Error("RANMobileEngine", "Cannot LoadSceneWithManifest before Initialize()");
        return false;
    }
    RANMobile::EP7SceneBuildOptions options;
    options.validationMode = RANMobile::EP7ValidationMode::StrictAuthentic;
    options.manifestPath = manifestPath;
    const bool ok = m_scene->Load(root, m_renderer.get(), m_resourceManager.get(), options);
    if (ok) SyncCameraOnWorldLoad();
    return ok;
}

bool RANMobileEngine::LoadStandaloneWorld(const std::string& root, const std::string& wldFileName) {
    if (!m_initialized || !m_scene) {
        RANMobile::Logger::Error("RANMobileEngine", "Cannot LoadStandaloneWorld before Initialize()");
        return false;
    }
    const bool ok = m_scene->LoadStandaloneWorld(root, wldFileName,
                                                 m_renderer.get(), m_resourceManager.get());
    if (ok) SyncCameraOnWorldLoad();
    return ok;
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

    if (m_screenState == EngineScreenState::InGame && m_mobileHUD && m_camera && m_character) {
        m_mobileHUD->Update(deltaTime);
        m_character->Update(deltaTime, m_mobileHUD->GetJoystick(), *m_camera);
        if (m_character->IsMoving()) {
            m_camera->SetTarget(m_character->GetPosX(),
                                m_character->GetPosY(),
                                m_character->GetPosZ());
        }
    }

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
    } else if (m_screenState == EngineScreenState::InGame) {
        if (m_scene && m_scene->HasWorld3D()) {
            m_scene->Draw(m_renderer.get(), m_camera.get());
        }
        if (m_mobileHUD) {
            m_mobileHUD->Draw(m_renderer.get());
        }
    } else if (m_screenState == EngineScreenState::ScenePreview && m_scene) {
        m_scene->Draw(m_renderer.get(), m_camera.get());
    }
    if (m_showDiagnostics) {
        const auto& st = m_renderer->GetStats();
        std::string diag = "EP7 | " + GetSceneStatus() +
                           " | DC=" + std::to_string(st.drawCalls) +
                           " TRI=" + std::to_string(st.triangles) +
                           " CULL=" + std::to_string(st.culledObjects);
        m_renderer->DrawUIQuad(8.0f, 8.0f, m_renderer->GetDesignWidth() - 16.0f, 30.0f,
                               0.02f, 0.05f, 0.12f, 0.65f);
        m_renderer->DrawUIText(14.0f, 14.0f, diag, 0.58f,
                               0.75f, 0.90f, 1.0f, 0.92f);
    }
    m_renderer->EndFrame();
}

void RANMobileEngine::Resize(int width, int height) {
    if (m_renderer) {
        m_renderer->Resize(width, height);
        m_renderSurfaceReady = m_renderer->HasGPUContext();
        if (m_camera) {
            m_camera->SetViewportAspect(m_renderer->GetDesignWidth() /
                                        std::max(1.0f, m_renderer->GetDesignHeight()));
        }
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
    m_camera->SetViewportAspect(m_renderer->GetDesignWidth() /
                                std::max(1.0f, m_renderer->GetDesignHeight()));
    m_mobileHUD->Initialize(static_cast<int>(std::lround(m_renderer->GetDesignWidth())),
                            static_cast<int>(std::lround(m_renderer->GetDesignHeight())));
    Start();
    RANMobile::Logger::Info("RANMobileEngine", "Surface created; renderer and input viewport updated");
}

void RANMobileEngine::OnSurfaceLost() {
    if (!m_initialized) return;
    m_renderSurfaceReady = false;
    m_running = false;
    m_cameraPointers.clear();
    m_lastPinchDist = 0.0f;
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

void RANMobileEngine::UpdatePinchState() {
    if (m_cameraPointers.size() < 2 || !m_touchSystem) {
        m_lastPinchDist = 0.0f;
        return;
    }
    const auto* p0 = m_touchSystem->GetTouch(m_cameraPointers[0]);
    const auto* p1 = m_touchSystem->GetTouch(m_cameraPointers[1]);
    if (!p0 || !p1) {
        m_lastPinchDist = 0.0f;
        return;
    }
    const float dx = p1->designX - p0->designX;
    const float dy = p1->designY - p0->designY;
    m_lastPinchDist = std::sqrt(dx * dx + dy * dy);
    m_lastPinchMidX = 0.5f * (p0->designX + p1->designX);
    m_lastPinchMidY = 0.5f * (p0->designY + p1->designY);
}

void RANMobileEngine::OnTouchDown(int pointerId, float x, float y) {
    if (m_touchSystem) m_touchSystem->OnTouchDown(pointerId, x, y);
    const float dx = m_touchSystem ? m_touchSystem->ToDesignX(x) : x;
    const float dy = m_touchSystem ? m_touchSystem->ToDesignY(y) : y;

    if (m_screenState == EngineScreenState::Login) {
        if (m_loginScreen) m_loginScreen->HandleTouchDown(pointerId, dx, dy);
        return;
    }

    if (m_screenState == EngineScreenState::InGame && m_mobileHUD) {
        if (m_mobileHUD->OnTouchDown(pointerId, dx, dy)) {
            return;
        }
    }

    if (m_screenState == EngineScreenState::InGame || m_screenState == EngineScreenState::ScenePreview) {
        if (std::find(m_cameraPointers.begin(), m_cameraPointers.end(), pointerId) == m_cameraPointers.end()) {
            m_cameraPointers.push_back(pointerId);
            UpdatePinchState();
        }
    }
}

void RANMobileEngine::OnTouchMove(int pointerId, float x, float y) {
    if (m_touchSystem) m_touchSystem->OnTouchMove(pointerId, x, y);
    const float dx = m_touchSystem ? m_touchSystem->ToDesignX(x) : x;
    const float dy = m_touchSystem ? m_touchSystem->ToDesignY(y) : y;

    if (m_screenState == EngineScreenState::Login) {
        if (m_loginScreen) m_loginScreen->HandleTouchMove(pointerId, dx, dy);
        return;
    }

    const bool isCameraPointer =
        std::find(m_cameraPointers.begin(), m_cameraPointers.end(), pointerId) != m_cameraPointers.end();

    if (m_screenState == EngineScreenState::InGame && m_mobileHUD && !isCameraPointer) {
        m_mobileHUD->OnTouchMove(pointerId, dx, dy);
    }

    if (isCameraPointer && m_camera && m_touchSystem) {
        if (m_cameraPointers.size() == 1) {
            const auto* pt = m_touchSystem->GetTouch(pointerId);
            if (pt) {
                m_camera->OnDrag(pt->deltaX, pt->deltaY);
            }
        } else if (m_cameraPointers.size() >= 2) {
            const auto* p0 = m_touchSystem->GetTouch(m_cameraPointers[0]);
            const auto* p1 = m_touchSystem->GetTouch(m_cameraPointers[1]);
            if (p0 && p1) {
                const float spanX = p1->designX - p0->designX;
                const float spanY = p1->designY - p0->designY;
                const float dist = std::sqrt(spanX * spanX + spanY * spanY);
                const float midX = 0.5f * (p0->designX + p1->designX);
                const float midY = 0.5f * (p0->designY + p1->designY);
                if (m_lastPinchDist > 0.0f) {
                    const float pinchDelta = (dist - m_lastPinchDist) * 0.08f;
                    m_camera->OnPinch(pinchDelta);
                    m_camera->OnPan(midX - m_lastPinchMidX, midY - m_lastPinchMidY);
                }
                m_lastPinchDist = dist;
                m_lastPinchMidX = midX;
                m_lastPinchMidY = midY;
            }
        }
    }
}

void RANMobileEngine::OnTouchUp(int pointerId, float x, float y) {
    if (m_touchSystem) m_touchSystem->OnTouchUp(pointerId, x, y);
    const float dx = m_touchSystem ? m_touchSystem->ToDesignX(x) : x;
    const float dy = m_touchSystem ? m_touchSystem->ToDesignY(y) : y;

    if (m_screenState == EngineScreenState::Login) {
        if (m_loginScreen) m_loginScreen->HandleTouchUp(pointerId, dx, dy);
        return;
    }

    auto it = std::find(m_cameraPointers.begin(), m_cameraPointers.end(), pointerId);
    if (it != m_cameraPointers.end()) {
        m_cameraPointers.erase(it);
        UpdatePinchState();
    } else if (m_screenState == EngineScreenState::InGame && m_mobileHUD) {
        m_mobileHUD->OnTouchUp(pointerId, dx, dy);
    }
}

void RANMobileEngine::OnTouchCancel(int pointerId, float x, float y) {
    if (m_touchSystem) m_touchSystem->OnTouchCancel(pointerId, x, y);
    if (m_screenState == EngineScreenState::Login) {
        if (m_loginScreen) m_loginScreen->HandleTouchCancel(pointerId);
        return;
    }
    auto it = std::find(m_cameraPointers.begin(), m_cameraPointers.end(), pointerId);
    if (it != m_cameraPointers.end()) {
        m_cameraPointers.erase(it);
        UpdatePinchState();
    } else if (m_screenState == EngineScreenState::InGame && m_mobileHUD) {
        const float dx = m_touchSystem ? m_touchSystem->ToDesignX(x) : x;
        const float dy = m_touchSystem ? m_touchSystem->ToDesignY(y) : y;
        m_mobileHUD->OnTouchUp(pointerId, dx, dy);
    }
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
    m_cameraPointers.clear();
    if (m_networkClient) m_networkClient->Disconnect();
    if (m_scene) m_scene->Unload(m_renderer.get());
    if (m_renderer) m_renderer->Shutdown();
    if (m_resourceManager) m_resourceManager->UnloadAll();
    m_initialized = false;
}
