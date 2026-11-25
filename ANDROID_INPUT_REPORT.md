# ANDROID_INPUT_REPORT.md

- **Touch**: `native_bridge.cpp` handles `AMotionEvent` (`ACTION_DOWN`, `ACTION_POINTER_DOWN`, `ACTION_MOVE`, `ACTION_POINTER_UP`, `ACTION_UP`, `ACTION_CANCEL`) with Pointer IDs -> `RANMobileEngine::OnTouchDown/Move/Up`.
- **Keyboard**: `ANativeActivity_showSoftInput` / `hideSoftInput` on `LoginFocus`, `AKEYCODE_DEL` -> `OnBackspace()`, `AKEYCODE_ENTER` -> `OnLoginPressed()`, `AKEYCODE_A..Z` / `0..9` -> `OnTextInput()`.
