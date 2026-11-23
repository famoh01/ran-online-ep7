#pragma once

#include "../../Engine/Renderer/GraphicsSettings.h"

namespace RANMobile {

struct ControlSettings {
    float buttonSizeScale = 1.0f;
    float buttonOpacity = 0.85f;
    float joystickSizeScale = 1.0f;
    float cameraSensitivity = 0.25f;
    float cameraSpeed = 1.0f;
};

class SettingsUI {
public:
    void SetGraphicsPreset(GraphicsQuality quality) { m_graphics.ApplyPreset(quality); }
    const GraphicsSettings& GetGraphicsSettings() const { return m_graphics; }
    ControlSettings& GetControlSettings() { return m_controls; }
    void ResetControls() { m_controls = ControlSettings{}; }

private:
    GraphicsSettings m_graphics;
    ControlSettings m_controls;
};

} // namespace RANMobile
