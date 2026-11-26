# Phase 3B — RANMobile Engine, Touch Input, UI and Asset Foundation

Date: 2026-11-26. Base commit: `6aa6715` (Phase 3A). Work performed on the active branch `arena/075e1725-ran-online-ep7` because this session's branch is fixed; PR #3 remains OPEN / DRAFT and was not merged.

This phase implements real C++ changes in `RANMobile/` only. No original PC source, protocol constant, packet structure, server, DB or game data was modified. The Phase 3A protocol blocker remains open.

## 1. Files changed

### New source files
- `RANMobile/Engine/Renderer/RenderLayout.h` — shared virtual-canvas transform (aspect-fit, no stretching, portrait canvas, safe-area insets).
- `RANMobile/UI/Foundation/MobileUIKit.h/.cpp` — reusable panel/button/status-pill drawing and hit testing.
- `RANMobile/Data/Importer/EP7AssetPipeline.h/.cpp` — read-only source-format scanner and `ResourceManager` registration.

### Modified source/build files
- `RANMobile/Engine/Renderer/MobileRenderer.h/.cpp` — surface lifecycle, cached projection, GL error checks, glyph/texture ownership, design-space drawing.
- `RANMobile/Engine/Touch/TouchSystem.h/.cpp` — pointer phases, cancellation, multi-touch, design-coordinate conversion.
- `RANMobile/Engine/Core/RANMobileEngine.h/.cpp` — surface lifecycle, asset pipeline integration, design-space touch routing, render guard.
- `RANMobile/Android/native_bridge.cpp` — EGL surface recreation, config-change resize, all-pointer MOVE events, distinct CANCEL handling.
- `RANMobile/UI/Login/LoginScreen.h/.cpp` — responsive login redesign, accurate status states, touch feedback, password reveal control.
- `RANMobile/Tests/test_main.cpp` — 6 new focused test sections (21–26).
- `RANMobile/CMakeLists.txt`, `.github/workflows/build-ranmobile-android.yml` — register the two new `.cpp` files in native and CI builds.

### Documentation
- `PHASE3B_ENGINE_IMPLEMENTATION.md` (this file), `UI_LOGIN_REDESIGN.md`, `TESTING_CHECKLIST.md`, `BLOCKERS.md`.

## 2. Engine and renderer architecture changes

Before this phase, `MobileRenderer` held raw physical dimensions, rebuilt an orthographic matrix inside every draw call, allocated a glyph bitmap buffer per new glyph, and had no surface-loss path — `RANMobileEngine::Initialize()` ran once while Android could destroy and recreate the EGL surface. Now:

1. `RenderLayout` is the single source of truth for the virtual canvas. Landscape uses 1280×720, portrait uses 720×1280, both with uniform `min(sx, sy)` scaling and centered letterboxing. UI quads are authored in design units and never stretch.
2. `MobileRenderer::OnSurfaceCreated()` / `OnSurfaceLost()` recreate or discard all context-owned objects (program, VAO/VBO, font atlas, uploaded textures, glyph cache) without restarting the engine. `DestroyGPUResources(false)` deliberately does not call GL after context loss.
3. The projection matrix is rebuilt only in `Initialize`, `Resize` and `OnSurfaceCreated`; draw calls bind the cached matrix.
4. `BeginFrame()` returns `false` when no GPU context exists, so the engine skips the UI pass instead of issuing GL calls against a lost surface.
5. `CheckGLError()` logs shader, program, viewport, clear and texture-upload failures with operation names. Shader compile/link logs are surfaced.
6. Glyph rasterization reuses a member scratch buffer and glyph UVs are invalidated when the atlas is recreated.
7. `RANMobileEngine` separates render-surface state (`HasRenderSurface()`) from UI/engine state, routes touch coordinates through the same layout used by the renderer, and tracks the read-only asset-pipeline report.

No original scene, map, monster, NPC, item, skill or effect was generated. The renderer remains a UI renderer; no claim of original world rendering is made.

## 3. Touch input implementation status

`TouchSystem` now tracks every pointer ID independently with `Down/Move/Up/Cancel` phases, start/current positions in physical and design space, move deltas, active-pointer enumeration and explicit `Clear()` on surface loss. Unknown pointer moves are ignored rather than synthesized.

Android native bridge changes:
- `APP_CMD_INIT_WINDOW` → `engine.OnSurfaceCreated(w, h)` (also on surface recreation).
- `APP_CMD_TERM_WINDOW` → `engine.OnSurfaceLost()` before destroying EGL objects.
- `APP_CMD_CONFIG_CHANGED` → re-query surface size and resize viewport/layout.
- `AMOTION_EVENT_ACTION_MOVE` iterates `AMotionEvent_getPointerCount()` and forwards every active pointer.
- `AMOTION_EVENT_ACTION_CANCEL` is a distinct cancellation, not a normal release.

### Verified original PC input references and mobile status

| Original action | Source evidence | Mobile status |
|---|---|---|
| Lobby channel left/right via `DIK_LEFT` / `DIK_RIGHT` | `RanClientLib/DxLobyStage.cpp:493–500` | **Unimplemented** — no channel-select screen or server packet layer. |
| Attack / pickup / target via left/right mouse `DXKEY_DOWNED` | `RanClientLib/G-Logic/GLCharacter.cpp:2384, 2428, 2479, 2708, 2760` | HUD buttons exist visually; **no server action mapping** in this phase. |
| Jump / interact via `dwkeySPACE` | `RanClientLib/G-Logic/GLCharacter.cpp:2726` | HUD Jump button exists visually; **unimplemented**. |
| Modifier actions (`Shift+A`, `Shift+Q`, `Shift+Z`) | `RanClientLib/G-Logic/GLCharacter.cpp:2103, 2371–2384, 2201–2214` | **Unimplemented**; deliberately not invented. |
| UI navigation via `UIKeyCheck` / `DIK_RETURN` / `DIK_BACK` | `EngineUILib/GUInterface/BasicButton.cpp:223–228`, `UIMultiEditBox.cpp:57–73` | Login text entry handles hardware key events in `native_bridge.cpp`; IME UTF-8 commit callback is still unimplemented. |

The existing `VirtualJoystick` and `CharacterController` remain local input demonstrations; they are not connected to gameplay packets. No gameplay command was fabricated.

## 4. Asset pipeline status

`EP7AssetPipeline::ScanDirectory()` walks a directory read-only, classifies files against extensions documented in `SOURCE_ASSET_INVENTORY.md` (`.mst/.wld/.lev/.mnsf/.ntk/.isf/.ssf/.qst/.x/.mxf/.cps/.chf/.dds/.mtf/.tga/.wav/.ogg`…), rejects empty/unreadable files and unsupported formats, validates the `.dds` magic, and optionally registers accepted files in `ResourceManager`. Diagnostics are bounded (64 messages, 20,000 files, 256 retained items) to avoid unbounded logs. `RANMobileEngine::LoadGameData()` invokes the scanner after the existing `DataValidator` step.

No file is converted, generated, renamed or modified. Formats that require original DirectX loaders (`.x`, `.mxf`, `.egp`, `.mtf`, `.wld`, …) are counted and reported but are **not decoded for rendering**; this limitation is recorded in `BLOCKERS.md`.

## 5. Tests and verification

See `TESTING_CHECKLIST.md` for commands and exact outcomes. Locally: compile exit 0 with `-Wall -Wextra -Werror`; native test exit 0 (`ALL TESTS PASSED!`), including new coverage for layout, touch lifecycle, UI kit, login states, asset validation and renderer lifecycle. Android ARM64/APK/AAB verification was delegated to the existing GitHub Actions workflow because this sandbox has no JDK/Gradle/Android SDK/NDK.

## 6. Known limitations / next phase

- Login protocol remains blocked (B6); `LoginScreen` never displays `LOGIN ACCEPTED` from a local action.
- No live server, character-select screen, gameplay packets, original world rendering, or device test.
- IME UTF-8 commit and safe-area values from Android DisplayCutout are not queried from Java; `SetSafeAreaInsets()` is available but defaults to zero.
- Android manifest still locks landscape. Portrait virtual-canvas reflow is implemented, but the activity does not yet allow rotation.
- `MobileServerHost` remains a test-only stub and was not treated as an EP7 server.

Recommended next phase: wire a source-verified authentication flow and response handling **after** B6 is resolved, then connect UI action buttons to verified gameplay packets. Do not start that work in Phase 3B.
