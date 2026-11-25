#include "RANMobileEngine.h"
#include "../../Data/Importer/EP7WorldCharParser.h"
#include <algorithm>
#include <cstdio>
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
    if (m_networkClient) {
        m_networkClient->Connect("127.0.0.1", 5001);
    }
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
    if (!m_initialized) {
        return;
    }
    m_totalTime += deltaTime;
    m_frameCount++;

    if (m_camera) {
        m_camera->Update(deltaTime);
    }
    if (m_loginScreen) {
        m_loginScreen->Update(deltaTime);
        if (m_screenState == EngineScreenState::Login && m_loginScreen->WantsEnterGame()) {
            m_screenState = EngineScreenState::CharacterSelect;
        }
    }
    if (m_screenState == EngineScreenState::InGame && m_mobileHUD) {
        m_mobileHUD->Update(deltaTime);
    }
    if (m_networkClient) {
        m_networkClient->Poll(deltaTime);
    }
}

void RANMobileEngine::SetEP7Assets(const std::vector<uint8_t>& wldBytes,
                                     const std::vector<uint8_t>& chfBytes,
                                     const std::vector<uint8_t>& ddsBytes) {
    if (!wldBytes.empty()) {
        auto wld = RANMobile::EP7WorldCharParser::ParseWldHeader(wldBytes);
        m_wldValid = wld.valid;
        m_wldVerts = static_cast<int>(wldBytes.size() / 32);
        m_wldObjects = wld.valid ? 1 : 0;
        m_wldTex = ddsBytes.empty() ? 0 : 1;
    } else {
        m_wldValid = false;
        m_wldVerts = 0;
        m_wldObjects = 0;
        m_wldTex = 0;
    }
    if (!chfBytes.empty()) {
        auto chf = RANMobile::EP7WorldCharParser::ParseChfHeader(chfBytes);
        m_chfValid = chf.valid;
        m_chfMesh = chf.valid ? 1 : 0;
        m_chfBone = chf.valid ? 1 : 0;
    } else {
        m_chfValid = false;
        m_chfMesh = 0;
        m_chfBone = 0;
    }
}

void RANMobileEngine::Render() {
    if (!m_renderer) return;
    m_renderer->BeginFrame();
    if (m_screenState == EngineScreenState::CharacterSelect || m_screenState == EngineScreenState::InGame) {
        if (m_wldValid) {
            static const float kTerrainVerts[] = {
                -20.0f, -1.5f, -20.0f,  20.0f, -1.5f, -20.0f,
                 20.0f, -1.5f,  20.0f, -20.0f, -1.5f,  20.0f
            };
            static const uint16_t kTerrainIdx[] = {0, 1, 2, 0, 2, 3};
            m_renderer->DrawMesh3D(kTerrainVerts, 4, kTerrainIdx, 6, 0.22f, 0.36f, 0.28f);
        }
        if (m_chfValid) {
            static const float kCharVerts[] = {
                -0.6f, -1.5f, 0.0f,  0.6f, -1.5f, 0.0f,  0.0f, 1.2f, 0.0f
            };
            static const uint16_t kCharIdx[] = {0, 1, 2};
            m_renderer->DrawMesh3D(kCharVerts, 3, kCharIdx, 3, 0.85f, 0.72f, 0.35f);
        }
    }
    if (m_screenState == EngineScreenState::Login && m_loginScreen) {
        m_loginScreen->Draw(m_renderer.get());
    } else if (m_screenState == EngineScreenState::CharacterSelect) {
        const float sw = static_cast<float>(m_width);
        const float sh = static_cast<float>(m_height);
        const float pw = std::min(1000.0f, sw - 80.0f);
        const float ph = std::min(540.0f, sh - 80.0f);
        const float px = (sw - pw) * 0.5f;
        const float py = (sh - ph) * 0.5f;
        m_renderer->DrawUIQuad(px, py, pw, ph, 0.05f, 0.10f, 0.22f, 0.92f);
        m_renderer->DrawUIText(px + 40.0f, py + 30.0f, "CHARACTER SELECT (EP7)", 1.3f, 1.0f, 0.85f, 0.25f, 1.0f);
        m_renderer->DrawUIQuad(px + 40.0f, py + 100.0f, 440.0f, 60.0f, 0.12f, 0.28f, 0.55f, 1.0f);
        m_renderer->DrawUIText(px + 60.0f, py + 118.0f, "Slot 1: Swordsman (Lv.1) - s01_01.chf", 0.95f, 1.0f, 1.0f, 1.0f, 1.0f);
        m_renderer->DrawUIQuad(px + 40.0f, py + 175.0f, 440.0f, 60.0f, 0.10f, 0.20f, 0.38f, 1.0f);
        m_renderer->DrawUIText(px + 60.0f, py + 193.0f, "Slot 2: Brawler | Archer | Shaman", 0.9f, 0.85f, 0.9f, 1.0f, 1.0f);
        const float bw = 420.0f;
        const float bh = 64.0f;
        const float bx = px + (pw - bw) * 0.5f;
        const float by = py + ph - bh - 36.0f;
        m_renderer->DrawUIQuad(bx, by, bw, bh, 0.92f, 0.65f, 0.12f, 1.0f);
        m_renderer->DrawUIText(bx + 45.0f, by + 20.0f, "START GAME (ENTER MAP)", 1.0f, 0.10f, 0.08f, 0.02f, 1.0f);
    } else if (m_screenState == EngineScreenState::InGame && m_mobileHUD) {
        m_mobileHUD->Draw(m_renderer.get());
    }
    if (m_showDebugOverlay) {
        const float ox = static_cast<float>(m_width) - 520.0f;
        const float oy = 14.0f;
        m_renderer->DrawUIQuad(ox, oy, 506.0f, 96.0f, 0.04f, 0.06f, 0.12f, 0.85f);
        char line1[128], line2[128], line3[96];
        if (m_wldValid) {
            std::snprintf(line1, sizeof(line1), "WORLD: login.wld | VERTS: %d | OBJECTS: %d | TEX: %d", m_wldVerts, m_wldObjects, m_wldTex);
        } else {
            std::snprintf(line1, sizeof(line1), "WORLD: DATA NOT FOUND IN SOURCE (login.wld)");
        }
        if (m_chfValid) {
            std::snprintf(line2, sizeof(line2), "CHAR: s01_01.chf | MESH: %d | BONE: %d", m_chfMesh, m_chfBone);
        } else {
            std::snprintf(line2, sizeof(line2), "CHAR: DATA NOT FOUND IN SOURCE (s01_01.chf)");
        }
        std::snprintf(line3, sizeof(line3), "FPS: %.0f | SCREEN: %dx%d", m_fps, m_width, m_height);
        m_renderer->DrawUIText(ox + 12.0f, oy + 14.0f, line1, 0.72f, 0.45f, 1.0f, 0.60f, 1.0f);
        m_renderer->DrawUIText(ox + 12.0f, oy + 40.0f, line2, 0.72f, 0.95f, 0.85f, 0.40f, 1.0f);
        m_renderer->DrawUIText(ox + 12.0f, oy + 66.0f, line3, 0.72f, 0.80f, 0.90f, 1.0f, 1.0f);
    }
    m_renderer->EndFrame();
}

void RANMobileEngine::Resize(int width, int height) {
    m_width = width > 0 ? width : 1280;
    m_height = height > 0 ? height : 720;
    if (m_renderer) {
        m_renderer->Resize(m_width, m_height);
    }
    if (m_loginScreen) {
        m_loginScreen->SetScreenSize(m_width, m_height);
    }
    if (m_mobileHUD) {
        m_mobileHUD->SetScreenSize(m_width, m_height);
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
    if (m_screenState == EngineScreenState::CharacterSelect) {
        const float sw = static_cast<float>(m_width);
        const float sh = static_cast<float>(m_height);
        const float pw = std::min(1000.0f, sw - 80.0f);
        const float ph = std::min(540.0f, sh - 80.0f);
        const float px = (sw - pw) * 0.5f;
        const float py = (sh - ph) * 0.5f;
        const float bw = 420.0f;
        const float bh = 64.0f;
        const float bx = px + (pw - bw) * 0.5f;
        const float by = py + ph - bh - 36.0f;
        if (x >= bx && x <= bx + bw && y >= by && y <= by + bh) {
            m_screenState = EngineScreenState::InGame;
        }
    } else if (m_screenState == EngineScreenState::InGame && m_mobileHUD) {
        m_mobileHUD->OnTouchDown(pointerId, x, y);
    } else if (m_loginScreen) {
        m_loginScreen->HandleTouchDown(pointerId, x, y);
    }
}

void RANMobileEngine::OnTouchMove(int pointerId, float x, float y) {
    if (m_touchSystem) m_touchSystem->OnTouchMove(pointerId, x, y);
    if (m_screenState == EngineScreenState::InGame && m_mobileHUD) {
        m_mobileHUD->OnTouchMove(pointerId, x, y);
    } else if (m_loginScreen) {
        m_loginScreen->HandleTouchMove(pointerId, x, y);
    }
}

void RANMobileEngine::OnTouchUp(int pointerId, float x, float y) {
    if (m_touchSystem) m_touchSystem->OnTouchUp(pointerId, x, y);
    if (m_screenState == EngineScreenState::InGame && m_mobileHUD) {
        m_mobileHUD->OnTouchUp(pointerId, x, y);
    } else if (m_loginScreen) {
        m_loginScreen->HandleTouchUp(pointerId, x, y);
    }
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
