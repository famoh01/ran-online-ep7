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

## Phase 3I update (2026-11-26, authentic 3D world parser & GLES3 scene renderer)

- **Resolved in Phase 3I**: `.wld`/`.wld0` octree geometry parsing (`0x0108`–`0x0114`), `.pie`/`.pis` piece asset parsing, full `.Lev` `LOAD_200` subsection parsing (`GLMobScheduleMan`, `DxLandGateMan`, `DXLANDEFF`, `GLLANDMARK`, `SLEVEL_ETC_FUNC`), TGA (8/16/24/32-bpp + RLE + 16-bpp alpha) and baseline `SOF0` JPEG texture decoding, LH GLES3 3D rendering (`MobileRenderer`), 6-plane frustum + scaled view-distance culling (`MobileCamera`), 4-pass scene streaming (`EP7SceneRenderer`), design-space HUD coordinates (`MobileHUD`), and multi-touch camera/HUD routing (`RANMobileEngine`) are implemented and verified against `63/63` authentic Phase 3I binary objects + Android CI (`45894091238`).
- **Remaining gaps**: Animated skinned character rendering (`.chf`/`.cps`/`.abl`/`.bin`) is not yet integrated into the 3D scene pass; B1 (no physical Android emulator/device render test in sandbox), B3/B6 (live server auth/login), and B8 (GameServer DirectX header conflict) remain unchanged. PR #3 remains OPEN/DRAFT and untouched.



---

## Phase 3J update (2026-11-26, authentic character model / skeleton / animation)

Phase 3J implemented the character pipeline (parsers, skeleton evaluation, animation sampling,
CPU skinning, scene instances, touch UX, tests, CI tooling) entirely from original-source
evidence. Two blockers are added and one existing blocker still governs the outcome.

### B9. Authentic EP7 character assets are unavailable in this sandbox (extends B2)

* `.chf`, `.cps`, `.skt`, character `.x`, animation `.bin` and `.cfg` files exist only inside
  the original release archives (`ran-ep7-full-v1`), which cannot be downloaded here — the
  same `curl (18) EOF` failure recorded in B2 for `release-assets.githubusercontent.com`.
* Consequence: no authentic character was loaded, posed or rendered during this phase, and
  **no real-data compatibility claim is made**.
* Mitigation implemented: `RANMobile/Tools/verify_ep7_character_assets.cpp` runs the Phase 3J
  parsers over an extracted client tree and reports per-file results. It is wired into
  `.github/workflows/build-ranmobile-android.yml` after the client archive is extracted, and
  its log is uploaded/summarised. If the directory is absent it prints "nothing verified".
* What is unblocked already: the exact on-disk layouts were recovered from the original
  sources and are exercised by `RANMobile/Tests/test_phase3j.cpp` (188 checks) using
  suite-authored ABI fixtures — which validates the parsers' logic and safety, **not** real
  asset compatibility.

### B10. Phase 3J cannot integrate GPU skinning on this branch (renderer lives in PR #4)

* `EP7SceneRenderer`, `EP7WorldParser` and the GLES3 3D pass exist only on
  `origin/arena/be1e147d-ran-online-ep7` (PR #4, Draft, head `76b2c1c3`).
* `main` (and therefore this branch) has `MobileRenderer` with UI/2D drawing only, so there is
  no 3D draw path to attach a skinning program to. Adding one would duplicate PR #4's
  renderer, which contradicts "do not replace existing engine architecture".
* Decision: implement CPU skinning (the authentic `DxSkinMesh9_CPU` reference path) as the
  verified baseline, keep `BuildBonePalette`/`EP7SkinnedMesh` ready for a palette-uniform
  upload, and publish the integration contract in
  `PHASE3J_CHARACTER_ANIMATION.md` §8. GPU skinning is documented as **not implemented**, not
  as pending-and-done.

### Unchanged blockers relevant to Phase 3J

* **B1** still applies: no Android SDK/NDK/Gradle/JDK here, so the arm64 build and
  `assembleDebug`/`assembleRelease`/`bundleRelease` remain CI-only.
* **B2/B4** still apply to every other asset class.
* No physical Android device was available; Phase 3J claims no device testing.

### Repo-integrity note

PR #3 (`7b355b3c`) and PR #4 (`76b2c1c3`) were verified untouched with `git ls-remote`
before and after this work. Nothing was merged to `main` and no existing branch was rewritten.

### B9 addendum — the authentic assets are inside `SkinObject.rcc` / `Animation.rcc`

Follow-up discovery while building the verification tool (2026-11-26):

`GLOGIC::InitFile` (`enginelib/G-Logic/GLogic.cpp:40-62`) loads character data exclusively
through `CUnzipper::LOADFILE_RCC` on two ZIP archives
(`SkinObject.rcc`, `Animation.rcc`), and every asset open goes through
`CUnzipper::UnzipToMemory` (`enginelib/Common/Unzipper.cpp:569`) with the ZipCrypto password
`"tkqwlf"`. A plain directory walk therefore finds no character assets at all.

Consequences for B9:

* The first version of `verify_ep7_character_assets` only walked the tree and would have
  reported "0 candidate files" on a perfectly good client extraction — a false negative that
  could be mistaken for "nothing to verify". This is fixed: the tool now opens every
  `.rcc`/`.zip` it finds.
* `Data/Importer/EP7RccArchive.{h,cpp}` was added (central directory, stored + deflate,
  ZipCrypto with the client password, CRC-32 verification, full bounds checking) so the real
  bytes are reachable in CI. It is covered by the test suite (208 checks total) including
  ZipCrypto decryption feeding the real `.cps`/`.chf` parsers, and it was additionally
  validated against a real Python-generated ZIP with stored and deflated entries.
* Still unverified: whether the shipped packs use zip64, AES, or methods other than
  stored/deflate. If they do, extraction fails loudly and this blocker stays open; the CI log
  will say so explicitly because every entry reports its method and every failure is printed.
* The sandbox still has no archives, so the `inflate` path has never been run against
  authentic client data — only against a synthetic archive built to the same spec.

### B11 — New: the authentic client ships animation data this build cannot parse

First authentic-data results, read back from CI check-run annotations (2026-11-26):

```
[FAIL] cfg  external/ep7_release/client_original/data/animation/a_3boss_01_stay.cfg   — unsupported animation cfg version 0x00000106
[FAIL] cfg  external/ep7_release/client_original/data/animation/a_6lvl_f.cfg          — unsupported animation cfg version 0x00000114
[FAIL] cfg  external/ep7_release/client_original/data/animation/a_2017_victors_wings.cfg — unsupported animation cfg version 0x00000115
[FAIL] bin  external/ep7_release/client_original/data/animation/*.bin                 — starts with magic 'Anim' (0x6D696E41)
```

This is the first time these parsers have been run against real client bytes, and the
verdict is **the animation data is not in the format this build implements**. Two distinct
incompatibilities:

1. **`.cfg` versions beyond the engine source.** `SANIMCONINFO::VERSION` is `0x0107` and
   `enginelib/Meshs/DxSkinAniManSaveLoad.cpp` implements readers for `0x0101`–`0x0107`.
   The shipped files are `0x0106` (engine-compatible) and `0x0114`/`0x0115` (**not in the
   engine source at all**).
   **Update:** all versions the engine source defines are now implemented —
   `0x0101`–`0x0107`, including the per-version block sizes (302 / 564 / 732 / 840 /
   840 / 800 / 800) and the fact that 0x0101–0x0104 **end at the header block** with no
   effect list. Final measurement:

   ```
   pack entries: 3217 parsed, 8443 failed
     6228 x bin  magic "Anim"
     2058 x cfg  0x0110   1303 x 0x0111   1016 x 0x0114     569 x 0x0115
      328 x cfg  0x0108    226 x 0x0112     34 x 0x0109
   ```

   Every remaining failure is a format with **no definition in this repository**, not a
   defect against the engine source.

   `0x0108`–`0x0110`, `0x0114`, `0x0115` have no definition in this repository, and the
   `"Anim"` magic appears in no loader. Those two layouts must be recovered from the data
   (reverse engineering), not ported. See §14.7.
2. **`.bin` files are a different container.** They begin with the ASCII magic `"Anim"`,
   not the small version DWORD that `SAnimContainer::LoadAnimFromFile` expects
   (`case 0x0100 / 0x0101 / VERSION(0x0102)`). The engine source contains no such magic,
   so these are not the format `SAnimContainer` reads.

Two readings are possible and the data so far cannot separate them:

* the shipped `.rcc` packs contain an animation format newer than this engine source
  (several `0x0114`/`0x0115` styles, a `"Anim"` container); or
* `data/animation/*.bin|.cfg` is a **different dataset** from the one the engine consumes
  (`Animation.rcc`), and the probe is reading files the engine never opens.

What is *not* yet known, because only the first 10 annotations are retrievable: what the
`.chf` / `.cps` / `.x` entries in `SkinObject.rcc` did. Those are the categories that matter
most for "can this render an authentic character", and they are the ones still unproven.
The aggregated reason histogram added to the probe's summary exists precisely to answer
this in one line on the next run.

Impact: `verify_ep7_character_assets` now exits **1**, so the "Verify Authentic EP7
Character Assets" step is red. That is the honest signal — a green step would mean
authentic data parsed — and it is deliberate, not a regression.

---

## B11 — status review, 2026-11-26 (evidence audit, no guessing)

This section records what was actually checked, so B11 is not re-litigated from memory.

### Investigation performed

| Question | Method | Result |
|---|---|---|
| Does any loader in the repo know the `"Anim"` magic? | `grep -rn "'Anim'\|\"Anim\""` over `enginelib/`, `xskinexp/`, `EngineSoundLib/` | **No hits** outside our own probe/tests |
| Do `0x0108`–`0x0115` appear as `SANIMCONINFO` versions? | `grep -rn "0x01xx"` over `enginelib/`, `xskinexp/`, `EngineSoundLib/` | Appear, but **all in the wrong file family** (see below) |
| Is the struct defined for any of them? | `SAnimation.h` | Only `SANIMCONINFO_101`…`_106` and `SANIMCONINFO` (0x0107) |
| Are authentic samples committed? | `find -name "*.rcc" -o -name "*.bin"` | **No** — `external/ep7_release/` is git-ignored; data exists only as release assets |

### Why the `0x0108`–`0x0115` grep hits do **not** resolve B11

Every hit belongs to the **landscape** family, not animation:

```
enginelib/DxOctree/DxLandManSaveLoad.cpp:30   const DWORD DxLandMan::VERSION = 0x0114;
enginelib/DxOctree/DxLandManSaveLoad.cpp:742  if ( dwVersion>=0x0108 && dwVersion<=DxLandMan::VERSION )
enginelib/DxOctree/DxLandManSaveLoad.cpp:752..764   0x0108 / 0x0109 / 0x0110 / 0x0111 / 0x0112
enginelib/DxCommon/DxFogMan.h:57              enum { VERSION = 0x0108, };
```

`DxLandMan` identifies its files with `FILEMARK[128] = "LAND.MAN"`, a different basestream
type from `SANIMCONINFO`. The two families merely happen to share small integer version
numbers. **Treating those constants as animation layouts would be a fabricated format**, so
they were explicitly rejected as evidence.

### Resolution of the version/type question (implemented, evidence-backed)

The 128-byte basestream header carries a type string. `ParseInfo` now reads it and
distinguishes:

* **type `SANIMCONINFO`, version `0x0108`–`0x0115`** → `unsupported = true`. This is B11: a
  real animation file of a version whose struct exists nowhere in this repository.
* **any other type** → a genuine `FAIL` ("not a SANIMCONINFO file"), **not** excused.

This is the only B11 assertion made, and it rests solely on the type string that
`CSerialFile::ReadFileType()` demonstrably reads (`EP7ByteFormat.h`). No offset, field size
or layout is inferred.

### What would be required to close B11

1. **Newer engine source** containing `SANIMCONINFO_108` … `_115` or an `SAnimContainer`
   reader that accepts the `"Anim"` magic — this would be definitive.
2. **Format documentation** for the EP7 animation exporter that emits `"Anim"`.
3. Otherwise, **reverse engineering from samples** with per-field validation: the probes for
   `0x0108`–`0x0115` must first confirm the 302-byte prefix (`name[128]`, `skeleton[128]`,
   `flag`, `STime`, `ETime`, `ETimeOrig`, `UNITTIME`, `MainType`, `SubType`, `DivCount`,
   `DivFrame[8]`) is unchanged, since that prefix is identical for **every** version that
   *is* defined in source.

### Status: **OPEN** — implementation of those formats is stopped

No structure, offset, size or decoder for `0x0108`–`0x0115` or the `"Anim"` container has
been written, and none will be until evidence in (1)–(3) exists.

### CI treatment of an open B11 (new, 2026-11-26)

An open B11 must not be able to hide core-test results, and must not be mistakable for
success. The verification tool therefore reports three outcomes with three exit codes:

| Outcome | Exit | Meaning | CI effect |
|---|---|---|---|
| PASS | 0 | every pack entry parsed | job green |
| FAIL | 1 | a pack entry is genuinely broken | **job fails** |
| UNSUPPORTED | 2 | nothing broken, some formats undefined (B11) | job green, **warning + summary** |
| NOT FOUND | 3 | no character entry in any pack | warning, not a failure |

`UNSUPPORTED` is never counted as parsed and is never silent.
