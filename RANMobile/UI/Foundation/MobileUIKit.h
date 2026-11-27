#pragma once

#include <string>

namespace RANMobile {
    class MobileRenderer;
}

namespace RANMobile {
namespace UI {

struct UIRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

enum class ButtonVisualState {
    Normal,
    Focused,
    Pressed,
    Disabled,
    Loading
};

// Shared visual language for mobile screens. Presentation only: it performs
// no game or network action.
struct MobileUITheme {
    float minTargetSize = 48.0f;   // approximately 48dp in the virtual canvas
    float padding = 16.0f;
    float accentR = 0.94f;
    float accentG = 0.72f;
    float accentB = 0.22f;
    float panelR = 0.05f;
    float panelG = 0.10f;
    float panelB = 0.20f;
    float textR = 0.96f;
    float textG = 0.97f;
    float textB = 1.00f;
};

bool HitTest(const UIRect& rect, float x, float y);
UIRect CenteredRect(float canvasWidth, float canvasHeight, float width, float height);
UIRect InsetRect(const UIRect& rect, float amount);

void DrawPanel(MobileRenderer* renderer, const UIRect& rect,
               float r, float g, float b, float a);
void DrawButton(MobileRenderer* renderer, const UIRect& rect, const std::string& label,
                ButtonVisualState state, float textScale, const MobileUITheme& theme);
void DrawStatusPill(MobileRenderer* renderer, const UIRect& rect, const std::string& label,
                    float r, float g, float b, const MobileUITheme& theme);

} // namespace UI
} // namespace RANMobile
