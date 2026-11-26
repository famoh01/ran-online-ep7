# Phase 3B Testing Checklist

All commands below were run from the repository root on 2026-11-26. Exit codes are real; no live server or Android device test was performed.

## 1. Local native compile (Linux, g++ 12.2.0)

Command used the exact source list from `.github/workflows/build-ranmobile-android.yml`, including the new `EP7AssetPipeline.cpp` and `MobileUIKit.cpp`, with `-std=c++17 -Wall -Wextra -Werror`.

- Result: **exit 0**.
- Log: `external/recovery/phase3b_compile.log` (git-ignored; empty on success).
- Binary: `external/recovery/phase3b_tests` (git-ignored).

## 2. Local native tests

```
./external/recovery/phase3b_tests
```

- Result: **exit 0**, final line `ALL TESTS PASSED!`.
- Log: `external/recovery/phase3b_test_output.txt`.
- Coverage before Phase 3B: sections 1–20 unchanged (engine lifecycle, joystick, HUD, data validation, parsers, ThaiPack counts, original protocol constants, test stub server).
- Coverage added in Phase 3B:
  - 21 `RenderLayout` uniform scaling, physical↔design round trip, portrait canvas, safe-area insets.
  - 22 `TouchSystem` multi-touch, move deltas, cancellation, unknown-pointer rejection, design conversion.
  - 23 `MobileUIKit` hit testing, centered/inset rects, 48-unit theme target.
  - 24 Login UI show/hide password, empty-input rejection, accurate `SERVER NOT CONNECTED`, no false `LoginAccepted`, portrait reflow and landscape return.
  - 25 `EP7AssetPipeline` missing root, unsupported format, invalid DDS, valid DDS registration.
  - 26 Renderer lifecycle: `OnSurfaceLost()` suspends `BeginFrame()`, `OnSurfaceCreated()` restores it.

## 3. Android CI (existing workflow)

Workflow: `.github/workflows/build-ranmobile-android.yml` (`Build RANMobile Android (ARM64) & Verify EP7 Release`).

To be dispatched on this branch after the Phase 3B commit. Record here:

- Run ID: **45856360196** on commit `c7a1f2d` — job `build-android-arm64` **success** (all steps 1–14 success).
- Step 4 `Run Linux C++17 Unit & Integration Tests`: **success** (compile with `-Wall -Wextra -Werror`, including the two new source files).
- Step 5 `Run Linux C++17 Tests`: **success** (`ALL TESTS PASSED!`).
- Step 9 `Build RANMobile Native Library (arm64-v8a)`: **success**.
- Step 13 `Build Android APK & AAB (assembleDebug, assembleRelease, bundleRelease)`: **success**.
- Artifacts: `libranmobile-arm64-v8a` 1,566,032 B; `ranmobile-android-packages` 1,674,880 B; `linux-test-failure-log` 862 B (uploaded by `if: always()`, not a failure).

The workflow downloads original EP7 release assets on the runner and asserts the recovered ThaiPack (309 `.ntk`, 730 `.qst`). It compiles and packages; it does not render on a device or contact a live server.

## 4. Explicitly not claimed

- No Android device/Emulator run (no `adb`, SDK, NDK, Gradle or JDK in this sandbox; blocker B1).
- No live server login or gameplay test (blocker B3/B6).
- No original game scene rendering claim: only UI primitives are drawn.
- No GameServer build workaround: B8 remains unchanged.
- No modification of original PC source, protocol constants, packets, DB or game data.

## 5. Phase 3C verification (2026-11-26)

Local compile/test (sandbox, no Android toolchain installed):

- Compile command: `g++ -std=c++17 -Wall -Wextra -Werror` over `RANMobile/**/*.cpp` (36 sources including `EP7SerialFile.cpp`, `EP7SceneData.cpp`, `EP7SceneRenderer.cpp`) + `RANMobile/Tests/test_main.cpp` → **exit 0**. Log: `external/recovery/phase3c_compile.log`.
- Test command: `./external/recovery/phase3c_tests` → **exit 0**, final line `ALL TESTS PASSED!`. Log: `external/recovery/phase3c_test_output.txt`.
- New coverage:
  - 27: DDS header metadata, A8R8G8B8 mask decode, original alpha suffixes/modes, corrupt pixel-format rejection.
  - 28: source-exact `compbyte` encode/decode table plus synthetic `CSerialFile` header/`std::string` layout from `SerialFile.cpp`.
  - 29: `mapslist.mst` field order 0..19 from `GLMapList::LoadMapList`, skipped unused rows, malformed row diagnostic.
  - 30: source-layout synthetic `.glmap` + `SLEVEL_HEAD` + DDS → scene index → renderer texture cache → `ScenePreview` draw → safe-area setter → unload.
- DDS/MTF fixtures in tests are synthetic but derived from original source constants; recovered ThaiPack `.ntk`/`.qst` are not decoded (format unknown). No fake original map/texture is claimed.

Android CI on commit `d2db415` (final Phase 3C implementation):

- Run ID **45863166305**, workflow `Build RANMobile Android (ARM64) & Verify EP7 Release`, job `build-android-arm64`: **success**.
- Steps 1–14 success: source asset extraction, native C++17 compile with `-Wall -Wextra -Werror`, native tests, NDK r26d ARM64 native library, Java 17/Gradle, `assembleDebug`, `assembleRelease`, `bundleRelease`, artifact uploads.
- Artifacts: `libranmobile-arm64-v8a` **1,824,333 B**; `ranmobile-android-packages` **1,783,293 B**; `linux-test-failure-log` 1,015 B (uploaded by `if: always()`, not a failure).
- Failed diagnostic run **45862660028** on `6a63afe` proved an NDK API 24 build error: `AKeyEvent_getUnicodeChar` undeclared in `native_bridge.cpp:300`. That API path was removed and the final run above is green. IME UTF-8 commit remains unimplemented.
- No device/emulator run, no visual/FPS measurement and no live server connection was performed.

## 6. Phase 3C explicit non-claims

- No recovered original DDS/MTF/WLD/GLMAP file exists in the available trees, so no original binary asset was rendered.
- No playable scene, NPC, monster, item, skill, quest or fabricated map is present.
- No login/server/DB/gameplay change; PR #3 remains OPEN/DRAFT.
