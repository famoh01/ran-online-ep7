#pragma once
#include <string>

#include "MobileRenderer.h"
#include "../Scene/EP7CharacterScene.h"

namespace RANMobile {

// ---------------------------------------------------------------------------
// B19: live-GLES3 GPU verification harness (Engine GpuSelfTest).
//
// Purpose: verify the B18 GPU skinning/texture pipeline against a REAL GLES3
// implementation (device or emulator) — shader compile/link with driver
// diagnostics, CPU-vs-GPU skinning equivalence via transform feedback,
// pixel-level rasterization of texture/flat/alpha/subset-offset draws, GL
// resource lifecycle, and (when authentic EP7 assets are bundled) an authentic
// character render with per-piece subset/material evidence.
//
// This harness is DIAGNOSTIC and fully isolated: nothing in the normal render
// path calls it.  It returns a JSON verdict document the caller (Android
// bridge / CI) writes to disk and pulls off the device.
//
// `assetManager` is an AAssetManager* on Android (for APK-bundled authentic
// assets under assets/ep7auth/); `authenticAssetRoot` is an optional filesystem
// directory used before the asset manager (CI can adb-push files instead of
// bundling).  `screenshotOutPath` receives a PPM capture of the authentic
// character draw (empty string skips the file write).
// ---------------------------------------------------------------------------
struct GpuSelfTestResult {
    bool attempted = false;       // false when no GL context is available
    bool overallPass = false;
    std::string json;             // full verdict document (also returned when skipped)
    std::string screenshotPath;   // empty when no screenshot was captured
};

GpuSelfTestResult RunGpuSelfTest(MobileRenderer& renderer, void* assetManager,
                                 const std::string& authenticAssetRoot,
                                 const std::string& screenshotOutPath);

} // namespace RANMobile
