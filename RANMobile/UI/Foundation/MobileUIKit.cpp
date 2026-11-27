#include "MobileUIKit.h"
#include "../../Engine/Renderer/MobileRenderer.h"

#include <algorithm>

namespace RANMobile {
namespace UI {

bool HitTest(const UIRect& rect, float x, float y) {
    return rect.width > 0.0f && rect.height > 0.0f &&
           x >= rect.x && x <= rect.x + rect.width &&
           y >= rect.y && y <= rect.y + rect.height;
}

UIRect CenteredRect(float canvasWidth, float canvasHeight, float width, float height) {
    UIRect rect;
    rect.width = width;
    rect.height = height;
    rect.x = (canvasWidth - width) * 0.5f;
    rect.y = (canvasHeight - height) * 0.5f;
    return rect;
}

UIRect InsetRect(const UIRect& rect, float amount) {
    UIRect out = rect;
    out.x += amount;
    out.y += amount;
    out.width = std::max(0.0f, rect.width - amount * 2.0f);
    out.height = std::max(0.0f, rect.height - amount * 2.0f);
    return out;
}

void DrawPanel(MobileRenderer* renderer, const UIRect& rect,
               float r, float g, float b, float a) {
    if (!renderer) return;
    renderer->DrawUIQuad(rect.x, rect.y, rect.width, rect.height, r, g, b, a);
}

void DrawButton(MobileRenderer* renderer, const UIRect& rect, const std::string& label,
                ButtonVisualState state, float textScale, const MobileUITheme& theme) {
    if (!renderer) return;
    float r = 0.10f, g = 0.22f, b = 0.42f, a = 1.0f;
    switch (state) {
        case ButtonVisualState::Normal:
            r = 0.11f; g = 0.24f; b = 0.44f; break;
        case ButtonVisualState::Focused:
            r = 0.16f; g = 0.33f; b = 0.58f; break;
        case ButtonVisualState::Pressed:
            r = theme.accentR; g = theme.accentG; b = theme.accentB; break;
        case ButtonVisualState::Disabled:
            r = 0.10f; g = 0.12f; b = 0.16f; a = 0.65f; break;
        case ButtonVisualState::Loading:
            r = 0.18f; g = 0.18f; b = 0.22f; break;
    }
    renderer->DrawUIQuad(rect.x, rect.y, rect.width, rect.height, r, g, b, a);
    // Accent rail gives a consistent pressed/focus affordance without images.
    if (state == ButtonVisualState::Pressed || state == ButtonVisualState::Focused) {
        renderer->DrawUIQuad(rect.x, rect.y, 4.0f, rect.height,
                             theme.accentR, theme.accentG, theme.accentB, 1.0f);
    }
    float textR = (state == ButtonVisualState::Pressed) ? 0.08f : theme.textR;
    float textG = (state == ButtonVisualState::Pressed) ? 0.07f : theme.textG;
    float textB = (state == ButtonVisualState::Pressed) ? 0.02f : theme.textB;
    if (state == ButtonVisualState::Disabled) {
        textR = 0.55f; textG = 0.58f; textB = 0.62f;
    }
    const float labelWidth = renderer->MeasureTextWidth(label, textScale);
    const float tx = rect.x + std::max(8.0f, (rect.width - labelWidth) * 0.5f);
    const float ty = rect.y + std::max(6.0f, (rect.height - 28.0f * textScale) * 0.5f);
    renderer->DrawUIText(tx, ty, label, textScale, textR, textG, textB, 1.0f);
}

void DrawStatusPill(MobileRenderer* renderer, const UIRect& rect, const std::string& label,
                    float r, float g, float b, const MobileUITheme& theme) {
    if (!renderer) return;
    renderer->DrawUIQuad(rect.x, rect.y, rect.width, rect.height, r, g, b, 0.85f);
    renderer->DrawUIText(rect.x + theme.padding, rect.y + 9.0f, label, 0.78f,
                         theme.textR, theme.textG, theme.textB, 1.0f);
}

} // namespace UI
} // namespace RANMobile
