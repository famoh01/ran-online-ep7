//
// CharacterPreviewOverlay.h — Touch-friendly character inspect UI (Phase 3J)
//
// Scope: character *rendering foundation* UX only. It is intentionally not a game menu
// and adds no gameplay actions. It gives the player/QA the controls needed to inspect the
// authentic animated character on a phone:
//
//   * drag anywhere on the character strip  -> orbit (drives MobileCamera)
//   * pinch                                 -> zoom  (drives MobileCamera)
//   * double-tap                            -> reset camera
//   * clip chips (left, above the thumb)    -> play an authentic animation by name
//   * left/right rotate buttons             -> continuous orbit
//   * diagnostics panel (toggle)            -> FPS, bones, animation, skin ms, vertex count
//
// All chrome is positioned in the renderer's design space and kept clear of:
//   - the left thumb joystick    (x < 0.30w,  y > 0.55h)
//   - the right action cluster   (x > 0.70w,  y > 0.60h)
// so the character is never obscured.
//
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../Buttons/MobileButton.h"

namespace RANMobile {

class MobileRenderer;
class MobileCamera;
class EP7CharacterScene;

struct CharacterOverlayRect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    bool Contains(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

class CharacterPreviewOverlay {
public:
    void Initialize(int screenWidth, int screenHeight);
    void Resize(int screenWidth, int screenHeight);

    // Returns true when the UI consumed the touch (caller must not forward it to gameplay).
    bool OnTouchDown(int32_t pointerId, float x, float y);
    bool OnTouchMove(int32_t pointerId, float x, float y);
    bool OnTouchUp(int32_t pointerId, float x, float y);

    void Update(float deltaTime, EP7CharacterScene& scene, MobileCamera& camera);
    void Draw(MobileRenderer* renderer, const EP7CharacterScene& scene, const MobileCamera& camera);

    // Loading / error feedback.
    void SetStatus(const std::string& status);
    void SetError(const std::string& error);
    void ClearError() { m_error.clear(); }
    const std::string& Status() const { return m_status; }
    const std::string& Error() const { return m_error; }

    void SetVisible(bool visible) { m_visible = visible; }
    bool IsVisible() const { return m_visible; }
    void ToggleDiagnostics() { m_showDiagnostics = !m_showDiagnostics; }
    bool DiagnosticsVisible() const { return m_showDiagnostics; }

    // Camera preset used by "reset camera" and first show.
    void ResetCamera(MobileCamera& camera) const;

    // Exposed for tests: the clip chip under a point, or -1.
    int32_t HitTestClipChip(float x, float y) const;

private:
    void LayoutChips(const std::vector<std::string>& names);

    bool m_visible = true;
    bool m_showDiagnostics = true;
    int m_width = 1280;
    int m_height = 720;

    std::string m_status;
    std::string m_error;

    // Drag/orbit state
    int32_t m_orbitPointer = -1;
    float m_lastX = 0.0f;
    float m_lastY = 0.0f;
    float m_accumulatedDragX = 0.0f;
    float m_accumulatedDragY = 0.0f;
    bool m_dragging = false;

    // Pinch zoom (two simultaneous pointers on the character strip)
    int32_t m_pinchPointerA = -1;
    int32_t m_pinchPointerB = -1;
    float m_pinchStartDistance = 0.0f;
    float m_pinchPrevDistance = -1.0f;
    float m_pendingPinch = 1.0f;
    float m_lastPointerAX = 0.0f, m_lastPointerAY = 0.0f;
    float m_lastPointerBX = 0.0f, m_lastPointerBY = 0.0f;

    // Continuous rotate buttons
    bool m_rotateLeftHeld = false;
    bool m_rotateRightHeld = false;

    // Double tap
    float m_lastTapTime = -1.0f;
    float m_clock = 0.0f;

    // Deferred intents applied in Update() where scene/camera are available
    bool m_pendingCameraReset = false;
    int32_t m_pendingChip = -1;

    // Layout
    CharacterOverlayRect m_rotateLeftRect;
    CharacterOverlayRect m_rotateRightRect;
    CharacterOverlayRect m_resetRect;
    CharacterOverlayRect m_diagRect;
    CharacterOverlayRect m_characterStrip;
    std::vector<CharacterOverlayRect> m_chipRects;
    std::vector<std::string> m_chipNames;
    uint32_t m_activeChip = 0;
};

}  // namespace RANMobile
