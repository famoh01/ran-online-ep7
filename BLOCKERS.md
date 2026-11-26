# BLOCKERS.md — RAN ONLINE EP7 PC → MOBILE (Phase 1)
Per the master command's iron rules: when a blocker is found, record it here, state exactly what is needed, and stop for the user's next instruction. **Work is paused after Phase 1; Phases 2–6 are not started.** Each item below was re-verified in this session (2026-11-26), not only copied from older reports.

## B1. Android SDK / NDK / Gradle / JDK not installed in this sandbox — local APK build impossible

- Evidence: `ANDROID_HOME` and `ANDROID_SDK_ROOT` unset; `java`, `gradle`, `cmake`, `ninja`, `adb`, `ndk-build` not found in `PATH`; no `/opt/android*`, `~/Android*` directories.
- Impact: cannot compile `RANMobile/` for `arm64-v8a`, run on a device/emulator, or validate the renderer/touch path locally. GitHub Actions runners have built `app-debug.apk` before (`apk-latest`, 1,510,336 B), so CI compilation remains possible.
- **Needed from user**: either (a) install Android SDK + NDK r25+ + JDK 17 + Gradle 8.x in this environment, or (b) explicitly authorize continuing with CI-only builds (workflow edits + native Linux tests) and accept that device verification stays pending.

## B2. Original client binary assets cannot be downloaded here — asset pipeline cannot be fully validated

- Evidence:
  - GitHub Release assets: `gh release view ran-ep7-full-v1` lists the files, but the redirect target `release-assets.githubusercontent.com:443` fails with SSL_ERROR_SYSCALL in this sandbox (host not in outbound allowlist).
  - Google Drive: `RAN-EP7-Client-TH.00.zip` = 1,572,864,000 B and `RAN-EP7-Client-TH.001.zip` = 1,010,562,339 B exceed the Drive connector's 100 MB inline download cap; no streaming download tool is available. `RAN-EP7-ServerDBTools.zip` (49,791,515 B) was previously reported as failing retrieval on retry.
  - Backup repo `matheomedrana/Ran-Online-EP7-Complete-Files`: tree is available (60,457 paths, including `data/gui`, `.wld`, `.dds`, `.cps`, `.qst`, `.ntk`), but all binary files are Git LFS pointers (~130 B). The LFS batch API returned HTTP 403 (`Resource not accessible by integration`) and `media.githubusercontent.com` is unreachable.
  - `external/ep7_release/` is empty/absent in this checkout; only Thai text was imported in earlier work.
- Impact: `.rcc`, `data/gui`, maps, meshes, textures, animation data cannot be converted/screenshotted here. UI redesign (Phase 5) may use existing original art only if the art is made available; generating new logos/art is forbidden.
- **Needed from user**: provide the original archives locally (e.g. place `RAN-EP7-Client-TH.zip.part00/01` and `RAN-EP7-ServerDBTools.zip` into `external/ep7_release/original/`), or grant an environment/network path that can download them and resolve the backup repo's Git LFS objects. If the user prefers, confirm explicitly that asset-dependent steps should stay as documented CI steps only.

## B3. No live EP7 server / MSSQL database for end-to-end testing

- Evidence: no running Login/Session/Agent/Field service or MSSQL instance in this Linux sandbox; original server code is Win32/Winsock/ODBC and cannot run here. `LIVE_SERVER_TEST_GUIDE.md` documents the required setup; `PROJECT_STATE.md` marks `REAL SERVER TESTED: NO`.
- Impact: authentication, character select and gameplay packets cannot be verified against the real PC server; only protocol-constant review and local unit tests are possible.
- **Needed from user**: a reachable EP7 server + MSSQL instance (host/port/credentials supplied by the user through a secure channel, never committed) and permission to test, or explicit acceptance that mobile↔PC compatibility remains unverified until such a server is available.

## B4. Database backups / server configs unavailable as bytes (sub-case of B2)

- Evidence: backup repo tree contains `Ran Online Database/Ran Online Database Backup Files (.bak)/Ran*.bak`, `... SQL Script Files/Ran*.sql`, and `cfg/ServerLogin.cfg`, `ServerAgent.cfg`, `ServerField.cfg`, `ServerSession.cfg` — but every one is an LFS pointer; the Drive `RAN-EP7-ServerDBTools.zip` bytes are not retrievable here.
- Impact: Phase 4 (server/GM compatibility) cannot validate Auth/DB/GM tools against the real schema and configs.
- **Needed from user**: the actual DB scripts/backups and server cfg files (uploaded to the workspace), or confirmation to document requirements from source code only.

## B5. Raw CI logs and artifact bytes cannot be downloaded here (added in Phase 2)

- Evidence: `gh run view 45809902810 --log` fails with EOF on `results-receiver.actions.githubusercontent.com:443`; GitHub API does provide job/step conclusions and artifact names/sizes (`ranmobile-android-packages` 1,585,823 B; `libranmobile-arm64-v8a` 1,343,383 B; test log 496 B).
- Impact: CI success/failure and artifact existence are verified, but the actual test output, compiler warnings and artifact contents cannot be inspected in this sandbox. The claim "tests passed" rests on step conclusions and the test source's `ALL TESTS PASSED!` path, not on raw logs.
- **Needed from user**: access to the log/artifact hosts, or paste/export of the run summary + `test_output.log` from a machine with access. If unavailable, accept API-level build status as the evidence ceiling.

## Phase 2 re-verification note

2026-11-26: B1–B4 were re-confirmed directly (Android env vars/tools absent; release asset host SSL failure; Drive archive sizes over connector cap; backup repo files are LFS pointers with batch API HTTP 403). No new access attempts were made after these failures, per instruction. The functional gaps found in code review (`NetworkClient::Connect()` never called, no character-select screen, no IME Thai text path, no world asset pipeline) are documented in `PROGRESS_REVIEW.md` as work for the next phase; they are not access blockers.

## Decision requested

Please tell me which blockers to clear and how (provide files/toolchain/server access, or explicitly authorize CI-only/documentation-only continuation for the blocked parts). I will then resume at Phase 3 in small Draft PRs, one phase at a time.

## B6. Protocol constant conflict discovered during source recovery (critical, added in pre-Phase 3)

- Evidence: `RANMobile/Engine/Network/EP7Protocol.h` sets `EP7_NET_MSG_LOGIN_2 = 0 + 1013 + 107 = 1120`, taken from a **commented-out** lobby block in `netclientLib/s_NetGlobal.h`. The active source defines `NET_MSG_LOBBY = NET_MSG_BASE + 950` (region base e.g. `TH_PARAM = 989`, default `995`); recovered `Ran-Online-Episode-7-main/Dependency/NetGlobal/s_NetGlobal.h` uses base `888` with the same `+950`. Real `NET_MSG_LOGIN_2` is therefore `2046` (TH), `2052` (default) or `1945` (older recovered source), not `1120`.
- Impact: even after network connect is implemented, the login packet ID may be rejected by the real PC server. No live server is available to disambiguate.
- **Needed from user**: the authoritative build flag/server edition for "FamHawaii Ep7" (or a live server capture), plus explicit permission to correct `EP7Protocol.h` in a later phase. Per task rules the constant was **not** changed in this recovery task.

## Recovery-task status (pre-Phase 3, 2026-11-26)

- Downloaded and integrity-verified: `RanOnlineEP7-Client.zip`, `Ran-Online-Episode-7-main.zip`, `RAN-EP7-ThaiPack-FamHawaii.zip` (see `SOURCE_DATA_RECOVERY.md` for SHA-256).
- Connector failures (not retried): `RanOnlineEP7-Server.zip`, `RAN-EP7-ServerDBTools.zip` — user must provide these bytes if server configs/DB are needed.
- No game content was fabricated; no PC source/protocol/UI was modified.

## B7. Strict Windows build verification still running (added in recovery task)

- Run `45819356699` (workflow `Strict Build Verification`, commit `0680b8e`), Windows job `139907264786`, started 2026-11-26T12:24:32Z. At report time (~26 min later) the MSBuild step was still in progress; no project exit codes or output-binary checks had been produced.
- Impact: Windows client/server compilation is **unverified** in this task. The green Android/Linux jobs in the same run are separately verified and must not be treated as Windows proof.
- **Needed**: re-check that run (or wait for its completion) before Phase 3; the workflow has no `continue-on-error`, so a failing project will fail the job and upload `build_logs/results.txt`.

## Recovery progress update (continuation task)

- B2/B4 **partially cleared**: `RAN-EP7-ServerDBTools.zip` was recovered with an alternate authorized connector input; 4 `.sql` scripts, 4 `.bak` backups, 4 non-empty `Server*.cfg` files and 15 editor/GM tools are now present locally (`external/ep7_release/serverdb_original/`, git-ignored). The >1 GB client archives and the 19 MB `RanOnlineEP7-Server.zip` remain unavailable.
- B6 **refined, not yet fixed**: project files define `TH_PARAM` for Thailand builds (`NET_MSG_BASE=989`, active lobby `+950`), so the correct `LOGIN_2` is 2046 (not 1120). No code change was made in this recovery task; Phase 3 must correct the mobile constant and re-verify against the server.
- B7 update: first strict Windows run `45819356699` **failed** at the MSBuild step; second run `45828309380` was dispatched with error annotations enabled.

## B8. GameServer cannot compile with modern VS2022 headers (added in continuation task)

- Exact errors (CI run `45841659289`, annotations): `LegacyDX_Include\D3d8types.h(160/194/201,14): error C2011` — `_D3DLIGHTTYPE`, `_D3DSHADEMODE`, `_D3DFILLMODE` enum redefinitions.
- Cause: original `GameServer/stdafx.h` includes both `<d3d9.h>` and `<D3d8types.h>`/D3DX8; modern Windows SDK defines these enums. `GameServer.vcxproj` also uses legacy numeric `<PrecompiledHeader>3</PrecompiledHeader>` (MSB6011) — handled only in a CI-generated project copy, not in committed source.
- Status: Login/Session/Agent/Field servers and `Smtm2022.sln` build with exit 0; GameServer build is stopped and **unverified**. Needs either the original DirectX 8 SDK/header set or explicit permission for a source-level build-compatibility change (headers/PCH only, no gameplay logic).

## Phase 3A audit update (2026-11-26, audit-only)

- `PHASE3A_PROTOCOL_AUDIT.md` is the detailed login/protocol report. B6 remains **open**: `2046` is proven only as the compile-time `NET_MSG_LOGIN_2` for a `TH_PARAM` build (`989 + 950 + 107` from `netclientLib/s_NetGlobal.h:629–630,654–657,723–724`); it is **not** proven as the ID accepted by the live/FamHawaii server.
- Additional ambiguity found: a `TH_PARAM` client UI routes to `ThaiSndLogin` (`RanClientUILib/Interface/LoginPage.cpp:225–246`, `Basic/Basic.cpp:71–73`), whose packet is `THAI_NET_MSG_LOGIN = 989 + 950 + 140 = 2079` with a different 56-byte TEA-encrypted payload (`netclientLib/s_NetGlobal.h:2260–2276`, `netclientLib/s_NetClientMsgLogin.cpp:282–311`). Which of 2046/2079 the deployed server expects is unresolved.
- `RANMobile` remains incompatible on both ID (1120, from the commented-out `+1013` lobby block) and payload (74-byte packed plaintext vs original 76-byte natural-alignment 4-field TEA payload); no mobile handshake/response handling exists. No protocol file or test was changed in this audit phase.
- Recovered template CFG files contain `service_provider 3` (`SP_CHINA`) and come from a source lineage described as different from the Thai client package; they cannot establish the live build flags.
- **Needed from user**: authoritative deployed server build configuration/edition or a real login-packet capture, plus explicit permission to change the mobile protocol/constants/handshake in a separate later phase. Per task rules, no such change was made and no live handshake was claimed.
- Non-strict CI compile check `build.yml` `Release_Thailand|Win32` run `45850495046` was in progress at report time. This is compile-only evidence; B8 (GameServer C2011 DirectX header conflict) remains unchanged and un-bypassed.

## Phase 3B update (2026-11-26, implementation phase)

- B1 unchanged: no local JDK/Gradle/Android SDK/NDK, so Android ARM64/APK/AAB verification must run in GitHub Actions. Local g++ native tests are the only sandbox-side execution.
- B6 unchanged and deliberately not addressed: `LoginScreen` now reports `AwaitingServerResponse` / `SERVER NOT CONNECTED` accurately and never sets `LoginAccepted` from a local action. No protocol constant, payload or handshake was changed.
- New documented limitation (not a fabricated substitute): `EP7AssetPipeline` counts and registers source-format assets but does **not** decode DirectX formats (`.x`, `.mxf`, `.egp`, `.mtf`, `.wld`, …) for rendering. `MobileRenderer` remains a UI renderer; no original map/character scene is claimed.
- New documented limitation: Android `DisplayCutout`/safe-area values are not queried from Java; `LoginScreen::SetSafeAreaInsets()` exists but defaults to zero. Soft-keyboard IME UTF-8 commit is still not delivered; only hardware key events are mapped in `native_bridge.cpp`.
- New documented limitation: the Android manifest still hard-locks landscape even though the UI now reflows to a portrait virtual canvas; enabling rotation is deferred and must not be mistaken for a verified portrait device layout.
- Verified original PC input mappings (DirectInput `DIK_*` / mouse actions in `RanClientLib/DxLobyStage.cpp` and `RanClientLib/G-Logic/GLCharacter.cpp`) are documented as **unimplemented** mobile actions; no gameplay command or packet was invented.
- B8 untouched: no GameServer/DirectX compatibility change was attempted.

## Phase 3C update (2026-11-26, source-grounded asset/scene work)

- `.wld` geometry unavailable: `GLAGLandMan::LoadWldFile` (`RanClientLib/G-Logic/GLAGLandMan.cpp:22-60`) proves a `DxLandMan::FILEMARK`/version/`SLAND_FILEMARK` layout, but no `.wld` file exists in `external/ep7_release/thaipack_original` or the client path inventory. Full map geometry, collisions, gates and entity placement are therefore **not implemented**; only `.glmap` `SLEVEL_HEAD` metadata is parsed.
- No original DDS/MTF texture or `mapslist.mst`/`.glmap` file is present in the recovered trees. Texture decoders were validated against source-derived synthetic fixtures and real source constants, not against original binary texture assets. A real texture/map drop is needed for binary-level verification.
- The recovered 309 `.ntk` and 730 `.qst` files have the original 128-byte `"default"` file-type header but no matching loader path or decoded layout in this checkout. Their payload format is **unknown**; no decoding was guessed.
- Encoded `mapslist.mst` uses Rijndael version headers or `compbyte` fallback (`IniLoader.cpp:239-253`, `StringFile.cpp:71-100,150-180`). Only the plain-text variant is implemented; an encoded original manifest needs a key/version sample to implement safely.
- DDS support is a subset: DXT1/3/5 plus six uncompressed mask formats; mip levels, cube/volume textures and the remaining original D3D formats are verified in source but not implemented.
- Android IME UTF-8 commit is still unimplemented. NDK r26d with `ANDROID_PLATFORM=android-24` has no `AKeyEvent_getUnicodeChar` (proven by CI run `45862660028` compile error); only hardware ASCII key events and focus/keyboard lifecycle are implemented. A Java `InputConnection`/commit callback bridge is required for Thai UTF-8 IME input.
- DisplayCutout insets are queried through JNI (API 28+), but no device test was performed; `ANDROID_PLATFORM=android-24` runs degrade to zero insets. The manifest still locks landscape; portrait virtual-canvas support is implemented but unverified on device.
- B1 (no local Android SDK/NDK/JDK), B6/B8 (protocol/server uncertainty, GameServer DirectX header conflict) remain unchanged and untouched.

## Phase 3D update (2026-11-26, authorized recovery + IME implementation)

- Asset availability blocker **partially audited, not cleared**: three additional authorized Drive archives were recovered and validated, but they contain only `.ntk`/`.qst`/`.txt` data and Windows executables. No DDS/MTF/WLD/GLMAP/manifest asset was found. Scene rendering from original binary assets remains blocked.
- The two 1.57 GB + 1.01 GB client split archives exceed the Google Drive connector's 100 MB cap; the GitHub release-asset host remains unreachable from this sandbox. Their recorded path inventory has no asset-like extensions, but binary-level verification is still unavailable.
- Backup GitHub repository `matheomedrana/Ran-Online-Episode-7-Complete-Files` returns HTTP 404 (verified with `gh repo view` and `gh api`); that recovery route is stopped.
- Android IME is now implemented in Java/Kotlin-side Android classes plus JNI callbacks, but no physical device/emulator test was performed. Compile/unit evidence only; Thai IME composition behaviour on a real keyboard remains unverified.
- Existing blockers B1 (no local Android SDK/NDK/JDK), B6/B8 (protocol/server uncertainty, GameServer DirectX header conflict) remain untouched.

## Phase 3G update (2026-11-26, authentic LFS retrieval)

- Local `git-lfs` cannot be installed: Debian mirrors are unreachable and the sandbox token gets HTTP 403 from GitHub’s LFS batch endpoint; `media.githubusercontent.com` and artifact/log blob hosts fail TLS. Resolved for retrieval only by the standard GitHub-hosted runner workflow `phase3g_focused_lfs_fetch.yml` plus GitHub blob API transfer; local LFS remains unavailable.
- Backup `.gitattributes` is itself an LFS pointer, so `git lfs pull` needs explicit local `.git/info/attributes` for the requested paths. This is local workflow configuration, not a change to the backup repo.
- Authentic `mapslist.mst` is binary `GLMAPS_LIST`/`EMBYTECRYPT_MAPSLIST`; the Android importer accepts only plain INI and therefore reports `encoded mapslist is unsupported`. Authentic `innerzone_01.Lev` uses `SLEVEL_HEAD 0x0102`; the Android parser accepts only `0x0101` and reports `UNSUPPORTED GLLEVEL VERSION`. No fallback decoder is enabled, and no fake plain-text fixture was substituted.
- `innerzone_01.wld`/`.wld0` were identified from verified data but not downloaded; no WLD geometry parser/renderer exists, so no original scene is rendered. The two large Drive client split archives and release-asset host remain unchanged blockers from earlier phases.
- No isolated Phase 3G branch/PR was available in this session; only a new workflow/report and append-only doc sections were added on the current branch. PR #3 remains OPEN/DRAFT; no merge, close, overwrite or force push occurred.

## Phase 3H update (2026-11-26, authentic binary map list & LEV importer)

- **Resolved in Phase 3H**: The `mapslist.mst` (`GLMAPS_LIST 0x0200` / `EMBYTECRYPT_MAPSLIST` / `SMAPNODE_DATA 0x0203`) and `.Lev` (`glmap 0x0200` / `EMBYTECRYPT_LEVEL_NEW` / `SLEVEL_HEAD 0x0102`) decode blocker from Phase 3G is **resolved** with pure C++17 strict no-fallback parsers (`EP7BinaryMapListParser`, `EP7LevelHeaderParser`, `EP7HashVerify`) verified against all 24 authentic Phase 3G objects, local `-Wall -Wextra -Werror` tests, and CI Run `45882553115`.
- **Remaining unparsed `.Lev` subsections**: `LOAD_200` subsections following `SLEVEL_HEAD` (`12,480` bytes in `innerzone_01.Lev` covering `SLEVEL_REQUIRE`, `GLMobScheduleMan`, `DxLandGateMan`, `DXLANDEFF`, `GLLANDMARK`, and `SLEVEL_ETC_FUNC`) remain unparsed because their source headers were not retrieved in Phase 3G; they are explicitly excluded rather than guessed.
- **Remaining scene/geometry gaps**: Only `innerzone_01.Lev`, `mapslist.mst`, `login.wld`, and `loading_032.dds` are present locally; the remaining 103 referenced `.Lev` files and `innerzone_01.wld`/`.wld0` geometry files were not downloaded, and no `.wld` geometry parser or playable 3D scene renderer exists yet.
- **Unchanged blockers**: B1 (no local Android SDK/NDK/JDK or emulator/device execution), B3 (live server auth/login verification), B6/B8 (network protocol uncertainty and GameServer DirectX header conflict), and Android Thai IME hardware/device testing remain unchanged. PR #3 (`arena/075e1725-ran-online-ep7`, `7b355b3`) remains OPEN/DRAFT and untouched.

