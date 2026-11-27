#pragma once

#include <algorithm>
#include <cstdint>

namespace RANMobile {

// Design-space layout shared by the renderer and the touch layer.
//
// The UI is authored against a fixed virtual canvas. It is uniformly scaled
// into the physical Android surface with centered letterboxing, so controls
// never stretch when the aspect ratio changes. Portrait devices use a
// portrait virtual canvas; the login screen reflows against that canvas.
struct RenderLayout {
    static constexpr float kLandscapeWidth = 1280.0f;
    static constexpr float kLandscapeHeight = 720.0f;
    static constexpr float kPortraitWidth = 720.0f;
    static constexpr float kPortraitHeight = 1280.0f;

    void SetPhysicalSize(int width, int height) {
        m_physicalWidth = width > 0 ? width : static_cast<int>(kLandscapeWidth);
        m_physicalHeight = height > 0 ? height : static_cast<int>(kLandscapeHeight);
        Recalculate();
    }

    void SetSafeAreaInsets(float left, float top, float right, float bottom) {
        m_safeLeft = std::max(0.0f, left);
        m_safeTop = std::max(0.0f, top);
        m_safeRight = std::max(0.0f, right);
        m_safeBottom = std::max(0.0f, bottom);
    }

    int GetPhysicalWidth() const { return m_physicalWidth; }
    int GetPhysicalHeight() const { return m_physicalHeight; }
    float GetDesignWidth() const { return m_designWidth; }
    float GetDesignHeight() const { return m_designHeight; }
    float GetScale() const { return m_scale; }
    float GetOffsetX() const { return m_offsetX; }
    float GetOffsetY() const { return m_offsetY; }
    bool IsPortrait() const { return m_physicalHeight > m_physicalWidth; }

    // Physical pixel -> virtual design coordinate.
    float ToDesignX(float physicalX) const {
        return m_scale > 0.0f ? (physicalX - m_offsetX) / m_scale : physicalX;
    }
    float ToDesignY(float physicalY) const {
        return m_scale > 0.0f ? (physicalY - m_offsetY) / m_scale : physicalY;
    }

    // Virtual design coordinate -> physical pixel (used by diagnostics/tests).
    float ToPhysicalX(float designX) const { return designX * m_scale + m_offsetX; }
    float ToPhysicalY(float designY) const { return designY * m_scale + m_offsetY; }

    // Safe area expressed in design coordinates.
    float GetSafeLeft() const { return m_safeLeft / std::max(m_scale, 0.001f); }
    float GetSafeTop() const { return m_safeTop / std::max(m_scale, 0.001f); }
    float GetSafeRight() const { return m_safeRight / std::max(m_scale, 0.001f); }
    float GetSafeBottom() const { return m_safeBottom / std::max(m_scale, 0.001f); }

    // True when a physical touch lands inside the active design canvas.
    bool IsInsideDesignCanvas(float physicalX, float physicalY) const {
        float dx = ToDesignX(physicalX);
        float dy = ToDesignY(physicalY);
        return dx >= 0.0f && dx <= m_designWidth && dy >= 0.0f && dy <= m_designHeight;
    }

private:
    void Recalculate() {
        const bool portrait = m_physicalHeight > m_physicalWidth;
        m_designWidth = portrait ? kPortraitWidth : kLandscapeWidth;
        m_designHeight = portrait ? kPortraitHeight : kLandscapeHeight;
        const float sx = static_cast<float>(m_physicalWidth) / m_designWidth;
        const float sy = static_cast<float>(m_physicalHeight) / m_designHeight;
        m_scale = std::min(sx, sy); // uniform scale: no UI stretching
        m_offsetX = (static_cast<float>(m_physicalWidth) - m_designWidth * m_scale) * 0.5f;
        m_offsetY = (static_cast<float>(m_physicalHeight) - m_designHeight * m_scale) * 0.5f;
    }

    int m_physicalWidth = 1280;
    int m_physicalHeight = 720;
    float m_designWidth = kLandscapeWidth;
    float m_designHeight = kLandscapeHeight;
    float m_scale = 1.0f;
    float m_offsetX = 0.0f;
    float m_offsetY = 0.0f;
    float m_safeLeft = 0.0f;
    float m_safeTop = 0.0f;
    float m_safeRight = 0.0f;
    float m_safeBottom = 0.0f;
};

} // namespace RANMobile
