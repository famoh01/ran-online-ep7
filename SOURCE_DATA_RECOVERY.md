# SOURCE_DATA_RECOVERY.md — Pre-Phase 3 recovery & real build verification

Task performed on branch `arena/075e1725-ran-online-ep7` (commit `0680b8e` at the time of the strict-build run). No game content, protocol constant, gameplay logic, PC source, Login UI, HUD or menu was changed. All downloaded archives are staged under `google_drive/` and ignored by Git; inspection artifacts are under `external/recovery/` (also ignored).

## 1. Original archives successfully downloaded

All downloads used the authorized `google_drive:download_file` connector (one call per file; no access-control bypass). Sizes match the prior inventory byte-for-byte.

| File (Drive ID) | Expected / actual bytes | SHA-256 | Integrity |
|---|---|---|---|
| `RanOnlineEP7-Client.zip` (`1JzB8JrQkL3plaHipFpEqAUx5_WcDLLuG`) | 22,378,796 / 22,378,796 | `dd573b9ba7ae79ac51d0a897abc6fded3f1e1b11be2d940483cb894d12ef87eb` | `unzip -t`: no errors |
| `Ran-Online-Episode-7-main.zip` (`1_gsE1YqgiWYtfc_Ofr3St4qgFwuRAHnD`) | 6,538,502 / 6,538,502 | `9c11d62d26bfee552911cce108b3af86f5abef819efe3481f8f24c6b558c256c` | `unzip -t`: no errors |
| `RAN-EP7-ThaiPack-FamHawaii.zip` (`1IL8jlmvl5PGGHdaf5nlnoZdBMhTu25Ag`) | 1,359,185 / 1,359,185 | `4a20b17fd4afd2a4d37532ebcccb6a3daf68b56f82118223a2a4ef083882f969` | `unzip -t`: no errors |

Safe-inspection results: 0 unsafe paths (no absolute paths, `..` segments, drive letters or symlink entries), 0 encrypted/flag-bit-1 entries in all three archives. Files were inspected in memory with Python `zipfile`; no unknown binary or script from an archive was executed. Full internal path lists: `external/recovery/*_paths.txt` (7,310 client entries, 2,507 main entries, 1,051 ThaiPack entries including directories).

## 2. Archives that failed

| File | Size | Result |
|---|---|---|
| `RanOnlineEP7-Server.zip` (`1AGnE8GtTUCMQ2KH68M9BxUX2LRo7lAsG`) | 19,230,299 | Connector error: "Could not retrieve the file from the connector. Do not repeat this exact call." Not retried. |
| `RAN-EP7-ServerDBTools.zip` (`1YZdc1OqAe8rpEfBZ6j_kb9YarLZSZtrO`) | 49,791,515 | Same connector error. Not retried. |
| `RAN-EP7-Client-TH.00.zip` / `.001.zip` | 1.57 GB / 1.01 GB | Not attempted per instruction (exceed connector cap; already documented in `BLOCKERS.md` B2). |

## 3. Actual raw source files recovered

- `RanOnlineEP7-Client.zip`: 6,946 files — 1,104 `.cpp`, 1,195 `.h`, 3,551 `.hpp`, 100 `.ipp`, 26 `.c`, 22 `.vcproj`, 57 `.lib`, 13 `.dll`, 3 `.bat`, 2 BugTrap `.cfg`; prefix `ran-online-ep7-client/`; no `.sln`, no game binary data.
- `Ran-Online-Episode-7-main.zip`: 2,404 files — 1,083 `.cpp`, 1,142 `.h`, 23 `.vcxproj` + 23 `.vcxproj.filters`, 1 `RanOnline.sln`, 1 `.bat`-adjacent `.ini`; includes client (`[Client]__Game`, `[Lib]__Engine`, `[Lib]__EngineUI`, `[Lib]__NetClient`), servers (`[Server]__Login`, `[Server]__Session`, `[Server]__Agent`, `[Server]__Field`), tools (`[Tool]__*Editor`) and `Dependency/NetGlobal`.
- These are genuine original sources; no files were reconstructed or fabricated.

## 4. Actual original game data recovered

- ThaiPack: 1,046 files extracted to `external/ep7_release/thaipack_original/` — 309 `.ntk`, 730 `.qst`, 7 Thai `.txt` (`README_TH.txt`, `gameintext_th.txt`, `gameextext_th.txt`, `gameword_th.txt`). Counts verified locally and in CI (`SOURCE_DATA_RECOVERY` test step asserts 309/730).
- No `.dds`, `.mtf`, `.wld`, `.chf`, `.cps`, `.rcc`, maps, meshes or textures were found in the three recovered archives.

## 5. Database files recovered

None. No `.sql`, `.mdf`, `.ldf` or real `.bak` found in the recovered archives (the only `.bak` is `Ran-Online-Episode-7-main/[Client]__BugTrap/[Client]__BugTrap.vcxproj.bak`, a Visual Studio backup of a project file). Backup-repo DB files remain LFS pointers.

## 6. Server configuration files recovered

None of `ServerLogin.cfg`, `ServerSession.cfg`, `ServerAgent.cfg`, `ServerField.cfg`, `Filter.cfg`, `IPFilter.cfg`. The only `.cfg` files are `BugTrap/BugTrap.cfg` and `BugTrap/CompleteBugTrap.cfg` in the client archive, plus one `.ini`. Server `.cfg` bytes remain unavailable (backup repo LFS, failed ServerDBTools download).

## 7. Missing original files

- `RanOnlineEP7-Server.zip`, `RAN-EP7-ServerDBTools.zip` (connector failure — user must supply bytes).
- `RAN-EP7-Client-TH.zip.part00/01` (1.57 GB + 1.01 GB; need local upload or unrestricted network).
- Original maps/meshes/textures/animation/`.rcc` archives, DB `.bak/.sql/.mdf`, server `.cfg` files (LFS pointers / blocked hosts).
- No replacement content was created for any of these.

## 8. Original source comparison (no replacements made)

- Client archive vs repository, exact relative paths: **6,358 identical SHA-256**, **84 different**. Full differing-path manifest: `external/recovery/client_zip_vs_repo_diff.txt` (examples: `MinWebLauncher/s_CHttpPatch.cpp`, `RanClientLib/ClientApex.cpp`, `=ZLIB/zconf.h`). These are genuine local variants; nothing was overwritten.
- `Ran-Online-Episode-7-main/Dependency/NetGlobal/s_NetGlobal.h` (recovered) vs `netclientLib/s_NetGlobal.h` (repo) are different revisions: recovered `NET_MSG_BASE = 888`, active `NET_MSG_LOBBY = BASE + 950`; repo uses region-dependent `NET_MSG_BASE` (e.g. `TH_PARAM = 989`, default `995`) with the same active `+950` lobby offset. Other lengths/fields (`USR_ID_LENGTH 20`, `USR_PASS_LENGTH 20`, `USR_RAND_PASS_LENGTH 6`, `ENCRYPT_KEY 12`, `NET_MSG_HEARTBEAT_CLIENT_ANS 161`) agree.
- **Critical discrepancy (documented, not changed):** `RANMobile/Engine/Network/EP7Protocol.h` derives `EP7_NET_MSG_LOGIN_2` from `NET_MSG_BASE=0` + the **commented-out** `+1013` lobby block in the repo header, yielding **1120**. The active source defines lobby as `NET_MSG_BASE + 950`, so the real value is `NET_MSG_BASE + 950 + 107` — **2046** for `TH_PARAM` (base 989), **2052** for the default base 995, and **1945** in the older recovered archive (base 888). This must be resolved from server config/build flags before any protocol work. Per task rules, no protocol constant was changed.

## 9. Windows client compilation results

Strict workflow `.github/workflows/strict-build-verification.yml` (no `continue-on-error`) was created and run: **run `45819356699`**, commit `0680b8e`. Windows job builds `Smtm2022.sln` then each of LoginServer/SessionServer/AgentServer/FieldServer/GameServer projects with real MSBuild exit codes, records per-project results and uploads logs. Status at report time: **IN PROGRESS / UNVERIFIED** — job `139907264786`, started `2026-11-26T12:24:32Z`, MSBuild step still running after ~26 minutes (workflow timeout 180 min). No exit codes or output binaries have been observed, so no success is claimed.

## 10. Windows server compilation results

Same job and run as §9; per-project exit codes will be recorded in `build_logs/results.txt` (artifact `strict-original-windows-build-logs`) when the job finishes. Status at report time: **IN PROGRESS / UNVERIFIED** — the job has not reached the individual server-project builds yet. The earlier `build-ep7-windows-server.yml` job was not used as evidence because it sets `continue-on-error: true`.

## 11. Android compilation results

- Strict run `45819356699` job `Android ARM64 + APK + AAB (strict)`: **all steps success**, including `test -s build-android-arm64/libranmobile.so`, three `test -s` checks for debug APK / release APK / release AAB, and SHA-256 recording. Artifact `strict-ranmobile-android-packages` = 1,585,830 bytes (not expired).
- Local native verification (this sandbox, g++ 12.2.0): compile exit **0** (`external/recovery/compile_output.txt`) and `./ranmobile_tests` exit **0** ending `ALL TESTS PASSED!` (`external/recovery/test_output.txt`) using the recovered ThaiPack. This is the portable C++ core only — not an Android APK built locally.
- CI Linux job `RANMobile native tests (real data counts)` also succeeded with 309/730 file-count assertions (same run).

## 12. Exact artifact paths and sizes

- Downloaded (workspace, git-ignored): `google_drive/RanOnlineEP7-Client.zip` 22,378,796 B; `google_drive/Ran-Online-Episode-7-main.zip` 6,538,502 B; `google_drive/RAN-EP7-ThaiPack-FamHawaii.zip` 1,359,185 B.
- Extracted Thai text: `external/ep7_release/thaipack_original/` (1,046 files; 309 `.ntk`, 730 `.qst`, 7 `.txt`).
- Inspection outputs: `external/recovery/RanOnlineEP7-Client_paths.txt`, `Ran-Online-Episode-7-main_paths.txt`, `RAN-EP7-ThaiPack-FamHawaii_paths.txt`, `client_zip_vs_repo_diff.txt`, `compile_output.txt`, `test_output.txt`, `ranmobile_tests` (built binary).
- CI artifacts: `strict-ranmobile-android-packages` 1,585,830 B (`.so` not uploaded separately in strict workflow; APK/AAB inside); Windows logs artifact expected from the same run; full 6.5 GB client data was deliberately not downloaded by the strict workflow.

## 13. SHA-256 checksums

See §1 for the three recovered archives. Native test binary and extracted text are regenerable; CI records SHA-256 for `libranmobile.so`, debug APK, release APK and release AAB in the job output (raw log download is blocked in this sandbox, so only job conclusions and artifact sizes could be read here).

## 14. Remaining blockers / updates

- B1 (no local Android SDK/NDK), B2 (big client archives/DB tools unreachable), B3 (no live server), B4 (DB/config LFS), B5 (CI raw logs blocked) remain.
- **B6 (new, critical): protocol constant conflict** between `EP7Protocol.h` (1120) and the active PC source (`NET_MSG_BASE + 950 + 107`). No change made; needs user/server-build-flag decision before Phase 3.
- Recovery downloads of `RanOnlineEP7-Server.zip` and `RAN-EP7-ServerDBTools.zip` failed at the connector; exact user-supplied files needed (see §2).

- **B7 (new): strict Windows build run still in progress.** Run `45819356699`, job `139907264786` was still executing the full-solution MSBuild step at the time of this report; Windows client/server compilation therefore remains **unverified**. No `continue-on-error` was used, so when it finishes the job will fail if any project returns a non-zero exit code. Check that run before starting Phase 3.

## 15. Branch / PR note

All recovery work was committed to the session's fixed branch `arena/075e1725-ran-online-ep7` (platform rule: no branch creation or switching in this session). PR #3 remains **OPEN DRAFT**; no PR was merged, closed or edited, and no separate branch/PR was created.
