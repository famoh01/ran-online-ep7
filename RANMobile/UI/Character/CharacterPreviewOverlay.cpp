#include "CharacterPreviewOverlay.h"

#include <cmath>

#include "../../Engine/Camera/MobileCamera.h"
#include "../../Engine/Renderer/MobileRenderer.h"
#include "../../Engine/Scene/EP7CharacterScene.h"

namespace RANMobile {
namespace {

constexpr float kChipHeight = 56.0f;
constexpr float kChipSpacing = 10.0f;
constexpr float kChipMaxWidth = 190.0f;
constexpr float kRoundButton = 84.0f;
constexpr float kDoubleTapSeconds = 0.35f;
constexpr float kRotateSpeedDegPerSec = 80.0f;

float TextScaleForScreen(int height) {
    // Scale text with the viewport so labels stay readable from a phone's typical
    // 720p design space to a 1440p tablet without re-flowing the layout.
    const float scale = static_cast<float>(height) / 720.0f;
    return scale < 0.85f ? 0.85f : (scale > 2.0f ? 2.0f : scale);
}

}  // namespace

void CharacterPreviewOverlay::Initialize(int screenWidth, int screenHeight) {
    Resize(screenWidth, screenHeight);
}

void CharacterPreviewOverlay::Resize(int screenWidth, int screenHeight) {
    m_width = screenWidth > 0 ? screenWidth : 1280;
    m_height = screenHeight > 0 ? screenHeight : 720;

    const float w = static_cast<float>(m_width);
    const float h = static_cast<float>(m_height);
    const float margin = std::round(h * 0.03f);

    // Character strip: the central 40% of the screen above the joystick band.
    // Nothing interactive is placed inside it, so drags there always orbit the camera.
    m_characterStrip.x = w * 0.30f;
    m_characterStrip.y = h * 0.12f;
    m_characterStrip.w = w * 0.40f;
    m_characterStrip.h = h * 0.46f;

    // Rotate buttons: bottom-left corner, outboard of the drag strip and above the
    // joystick zone (joystick centre is 0.14w / 0.72h with radius 0.14h).
    m_rotateLeftRect.x = margin;
    m_rotateLeftRect.y = h - kRoundButton - margin - h * 0.28f;
    m_rotateLeftRect.w = kRoundButton;
    m_rotateLeftRect.h = kRoundButton;

    m_rotateRightRect.x = margin + kRoundButton + kChipSpacing;
    m_rotateRightRect.y = m_rotateLeftRect.y;
    m_rotateRightRect.w = kRoundButton;
    m_rotateRightRect.h = kRoundButton;

    // Reset camera + diagnostics toggle: top-right corner, below the FPS/status band.
    m_resetRect.w = 150.0f * TextScaleForScreen(m_height) * 0.8f;
    m_resetRect.h = 52.0f;
    m_resetRect.x = w - m_resetRect.w - margin;
    m_resetRect.y = margin + 56.0f;

    m_diagRect.w = m_resetRect.w;
    m_diagRect.h = 52.0f;
    m_diagRect.x = m_resetRect.x;
    m_diagRect.y = m_resetRect.y + m_resetRect.h + kChipSpacing;

    LayoutChips(m_chipNames);
}

void CharacterPreviewOverlay::LayoutChips(const std::vector<std::string>& names) {
    m_chipNames = names;
    m_chipRects.clear();

    const float scale = TextScaleForScreen(m_height);
    const float h = static_cast<float>(m_height);
    float x = static_cast<float>(m_width) * 0.03f;
    float y = h * 0.16f;  // top-left column, clear of the joystick and the strip

    for (size_t i = 0; i < names.size(); ++i) {
        CharacterOverlayRect rect;
        rect.x = x;
        rect.y = y;
        rect.w = kChipMaxWidth * scale;
        rect.h = kChipHeight * scale;
        m_chipRects.push_back(rect);
        y += rect.h + kChipSpacing;
        // Wrap to a second column if the list is long; never intrude into the strip.
        if (y + rect.h > h * 0.52f) {
            y = h * 0.16f;
            x += rect.w + kChipSpacing;
        }
    }
}

void CharacterPreviewOverlay::ResetCamera(MobileCamera& camera) const {
    camera.SetMode(CameraMode::Free);
    // Fixed three-quarter view so the character's front and weapon side are visible.
    camera.OnDrag(-35.0f, 0.0f);
    camera.SetDistanceBounds(4.0f, 30.0f);
    camera.OnPinch(1.0f);
}

int32_t CharacterPreviewOverlay::HitTestClipChip(float x, float y) const {
    for (size_t i = 0; i < m_chipRects.size(); ++i) {
        if (m_chipRects[i].Contains(x, y)) return static_cast<int32_t>(i);
    }
    return -1;
}

void CharacterPreviewOverlay::SetStatus(const std::string& status) { m_status = status; }
void CharacterPreviewOverlay::SetError(const std::string& error) { m_error = error; }

bool CharacterPreviewOverlay::OnTouchDown(int32_t pointerId, float x, float y) {
    if (!m_visible) return false;

    // Pinch bookkeeping: remember up to two pointers anywhere on the strip.
    if (m_pinchPointerA < 0) {
        m_pinchPointerA = pointerId;
        m_pinchStartDistance = -1.0f;
    } else if (m_pinchPointerB < 0 && pointerId != m_pinchPointerA) {
        m_pinchPointerB = pointerId;
        m_pinchStartDistance = -1.0f;
    }

    if (m_rotateLeftRect.Contains(x, y)) {
        m_rotateLeftHeld = true;
        return true;
    }
    if (m_rotateRightRect.Contains(x, y)) {
        m_rotateRightHeld = true;
        return true;
    }
    if (m_resetRect.Contains(x, y)) {
        m_pendingCameraReset = true;
        return true;
    }
    if (m_diagRect.Contains(x, y)) {
        ToggleDiagnostics();
        return true;
    }
    if (HitTestClipChip(x, y) >= 0) {
        // The actual clip switch is applied in Update() where the scene is available.
        m_pendingChip = HitTestClipChip(x, y);
        return true;
    }

    // Double tap anywhere in the strip resets the viewpoint.
    if (m_characterStrip.Contains(x, y)) {
        if (m_lastTapTime >= 0.0f && (m_clock - m_lastTapTime) <= kDoubleTapSeconds) {
            m_pendingCameraReset = true;
            m_lastTapTime = -1.0f;
        } else {
            m_lastTapTime = m_clock;
        }
    }

    if (m_orbitPointer < 0) {
        m_orbitPointer = pointerId;
        m_lastX = x;
        m_lastY = y;
        m_dragging = true;
        return m_characterStrip.Contains(x, y);
    }
    return false;
}

bool CharacterPreviewOverlay::OnTouchMove(int32_t pointerId, float x, float y) {
    if (!m_visible) return false;

    // Track absolute positions for pinch zoom.
    bool touched = false;
    if (pointerId == m_pinchPointerA) {
        m_lastPointerAX = x;
        m_lastPointerAY = y;
        touched = true;
    }
    if (pointerId == m_pinchPointerB) {
        m_lastPointerBX = x;
        m_lastPointerBY = y;
        touched = true;
    }
    if (touched && m_pinchPointerA >= 0 && m_pinchPointerB >= 0) {
        const float dx = m_lastPointerAX - m_lastPointerBX;
        const float dy = m_lastPointerAY - m_lastPointerBY;
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (m_pinchPrevDistance > 0.0f && distance > 0.0f) {
            // MobileCamera::OnPinch takes a multiplicative factor.
            m_pendingPinch *= distance / m_pinchPrevDistance;
        }
        m_pinchPrevDistance = distance;
    }

    if (pointerId == m_orbitPointer && m_dragging) {
        const float dx = x - m_lastX;
        const float dy = y - m_lastY;
        m_lastX = x;
        m_lastY = y;
        m_accumulatedDragX += dx;
        m_accumulatedDragY += dy;
        return true;
    }
    if (touched) {
        return true;
    }
    return false;
}

bool CharacterPreviewOverlay::OnTouchUp(int32_t pointerId, float x, float y) {
    (void)x;
    (void)y;
    if (!m_visible) return false;

    if (pointerId == m_pinchPointerA) m_pinchPointerA = -1;
    if (pointerId == m_pinchPointerB) m_pinchPointerB = -1;
    m_pinchPrevDistance = -1.0f;
    if (pointerId == m_orbitPointer) {
        m_orbitPointer = -1;
        m_dragging = false;
    }
    m_rotateLeftHeld = false;
    m_rotateRightHeld = false;
    return false;
}

void CharacterPreviewOverlay::Update(float deltaTime, EP7CharacterScene& scene, MobileCamera& camera) {
    m_clock += deltaTime;

    // Apply deferred UI intents now that the scene/camera are available.
    if (m_pendingCameraReset) {
        ResetCamera(camera);
        m_pendingCameraReset = false;
    }
    if (m_pendingChip >= 0) {
        const uint32_t target = scene.PreviewTarget();
        if (target != 0 && static_cast<size_t>(m_pendingChip) < m_chipNames.size()) {
            if (scene.PlayAnimation(target, m_chipNames[static_cast<size_t>(m_pendingChip)])) {
                m_activeChip = static_cast<uint32_t>(m_pendingChip);
                ClearError();
                SetStatus("ANIM: " + m_chipNames[static_cast<size_t>(m_pendingChip)]);
            } else {
                SetError("animation clip could not be played: " + m_chipNames[static_cast<size_t>(m_pendingChip)]);
            }
        }
        m_pendingChip = -1;
    }

    // Orbit from drag.
    if (m_dragging && (std::fabs(m_accumulatedDragX) > 0.01f || std::fabs(m_accumulatedDragY) > 0.01f)) {
        camera.OnDrag(m_accumulatedDragX, m_accumulatedDragY);
        m_accumulatedDragX = 0.0f;
        m_accumulatedDragY = 0.0f;
    }

    // Pinch zoom: the ratio accumulated in OnTouchMove is applied once per frame.
    if (m_pendingPinch > 0.0f && std::fabs(m_pendingPinch - 1.0f) > 1e-4f) {
        camera.OnPinch(m_pendingPinch);
        m_pendingPinch = 1.0f;
    }

    // Continuous rotation buttons.
    if (m_rotateLeftHeld) camera.OnDrag(-kRotateSpeedDegPerSec * deltaTime, 0.0f);
    if (m_rotateRightHeld) camera.OnDrag(kRotateSpeedDegPerSec * deltaTime, 0.0f);

    // Refresh the chip list from the previewed character's authentic clips.
    const uint32_t target = scene.PreviewTarget();
    if (target != 0) {
        if (EP7CharacterInstance* instance = scene.Find(target)) {
            if (instance->asset && m_chipNames.size() != instance->asset->file.animations.size()) {
                LayoutChips(instance->asset->file.animations);
            }
            if (!instance->poseValid) {
                SetError(instance->asset ? instance->asset->error : std::string("character pose unavailable"));
            }
        }
    }
}

void CharacterPreviewOverlay::Draw(MobileRenderer* renderer, const EP7CharacterScene& scene,
                                   const MobileCamera& camera) {
    if (!renderer || !m_visible) return;

    const float scale = TextScaleForScreen(m_height);
    const float w = static_cast<float>(m_width);

    // --- status band (top-left, never covers the character strip) -----------
    renderer->DrawUIQuad(16.0f, 14.0f, w * 0.30f, 44.0f * scale, 0.03f, 0.06f, 0.12f, 0.72f);
    renderer->DrawUIText(26.0f, 20.0f, m_status.empty() ? "CHARACTER PREVIEW" : m_status, 0.72f * scale,
                         0.85f, 0.92f, 1.0f, 1.0f);

    // --- error banner (below the status band) ------------------------------
    if (!m_error.empty()) {
        renderer->DrawUIQuad(16.0f, 62.0f * scale, w * 0.34f, 52.0f * scale, 0.32f, 0.05f, 0.05f, 0.82f);
        renderer->DrawUIText(26.0f, 68.0f * scale, m_error, 0.62f * scale, 1.0f, 0.86f, 0.86f, 1.0f);
    }

    // --- diagnostics panel ------------------------------------------------
    if (m_showDiagnostics) {
        const EP7CharacterDiagnostics diag = scene.GetDiagnostics();
        const float panelX = w - 330.0f * scale;
        const float panelY = m_diagRect.y + m_diagRect.h + kChipSpacing;
        renderer->DrawUIQuad(panelX, panelY, 320.0f * scale, 176.0f * scale, 0.02f, 0.05f, 0.10f, 0.72f);

        char line[160];
        float y = panelY + 10.0f * scale;
        const float dy = 26.0f * scale;
        std::snprintf(line, sizeof(line), "FPS %.1f  SKIN %.2fms", diag.fps, diag.cpuSkinMs);
        renderer->DrawUIText(panelX + 12.0f, y, line, 0.56f * scale, 0.62f, 0.95f, 0.72f, 1.0f);
        y += dy;
        std::snprintf(line, sizeof(line), "BONES %u  ANIM-BONES %u", diag.totalBones, diag.animatedBones);
        renderer->DrawUIText(panelX + 12.0f, y, line, 0.56f * scale, 0.80f, 0.90f, 1.0f, 1.0f);
        y += dy;
        std::snprintf(line, sizeof(line), "VERTS %u  TRIS %u", diag.totalVertices, diag.totalTriangles);
        renderer->DrawUIText(panelX + 12.0f, y, line, 0.56f * scale, 0.80f, 0.90f, 1.0f, 1.0f);
        y += dy;
        std::snprintf(line, sizeof(line), "CHAR %u/%u  DRAWS %u", diag.visibleCharacters, diag.characters,
                      diag.drawCalls);
        renderer->DrawUIText(panelX + 12.0f, y, line, 0.56f * scale, 0.80f, 0.90f, 1.0f, 1.0f);
        y += dy;
        std::snprintf(line, sizeof(line), "CLIPS %u  GEOM %u/%u", diag.animationCount, diag.decodedGeometryPieces,
                      diag.characters);
        renderer->DrawUIText(panelX + 12.0f, y, line, 0.56f * scale, 0.80f, 0.90f, 1.0f, 1.0f);
        y += dy;
        renderer->DrawUIText(panelX + 12.0f, y, diag.authenticAssetsOnly ? "AUTHENTIC ASSETS: YES" : "AUTHENTIC ASSETS: INCOMPLETE",
                             0.56f * scale, diag.authenticAssetsOnly ? 0.55f : 1.0f,
                             diag.authenticAssetsOnly ? 1.0f : 0.55f, 0.6f, 1.0f);
    }

    // --- rotate + reset + diagnostics buttons -----------------------------
    renderer->DrawUIQuad(m_rotateLeftRect.x, m_rotateLeftRect.y, m_rotateLeftRect.w, m_rotateLeftRect.h,
                         0.10f, 0.18f, 0.30f, m_rotateLeftHeld ? 0.85f : 0.55f);
    renderer->DrawUIText(m_rotateLeftRect.x + 26.0f, m_rotateLeftRect.y + 24.0f, "<", 1.0f * scale, 1.0f, 1.0f,
                         1.0f, 0.95f);

    renderer->DrawUIQuad(m_rotateRightRect.x, m_rotateRightRect.y, m_rotateRightRect.w, m_rotateRightRect.h,
                         0.10f, 0.18f, 0.30f, m_rotateRightHeld ? 0.85f : 0.55f);
    renderer->DrawUIText(m_rotateRightRect.x + 26.0f, m_rotateRightRect.y + 24.0f, ">", 1.0f * scale, 1.0f, 1.0f,
                         1.0f, 0.95f);

    renderer->DrawUIQuad(m_resetRect.x, m_resetRect.y, m_resetRect.w, m_resetRect.h, 0.10f, 0.18f, 0.30f, 0.60f);
    renderer->DrawUIText(m_resetRect.x + 16.0f, m_resetRect.y + 14.0f, "RESET VIEW", 0.62f * scale, 1.0f, 1.0f,
                         1.0f, 0.92f);

    renderer->DrawUIQuad(m_diagRect.x, m_diagRect.y, m_diagRect.w, m_diagRect.h, 0.10f, 0.18f, 0.30f, 0.60f);
    renderer->DrawUIText(m_diagRect.x + 16.0f, m_diagRect.y + 14.0f, m_showDiagnostics ? "DIAG: ON" : "DIAG: OFF",
                         0.62f * scale, 1.0f, 1.0f, 1.0f, 0.92f);

    // --- animation clip chips (authentic names only) ----------------------
    for (size_t i = 0; i < m_chipRects.size(); ++i) {
        const bool active = (i == m_activeChip);
        const CharacterOverlayRect& r = m_chipRects[i];
        renderer->DrawUIQuad(r.x, r.y, r.w, r.h, active ? 0.18f : 0.08f, active ? 0.34f : 0.14f,
                             active ? 0.52f : 0.24f, 0.88f);
        renderer->DrawUIText(r.x + 12.0f, r.y + 14.0f * scale, m_chipNames[i], 0.60f * scale, 1.0f, 1.0f, 1.0f,
                             0.95f);
    }

    // --- camera readout ---------------------------------------------------
    char cam[128];
    std::snprintf(cam, sizeof(cam), "YAW %.0f  PITCH %.0f  DIST %.1f", camera.GetYaw(), camera.GetPitch(),
                  camera.GetDistance());
    renderer->DrawUIQuad(16.0f, static_cast<float>(m_height) - 52.0f * scale, w * 0.28f, 40.0f * scale, 0.03f,
                         0.06f, 0.12f, 0.65f);
    renderer->DrawUIText(26.0f, static_cast<float>(m_height) - 44.0f * scale, cam, 0.55f * scale, 0.75f, 0.85f,
                         1.0f, 0.95f);
}

}  // namespace RANMobile
