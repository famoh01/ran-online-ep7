# PROGRESS_REVIEW.md — RAN ONLINE EP7 PC → MOBILE (Phase 2 read-only review)

Reviewed on 2026-11-26 on `arena/075e1725-ran-online-ep7` (HEAD after review commit will be recorded in the commit itself; review base `ce1aab4`, branch base `47f3c6d` = merge of PR #2). No engine/UI/game-data/protocol files were modified in this phase. Every conclusion below cites a file, commit, PR, or CI run ID.

## 0. Scope and method

- Inspected repository files directly (source, CMake/Gradle, manifests, workflows, tests), `git log`/`git diff`, `gh pr view`/`gh pr diff` for PRs #1–#3, and `gh run list`/`gh api .../jobs`/artifact lists for CI.
- Did **not** merge, close, or edit any PR; did **not** start Phase 3–6; did **not** install an Android toolchain or contact a live server.
- CI log archives could not be downloaded (`gh run view --log` → `results-receiver.actions.githubusercontent.com` EOF), so conclusions about CI rest on job/step conclusions and artifact metadata returned by the GitHub API, not on raw logs.

## 1. Verified completed work

| Area | Evidence | What is really there |
|---|---|---|
| RANMobileEngine lifecycle | `RANMobile/Engine/Core/RANMobileEngine.cpp/.h` (145/79 lines, PR #1 `a35cb8c…`, extended in PR #2 `963750d…`) | Initializes renderer/resource/touch/camera/network/loader/login/HUD; tick/render/pause/resume/shutdown; `EngineScreenState` enum exists |
| Renderer / GLES3 | `RANMobile/Engine/Renderer/MobileRenderer.cpp` (427 lines), `MobileRenderer.h`, `GraphicsSettings.h`; `RANMobile/Android/native_bridge.cpp` EGL block | Real GLES3 shader program, VAO/VBO, font atlas texture, `glViewport`/`glClear`/`glDrawArrays`, RGBA texture upload; EGL context requests `EGL_OPENGL_ES3_BIT`; frustum check and settings |
| Android JNI/native activity | `RANMobile/Android/native_bridge.cpp` (235 lines), `AndroidManifest.xml`, `app/build.gradle`, `settings.gradle` | `android_main` loop, `APP_CMD_INIT_WINDOW/TERM_WINDOW/PAUSE/RESUME`, motion/key events, JNI `InputMethodManager` show/hide, NativeActivity manifest (GLES 3.0 required, landscape, minSdk 24/targetSdk 34, arm64-v8a) |
| Thai font | `MobileRenderer.cpp:98–185, 226–365`, vendored `stb_truetype.h` (5,321 lines) | UTF-8 decode + sticky Thai range selection (U+0E00–0E7F), runtime glyph atlas from device fonts (`/system/fonts/NotoSansThai-Regular.ttf` etc.); unit test 16 decodes Thai codepoints and measures text width |
| LoginScreen | `RANMobile/UI/Login/LoginScreen.cpp` (223 lines) + `.h` | Thai UI text drawn, username/password fields, focus/text input/backspace/masking, server display name "FAM HAWAII", Login/Register/Settings/Exit hit boxes and dialogs, `LoginAuthState` tracking; tests 2/15/17 cover it |
| HUD / touch widgets | `RANMobile/UI/HUD/MobileHUD.cpp/.h`, `UI/Joystick/VirtualJoystick.cpp`, `UI/Buttons/MobileButton.cpp`, `MobileSkillButton.cpp`, `Engine/Touch/TouchSystem.cpp` | Joystick radius/axis/run state, attack/target/potion/jump/pickup + 6 skill buttons with hit tests and cooldowns; tests 3/5/7/8 exercise them |
| Data tooling | `RANMobile/Data/Importer/EP7TextureDecoder.cpp` (167 lines), `EP7WorldCharParser.cpp` (85), `EP7DependencyScanner.cpp` (108), `Data/Validator/DataValidator.cpp` (71), `ThaiPackLoader.cpp` (29) | Real `.mtf` decrypt + `.dds` DXT1/DXT5 decode, `.wld/.chf/.cps` header/bounds parsing, presence validator, Thai pack file counter; tests 6/10/14/19/20 cover these |
| Network transport | `RANMobile/Engine/Network/NetworkClient.cpp` (108 lines) | POSIX TCP connect, non-blocking poll, 15 s heartbeat (type 161), send/receive of `EP7NetMsgGeneric` |
| Protocol constants | `RANMobile/Engine/Network/EP7Protocol.h` vs `netclientLib/s_NetGlobal.h:161–185, 639, 683, 724` | `LOGIN_2 = 1013+107 = 1120`, heartbeat 160/161, ID/PW/random/encrypt lengths and `NET_LOGIN_DATA` field order match source; tests 9 `static_assert` these values |
| Android build pipeline | `.github/workflows/build-ranmobile-android.yml`, `RANMobile/CMakeLists.txt`, `app/build.gradle` | CI run **45809902810** on commit `ce1aab4` (this branch): all 15 steps `success`, artifacts `ranmobile-android-packages` (1,585,823 B), `libranmobile-arm64-v8a` (1,343,383 B), test log (496 B) |
| Original PC source tree | `Basic/`, `RanClientLib/`, `RanClientUILib/`, `EngineUILib/`, `enginelib/`, `netclientLib/`, `netserverLib/`, `LoginServer/`, `SessionServer/`, `AgentServer/`, `FieldServer/`, `GameServer/`, 243 `.lib` + 15 `.dll` runtime files | Unmodified; `git diff main..HEAD` on this branch contains only `ARCHITECTURE.md` and `BLOCKERS.md` |

## 2. Partially implemented work

1. **Authentication flow** — `AuthenticationManager.cpp:12–32` builds `EP7NetLoginData` correctly on the wire (payload + header assembled by `NetworkClient::SendPacket`), but `grep -r '\.Connect\|->Connect' RANMobile` finds **no call site**: no code reads `Config/server_config.json`, so `LoginScreen::OnLoginPressed()` always fails with `SERVER NOT CONNECTED`. No random-password / encrypt-key / key-exchange handshake (`NET_MSG_REQ_RAND_KEY`, `NET_MSG_SND_ENCRYPT_KEY` exist in the original at `s_NetGlobal.h:760–762`).
2. **Thailand packet variant** — PC Thai client calls `CNetClient::ThaiSndLogin` (`netclientLib/s_NetClientMsgLogin.cpp:282+`) using `THAI_NET_LOGIN_DATA` (ID 1153) with TEA encryption; mobile sends plain `NET_MSG_LOGIN_2` (1120). Whether the live "FamHawaii Ep7" server accepts 1120 is **unverified**; constants alone do not prove compatibility.
3. **Screen flow** — `EngineScreenState::CharacterSelect` exists but no character-select screen is drawn; `RANMobileEngine::Render()` only branches Login/InGame and `RANMobileEngine::OnTouch*()` only forwards to `LoginScreen`; `MobileHUD::OnTouch*/Update` are never called by the engine. Login → character select → game transition is not wired.
4. **World rendering** — `MobileRenderer` only draws UI quads/text and uploads raw RGBA textures; no mesh/map/model pipeline connects `EP7AssetImporter`, `EP7TextureDecoder`, `EP7WorldCharParser` or `GameDataLoader` to the renderer. Importer/converter files are 19–26 lines (thin wrappers).
5. **Text input** — `native_bridge.cpp:170–205` maps hardware keycodes A–Z/0–9 and a few punctuation keys; no IME commit/`onKeyDown` UTF-8 callback, so typed Thai via soft keyboard is not delivered. JNI toggles the keyboard but does not relocate the layout above it.
6. **Font asset** — glyph data comes from device system fonts only; no font file is bundled in `RANMobile/` or Gradle assets. If a device lacks Thai fonts, the renderer's fallback uses a plain advance box.
7. **Responsive UI** — layout is derived from renderer size, but the Android manifest hard-locks `screenOrientation="landscape"`; portrait/responsive behavior and 48dp-equivalent touch sizing have not been evaluated (Phase 5 scope).
8. **CI workflows coverage** — `original-asset-import.yml` and `phase1-complete-files-audit.yml` restrict `push` triggers to old branch `arena/6c988b52-ran-online-ep7`; only `build-ranmobile-android.yml` uses `arena/**`. Those two workflows will not auto-run for this branch (manual `workflow_dispatch` remains).
9. **Windows server build workflow** — `build-ep7-windows-server.yml` sets `continue-on-error: true` on the MSBuild step, so a green job does not prove Login/Agent/Field/GameServer compiled. Latest success: run 45545927425; failures also exist (45495657182, 45489604587).

## 3. Missing work (not implemented)

- No `NetworkClient::Connect()` invocation, no runtime server config/host/port UI, no reconnect logic despite `Reconnect` being claimed in older reports.
- No character select UI or gameplay stage rendering; no server-driven character slot population (`CharacterSelectManager.cpp` is only 27 lines of vector bookkeeping).
- No original asset conversion output consumed at runtime; `GameDataLoader::LoadFromDirectory()` only returns `DataValidator` presence results.
- No Thai text resource strings loaded into the UI from ThaiPack (`.ntk/.qst` counted only).
- No release signing config/keystore; Gradle `buildTypes.release` has `minifyEnabled false` and no `signingConfigs`.
- No local Android SDK/NDK toolchain in this sandbox (`ANDROID_HOME` unset; no java/gradle/cmake/ninja/adb).

## 4. Unverified functionality

- Actual device rendering/touch/IME/keyboard behavior (no device run in this sandbox).
- Login against a real EP7 server (no authorized live server).
- Whether the 1120 plaintext login is accepted by the Thai server (see §2.2).
- Binary protocol equivalence with `CNetClient::Send()` at byte level under real server load (only constants/struct order compared statically).
- Renderer output correctness on GLES3 hardware (CI compiles but does not render; headless tests only exercise count of draw calls).
- Exact contents of CI logs/artifacts (log download blocked; only conclusions + sizes available via API).

## 5. Build status

| Target | Evidence | Status |
|---|---|---|
| Linux C++17 unit/integration tests | CI run 45809902810 steps 4–5 `success`; `test_main.cpp` has 20 sections and ends with `ALL TESTS PASSED!` | CI PASS on `ce1aab4`; not rerun locally in Phase 2 (read-only) |
| Android native `libranmobile.so` arm64-v8a | same run step 9 `success`, artifact 1,343,383 B (NDK r26d, platform 24) | CI PASS |
| APK + AAB (debug/release) | same run steps 12–13 `success`, artifact 1,585,823 B | CI PASS |
| Full PC solution | `build.yml` latest run 44677351546 on `main` `success` (MSBuild Win32 Release); older failures e.g. 44641086731 | CI PASS on main; not re-run here |
| Original server projects | `build-ep7-windows-server.yml` mixed results, `continue-on-error: true` | INCONCLUSIVE |
| Local build in this sandbox | `ANDROID_HOME`/`ANDROID_SDK_ROOT` unset; no java/gradle/cmake/ninja/adb | BLOCKED (B1) |

The Android workflow is therefore **CI-feasible**; no new build was triggered in this phase.

## 6. Original data availability

- **Repo**: all C++ client/server source and shared protocol/logic headers present; `RANMobile/` reports/data lists present. `external/ep7_release/` is empty in this checkout — no original binary game data locally.
- **Drive folder** (re-listed 2026-11-26): 12 files. Client archives `RAN-EP7-Client-TH.00.zip` (1,572,864,000 B) and `.001.zip` (1,010,562,339 B) exceed the connector 100 MB cap; `RAN-EP7-ServerDBTools.zip` (49,791,515 B) previously failed retrieval; ThaiPack copies (1,359,185 / 2,266,660 B) and smaller zips listed but not re-imported here.
- **Release `ran-ep7-full-v1`**: same 4 assets (part00/part01/ServerDBTools/ThaiPack). Metadata accessible; bytes blocked because `release-assets.githubusercontent.com:443` fails SSL in sandbox.
- **Backup repo**: clone of metadata succeeds (60,457 tracked paths: `.dds` ×15,449, `.cps` ×11,357, `.wld`, `.qst` ×882, `.ntk` ×318, `data/gui/*.xml`, DB `.sql/.bak`, `cfg/Server*.cfg`). All large files are LFS pointers (e.g. `RanGame1.bak` is a 132-byte pointer); batch API HTTP 403 and `media.githubusercontent.com` unreachable → no original bytes.
- **CI**: run 45809902810 successfully downloaded/extracted release assets (its test asserts ThaiPack 309 `.ntk` / 730 `.qst`); artifacts cannot be downloaded here, so data was not brought into the workspace.
- Missing: original maps/meshes/textures/animation archives, DB dumps/schema bytes, server `.cfg` bytes, original logo/art assets. None were invented or reconstructed.

## 7. Client compatibility status

- PC client source (`Basic/`, `RanClientLib/`, `RanClientUILib/`, `EngineUILib/`, `enginelib/`, `netclientLib/`) is unchanged and still Win32/MFC/Direct3D 9.
- Mobile client does not compile or link any of that code; it is a new C++17 layer mirroring only packet constants and data formats (per `ARCHITECTURE.md`). No regression to the PC client from PR #1/#2/#3 (changed-file lists contain only `RANMobile/`, docs and workflows).
- Feature parity is incomplete: no character select, no map/world/monster/NPC rendering, no inventory/skills/quest screens wired into the live game loop (classes exist and are unit-tested in isolation).

## 8. Android compatibility status

- Gradle/CMake/NDK configuration is real and compiles in CI for `arm64-v8a` (API 24, GLES3, NativeActivity).
- `native_bridge.cpp` implements EGL lifecycle, touch/key input and keyboard toggling; runtime behavior on a device is unverified.
- Manifest requires GLES3 while `InitEGL()` contains a GLES2 fallback config/context path — inconsistent with the manifest's `uses-feature ... required="true"`.
- No portrait layout, no APK signing, no device test result in this phase.

## 9. Server compatibility status

- `LoginServer/`, `SessionServer/`, `AgentServer/`, `FieldServer/`, `GameServer/`, `netserverLib/` (ODBC/Winsock) are untouched; no server-side mobile support was added.
- `RANMobile/Server/MobileServerHost.cpp` is a local test-only listener using FNV-1a password hashes and a single `recv` per connection; it is **not** an EP7 server and does not replace Auth/DB/GM validation.
- No live server test: `LIVE_SERVER_TEST_GUIDE.md` documents requirements; `PROJECT_STATE.md` records `REAL SERVER TESTED: NO`.

## 10. Protocol verification status

- Static verification PASSED: `EP7Protocol.h` values match `netclientLib/s_NetGlobal.h` (heartbeat 160/161, `NET_MSG_LOBBY = 1013`, `NET_MSG_LOGIN_2 = 1120`, `USR_ID_LENGTH = 20`, `USR_PASS_LENGTH = 20`, `USR_RAND_PASS_LENGTH = 6`, `ENCRYPT_KEY = 12`, `NET_LOGIN_DATA` field order). Tests 9 assert the two IDs.
- On-wire assembly: `AuthenticationManager` strips the struct header and `NetworkClient::SendPacket` writes `dwSize`/`nType` again — byte layout matches `CNetClient::SndLogin`'s `Send((char*)&nld)` for the generic variant, but no byte-level test exists.
- Not verified: server-side acceptance, Thai/TEA login variant, key exchange, full packet set beyond login/heartbeat, TCP fragmentation handling (`recv` assumes one packet per read), and any character/gameplay packets.

## 11. Current blockers

Unchanged and re-confirmed in `BLOCKERS.md`: B1 (no local Android SDK/NDK/Gradle/JDK), B2 (original binary assets unreachable on all three sources), B3 (no authorized live EP7 server/MSSQL), B4 (DB dumps and server `.cfg` bytes are LFS pointers/blocked). Additionally: raw CI logs/artifacts cannot be fetched from `results-receiver.actions.githubusercontent.com` (documented in B5). Functional gaps in §2–§4 are review findings for the next phase, not new access blockers.

## 12. PR review and duplicate/conflicting work

- PR #1 MERGED `1a41e285…` (3 commits, ~90 files): creates the whole `RANMobile/` tree + initial Android workflow + reports; not a draft.
- PR #2 MERGED `47f3c6d…` (63 commits, 60 files): modifies many PR #1 files (`MobileRenderer.*`, `LoginScreen.*`, `MobileHUD.*`, `RANMobileEngine.*`, `CMakeLists.txt`, `native_bridge.cpp`, tests, plus 4 workflows) and adds importer/decoder/scanner/host code. No duplicate classes or conflicting parallel implementations were found in the final tree; this was iterative extension rather than a second engine.
- PR #3 OPEN **DRAFT** on this branch (`ce1aab4`, 2 doc files: `ARCHITECTURE.md`, `BLOCKERS.md`). No PR was merged, closed, or edited during Phase 2.
- Possible regressions/claims to watch: older reports still claim features not present in code (e.g. reconnect, full character select, real asset streaming); workflow trigger paths and `continue-on-error` may mislead future status checks.

## 13. Recommended next phase (not started)

After the user clears or explicitly accepts B1–B5: Phase 3 should begin with a small Draft PR for the network/auth gap (server config load + `Connect()` + handshake/Thai-variant decision), then separate PRs for engine/renderer/asset and input/UI work; Phase 4 requires real DB/cfg access; Phase 5 (Login/HUD redesign) must preserve existing auth/packet logic and use only original art. No Phase 3+ work was performed in this review.

---

## Phase 3J review (2026-11-26 — character model, skeleton & animation)

### What was requested

Authentic EP7 `.chf/.cps/.abl/.bin` loading, bone hierarchy, animation sampling, skinning and a
mobile character-rendering foundation on top of Phases 3G–3I, with no fabrication, no
exaggeration and strict evidence.

### What was actually produced

| Deliverable | State |
|---|---|
| `EP7ByteFormat.h` — bounds-checked basestream + compbyte | complete |
| `EP7Math.h` — D3DX-order matrix/quaternion math | complete |
| `EP7XFile.{h,cpp}` — `.x` text tokenizer with correct bin/txt discrimination | complete |
| `EP7Skeleton.{h,cpp}` — Frame hierarchy, UPBODY, palette order, pose accumulation | complete |
| `EP7SkinPiece.{h,cpp}` — `.cps` 0x0104–0x0107 through the trace list | complete |
| `EP7CharacterFile.{h,cpp}` — `.chf` 0x0100–0x0102 | complete |
| `EP7SkinnedMesh.{h,cpp}` — Mesh + SkinWeights + materials + CPU skinning | complete |
| `EP7Animation.{h,cpp}` — `.bin` 0x0100–0x0102, `.cfg` 0x0107, player | complete |
| `Engine/Scene/EP7CharacterScene.{h,cpp}` — loader, instances, culling, diagnostics | complete |
| `UI/Character/CharacterPreviewOverlay.{h,cpp}` — touch preview UX | complete |
| `Tools/verify_ep7_character_assets.cpp` — real-data probe | complete, CI-wired |
| `Data/Importer/EP7RccArchive.{h,cpp}` — `.rcc` reader (inflate + ZipCrypto) | complete |
| `Tests/test_phase3j.cpp` | 256 checks, 0 failures |
| CMake + workflow wiring | complete |
| GPU skinning | **not implemented** (B10) |
| Renderer draw pass for characters | **not implemented** here (PR #4 owns it) |

### Defects found and fixed during this phase (worth recording)

Six of these were real layout/semantic errors, not typos, and each was caught by a test:

1. `.x` detection keyed on the `"xof "` magic — but **every** `.x` file, text included, starts
   with `"xof "`. Now inspects the format field at offset 8.
2. Skeleton fixture/loader ambiguity about a file-level `__ROOT`; the engine synthesises it,
   files do not contain it.
3. `SkinWeights` has **no instance name** in the D3D template — the bone name is the first
   body element. The first implementation looked for a leading name and failed on real layout.
4. `SkinWeights` are children of `Mesh` in files this exporter writes
   (`xskinexp/ExportXFile.cpp:2823`), not siblings.
5. `SMatrixKey` carries a 16-byte-aligned `D3DXMATRIXA16`, so 12 bytes of padding sit between
   `dwTime` and the matrix — the track parser was reading the matrix 12 bytes early.
6. The animation clock was advanced in *seconds*; the original advances
   `delta × UNITANIKEY_PERSEC` (4800). The player also now reproduces the two-unit early-end
   rule.

7. The verification tool assumed character assets are loose files. They are not:
   `GLOGIC::InitFile` loads them from `SkinObject.rcc` / `Animation.rcc` through
   `CUnzipper`, with ZipCrypto password `"tkqwlf"`. A tree walk reports "0 candidates" on a
   perfectly good client extraction. Added `EP7RccArchive` (inflate + ZipCrypto, 208-test
   coverage) and the probe now opens the packs.
8. A skinned mesh `.x` also begins with `Frame` objects, so "the skeleton parser accepted it"
   does not prove it is a skeleton — the probe must try the mesh reader first. Caught by the
   end-to-end `.rcc` run, where `body.x` was initially reported as a 2-bone skeleton.

Also corrected: `.chf`/`.cps` fixture headers (file ID vs payload version), `.cps` material
records genuinely carry a block size (back-patched by `EndBlock`), and `.cfg`'s
`m_sStrikeEff[9]` is a fixed array read regardless of the declared count.

### Honest assessment

The **parsing and evaluation foundation is real and tested**; the **rendering is not**.
No authentic asset was available, so the riskiest remaining unknown is real-file
compatibility — exactly what B9 records and what the CI probe measures. This phase does not
move the mobile client any closer to showing a character on a phone by itself; it removes the
data-format unknown so that PR #4's renderer only needs the four hooks in
`PHASE3J_CHARACTER_ANIMATION.md` §8.
