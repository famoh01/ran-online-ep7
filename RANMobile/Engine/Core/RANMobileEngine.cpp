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
#include "../Scene/EP7CharacterScene.h"
#include "../../Data/Importer/EP7SkinPiece.h"

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
    m_loginScreen = std::make_unique<LoginScreen>();
    m_mobileHUD = std::make_unique<RANMobile::MobileHUD>();
    m_characterScene = std::make_unique<RANMobile::EP7CharacterScene>();

    if (!m_renderer->Initialize(1280, 720)) {
        RANMobile::Logger::Error("RANMobileEngine", "Failed to initialize MobileRenderer");
        return false;
    }

    // B18: the character scene draws through this renderer (GPU skinned meshes +
    // decoded textures are scene-scoped caches released at Clear/Shutdown).
    m_characterScene->SetRenderer(m_renderer.get());

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
    // B17 mobile-runtime clock hardening (documented): the original PC FrameMove
    // trusted fElapsedTime from the D3D loop and never clamped it. On Android the
    // wall clock can jump across app pause/resume or a blocked frame, which would
    // teleport animation poses (fTime leaps by whole clip lengths). Negative and
    // non-finite deltas are dropped; anything above 250 ms advances at most 250 ms.
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) {
        deltaTime = 0.0f;
    } else if (deltaTime > 0.25f) {
        deltaTime = 0.25f;
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
    if (m_screenState == EngineScreenState::CharacterPreview && m_characterScene && m_camera) {
        m_characterScene->Update(deltaTime);
    }
}

void RANMobileEngine::Render() {
    if (!m_initialized || !m_running || !m_renderer) {
        return;
    }
    m_renderer->BeginFrame();
    if (m_screenState == EngineScreenState::CharacterPreview && m_characterScene && m_camera) {
        // B17: authentic character pass — evaluated poses are submitted before the UI
        // so HUD/overlays always draw on top (BeginScene3D enables depth + culling,
        // EndScene3D restores the UI state).
        if (m_renderer->BeginScene3D(*m_camera)) {
            for (const auto& instance : m_characterScene->Instances()) {
                if (!instance->visible || !instance->poseValid || !instance->inFrustum) continue;
                const size_t vertexCount = instance->skinnedPositions.size() / 3;

                // ------------------------------------------------------------------
                // B18 GPU skinned path (authentic D3DINDEXEDHLSLVS parity): static
                // per-piece vertex/index buffers + the per-frame bone palette
                // (offset*combined, DxSkinMesh9_HLSL.cpp:220-229 parity) + per-subset
                // texture binding (SMATERIAL_PIECE::LoadTexture parity).  Pieces the
                // GPU path cannot represent (no decodable geometry, palette overflow,
                // invalid face ranges) fall back to the B17 CPU-evaluated path —
                // geometry is never substituted or fabricated.
                // ------------------------------------------------------------------
                bool submitted = false;
                if (instance->asset) {
                    for (uint32_t slot = 0; slot < RANMobile::kEP7PieceSize; ++slot) {
                        const auto& piece = instance->asset->pieces[slot];
                        if (!piece.loaded || piece.mesh.faces.empty()) continue;

                        const uint32_t gpuHandle = m_characterScene->EnsurePieceGpuHandle(instance->asset, slot);
                        const float* palette = nullptr;
                        uint32_t paletteCount = 0;
                        const bool havePalette = m_characterScene->GetPiecePalette(*instance, slot, &palette, &paletteCount);
                        if (gpuHandle != 0 && havePalette) {
                            const auto& subsets = piece.mesh.subsets;
                            if (!subsets.empty()) {
                                for (uint32_t s = 0; s < subsets.size(); ++s) {
                                    if (subsets[s].faceCount == 0) continue;
                                    const uint32_t texId = m_characterScene->GetPieceTextureId(instance->asset, slot, s);
                                    // cps FLAG_ALPHATEXTURE = 0x0001 (SMATERIAL_PIECE) drives
                                    // per-subset alpha blending; untextured subsets keep the
                                    // flat material colour.
                                    bool alphaBlend = false;
                                    if (s < piece.info.materials.size()) {
                                        alphaBlend = (piece.info.materials[s].flags & 0x0001u) != 0;
                                    }
                                    const float texColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                                    const float flatColor[4] = {0.75f, 0.75f, 0.75f, 1.0f};
                                    submitted = m_renderer->DrawGpuSkinnedMesh(
                                        gpuHandle, palette, paletteCount, texId,
                                        texId != 0 ? texColor : flatColor, alphaBlend,
                                        subsets[s].faceStart, subsets[s].faceCount) || submitted;
                                }
                            } else {
                                const float flatColor[4] = {0.75f, 0.75f, 0.75f, 1.0f};
                                submitted = m_renderer->DrawGpuSkinnedMesh(
                                    gpuHandle, palette, paletteCount, 0, flatColor, false,
                                    0, static_cast<uint32_t>(piece.mesh.faces.size() / 3)) || submitted;
                            }
                            continue;
                        }

                        // B17 CPU-evaluated fallback: skinned positions/normals are
                        // concatenated in piece order, so every submission advances the
                        // base pointer by the piece's vertex offset — piece.mesh.faces
                        // indexes piece-local vertices. A piece that fails the bounds
                        // check is skipped; geometry is never substituted or fabricated.
                        if (vertexCount == 0) continue;
                        const uint32_t offset = instance->pieceVertexOffsets[slot];
                        if (static_cast<size_t>(offset) + piece.mesh.vertexCount > vertexCount) continue;
                        RANMobile::SkinnedDrawInput pieceInput;
                        pieceInput.positions = instance->skinnedPositions.data() + static_cast<size_t>(offset) * 3;
                        pieceInput.normals = instance->skinnedNormals.data() + static_cast<size_t>(offset) * 3;
                        pieceInput.vertexCount = piece.mesh.vertexCount;
                        pieceInput.indices = piece.mesh.faces.data();
                        pieceInput.indexCount = static_cast<uint32_t>(piece.mesh.faces.size());
                        pieceInput.textureId = 0;
                        submitted = m_renderer->DrawSkinnedMesh(pieceInput) || submitted;
                    }
                }
                (void)submitted;
            }
        }
        m_renderer->EndScene3D();
    } else if (m_screenState == EngineScreenState::Login && m_loginScreen) {
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
    // B17: pause stops the update loop only; all animation state lives in RAM
    // (controls, clocks, skeletons), so resume continues the pose exactly where it
    // stopped. The post-resume wall-clock jump is absorbed by the delta clamp in
    // Update() — no animation state is rebuilt or reset on resume.
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
