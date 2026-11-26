# Phase 3D — Original Asset Recovery Audit & Android IME Input

Date: 2026-11-26 UTC. Active branch: `arena/075e1725-ran-online-ep7`. PR #3 remains **OPEN / DRAFT** and was not merged, closed, overwritten or edited. No isolated branch or Draft PR was created because this session is fixed to the active branch.

Implementation commits: `da95f90` (IME bridge + audit changes), `91b9db6` (Android build fixes). A final documentation commit follows. No original gameplay logic, protocol constant, packet structure, Server source, DB schema or live login behaviour was changed.

## 1. Workstream A — asset availability audit

### 1.1 Files reviewed before changes

`PHASE3C_ASSET_RENDERING.md`, `SOURCE_DATA_RECOVERY.md`, `ARCHITECTURE.md`, `BLOCKERS.md`, `EP7AssetPipeline.h/.cpp`, `MobileRenderer.h/.cpp`, `RANMobileEngine.h/.cpp`, `EP7SceneRenderer.h/.cpp`, `EP7TextureDecoder.*`, `EP7SceneData.*`, test fixtures and the existing `external/ep7_release/` / `google_drive/` workspace. The Phase 3C conclusion was confirmed: the recovered ThaiPack has only `.ntk`/`.qst`/`.txt`; no DDS/MTF/WLD/GLMAP/manifest file exists in the available trees.

### 1.2 Authorized sources inspected

- GitHub repository and release `ran-ep7-full-v1`: previously verified release assets; the asset host remains unreachable from this sandbox, so no duplicate download was attempted.
- Google Drive folder `1Y3VOeAeBzSbaB_o9oWLN16qD29b8Ro1m`: listed all 12 files through the connected Drive tool, then retrieved only three not previously recorded, small/directly relevant archives.
- Backup source `matheomedrana/Ran-Online-Episode-7-Complete-Files`: `gh repo view` and `gh api .../git/trees/HEAD?recursive=1` returned **HTTP 404 Not Found**. That path was stopped and is not claimed.

### 1.3 Files actually recovered

| File | Source | Size (B) | SHA-256 | Integrity | Types |
|---|---|---:|---|---|---|
| `RAN-EP7-ThaiPack-FamHawaii.zip` (variant) | Drive `1Z3O9kV1UyfhKG1GoqmuF-iaZTRDG0Bl2` | 2,266,660 | `6bfd50e712bb930acf69d06ca7903fb2b93fec66179fb0c707503a888d5c3ef7` | `unzip -t` OK | 1,049 files: 309 `.ntk`, 730 `.qst`, 10 `.txt` |
| `RAN-EP7-ItemText-Thai-Update.zip` | Drive `14Cjnc3WPpTAi09gIDa78fldph7xu0T6h` | 912,989 | `3d05465da479c2386bcad6580243b37f093f5d087d497e04238e61142dd5a403` | `unzip -t` OK | 3 `.txt` |
| `ไฟล์ exeEp7 Ran.zip` | Drive `111SsgbBXMqg0x6mHYPfLSIxEqmaLDEVo` | 23,584,218 | `397c71a13a51464858f02adb3900d22d83ac6d0f717a8fc4ee5b5a3b08deb944` | `unzip -t` OK | 20 `.exe`, 1 `.dll`, 1 `.ocx` — not executed |

The connector downloads by Drive filename and replaced the earlier canonical ThaiPack archive. The new variant was kept as `google_drive/RAN-EP7-ThaiPack-FamHawaii-2266660.zip`, and the original archive was re-downloaded from Drive ID `1IL8jlmvl5PGGHdaf5nlnoZdBMhTu25Ag`; its SHA-256 again matches the recorded `4a20b17f…f969`. Original files were not modified.

Python `zipfile` inspection found 0 unsafe paths and 0 asset-like extensions in all three new archives. No binary was executed, no secret was committed, and no recovered original asset was overwritten in the extracted tree. Full evidence is appended to `SOURCE_DATA_RECOVERY.md` §17.

### 1.4 Recovery limits and asset pipeline validation

- The two large client split archives (1,572,864,000 B and 1,010,562,339 B) exceed the connector's 100 MB cap; they were not downloaded. The earlier recorded 7,310-entry client path inventory contains no DDS/MTF/WLD/GLMAP/MST entries.
- Consequently **no authentic original texture or map file was recovered in Phase 3D**. No new real-asset decoder/upload/scene test could be added: doing so would require fabricated files, which is prohibited. The existing source-derived synthetic fixtures remain the only format tests.
- Scene rendering status is unchanged: `EP7SceneIndexBuilder` / `EP7SceneRenderer` work only when a real `mapslist.mst`, `.glmap` and DDS/MTF loading texture are supplied; none exists here. No substitute scene was created.

## 2. Workstream B — Android soft keyboard / UTF-8 input

### 2.1 Architecture actually implemented

- `RANMobile/Android/app/src/main/java/com/famhawaii/ranmobile/RanNativeActivity.java` — a `NativeActivity` subclass that installs a hidden `EditText` host and sets `SOFT_INPUT_ADJUST_RESIZE | STATE_HIDDEN`. Back press first dismisses IME focus; otherwise the native UI handles it.
- `RANMobile/Android/app/src/main/java/com/famhawaii/ranmobile/RanTextInputBridge.java` — a real Android `InputConnection` owner. `TextWatcher` forwards the complete field text as a UTF-8 Java string on every commit, replacement, paste or deletion. `focusField()` selects username/password input type, seeds native text, requests focus and shows the keyboard; `blurField()`/`releaseInput()` clear focus and hide it. `OnGlobalLayoutListener` reports keyboard visibility changes to native. `dispose()` removes the listener, clears focus and hides the keyboard.
- `RANMobile/Android/native_bridge.cpp` — `JNI_OnLoad` caches a global class reference; four `extern "C"` JNI callbacks deliver text, focus, IME action and visibility into `RANMobileEngine`. Native-to-Java focus calls pass UTF-8 strings. JNI threads are attached/detached safely and exceptions are cleared. `AKeyEvent_getUnicodeChar()` was **not** used; per-character key-to-text synthesis was removed so IME text is never treated as physical key events.
- `RANMobile/Engine/Core/RANMobileEngine.*` — new `OnIMETextChanged`, `OnIMEFocusChanged`, `OnIMEActionDone`, `SetKeyboardVisible` hooks.
- `RANMobile/UI/Login/LoginScreen.*` — `SetFieldText()` performs bounded UTF-8 replacement (20 **code points**, not bytes); backspace removes one complete code point; masked password length counts code points; keyboard visibility adds a 180-unit bottom inset so the panel reflows upward in landscape.
- `RANMobile/Android/AndroidManifest.xml` and `app/build.gradle` — activity class, `keyboard` config change, soft-input mode, Java source directory.

No authentication, network protocol or server behaviour was changed. IME "Done" reuses the existing UI login-button path; it does not claim server authentication.

### 2.2 Field state and lifecycle

| Event | Result |
|---|---|
| Touch username/password | Native focus is set; Java seeds the current text, selects input type, focuses the hidden editor and shows the keyboard. |
| Touch elsewhere / dialog | Java editor is blurred and keyboard hidden. |
| Text commit / paste / replacement / deletion | Java sends the full new field value as UTF-8; native replaces that field with validated, codepoint-limited text. |
| IME action Done/Go/Search/Send | Existing `LoginScreen::OnLoginPressed()` UI path runs; keyboard is hidden. |
| Back while editing | First press dismisses the keyboard and clears IME focus only. |
| Pause / surface loss / shutdown | `releaseInput()` clears Java focus and keyboard state; native keyboard inset is reset. |
| Password | `TYPE_TEXT_VARIATION_PASSWORD`; UI continues to display a fixed-length mask, never the entered text. |

Cursor and selection are owned by the Android `EditText`; the current native UI has no per-character cursor rendering, so only full-field replacement and deletion are mirrored. This is an explicit limitation, not a claim of native cursor editing.

## 3. Verification

- Native compile: `g++ -std=c++17 -Wall -Wextra -Werror` over all RANMobile sources + tests → **exit 0** (`external/recovery/phase3d_compile.log`).
- Native tests: `./external/recovery/phase3d_tests` → **exit 0**, `ALL TESTS PASSED!` (`external/recovery/phase3d_test_output.txt`). New section 31 covers UTF-8 commit, Thai text, mixed Unicode, codepoint deletion, 20-codepoint limit, password masking, focus transitions, keyboard-responsive layout and IME action without false login success.
- Android CI failure found and fixed: run `45866773886` on `da95f90` failed with three real diagnostics: `LoginScreen` had no `GetPassword()` accessor, and `ReleaseAndroidInput()` was used before declaration in `native_bridge.cpp`. Both were fixed in `91b9db6`.
- Android CI success: run **45867328078** on `91b9db6`, job `build-android-arm64`, **success**, all steps including native C++17 tests, NDK r26d ARM64 library, Java 17/Gradle, `assembleDebug`, `assembleRelease`, `bundleRelease`. Artifacts: `libranmobile-arm64-v8a` 1,825,929 B; `ranmobile-android-packages` 1,800,104 B.
- **No device or emulator test was performed.** JNI/IME behaviour is verified by compilation and native unit tests only; real Thai IME composition, keyboard occlusion and input lifecycle on a device remain unverified.

## 4. Remaining blockers and next phase

Original binary scene assets remain unavailable; `.ntk`/`.qst` payloads remain unknown; the backup GitHub source is 404; large client archives exceed the retrieval cap. Device-level IME verification is also outstanding.

Recommended next phase: (1) supply or authorize retrieval of original DDS/MTF/WLD/GLMAP/`mapslist.mst` files for real asset-pipeline and scene tests; (2) run the packaged APK on a device/emulator with a Thai IME to verify composition, keyboard resize, password display and back behaviour. No gameplay, Server, protocol or additional phase work should begin automatically.
