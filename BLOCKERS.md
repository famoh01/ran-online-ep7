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
