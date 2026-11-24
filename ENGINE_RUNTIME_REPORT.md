# ENGINE_RUNTIME_REPORT.md

- **RANMobileEngine**: Owns `MobileRenderer`, `ResourceManager`, `TouchSystem`, `MobileCamera`, `NetworkClient`, `GameDataLoader`, and `LoginScreen`.
- **Lifecycle**: `Initialize`, `Start`, `Update(dt)`, `Render()` (draws `m_loginScreen`), `Resize(w, h)`, `Pause()`, `Resume()`, `Shutdown()`.
- **Android Loop**: Uses `std::chrono::steady_clock` real delta time and EGL surface dimensions.
