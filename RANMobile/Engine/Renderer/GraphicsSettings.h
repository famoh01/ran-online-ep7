#pragma once

namespace RANMobile {

enum class GraphicsQuality {
    Low = 0,
    Medium = 1,
    High = 2,
    Ultra = 3
};

struct GraphicsSettings {
    GraphicsQuality quality = GraphicsQuality::Medium;
    float resolutionScale = 0.85f;
    int textureQuality = 1;   // 0=Low, 1=Med, 2=High
    int shadowQuality = 1;    // 0=Off, 1=Low, 2=High
    int effectQuality = 1;
    float viewDistance = 120.0f;
    int characterQuality = 1;
    int npcQuality = 1;
    int particleQuality = 1;
    int fpsLimit = 60;        // 30, 45, 60, 90

    void ApplyPreset(GraphicsQuality preset) {
        quality = preset;
        switch (preset) {
            case GraphicsQuality::Low:
                resolutionScale = 0.65f;
                textureQuality = 0;
                shadowQuality = 0;
                effectQuality = 0;
                viewDistance = 75.0f;
                characterQuality = 0;
                npcQuality = 0;
                particleQuality = 0;
                fpsLimit = 30;
                break;
            case GraphicsQuality::Medium:
                resolutionScale = 0.85f;
                textureQuality = 1;
                shadowQuality = 1;
                effectQuality = 1;
                viewDistance = 120.0f;
                characterQuality = 1;
                npcQuality = 1;
                particleQuality = 1;
                fpsLimit = 45;
                break;
            case GraphicsQuality::High:
                resolutionScale = 1.0f;
                textureQuality = 2;
                shadowQuality = 2;
                effectQuality = 2;
                viewDistance = 180.0f;
                characterQuality = 2;
                npcQuality = 2;
                particleQuality = 2;
                fpsLimit = 60;
                break;
            case GraphicsQuality::Ultra:
                resolutionScale = 1.0f;
                textureQuality = 2;
                shadowQuality = 2;
                effectQuality = 2;
                viewDistance = 250.0f;
                characterQuality = 2;
                npcQuality = 2;
                particleQuality = 2;
                fpsLimit = 90;
                break;
        }
    }
};

} // namespace RANMobile
