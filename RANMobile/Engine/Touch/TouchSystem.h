#pragma once

#include "../Renderer/RenderLayout.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace RANMobile {

enum class TouchPhase {
    Down,
    Move,
    Up,
    Cancel
};

struct TouchPoint {
    int32_t pointerId = -1;
    float x = 0.0f;          // physical pixels
    float y = 0.0f;
    float designX = 0.0f;    // virtual UI canvas coordinates
    float designY = 0.0f;
    float startX = 0.0f;
    float startY = 0.0f;
    float startDesignX = 0.0f;
    float startDesignY = 0.0f;
    float deltaX = 0.0f;
    float deltaY = 0.0f;
    bool active = false;
    TouchPhase phase = TouchPhase::Down;
};

// Tracks every Android pointer independently and converts physical touch
// coordinates into the same virtual canvas used by the renderer and UI.
class TouchSystem {
public:
    void SetViewport(int width, int height) { m_layout.SetPhysicalSize(width, height); }
    const RenderLayout& GetLayout() const { return m_layout; }

    float ToDesignX(float physicalX) const { return m_layout.ToDesignX(physicalX); }
    float ToDesignY(float physicalY) const { return m_layout.ToDesignY(physicalY); }
    bool IsInsideCanvas(float physicalX, float physicalY) const {
        return m_layout.IsInsideDesignCanvas(physicalX, physicalY);
    }

    void OnTouchDown(int32_t pointerId, float x, float y);
    void OnTouchMove(int32_t pointerId, float x, float y);
    void OnTouchUp(int32_t pointerId, float x, float y);
    void OnTouchCancel(int32_t pointerId, float x, float y);

    size_t GetActiveTouchCount() const { return m_touches.size(); }
    const TouchPoint* GetTouch(int32_t pointerId) const;
    std::vector<int32_t> GetActivePointerIds() const;
    TouchPhase GetLastPhase(int32_t pointerId) const;
    void Clear();

private:
    RenderLayout m_layout;
    std::unordered_map<int32_t, TouchPoint> m_touches;
    std::unordered_map<int32_t, TouchPhase> m_lastPhases;
};

} // namespace RANMobile
