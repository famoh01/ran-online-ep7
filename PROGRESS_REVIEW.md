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
| `Tests/test_phase3j.cpp` | 263 checks, 0 failures |
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

---

## 2026-11-26 — Phase 3J CI isolation and B11 management

### What was wrong

The Phase 3J verification work ended with an honest but badly-placed red light: the
authentic-asset step exited 1 because unimplemented formats remain, and because the whole
workflow was a **single job**, that failure also skipped the core C++17 suite, the Phase 3J
suite's summary, the log artifact and the entire Android APK/AAB build. The signal was
correct; its blast radius was not.

There was a second, quieter problem: the probe had only two outcomes. "This build has no
definition for this format" and "this build is broken" produced the same word — `FAIL` — so
a reviewer could not tell a tracked coverage gap from a genuine reader defect.

### What changed

1. **Three independent CI jobs** (no `needs:`): `core-tests`, `authentic-assets`,
   `android-arm64`. Core tests and the Android build no longer require the 2.5 GB client
   download, and no asset verdict can skip them.
2. **Three-way outcome in the probe** — `PASS` / `FAIL` / `UNSUPPORTED` (plus `NOT FOUND`),
   with exit codes 0 / 1 / 2 / 3. `UNSUPPORTED` is counted, printed, annotated and summarised
   separately and is **never** reported as parsed.
3. **Format family check** — `ParseInfo` now reads the 128-byte basestream type and only
   grants `UNSUPPORTED` to files that really say `SANIMCONINFO`. A different type is a
   genuine failure. This is the one B11 claim made, and it rests only on
   `CSerialFile::ReadFileType()`.
4. **B11 evidence audit** recorded in `BLOCKERS.md`: the `0x0108`–`0x0115` grep hits in the
   engine are `DxLandMan`/`DxFogMan` **landscape** versions (`FILEMARK "LAND.MAN"`), not
   animation layouts, and were rejected as fabricated evidence.

### Verified results (2026-11-26)

| Check | Result |
|---|---|
| Phase 3J suite | **276 checks, 0 failures** (was 263; +13 for the new classification rules) |
| ASan + UBSan + LSan | clean |
| Probe outcome matrix (PASS/UNSUPPORTED/FAIL/NOT FOUND) | 0 / 2 / 1 / 3 as designed |
| Core test binary compiles from the workflow's exact command | yes |
| Core failure propagation | reproduced (`test_main.cpp:186`, exit 134 → step exit 134) |
| `EP7RccArchive` / ZIP / Deflate / ZipCrypto / CRC | unchanged, still covered by the suite |
| `.cfg` 0x0101–0x0107 | unchanged, still covered |

### What is deliberately *not* done

No structure, offset, size or decoder for `.cfg` `0x0108`–`0x0115` or for the `"Anim"`
`.bin` container was written. B11 stays **OPEN** with an explicit evidence requirement list.
The asset job will therefore report `UNSUPPORTED` — green, warned, and clearly labelled —
until reliable definitions exist.

`SERVER_REQUIREMENTS.md` was inspected and needed no change (it covers server OS/DB/ports,
not client asset formats). `UI_LOGIN_REDESIGN.md` does not exist on this branch; it lives
only on the PR #3 / PR #4 branches and is unrelated to CI isolation, so it was not copied
here.

### First CI run with the isolation in place — run 45914698354

| Job | Result |
|---|---|
| Core C++17 tests (blocking) | **success** |
| Android ARM64 build (lib + APK/AAB) | **success** |
| Authentic EP7 asset verification | **failure** (probe exit 1) |

The isolation did its job on the first run: the asset job's failure is visible and
diagnosable on its own, while the core suite, its summary, its log artifact and the whole
Android build completed normally. Before this change, all three of those would have been
skipped.

The three-way outcome model immediately paid off by splitting the old
"8,443 failed" into **1,631 unsupported** (B11, no definition anywhere in this repo) and
**6,812 failed** (a real defect surface). The previous two-state report could not express
that difference at all.

**Honest status: this CI run is RED, and it should be.** The remaining 6,812 pack failures
are not a format-coverage question; the dominant reason (`2,664 x cps: truncated trace
name`) points at a stream-alignment defect in this build's `.cps` reader. Getting a byte
dump of one failing `.cps` is the next step — deliberately not attempted here, because the
only way to "fix" it without evidence would be to move offsets until something parses, and
that is exactly the fabrication this project has avoided.

Also recorded: the `"Anim"`-magic `.bin` files are **loose files only**; no `.bin` inside the
packs has been observed carrying that magic. If that holds, B11's `.bin` half may not affect
the runtime path at all. Resolving it needs more annotation budget than GitHub's ~10-line cap
allows, so the probe's aggregated summary is the right next tool.

---

## Phase 3J follow-up — CPS forensic investigation: root cause confirmed

**Date:** 2026-11-26 · **Branch:** `arena/42c5ac0c-ran-online-ep7` · **Base commit:** `95df8e5`

### Summary

The authentic `.cps` failure is **not** a missing format definition and **not** a wrong trace
count. It is a **single wrong constant**: `SVERTEXINFLU::VERSION`.

### Confirmed cause

`SVERTEXINFLU` records are written and read with the tag **`0x0101`**, in two independent
engine translation units:

- `enginelib/Meshs/DxSkinMeshContainer9.cpp:36` — `const DWORD SVERTEXINFLU::VERSION = 0x0101;`
- `enginelib/Meshs/SMeshContainer.cpp:29` — `const DWORD SVERTEXINFLU::VERSION = 0x0101;`

`SVERTEXINFLU::SaveFile()` (`DxSkinMeshContainer9.cpp:59`) writes `VERSION` as the record tag,
and `SVERTEXINFLU::LoadFile()` (`:94`) compares the tag it reads against the same constant.
The mobile reader in `RANMobile/Data/Importer/EP7SkinPiece.cpp` compared against **`0x0100`**,
so every authentic trace record took `LoadFile`'s *unknown-version* branch:

```cpp
else { SFile.SetOffSet ( SFile.GetfTell() + dwSize ); }
```

That skip is wrong for two independent reasons:

1. **The tag never matches**, so the body (`index`, `vector`, `normal`, `numBone`,
   `bones[n]`, `weights[n]`) is never consumed structurally.
2. **`dwSize` is over-stated by the writer.** `SaveFile` adds the bone/weight arrays twice —
   once in the initial `dwSize` expression (`sizeof(DWORD)*m_dwNumBone + sizeof(float)*m_dwNumBone`)
   and again in the `if (m_dwNumBone > 0 && m_dwNumBone != COL_ERR)` block. Measured:
   `numBone=1 → dwSize 0x30 vs 0x28 actual`, `numBone=2 → 0x40 vs 0x30`,
   `numBone=6 → 0x80 vs 0x50`. (With `numBone == 0` the two agree, `0x20`.)

The over-skip lands the read cursor *inside* the next trace's ASCII name. The next 4 bytes of
ASCII are then read as a length prefix, exceed `kMaxPathString` (1024), and surface as
**`truncated trace name`** — the exact message reported 2,664 times by CI.

### Corroborating evidence

- **Material stage is clean.** CI reported **zero** `material`-stage and **zero**
  `trace count` failures. Every authentic `.cps` therefore reaches the trace count with a
  correctly aligned stream, which rules out the header, path strings, mesh name, material
  array and `SMATERIAL_PIECE` block as the defect site.
- **The failure-count arithmetic closes exactly.**

  | Reason | Count | Class |
  |---|---|---|
  | `cps: truncated trace name` | 2,664 | desync cascade |
  | `cps: effect count exceeds sanity limit` | 438 | desync cascade |
  | `cps: truncated trace vertex influence` | 58 | desync cascade |
  | `cps: truncated piece flag` | 56 | desync cascade |
  | `cps: truncated effect count` | 5 | desync cascade |
  | **desync-cascade subtotal** | **3,221** | |
  | `cps: unsupported .cps version (0x0104..0x0107)` | 3,138 | separate gate issue |
  | `chf: unsupported .chf version (0x0100..0x0102)` | 243 | correctly gated |
  | `skeleton: no named Frame objects` | 210 | skeleton layer |
  | **TOTAL** | **6,812** | **= CI FAIL count exactly** |

  The 3,221 desync-cascade entries are precisely the `.cps` files whose trace list is non-empty.
  Files rejected earlier by the version gate (3,138) never reach the trace loop.

### Byte-level read trace (reconstructed from `SaveFile`, not from a sample)

The extraction steps (2–3) could **not** be executed in this sandbox: `external/` is absent and
`gh release download` for `ran-online-ep7` release assets fails with `EOF`
(`release-assets.githubusercontent.com` unreachable). No hex dump was fabricated. The layout
below is the engine's own writer/reader pair:

```
[128-byte basestream file-type block][header fileId]
[0x0107][m_dwRef]
[len][xFileName...]  [len][skeleton...]
[m_emType][m_emWeaponWhereBack]        <- 0x0106/0x0107 only
[len][meshName...]
[m_dwMaterialNum]  { SMATERIAL_PIECE... }
[traceCount]
   per trace:  [len][name...]                       <- CSerialFile::operator>>(std::string)
               [0x0101][dwSize]                     <- SVERTEXINFLU::VERSION
               [index][vector(3f)][normal(3f)][numBone]
               [bones u32 * n][weights f32 * n]     <- n = numBone when 0 < n <= 6
[effectCount] { TypeID + effect payload... }
[m_dwFlag]
```

`CSerialFile::operator>>(std::string&)` (`enginelib/Common/SerialFile.cpp:609`) and
`CSerialMemory::operator>>` (`:381`) both read **`DWORD dwSize` then `dwSize` bytes**, and
`operator<<(const std::string&)` (`:460`) writes `strlen+1` including the NUL — the reader's
`ReadString` / `ReadFixedString` convention already matched this correctly and was left alone.

### Fix applied

Minimal, source-backed, no offset guessing:

- `RANMobile/Data/Importer/EP7SkinPiece.cpp` — `kVertexInfluenceVersion` `0x0100` → **`0x0101`**,
  with the two engine definition sites cited in the comment.
- `RANMobile/Data/Importer/EP7SkinPiece.h` — `EP7VertexInfluence::kVersion` `0x0100` → **`0x0101`**.

No other reader logic changed. Field order, string convention, `dwSize` handling on the
*unknown-version* path and every sanity limit are untouched.

### Regression test

`RANMobile/Tests/test_phase3j.cpp` — `PieceCpsWithTrace()` + `TestSkinPieceTraceInfluence()`.
The fixture writes a trace record exactly as `SVERTEXINFLU::SaveFile()` does: tag `0x0101`,
an over-stated `dwSize` (`32 + 16*2`), and `numBone = 2`. It asserts the influence is parsed
structurally (`index`, `position`, `normal`, `boneCount`, `bones`, `weights`) and that the
trailing `effectCount` / `m_dwFlag` are reached — i.e. the cursor landed on the structure, not
on the over-stated skip. A negative check flips the tag to `0x0100` (located by value, not by a
hand-computed offset) and asserts the influence is left unpopulated, matching the engine's
unknown-version branch.

**Proof the test catches the defect:** rebuilding the suite against the old `0x0100` constant
reproduces 4 failures (`truncated trace vertex influence`, `traces.size() == 1`,
`error.empty()`, and the negative check) — 283 checks / 4 failures. With the fix: **294 checks,
0 failures**.

### Remaining uncertainties

- **No authentic sample was read.** The conclusion rests on the engine's writer/reader pair
  being mutually consistent and on the CI failure arithmetic closing exactly (3,221 + 3,138 +
  243 + 210 = 6,812). Both are strong, but a byte dump of one real `.cps` remains the
  gold-standard confirmation and is still unobtainable here.
- **The 3,138 `.cps` files rejected as "unsupported version" are a separate, unfixed issue.**
  Those are versions `0x0101`–`0x0103`, for which `DxSkinPiece::LoadPiece` *does* have
  complete branches (`DxSkinPieceContainer.cpp:366`, `:437`, `:513`). Widening the gate is a
  distinct format-implementation task and was deliberately **not** attempted under this
  investigation's scope.
- The 210 skeleton failures and the 243 `.chf` failures are unrelated to this defect. The
  `.chf` gate is correct (`DxSkinCharData.h:22` `VERSION = 0x0102`; the loader's `switch`
  accepts only `0x0100`–`0x0102` and returns `FALSE` otherwise).

### Verification against authentic data — executed in CI (run `45917078958`, commit `802f0c0`)

The fix was verified against the real client assets by the `Authentic EP7 asset verification`
job, which downloads the release asset and runs the probe. **This is executed evidence, not a
projection.** Job outcome: `failure` (the probe still exits 1, because 3,679 FAILs remain); the
other two jobs were `success` (`Core C++17 tests (blocking)`, `Android ARM64 build`).

**Failure reasons, aggregated by the probe** (the probe accumulates reasons in a map and prints
one line per reason precisely so they are not lost to GitHub's ~10-annotation cap):

| Reason | before (`95df8e5`) | after (`802f0c0`) |
|---|---|---|
| `cps: truncated trace name` | 2,664 | **0 — eliminated** |
| `cps: effect count exceeds sanity limit` | 438 | **0 — eliminated** |
| `cps: truncated trace vertex influence` | 58 | **0 — eliminated** |
| `cps: truncated effect count` | 5 | **0 — eliminated** |
| `cps: truncated piece flag` | 56 | 88 |
| `cps: unsupported .cps version (0x0104..0x0107)` | 3,138 | 3,138 (B13, unchanged) |
| `chf: unsupported .chf version` | 243 | 243 (unchanged) |
| `skeleton: no named Frame objects` | 210 | 210 (unchanged) |
| **total FAIL reasons** | **6,812** | **3,679** |

The four desync-cascade reasons are gone entirely. The net change reconciles exactly:

```
eliminated desync cascade        3,165   (2,664 + 438 + 58 + 5)
truncated piece flag  56 -> 88     +32
predicted net change            -3,133
observed  net change     3,679 - 6,812 = -3,133
```

The +32 on `truncated piece flag` is not a regression: those `.cps` files previously aborted at
a trace and never reached the flag, so they were counted under a trace reason. Now they get as
far as `m_dwFlag` and stop there, which means the trace section — and the `SVERTEXINFLU`
records inside it — parsed to completion and the file then ended early. They are a distinct,
newly *visible* truncation issue (see B15).

Caveat on reading CI annotations directly: the new run's annotations also show
`bin: not an EP7 basestream container` (6,228) and `cfg: UNSUPPORTED ...` (2,058 + 328 + 34)
lines that the old run did not surface. Those are **UNSUPPORTED** (informational, B11), not
FAIL, and their appearance is an artefact of the annotation cap: with ~10 annotations per run,
4 of the old run's FAIL reason lines consumed the budget that the new run has spare. The
aggregated FAIL table above is the authoritative comparison; annotation presence alone is not.

---

## Phase 3J follow-up — B13: `.cps` legacy versions 0x0101–0x0103 resolved

**Date:** 2026-11-26 · **Branch:** `arena/42c5ac0c-ran-online-ep7` · **Commit:** `a591320`

### Verdict

B13 was a **real parser gate defect**, proven entirely from engine source. Three of the four
conclusions listed in the task's Step 3 apply:

| Hypothesis | Finding |
|---|---|
| parser rejects valid versions unnecessarily | **TRUE** — 0x0101–0x0103 are fully implemented by `LoadPiece` |
| a version-specific field is missing | **TRUE** — 0x0101/0x0102 have no material count and no trace names |
| original source uses a different read sequence | **TRUE** — fixed 8/10-slot arrays instead of count+names |
| original source uses a different structure | partially — the difference is a *layout* switch, not a new struct |
| the rejection is legitimate | **FALSE** |

A **fourth, previously unreported defect** was found while auditing the same code: the reader
required a trailing `m_dwFlag` for **0x0104**, which the engine does not write.

### Source evidence — the complete branch map

`DxSkinPiece::LoadPiece` (`enginelib/Meshs/DxSkinPieceContainer.cpp`) has seven branches and a
terminal `else` that raises *"Can't Load Skin object"* and returns `E_FAIL` (`:934-943`):

| Version | Branch | Header through mesh name | Materials | Traces | Trailing flag |
|---|---|---|---|---|---|
| 0x0107 | `:374` | ref, x, skel, type, **weapon**, mesh | count + `SMATERIAL_PIECE` | count + name + `SVERTEXINFLU` | yes |
| 0x0106 | `:459` | ref, x, skel, type, **weapon**, mesh | count + `SMATERIAL_PIECE` | count + name + `SVERTEXINFLU` | yes |
| 0x0105 | `:544` | ref, x, skel, type, mesh | count + `SMATERIAL_PIECE` | count + name + `SVERTEXINFLU` | yes |
| 0x0104 | `:629` | ref, x, skel, type, mesh | count + `SMATERIAL_PIECE` | count + name + `SVERTEXINFLU` | **no** |
| 0x0103 | `:712` | ref, x, skel, type, mesh | **none** | count + name + `SVERTEXINFLU` | **no** |
| 0x0102 | `:783` | ref, x, skel, type, mesh | **none** | **10 fixed slots**, no names | **no** |
| 0x0101 | `:859` | ref, x, skel, type, mesh | **none** | **8 fixed slots**, no names | **no** |

Two details worth stating explicitly, because both are easy to get wrong:

1. **The slot arrays are not a count + list.** 0x0101 reads `for (int i=0; i<8; i++)
   sTraceArray[i].LoadFile ( SFile );` (`:891`) and 0x0102 reads the same loop with `<10`
   (`:815`). No count, no name strings, no version tag to skip — exactly N `SVERTEXINFLU`
   records back to back.
2. **Only four slots can ever become trace points.** After the loop, the two branches write
   `if ( sTraceArray[k].m_dwIndex != COL_ERR )` guards for k = 0..3 only (`:896-904`,
   `:820-829`), naming them from the hard-coded `STRACE_IMAGE01`/`02`/`11`/`12` macros
   (`DxSkinPieceContainer.h:22-25`), which `STRACOR` copies through `StringCchCopy(…, 32, …)`
   i.e. truncated to 31 characters. Slots 4..7 (or 4..9) are read and discarded.

`m_dwFlag` is *not* read by any of 0x0101–0x0104. It is initialised to `0L` in the constructor
(`:192`) and only ever `|=`-ed with effect flags (`:703`, `:774`, `:850`, `:925`).

The competing definition in `enginelib/Meshs/ReplaceContainer.cpp:19`
(`DxSkinPiece::VERSION = 0x0100`) is a **different class** — `DxReplaceContainer`, used by
`DxLandMan`/`DxPieceContainer`, included via `DxReplaceContainer.h` — and is not built from
that file into the character path. `enginelib.vcxproj:1526` compiles
`Meshs\DxSkinPieceContainer.cpp`, which is the authoritative `.cps` loader.

### Changes

Four switches, each derived one-to-one from the branches above — no offsets, no guesses:

```cpp
hasMaterials     = version >= 0x0104   // 0x0103 has no material count
hasTraceNames    = version >= 0x0103   // 0x0101/0x0102 use fixed slot arrays
hasTrailingFlag  = version >= 0x0105   // 0x0104 and older end after the effect list
legacySlots      = (version == 0x0102) ? 10 : (version == 0x0101) ? 8 : 0
```

`RANMobile/Data/Importer/EP7SkinPiece.cpp` / `.h`. The 0x0104–0x0107 read order, the
length-prefixed string convention, `ReadVertexInfluence`'s unknown-version skip and every
sanity limit are untouched. `EP7TracePoint` gained `syntheticName`, so a caller can tell an
engine-table name (`IMAGE01`) from one stored in the file.

`EP7WorldCharParser` needed no change: it only bounds `version <= CPS_VERSION (0x0107)`.

### Regression tests

`TestSkinPieceLegacyVersions()` in `RANMobile/Tests/test_phase3j.cpp`, with per-version fixtures
under `namespace legacy`. Coverage:

- 0x0104, 0x0103, 0x0102, 0x0101 each parse, with the field set each branch actually stores.
- the material list is empty for 0x0101–0x0103 and populated for 0x0104.
- slot names are `IMAGE01` / `IMAGE11` and flagged `syntheticName`.
- slots whose `m_dwIndex == COL_ERR` are filtered out (2 of 4 populated slots survive).
- **the 8-vs-10 slot constants are load-bearing**: exercised in both directions (a 10-slot body
  read as 0x0101 and an 8-slot body read as 0x0102 both fail).
- 0x0104 with a flag DWORD appended must *not* consume it (`info.flag == 0`).

The pre-existing gate test asserted that **0x0103 is rejected** — that assertion encoded the
bug. It now tests versions the engine genuinely does not implement (`0x0100`, `0x0108`,
`0x0200`), all of which reach `LoadPiece`'s terminal `else`.

**Control builds** (temporary copies, reverted afterwards):

| Injected fault | Result |
|---|---|
| `hasTrailingFlag >= 0x0104` (restore the old behaviour) | 3 failures — `truncated piece flag`, `error.empty()`, `flag == 0` |
| `legacySlots` = 10 for both 0x0101 and 0x0102 | 4 failures — `truncated trace vertex influence`, `traces.size() == 2`, `error.empty()`, and the cross-version check |
| pre-fix reader (gate 0x0104..0x0107 only) | 8+ failures across the legacy section |

So the new tests fail when the behaviour is wrong and pass when it is right; they are not
vacuous.

### Local test results (executed)

| Stage | Checks | Failures |
|---|---|---|
| before this change | 294 | 0 |
| after | **339** | **0** |

ASan + UBSan + LSan clean (exit 0). Core suite (`test_main.cpp`) compiles clean under
`-Werror`; its run still stops at the pre-existing `test_main.cpp:186` external-data assertion
(exit 134), unchanged. The verification probe compiles clean under `-Werror`.

### Authentic-asset results — executed in CI (run `45923517031`, commit `a591320`)

Jobs: `Core C++17 tests (blocking)` **success**, `Android ARM64 build (lib + APK/AAB)`
**success**, `Authentic EP7 asset verification` **failure** (correct — genuine FAILs remain).

Probe summary line, pack entries (what the engine actually loads):

| | before (`1e36e89`) | after (`a591320`) | Δ |
|---|---|---|---|
| parsed | 6,350 | **6,479** | **+129** |
| failed | 3,679 | **3,550** | **−129** |
| unsupported | 1,631 | 1,631 | 0 |
| total | 11,660 | 11,660 | 0 |

The **+129 reconciles exactly**, and by file type it is 100% `.cps`:

- **−41**: `cps: unsupported .cps version` fell **3,138 → 3,097**. Those 41 entries are
  versions 0x0101–0x0103 that are now accepted, and not one of them reappeared under a
  different failure reason — all 41 parsed completely.
- **−88**: `cps: truncated piece flag` **no longer appears at all**. That reason accounted for
  exactly 88 entries in the previous run. These were genuine 0x0104 files being wrongly
  required to carry a trailing `m_dwFlag`; with the flag read gated to 0x0105+, they now parse.
- 41 + 88 = **129** = the observed Δparsed, and Δfailed is the same 129.

Cross-check by category: total CPS failures went 3,226 → 3,097 (= −129), while `.chf` (243)
and skeleton (210) were unchanged, so every single change is attributable to `.cps`.

No other file type changed — `.bin` 6,228 unsupported, `.cfg` unsupported 2,058/1,303/328/34
and `.x`/`.skt` categories are identical to the previous run.

### Remaining blockers

- **B13 now stands at 3,097**, not 0. Those are `.cps` files whose **version DWORD itself** is
  outside `0x0101..0x0107`. The engine would reject them too (terminal `else`, `:942`), so the
  reader is behaving correctly; the reason string is the useful signal. Settling whether any of
  them are legitimate would need a byte dump, which is unobtainable here.
- **B15 is resolved as a side effect** (the 88 entries parse now), but a *new* small population
  may be hiding behind it and is **not investigated here** per Step 6.
- **B14 unchanged** (the `SVERTEXINFLU::SaveFile` `dwSize` over-statement is documented, not
  fixed; the reader is immune because it consumes the body structurally on a version match).
- The "why 0x0104 files exist without a flag" question is answered by source and needs no
  further work.

---

## Phase 3J follow-up — B14: `SVERTEXINFLU::SaveFile` size arithmetic audited

**Date:** 2026-11-26 · **Branch:** `arena/42c5ac0c-ran-online-ep7` · **Commit:** `80a6336`

### Verdict

B14 is **a genuine defect in the original engine's writer** — confirmed by source and by a
compiled verbatim transcription. It is **not a defect in this codebase**, and the mobile reader
is **already immune** to it. Therefore **no production code was changed**; this task adds the
regression coverage that pins the behaviour down and prevents a future "helpful" fix from
introducing the bug.

The previously recorded 8/16/48-byte figures were **correct but incomplete**. They sampled
`numBone` = 1, 2, 6 and missed two cases: `numBone == 0` (where the two terms coincide and
`dwSize` is right) and `numBone == COL_ERR` (where the DWORD arithmetic **wraps and
under-states**). Both are now documented and tested.

### Source evidence

`SVERTEXINFLU::SaveFile` — `enginelib/Meshs/DxSkinMeshContainer9.cpp:59-92`, byte-identical in
`enginelib/Meshs/SMeshContainer.cpp:52-85`:

```cpp
DWORD dwSize = sizeof(DWORD) +
               sizeof(D3DXVECTOR3) +
               sizeof(D3DXVECTOR3) +
               sizeof(DWORD) +
               sizeof(DWORD)*m_dwNumBone +
               sizeof(float)*m_dwNumBone;

if ( m_dwNumBone > 0 && m_dwNumBone!=COL_ERR )
{
    GASSERT(m_pBone);
    GASSERT(m_pWeights);
    dwSize += sizeof(DWORD)*m_dwNumBone + sizeof(float)*m_dwNumBone;   // <-- added AGAIN
}

SFile << VERSION;
SFile << dwSize;
SFile << m_dwIndex;
SFile << m_vVector;
SFile << m_vNormal;
SFile << m_dwNumBone;
if ( m_dwNumBone > 0 && m_dwNumBone!=COL_ERR )
{
    SFile.WriteBuffer ( m_pBone,    sizeof(DWORD)*m_dwNumBone );   // written ONCE
    SFile.WriteBuffer ( m_pWeights, sizeof(float)*m_dwNumBone );   // written ONCE
}
```

The initial expression already contains `sizeof(DWORD)*n + sizeof(float)*n`. The `if` block adds
the same two terms a second time, while the body writes each array exactly once. **The arrays are
counted twice and written once.**

### Exact size arithmetic (compiled, not hand-computed)

A verbatim transcription was compiled with `static_assert(sizeof(DWORD)==4)` and
`static_assert(sizeof(V3)==12)`, using `DWORD` (unsigned) arithmetic so it wraps like the original:

| `m_dwNumBone` | `dwSize` written | body actually written | delta |
|---|---|---|---|
| 0 | `0x20` (32) | 32 | **0 — agrees** |
| 1 | `0x30` (48) | 40 | **+8** over-states |
| 2 | `0x40` (64) | 48 | **+16** |
| 3 | `0x50` (80) | 56 | **+24** |
| 4 | `0x60` (96) | 64 | **+32** |
| 5 | `0x70` (112) | 72 | **+40** |
| 6 (`MAX_BONE`) | `0x80` (128) | 80 | **+48** |
| `COL_ERR` (0xFFFFFFFF) | **`0x18` (24)** | **32** | **−8 — UNDER-states** |

General form: body = `0x20 + 8n`; declared `dwSize` = `0x20 + 16n` for `1 <= n <= 6`, `0x20` for
`n == 0`, and `0x18` (wrapped) for `COL_ERR`.

The `COL_ERR` row is the new finding. `4*n` overflows the 32-bit sum, so `dwSize` becomes
*smaller* than the record. A reader that trusted `dwSize` as a body length would under-read
there and over-read everywhere else — wrong in both directions depending on the record.

### Block-size accounting: `SVERTEXINFLU` vs `SMATERIAL_PIECE`

The defect is specific to `SVERTEXINFLU`. `SMATERIAL_PIECE::SaveFile`
(`DxSkinPieceContainer.cpp:109-122`) wraps its body in `BeginBlock`/`EndBlock`, and `EndBlock`
(`enginelib/Common/SerialFile.cpp:189-208`) derives the size from the **actual file positions**:

```cpp
long nEndPos = GetfTell();
DWORD dwTotalSize = DWORD ( ( nEndPos - m_nMarkPos[m_dwBlockMarkPOS] ) - sizeof(DWORD) );
```

That size is measured, so it cannot be wrong. `SVERTEXINFLU` hand-computes its size from field
counts instead of measuring, which is why only it is affected. Confirmed: no
`BeginBlock`/`EndBlock` appears anywhere in either `SVERTEXINFLU::SaveFile` or `::LoadFile`.

### Why the reader is not affected, and why that is not luck

`SVERTEXINFLU::LoadFile` (`DxSkinMeshContainer9.cpp:94-127`) reads the tag and `dwSize`, then
branches:

- **tag matches** → read `index`, `vVector`, `vNormal`, `numBone`, then `bones[n]`/`weights[n]`
  by structure. **`dwSize` is never consulted.**
- **tag differs** → `SetOffSet ( SFile.GetfTell() + dwSize )`. This is the **only** place
  `dwSize` is authoritative, and the mobile reader mirrors it exactly with
  `s.Skip(blockSize)` (`EP7SkinPiece.cpp:44-46`).

A structural read cannot be misplaced by a size the writer got wrong. So the engine — and this
reader — simply never observe the bad value on real data, which is why the bug survived in a
shipping client.

`SetOffSet` on `CSerialFile` (`SerialFile.cpp:161-168`) is a bare `fseek` returning `FALSE` on
failure and is **not** range-checked. `Stream::Skip` (`EP7ByteFormat.h:294-298`) is
bounds-checked through `Ensure`, so on the mismatch path this reader is strictly safer than the
engine while producing the same cursor movement for in-range values.

### Version specificity

Not version-specific. `SVERTEXINFLU` is its own nested record with its own `VERSION` (`0x0101`),
independent of the `.cps` container version. The same writer produced these records for every
`.cps` version that stores traces — 0x0101 through 0x0107. The three writer call sites are
`DxSkinPiece::SavePiece` (`:1006`), plus the fixed-slot legacy paths that call
`sTraceArray[i].SaveFile`; all inherit the same arithmetic.

### Mobile parser impact: none, verified

- There is **no `.cps` writer in RANMobile** — `EP7SkinPiece` is reader-only (grep for
  `SavePiece`/`SaveFile`/`WriteBuffer` returns only comments). So B14 cannot be "fixed" here
  without inventing a serializer, which would be out of scope and would fabricate format code.
- The only consumer of the field is the mismatch-path `Skip`, which is what the engine does.

### Regression tests added (`TestSkinPieceInfluenceSizeAccounting`)

`RANMobile/Tests/test_phase3j.cpp` — the only file changed by this task.

| Case | Why it matters |
|---|---|
| `numBone == 0` | the one legal count where `dwSize` and body **agree**; a compensating reader breaks here |
| `numBone == COL_ERR` | `dwSize` **under-states** (wrapped `0x18` for a `0x20` body) — must still parse |
| `numBone` 0..6 loop | every legal count round-trips and lands exactly on `effectCount` |
| mismatch tag | `dwSize` must still be honoured as the skip distance, matching `LoadFile`'s `else` |

The new fixture `PieceCpsWithInfluence(numBone, index)` reproduces the writer's arithmetic
*before* it is printed, so the test data cannot drift away from the real encoding.

### Controls — proof the tests are load-bearing

Temporary copies with the behaviour deliberately broken, then reverted:

| Control | Injected change | Result |
|---|---|---|
| A | trust `dwSize` unconditionally on a match | many failures |
| B | trust `dwSize` when it "looks plausible" | many failures |
| C | drop the `COL_ERR` special case | many failures |
| D | add a `dwSize`-vs-body consistency check | **31 failures**, incl. `COL_ERR` and `numBone == 0` |

Control D is the important one. Adding a consistency check is the single most likely future
"hardening", and the suite now rejects it — because the writer, not the reader, is wrong.

### Results (executed)

| Stage | Checks | Failures |
|---|---|---|
| before | 339 | 0 |
| after | **398** | **0** |

ASan + UBSan + LSan clean (exit 0). Core suite compiles clean under `-Werror`; its run still
stops at the pre-existing `test_main.cpp:186` external-data assertion (exit 134), unchanged.

### Authentic-asset CI (run `45926387844`, commit `80a6336`)

| Job | Result |
|---|---|
| `Core C++17 tests (blocking)` | **success** |
| `Android ARM64 build (lib + APK/AAB)` | **success** |
| `Authentic EP7 asset verification` | failure — correct; genuine FAILs remain |

Pack entries: `6,479 parsed, 3,550 failed, 1,631 unsupported` — **identical** to the previous
run on `6ee1b02` (`6,479 / 3,550 / 1,631`) and to `a591320`. That is the expected result for a
test-only change and confirms **no authentic-asset regression**. No improvement is claimed.

### Remaining

B14 stays **OPEN as an observation about the original source**, not as a task for this
repository: there is nothing here to fix. It would matter only if someone (a) writes a `.cps`
serializer in RANMobile, or (b) changes the influence reader to trust `dwSize` on a version
match. Both are now guarded by tests and documented. `SMATERIAL_PIECE` needs no change; its
block size is measured by `EndBlock`.

---

## Phase 3J follow-up — B11 "Anim" container: source investigation

**Date:** 2026-11-26 · **Branch:** `arena/42c5ac0c-ran-online-ep7` · **Commit:** `73d6d6d`

**Scope discipline:** this was an investigation only. **No production code was changed.** The
finding is a classification defect in the *verification tool*, plus a correction to a B11
premise that had been carried forward unexamined.

### Headline

The unsupported format the probe reports as *magic `"Anim"`* is **`SAnimContainer`**, the EP7
animation container — and its layout is **already fully implemented** in
`RANMobile/Data/Importer/EP7Animation.cpp`. It is not an unknown format.

The `"Anim"` magic is not a magic at all: it is the first four bytes of the 128-byte
basestream **file-type string** `"AnimContainer"`, which is exactly how every EP7 basestream
begins.

### Source evidence

**Writer** — `SAnimContainer::SaveToFile`, `enginelib/Meshs/SAnimation.cpp:1573-1607`:

```cpp
const TCHAR SAnimContainer::szFILETYPE[] = _T("AnimContainer");   // SAnimation.cpp:22
...
SFile.SetFileType ( szFILETYPE, VERSION );                        // :1583
if ( !SFile.OpenFile ( FOT_WRITE, strPath.c_str() ) ) return E_FAIL;
SFile << (DWORD)m_listAniUPBODY.size();      // up-body list FIRST
for ( ... ) (*iter)->SaveToFile(SFile);
SFile << (DWORD)m_listAnimation.size();      // main list
for ( ... ) (*iter)->SaveToFile(SFile);
```

`SAnimContainer::VERSION = 0x0102` (`SAnimation.h:355`).

**Header encoding** — `CSerialFile::WriteFileType` (`SerialFile.cpp:57-66`) writes
`m_szFileType` as **128 bytes** then `m_FileID` as one DWORD. `SetFileType`
(`SerialFile.cpp:34-38`) `StringCchCopy`s the type string into that 128-byte field. So the file
literally begins:

```
offset 0x00 : 'A' 'n' 'i' 'm' 'C' 'o' 'n' 't' 'a' 'i' 'n' 'e' 'r' 0x00 ... (128 bytes)
offset 0x80 : 0x02 0x01 0x00 0x00        <- VERSION 0x0102
offset 0x84 : payload (first list count)
```

**Reader** — `SAnimContainer::LoadAnimFromFile`, `SAnimation.cpp:1754-1776`:

```cpp
SFile.GetFileType( szFileType, FILETYPESIZE, dwVersion );
switch ( dwVersion )
{
case 0x0100: Load_0100( ... ); break;
case 0x0101: Load_0101( ... ); break;
case VERSION: Load( ... );     break;     // 0x0102
default:     return E_FAIL;
}
```

Note carefully: **`szFileType` is read but never compared** in this function. Dispatch is
**purely on `dwVersion`**. (A `strcmp` against the type string exists only in the landscape
loader, `DxLandManSaveLoad.cpp:733`.) This matters for the recommendation below.

**Payload** — `SAnimation::SaveToFile` (`:1353-1400`) writes, per track: `VERSION`(0x0102),
`m_strBone` (length-prefixed), `m_cRotateKeys` + keys, `m_cPositionKeys` + keys, a "garbage"
DWORD, `m_cQuatPosKeys` + keys, a second "garbage" DWORD, `m_cMatrixKeys` + keys, `m_cScaleKeys`
+ keys. `SAnimation::VERSION = 0x0102` (`SAnimation.h:34`). `SAnimation::LoadFromFile`
(`:1400-1417`) dispatches on that per-track version (0x0102 / 0x0101 / 0x0100).

### Every premises verified, and one disproved

| Question | Answer | Evidence |
|---|---|---|
| Is `"Anim"` a magic? | **No** — prefix of `"AnimContainer"` | `SAnimation.cpp:22`, `SerialFile.cpp:57-66` |
| Is the format undocumented? | **No** — writer + reader + all three version branches | `:1573`, `:1754`, `:1779`/`:1796`/`:1812` |
| Is it loaded from packs? | Yes — `Animation.rcc`, via the same basestream path | `EP7ByteFormat.h` header model |
| Does the mobile parser implement it? | **Yes, completely** | `EP7Animation.cpp:267` `ParseContainer` |
| Is a file starting with `"Anim"` foreign? | **No — that is the normal, correct header** | see below |

### The actual defect: a wrong heuristic in the verification tool

`RANMobile/Tools/verify_ep7_character_assets.cpp:242-259` decides:

```cpp
// A .bin that begins with printable ASCII is not an EP7 basestream container at all
// (the client's own containers start with a small version DWORD).
if ( bytes.size() >= 4 && std::isprint(bytes[0]) && ... std::isprint(bytes[3]) ) {
    ... "not an EP7 basestream container — starts with magic '%s' ..."
    ReportOutcome("bin", path, Outcome::Unsupported, detail);
    return Outcome::Unsupported;      // <-- ParseContainer is NEVER CALLED
}
```

The comment states the opposite of what the engine does. EP7 basestreams **do not** start with a
version DWORD — they start with a 128-byte type string (`WriteFileType`). The version DWORD sits
at offset `0x80`. The heuristic therefore rejects every well-formed `SAnimContainer` file
**before** the parser that understands it is ever invoked.

**Proof, executed.** A source-accurate `SAnimContainer` (type string `"AnimContainer"`, version
`0x0102`, up-body list then main list, one 20-byte rotate key each) was built and passed to the
real parser:

```
valid=1  version=0x0102  up=1  main=1  err=''
  up[0].bone   = 'Bip01_Spine1'   rotKeys=1
  main[0].bone = 'Bip01_Head'     rotKeys=1
probe magic pre-check -> UNSUPPORTED (never parsed)
```

The parser handles the real header perfectly. The probe would never show it.

**Scope of the impact.** The heuristic exists **only** in the verification tool. It is absent
from `EP7AssetImporter`, `EP7Animation`, and the loader path (grep for `isprint` returns only
those two lines). So this is a **false-negative in reporting**, not a runtime parsing defect.

### Why this was never caught

All eight basestream fixtures in `test_phase3j.cpp` write **zero-filled** type fields
(`std::vector<uint8_t>(128, 0)`), so no test ever exercised a real EP7 type string. Every
fixture would still parse with the type field blanked because, per `SAnimContainer::LoadAnimFromFile`,
the type string is not consulted.

The `.cfg` path *does* check the family (`ParseInfo` rejects a foreign type with
*"not a SANIMCONINFO file"*, covered at `test_phase3j.cpp:1102-1117`), so the concept was
understood — it was simply implemented backwards for `.bin`.

### Evidence still missing

- **No authentic `.bin` was inspected.** The conclusion rests on the engine's writer/reader pair
  plus an executed parse of a source-derived fixture, not on real bytes. `external/` is absent
  and `gh release download` fails with `EOF` in this sandbox.
- Consequently the **6,228 count cannot yet be split**. If every one of those files begins with
  `"AnimContainer"`, they are well-formed and the count would collapse to near zero. If a subset
  begins with something else (the `.abf` skeleton assets, effect `.bin`s, `cFileList.bin`), those
  are genuinely foreign. Determining the split needs one run of a corrected classifier.
- Whether the 3,097 remaining `.cps` version failures share a similar reporting artefact was
  **not** examined here (out of scope per Step 6).

### Recommended next step — a separate, separately-scoped task

**Do not** add an Anim parser; none is needed. The indicated fix is in the **verification tool
only** (one file, `RANMobile/Tools/verify_ep7_character_assets.cpp`):

1. Delete the printable-magic pre-check — it encodes a false premise about the container format.
2. Let every `.bin` reach `ParseContainer`, which already handles `"AnimContainer"`, and classify
   on the actual result: header type `/` version accepted → PASS; recognised container with an
   unimplemented version → UNSUPPORTED; not a basestream at all → FAIL with the byte fingerprint.
3. Optionally keep the byte fingerprint as *diagnostic detail* only, never as a pre-emptive
   verdict.
4. Add a regression fixture carrying a **real** `"AnimContainer"` type string, plus a genuinely
   foreign one, so both branches are exercised.

That change would make the reported `.bin` numbers reflect the parser's true capability. It was
deliberately **not** made in this task, per the instruction not to modify production code and to
propose a separately scoped implementation.

### Test results (executed, no regression)

| Stage | Checks | Failures |
|---|---|---|
| before | 398 | 0 |
| after (no code changed) | **398** | **0** |

ASan + UBSan + LSan clean (exit 0). Production code and all parsers untouched.


> Preserved-branch B11 evidence: the following append was copied from
> commits 80f932b/cf4e03c on arena/1491d572-ran-online-ep7. Its CI results
> describe that source branch, NOT a newly integrated PR #5 head. Target CI
> must be rerun after integration. No Phase 3B–3I implementation is copied.


## 2026-11-27 — Phase 3J B11 verifier correction (implementation)

### Verified starting point and scope

- Session branch `arena/1491d572-ran-online-ep7`, HEAD `313b828`, clean tracked tree.
- PR #5 OPEN/DRAFT, head `arena/42c5ac0c-ran-online-ep7` at
  `01f391967c1121c581aa45033e56558702b1b599`. It is NOT this session's branch.
  Session restrictions prohibit pushing to that other branch. No branch switch,
  retarget, PR merge, or modifications to PR #3/#4/main are performed.
- Baseline merged CI run `45936773745`: core tests and Android ARM64 lib/APK/AAB
  succeeded; authentic assets failed. Its check annotations confirm the requested
  pack baseline: **6,479 parsed / 3,550 failed / 1,631 unsupported**.
- Only verifier, verifier tests/CI plumbing, and append-only documentation change.
  Production animation parser, engine, UI, gameplay and server remain untouched.

### Source evidence and correction

The old `.bin` gate treated four printable leading bytes as UNSUPPORTED before
calling the parser, falsely asserting that containers start with a version DWORD.
`enginelib/Meshs/SAnimation.cpp:22` defines `SAnimContainer::szFILETYPE` as
`"AnimContainer"`; `SaveToFile` at 1573-1617 calls `SetFileType` and writes up-body
then main lists. `enginelib/Common/basestream.h:16` defines FILETYPESIZE=128;
`SerialFile.cpp:34-65` copies the type, then writes/reads 128 type bytes and a
DWORD. `SAnimation.cpp:1619-1735` dispatches on header versions 0x0100/0101/0102.
`EP7Animation.cpp:267-360` already implements them; it reads but does not check
the type. No new animation format is implemented here.

Verifier now requires a complete 132-byte header and exact NUL-terminated
`AnimContainer` identity (padding after NUL is unconstrained), invokes the existing
header-aware parser, and preserves its explicit unsupported-version diagnostic.
Headerless retry is removed: it could overwrite the real header's error. Cases:

| Evidence | Existing outcome | Diagnostic bucket |
|---|---|---|
| Recognized header, parser success | PASS | parsed |
| Recognized header, explicit container-version rejection | UNSUPPORTED | unsupported-version |
| Fewer than 132 header bytes | FAIL | invalid-header |
| Different/nonterminated type | FAIL | unrecognized-type (foreign OR damaged; not asserted which) |
| Recognized header, other parser rejection | FAIL | parser-rejected (malformed payload OR reader limitation) |

The last case cannot honestly distinguish corruption from a parser defect. First
16 bytes (hex) and size are diagnostics, never verdict heuristics. `.cfg`, `.chf`,
`.cps`, skeleton and `.x` classification branches are unchanged. Pack/loose/all
`.bin` counts are now printed separately; binUnsupported is actually incremented.

### Executed local validation

- Verifier suite: **590 checks, 0 failures**, normal and ASan+UBSan builds.
- ASan + UBSan + integrated LSan (`detect_leaks=1`, halt-on-error): clean exit 0.
- Load-bearing mutation: reinstating the original printable gate causes **373 of
  590 checks to fail**, exit 1. This is not a parser-only test.
- Existing Phase 3J: **398 checks, 0 failures**. Union core compilation clean with
  `-Wall -Wextra -Werror`; local execution aborts at `tpStats.found` because real
  ThaiPack data is absent. CI must run that suite with release data.
- Source-layout fixtures contain the actual nonzero `AnimContainer` type string,
  DWORD at byte 128, nonzero padding, and a named nonempty zero-key track; these
  are ABI fixtures, NOT authentic game assets. Every strict truncation is tested.
- Android ARM64 and authentic-asset post-fix results pending remote CI; no claim
  that all 6,228 former printable-ASCII entries parse. Final results appended below.

CI adds verifier/sanitizer execution plus full authentic log artifact and compact
classification annotation, preserving existing blocking failure/exit semantics.


### 2026-11-27 — B11 post-correction CI evidence (final results)

Implementation commit `80f932b287ebb2c5144014b8eb59f897efe1fc74` was tested in
[CI run 45937494300](https://github.com/famoh01/ran-online-ep7/actions/runs/45937494300).
Core C++ tests with downloaded release data PASSED; Phase 3J PASSED; verifier
590/0 normal and 590/0 ASan+UBSan+integrated LSan PASSED. Android ARM64 native
library and debug/release APK + release AAB PASSED. The authentic verifier ran
against the extracted release and correctly remained FAIL (exit 1), not a green
coverage claim. Full log artifact: `phase3j-authentic-assets-log`; core/sanitizer
logs and Android outputs are also uploaded.

#### Exact comparison: same pack baseline, separate loose .bin result

| Scope / metric | Before | After | Delta |
|---|---:|---:|---:|
| Pack parsed | 6,479 | 6,479 | 0 |
| Pack failed | 3,550 | 3,550 | 0 |
| Pack unsupported | 1,631 | 1,631 | 0 |
| Loose .bin parsed | 0 | 982 | +982 |
| Loose .bin failed (parser-rejected after recognized header) | 0 | 1,209 | +1,209 |
| Loose .bin unsupported | 6,228 (incorrect printable gate) | 4,037 (explicit version rejection) | -2,191 |
| Loose .bin total | 6,228 | 6,228 | 0 |

After correction: **0 invalid-header**, **0 unrecognized-type** among these 6,228.
All have the recognized header, but only **982 parse successfully**. The 4,037
unsupported outer container versions are **0x0103: 1,533**, **0x0104: 2,495**,
**0x0200: 9**. All 1,209 recognized-header parser rejections report the existing
parser diagnostic `unsupported animation version` (track-level, NOT the explicit
outer-container-version error). They remain FAIL/parser-rejected; no assertion is
made that the original files are corrupt or that every such failure is a parser
bug. No parser implementation is changed to force them green.

**Critical scope finding:** every .bin candidate in this release scan is a loose
file under `data/animation`; **zero .bin entries were encountered inside packs**.
That is why the pack verdict/counts remain unchanged. This does not prove no pack
in any EP7 distribution contains .bin files; it describes this run only. The
historical 6,228 figure must not be presented as a pack-only count or as 6,228
valid animations. Header recognition/parse success also does not prove gameplay
or rendered animation correctness. The old binUnsupported summary counter was
not incremented; the before figure here is the actual emitted UNSUPPORTED outcome
count, not that erroneous zero-valued counter.

Remaining blockers: unsupported versions above; 1,209 parser rejections requiring
separate investigation; pre-existing pack .chf/.cps/.abf/.cfg coverage gaps; and
PR #5 head belonging to another session branch. The verifier correction is done
on `arena/1491d572-ran-online-ep7`; PR #5 remains OPEN/DRAFT, unmerged, with its
head unchanged. This task does not authorize changing runtime/parser behavior.


## 2026-11-27 — Phase 3J B12: Authentic Animation Container (`SAnimContainer` / `.bin`) Investigation (`arena/00fd13f9-ran-online-ep7`)

Completed read-only source + authentic-asset forensic investigation of all `6,228` loose `data/animation/*.bin` containers in `ran-ep7-full-v1` on branch `arena/00fd13f9-ran-online-ep7` (stacked Draft PR #7 -> `arena/42c5ac0c-ran-online-ep7` -> Draft PR #5 -> `main`). Production `RANMobile/Data/Importer/EP7Animation.{h,cpp}` and `RANMobile/Tools/verify_ep7_character_assets.cpp` remain 100% untouched.

### Deliverables Added

- `ANIMATION_CONTAINER_INVESTIGATION.md` — full version-by-version binary format specification (`SAnimContainer` `0x0100`–`0x0104` + `0x0200`/`EMBYTECRYPT_BIN`, `SAnimation` `0x0100`–`0x0103`), authentic SHA-256 hex dissections, `EP7Animation.cpp` defect audit, `.bin` x `.cfg` cross-version matrix, and 4-stage follow-up implementation plan.
- `RANMobile/Tools/inspect_ep7_animation_containers.cpp` — standalone read-only forensic C++17 CLI reproducing B11 buckets, pinpointing rejection offsets, running a read-only `SAnimationSaveLoad.cpp` shadow verifier, and verifying asset SHA-256 immutability.
- `RANMobile/Tests/test_ep7_animation_inspector.cpp` + `RANMobile/Tests/run_ep7_animation_inspector_tests.sh` — `5,574` checks passing under both `-O2` and `-fsanitize=address,undefined` (`detect_leaks=1`).
- `.github/workflows/phase3j_b12_animation_investigation.yml` — reproducible CI workflow retrieving and SHA-256-verifying all 19 `Lib_Engine` animation/serialization LFS files from `matheomedrana/Ran-Online-EP7-Complete-Files` (`f661fbf8...`) and surveying `ran-ep7-full-v1`.

### Summary of Verified Findings ([CI run 45982141864](https://github.com/famoh01/ran-online-ep7/actions/runs/45982141864))

- **B11 `data/animation/*.bin` reconciliation:** `982` parsed (`1` `0x0100` 0-track + `907` `0x0101` + `74` `0x0102`), `4,037` unsupported outer (`1,533` `0x0103` + `2,495` `0x0104` + `9` `0x0200`), `1,209` parser-rejected (`1,209` non-empty `0x0100`), `0` invalid-header, `0` unrecognized-type (`6,228` total; `0` `.bin` in `.rcc` packs; `357` non-animation `.bin` outside `data/animation/`).
- **100% authentic asset validity:** Read-only shadow verification implementing `Lib_Engine/Meshs/SAnimationSaveLoad.cpp` reaches **`EXACT_EOF` (`trailing_bytes == 0`) on `6,228 / 6,228` (`100.00%`)** authentic `.bin` files (`384,339` bone tracks).
- **Exact cause of `1,209` parser rejections:** `1,209 / 1,209` fail at `list=main, idx=0, offset=136` with `trackVer=0x0100` because `ParseContainer` accepts outer `0x0100` while `ReadBoneTrack` omits `SAnimation::Load_0100`.
- **Critical `SMatrixKey` padding defect uncovered:** `EP7Animation.cpp:94-104` and `168-178` check `kMatrixKeySize = 80` (`u32 time` + `12`B Win32 `D3DXMATRIXA16` `align(16)` padding + `64`B matrix) but read `68` bytes without `s.Skip(12)`. Masked in `test_phase3j.cpp:429` (`12` unread bytes, `mat[0]` silently read as `0.0f`) and unhit in the `982` accepted files (`0` have `m_cMatrixKeys > 0`), but affects **`1,205 / 1,210`** authentic `0x0100` files.


## 2026-11-27 — Phase 3J B13: Implement Authentic Animation Track Version `0x0100` & Fix 80-Byte `SMatrixKey` Alignment (`arena/00fd13f9-ran-online-ep7`)

Implemented Stage 1 of the B12 animation parser plan on `arena/00fd13f9-ran-online-ep7` (commit `1e11d2ec7f77bf938cdefa0bda6668a5f2a85b48`, stacked Draft PR #7 -> `arena/42c5ac0c-ran-online-ep7`; Draft PR #5 untouched).

### What Changed in B13

- **`RANMobile/Data/Importer/EP7Animation.{h,cpp}`:**
  - Added `EP7AnimationReader::kMatrixKeyAlignPadSize = 12` (with compile-time `static_assert` proving `4 + 12 + 64 == 80 == kMatrixKeySize`) and fixed `ReadMatrixKeys` to skip the 12-byte Win32 `D3DXMATRIXA16` `align(16)` padding via `s.Skip(12)` across all verified track versions (`0x0100`, `0x0101`, `0x0102`) without reading uninitialized padding or misaligning the stream.
  - Implemented authentic `SAnimation::Load_0100` (`version == 0x0100` in `ReadBoneTrack` per `enginelib/Meshs/SAnimation.cpp:1419-1462` and `Lib_Engine/Meshs/SAnimationSaveLoad.cpp:15-50`): `positionKeys` (`16`B), `rotateKeys` (`20`B), `scaleKeys` (`16`B), `matrixKeys` (`80`B), and `m_strBone` (preserving valid empty bone names `""`, which resolve to `boneIndex == -1` in `BindToSkeleton`).
  - Preserved explicit rejection of unimplemented outer container versions (`0x0103`, `0x0104`, `0x0200`) and inner track version `0x0103`.
- **`RANMobile/Tools/inspect_ep7_animation_containers.cpp`:**
  - Updated the inspector's production-reader cursor trace to match B13 `EP7AnimationReader` (`0x0100`–`0x0102` tracks + 80-byte `SMatrixKey`) so `acceptedTrailingBytes` directly verifies zero-trailing-byte consumption on every parsed file.
- **Regression tests (`test_phase3j.cpp`, `test_ep7_character_verifier.cpp`, `test_ep7_animation_inspector.cpp`):**
  - Added regression coverage for `0x0100` `pos`/`rot`/`scale`/`matrix` keys, non-zero 12-byte `SMatrixKey` padding sentinels, multi-track stream alignment, empty bone names (`""` and `len=0`), skeleton binding + `EP7AnimationPlayer::ApplyToSkeleton` matrix-key evaluation, invalid key/string count rejection, byte-by-byte truncation (`0 .. size-1`), `0x0101`/`0x0102` regression, and explicit `0x0103`/`0x0104`/`0x0200` rejection.

### Authentic Validation, Test & Android Build Results ([CI run 45990886364](https://github.com/famoh01/ran-online-ep7/actions/runs/45990886364), [CI run 45990886385](https://github.com/famoh01/ran-online-ep7/actions/runs/45990886385))

| Verification Target | Pre-B13 | Post-B13 | Status |
|---|---:|---:|---|
| Authentic `0x0100` `data/animation/*.bin` parsed | `1 / 1,210` | **`1,210 / 1,210` (`100.00%`, `63,226` tracks)** | **VERIFIED (`EXACT_EOF`)** |
| Authentic `0x0101` + `0x0102` `data/animation/*.bin` parsed | `981 / 981` | **`981 / 981` (`907` + `74`, `77,067` tracks)** | **VERIFIED (`EXACT_EOF`)** |
| Total `data/animation/*.bin` parsed (`EXACT_EOF`, `0` trailing bytes) | `982 / 6,228` | **`2,191 / 6,228` (`+1,209`)** | **VERIFIED** |
| `data/animation/*.bin` `parser-rejected` (FAIL) | `1,209` | **`0` (`-1,209`)** | **RESOLVED** |
| `data/animation/*.bin` `unsupported-version` (`0x0103`, `0x0104`, `0x0200`) | `4,037` | **`4,037` (unchanged, out of B13 scope)** | **TRACKED** |
| Phase 3J host tests (`test_phase3j.cpp`, `-O2` & `ASan+UBSan+LSan`) | `398 / 0` | **`952 checks, 0 failures`** | **PASS** |
| Verifier regression (`run_ep7_verifier_tests.sh`, `-O2` & `ASan+UBSan+LSan`) | `590 / 0` | **`1,571 checks, 0 failures`** | **PASS** |
| Inspector regression (`run_ep7_animation_inspector_tests.sh`, `-O2` & `ASan+UBSan+LSan`) | `5,574 / 0` | **`7,035 checks, 0 failures`** | **PASS** |
| CI `Core C++17 tests (blocking)` (`45990886364`, job `140478134000`) | pass | **pass** | **PASS** |
| CI `Android ARM64 build (lib + APK/AAB)` (`45990886364`, job `140478133579`) | pass | **`libranmobile-arm64-v8a` (`2,302,838` B), `ranmobile-android-packages` (`1,988,407` B)** | **PASS** |

*Explicit non-claims:* Parsing `2,191` authentic `.bin` containers (`1,208` of the `1,210` `0x0100` files also pair with supported `.cfg` `0x0103`–`0x0107`) proves binary container/track compatibility, not on-device rendering or live gameplay. No Android emulator/device or live server test was executed in this sandbox.


## 2026-11-27 — Phase 3J B14: Implement Authentic Animation `0x0103` Container & Track (`arena/00fd13f9-ran-online-ep7`)

Implemented authentic `SAnimContainer` outer version `0x0103` and `SAnimation` inner track version `0x0103` on `arena/00fd13f9-ran-online-ep7` (commit `63c8c805ff66c3c89a3e8143757efe11c4c29a9a`, stacked Draft PR #7 -> `arena/42c5ac0c-ran-online-ep7`; Draft PR #5 untouched).

### What Changed in B14

- **`RANMobile/Data/Importer/EP7Animation.cpp`:**
  - Implemented authentic outer `SAnimContainer::Load_0103` (`hdr.version == 0x0103` in `ParseContainer` per `Lib_Engine/Meshs/SAnimationSaveLoad.cpp:412-442,644-668`): `upBodyTracks` (`u32 numUpBody` + tracks), `uint32_t garbageValue` (`s.ReadU32(garbageValue)` where authentic writer emits `0x0103 + numUpBody + numMain`), and `mainTracks` (`u32 numMain` + tracks).
  - Implemented authentic inner `SAnimation::Load_0103` (`version == 0x0103` in `ReadBoneTrack` per `Lib_Engine/Meshs/SAnimationSaveLoad.cpp:192-293`): `m_strBone`, `rotateKeys` (`20`B), `dwGarbage1` (`u32`), `positionKeys` (`16`B), `quatPosKeys` (`36`B), `matrixKeys` (`80`B including `kMatrixKeyAlignPadSize = 12`), `dwGarbage2` (`u32`), and `scaleKeys` (`16`B), while preserving `0x0100`, `0x0101`, and `0x0102` behavior unchanged.
  - Preserved explicit rejection of unimplemented outer container versions (`0x0104` and `0x0200`) with `"unsupported animation container version 0x... (implemented: 0x0100, 0x0101, 0x0102, 0x0103)"`.
- **`RANMobile/Tools/inspect_ep7_animation_containers.cpp`:**
  - Synchronized the inspector's production-reader cursor trace with B14 `EP7AnimationReader` (`0x0100`–`0x0103` outer + inner) so `acceptedTrailingBytes` audits exact-EOF consumption across all `3,724` parsed authentic files.
- **Regression tests (`test_phase3j.cpp`, `test_ep7_character_verifier.cpp`, `test_ep7_animation_inspector.cpp`):**
  - Added regression coverage for outer `0x0103` (`upBodyTracks` + `dwGarbageValue` + `mainTracks` + non-formula `dwGarbageValue`), inner track `0x0103` (`rotateKeys`, `dwGarbage1`, `positionKeys`, `quatPosKeys`, `matrixKeys` 80B, `dwGarbage2`, `scaleKeys`, empty `m_strBone`), `0x0103` vs `0x0102` field-order differentiation, `EP7AnimationPlayer::ApplyToSkeleton` evaluation on `0x0103` `upBodyTracks` and `mainTracks`, invalid count/length bounds checks, byte-by-byte prefix truncations (`0 .. size-1`), 8-fixture fuzz sweep, `0x0100`/`0x0101`/`0x0102` non-regression, and explicit `0x0104`/`0x0200` rejection.

### Authentic Validation, Test & Android Build Results ([CI run 45995539967](https://github.com/famoh01/ran-online-ep7/actions/runs/45995539967), [CI run 45995540399](https://github.com/famoh01/ran-online-ep7/actions/runs/45995540399))

| Verification Target | Pre-B14 (B13) | Post-B14 | Status |
|---|---:|---:|---|
| Authentic `0x0103` `data/animation/*.bin` parsed | `0 / 1,533` | **`1,533 / 1,533` (`100.00%`, `114,210` tracks)** | **VERIFIED (`EXACT_EOF`)** |
| Authentic `0x0100` + `0x0101` + `0x0102` `data/animation/*.bin` parsed | `2,191 / 2,191` | **`2,191 / 2,191` (`140,293` tracks)** | **VERIFIED (`EXACT_EOF`)** |
| Total `data/animation/*.bin` parsed (`EXACT_EOF`, `0` trailing bytes) | `2,191 / 6,228` | **`3,724 / 6,228` (`+1,533`, `254,503` tracks)** | **VERIFIED** |
| `data/animation/*.bin` `parser-rejected` (FAIL) | `0` | **`0`** | **CLEAN** |
| `data/animation/*.bin` `unsupported-version` (`0x0104`: `2,495`, `0x0200`: `9`) | `4,037` | **`2,504` (`-1,533`, out of B14 scope)** | **TRACKED** |
| Phase 3J host tests (`test_phase3j.cpp`, `-O2` & `ASan+UBSan+LSan`) | `952 / 0` | **`1,499 checks, 0 failures`** | **PASS** |
| Verifier regression (`run_ep7_verifier_tests.sh`, `-O2` & `ASan+UBSan+LSan`) | `1,571 / 0` | **`2,784 checks, 0 failures`** | **PASS** |
| Inspector regression (`run_ep7_animation_inspector_tests.sh`, `-O2` & `ASan+UBSan+LSan`) | `7,035 / 0` | **`8,303 checks, 0 failures`** | **PASS** |
| CI `Core C++17 tests (blocking)` (`45995539967`, job `140493888315`) | pass | **pass** | **PASS** |
| CI `Android ARM64 build (lib + APK/AAB)` (`45995539967`, job `140493888339`) | pass | **`libranmobile-arm64-v8a` (`2,303,142` B), `ranmobile-android-packages` (`1,989,193` B)** | **PASS** |

*Explicit non-claims:* Parsing `3,724` authentic `.bin` containers (`1,533 / 1,533` `0x0103` files; `161` of those `0x0103` files already pair with supported `.cfg` `0x0107` while `1,349` pair with unimplemented `.cfg` `0x0108`–`0x0114`) proves binary container/track compatibility, not on-device rendering or live gameplay. Outer `.bin` versions `0x0104` (`2,495` files) and `0x0200` (`9` files) remain unimplemented per B14 scope, and no Android emulator/device or live server test was executed in this sandbox.


## 2026-11-27 — Phase 3J B15: Implement Authentic Animation Container `0x0104` (`arena/00fd13f9-ran-online-ep7`)

Implemented authentic `SAnimContainer` outer version `0x0104` on `arena/00fd13f9-ran-online-ep7` (commit `263ef9cf7ae0e355b1363c0a615415ecaac291a6`, stacked Draft PR #7 -> `arena/42c5ac0c-ran-online-ep7`; Draft PR #5 untouched).

### What Changed in B15

- **`RANMobile/Data/Importer/EP7Animation.cpp`:**
  - Implemented authentic outer `SAnimContainer::Load_0104` (`hdr.version == 0x0104` in `ParseContainer` per `Lib_Engine/Meshs/SAnimationSaveLoad.cpp:444-479, 670-699`): `mainTracks` (`u32 numMain` + tracks), `upBodyTracks` list 1 (`u32 numUpBody1` + tracks), and `upBodyTracks` list 2 (`u32 numUpBody2` + tracks appended in order to `clip.upBodyTracks` via `ReadTrackList(..., /*append=*/true)` with overflow-safe `count <= kMaxBoneTracks - out.size()` guard), reusing the verified `ReadBoneTrack` implementation (`0x0100`–`0x0103`).
  - Preserved `0x0100`, `0x0101`, `0x0102`, and `0x0103` behavior unchanged, and preserved explicit rejection of unimplemented `0x0200` (`EMBYTECRYPT_BIN`) with `"unsupported animation container version 0x00000200 (implemented: 0x0100, 0x0101, 0x0102, 0x0103, 0x0104)"`.
- **`RANMobile/Tools/inspect_ep7_animation_containers.cpp`:**
  - Synchronized the inspector's production-reader cursor trace with B15 `EP7AnimationReader` (`0x0100`–`0x0104` outer + `0x0100`–`0x0103` inner) so `acceptedTrailingBytes` directly verifies exact-EOF consumption across all `6,219` plaintext authentic `.bin` files.
- **Regression tests (`test_phase3j.cpp`, `test_ep7_character_verifier.cpp`, `test_ep7_animation_inspector.cpp`):**
  - Added dedicated regression coverage for `0x0104` list ordering (`main`, `upBody1`, `upBody2` appended in order), all 8 empty/populated `(main, upBody1, upBody2)` combinations, `EP7AnimationPlayer::ApplyToSkeleton` simultaneous pose evaluation across `main` + `upBody1` + `upBody2`, combined `upBody1 + upBody2` count boundaries (`2048 + 2048 == 4096` pass vs `2048 + 2049 == 4097` fail), malformed per-list counts (`4097`, `0xFFFFFFFF`), byte-by-byte strict prefix truncations (`0 .. size-1`), exact EOF + trailing data handling, 9-fixture fuzz sweep, `0x0100`–`0x0103` regressions, and explicit `0x0200` rejection.

### Authentic Validation, Test & Android Build Results ([CI run 45999738432](https://github.com/famoh01/ran-online-ep7/actions/runs/45999738432), [CI run 45999738400](https://github.com/famoh01/ran-online-ep7/actions/runs/45999738400))

| Verification Target | Pre-B15 (B14) | Post-B15 | Status |
|---|---:|---:|---|
| Authentic `0x0104` `data/animation/*.bin` parsed | `0 / 2,495` | **`2,495 / 2,495` (`100.00%`, `129,379` tracks)** | **VERIFIED (`EXACT_EOF`)** |
| Authentic `0x0100`–`0x0103` `data/animation/*.bin` parsed | `3,724 / 3,724` | **`3,724 / 3,724` (`254,503` tracks)** | **VERIFIED (`EXACT_EOF`)** |
| Total `data/animation/*.bin` parsed (`EXACT_EOF`, `0` trailing bytes) | `3,724 / 6,228` | **`6,219 / 6,228` (`+2,495`, `383,882` tracks; `100%` of plaintext `.bin`)** | **VERIFIED** |
| `data/animation/*.bin` `parser-rejected` (FAIL) | `0` | **`0`** | **CLEAN** |
| `data/animation/*.bin` `unsupported-version` (`0x0200` `EMBYTECRYPT_BIN`) | `2,504` | **`9` (`-2,495`, out of B15 scope)** | **TRACKED** |
| Phase 3J host tests (`test_phase3j.cpp`, `-O2` & `ASan+UBSan+LSan`) | `1,499 / 0` | **`2,084 checks, 0 failures`** | **PASS** |
| Verifier regression (`run_ep7_verifier_tests.sh`, `-O2` & `ASan+UBSan+LSan`) | `2,784 / 0` | **`3,755 checks, 0 failures`** | **PASS** |
| Inspector regression (`run_ep7_animation_inspector_tests.sh`, `-O2` & `ASan+UBSan+LSan`) | `8,303 / 0` | **`9,702 checks, 0 failures`** | **PASS** |
| CI `Core C++17 tests (blocking)` (`45999738432`, job `140508021895`) | pass | **pass** | **PASS** |
| CI `Android ARM64 build (lib + APK/AAB)` (`45999738432`, job `140508021781`) | pass | **`libranmobile-arm64-v8a` (`2,304,051` B), `ranmobile-android-packages` (`1,989,679` B)** | **PASS** |

*Explicit non-claims:* Parsing `6,219 / 6,228` authentic `.bin` containers (`2,495 / 2,495` `0x0104` files; `21` of those `0x0104` files pair with supported `.cfg` `0x0104`–`0x0107`, `2,464` pair with unsupported `.cfg` `0x0110`–`0x0115` which remain explicitly classified `UNSUPPORTED`, and `10` have no `.cfg` stem) proves binary container/track compatibility, not on-device rendering or live gameplay. Outer `.bin` version `0x0200` (`9` `EMBYTECRYPT_BIN` files) and `.cfg` `0x0108`–`0x0115`/`0x0200` remain unimplemented per B15 scope, and no Android emulator/device or live server test was executed in this sandbox.




