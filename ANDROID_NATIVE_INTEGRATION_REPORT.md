# ANDROID_NATIVE_INTEGRATION_REPORT.md

- **Native Entry**: `RANMobile/Android/native_bridge.cpp` (`android_main` with `ALooper_pollAll`)
- **Manifest**: `RANMobile/Android/AndroidManifest.xml` (`com.famhawaii.ranmobile`, landscape, OpenGL ES 3.0, INTERNET, ACCESS_NETWORK_STATE)
- **Engine Lifecycle**: `RANMobileEngine::Initialize()`, `Start()`, `Update()`, `Render()`, `Shutdown()`
- **Device Testing**: BLOCKED (`ANDROID BUILD BLOCKED` — requires Android NDK + device/emulator)
