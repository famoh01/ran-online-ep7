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

## 7. Phase 3D verification (2026-11-26)

Local sandbox:

- Compile: `g++ -std=c++17 -Wall -Wextra -Werror` over all RANMobile sources + tests → **exit 0**; log `external/recovery/phase3d_compile.log`.
- Tests: `./external/recovery/phase3d_tests` → **exit 0**, `ALL TESTS PASSED!`; log `external/recovery/phase3d_test_output.txt`.
- New section 31: UTF-8 full-field commit, Thai input, mixed Unicode, codepoint-aware deletion, 20-codepoint cap, password mask length, focus transitions, keyboard bottom inset and IME action without false `LoginAccepted`.
- Asset recovery: three additional Drive archives were integrity-checked and hashed (see `SOURCE_DATA_RECOVERY.md` §17); none contained DDS/MTF/WLD/GLMAP/manifest assets, so no real-asset rendering test was added.

Android CI:

- Run `45866773886` on `da95f90`: **failure** — `native_bridge.cpp` used `ReleaseAndroidInput()` before declaration and `LoginScreen::GetPassword()` did not exist. Fixed in `91b9db6`.
- Run **45867328078** on `91b9db6`: **success**, all steps (native tests, ARM64 native library, Java 17/Gradle, debug/release APK, release AAB). Artifacts: `libranmobile-arm64-v8a` **1,825,929 B**, `ranmobile-android-packages` **1,800,104 B**.
- No device/emulator test: IME composition, actual keyboard visibility and password presentation on hardware are **not** verified.

## 8. Phase 3G verification (2026-11-26, verified binary files only)

- Retrieval: 24 focused backup-repo objects pulled with official `git-lfs` on GitHub-hosted runner; each was checked as non-pointer, SHA-256 equal to LFS OID and byte size equal to pointer size. Manifest: `external/recovery/phase3g/retrieval_manifest.tsv`; bytes: `external/recovery/phase3g/authentic/`. Successful runs include `45877365090` (mapslist.mst + login.wld) and `45878536743` (final source).
- Source-derived decode: `decode_mapslist.py` parsed all 120 records and consumed exactly 13,714 bytes; `decode_lev_head.py` consumed exactly the 34-byte `SLEVEL_HEAD` block and recovered dependency `innerzone_01.wld`. No invented format or decoder was used.
- Android native check on verified texture: `EP7TextureDecoder::InspectDDS`/`DecodeDDS` on authentic `loading_032.dds` returned valid DXT1 1024×512, 1 mip, 2,097,152 RGBA bytes, exit 0.
- Android native check on verified conflict cases: authentic binary `mapslist.mst` → `valid=NO`, encoded-manifest diagnostic; authentic `innerzone_01.Lev` → `valid=0`, `UNSUPPORTED GLLEVEL VERSION`. Both are expected compatibility findings, not test failures.
- Regression baseline: `./external/recovery/phase3d_tests` → exit 0, `ALL TESTS PASSED!`. No game source changed.
- Not performed: local Git LFS fetch, APK/AAB build, device/emulator run, live server login, WLD geometry render, original EXE execution.

## 9. Phase 3H verification (2026-11-26, authentic binary map list & LEV importer)

- Local C++17 build: `g++ -std=c++17 -Wall -Wextra -Werror` across all 37 `RANMobile/` `.cpp` files + `RANMobile/Tests/test_main.cpp` → **exit 0**, 0 warnings (`external/recovery/phase3h_compile.log`, 0 B).
- Local unit, adversarial, and authentic Phase 3G tests:
  - `./external/recovery/phase3h_tests` → **exit 0**, `ALL TESTS PASSED!`.
  - `RANMOBILE_REQUIRE_AUTHENTIC_PHASE3G=1 ./external/recovery/phase3h_tests` → **exit 0**, `[PHASE 3H] Authentic Phase 3G verification PASSED (24/24 manifest objects, 13,714 B mapslist.mst, 12,654 B innerzone_01.Lev, 1024x512 loading_032.dds)` (`external/recovery/phase3h_test_output.txt`).
  - Verified sections: #30 (`StrictAuthentic` default rejects legacy `0x0104`/INI; explicit `LegacySourceCompat` succeeds), #32 (ByteCrypt bijection/round-trip across `None`/`Old`/`MapsList`/`LevelNew` + FIPS 180-4 SHA-256 vectors + mismatch rejection), #33 (synthetic binary `GLMAPS_LIST 0x0200` + `glmap 0x0200 / SLEVEL_HEAD 0x0102` + case-insensitive asset resolution + manifest tamper rejection), #34 (adversarial suite: truncated headers, INI/JSON rejection, invalid bools, out-of-range IDs/names, path traversal, duplicate IDs/filenames, trailing bytes, `dwNum=0`/`>1000`, block size mismatches, missing-`.Lev` rejection), and #35 (24/24 Phase 3G manifest SHA-256 validation, 13,714/13,714 B authentic `mapslist.mst`, 174/12,654 B authentic `innerzone_01.Lev` + 12,480 B `LOAD_200` remainder check, `login.wld` `LAND.MAN 0x0108`, staged `LoadSceneWithManifest` decoding `loading_032.dds` 1024×512, `requireAllReferencedLevels` failure, and 1-byte `.Lev`/`.dds` corruption rejection).
- Android CI: GitHub Actions Run **`45882553115`** (`job 140125196382`, duration 3m37s) on commit `2ea77ce3f364a7404d86b351e46febaf9399534a` (`arena/be1e147d-ran-online-ep7`) → **`success`** across Linux C++17 tests, ARM64 NDK `r26d` (`android-24`) build, and Gradle `assembleDebug`/`assembleRelease`/`bundleRelease`. Artifacts: `libranmobile-arm64-v8a` **2,032,860 B**, `ranmobile-android-packages` **1,938,104 B**.
- Not performed / still unverified: on-device or emulator Android execution, `LOAD_200` subsections beyond `SLEVEL_HEAD`, `.wld` geometry parsing/rendering, live server login, and original `.exe` execution.

## 10. Phase 3I verification (2026-11-26, 3D world parser, TGA/JPEG decoders & GLES3 scene renderer)

- Local C++17 build: `g++ -std=c++17 -O2 -Wall -Wextra -Werror` across all `RANMobile/` sources (including `EP7WorldParser.cpp`) + `RANMobile/Tests/test_main.cpp` → **exit 0**, 0 warnings.
- Local `__ANDROID__` GLES3 syntax/warning check (`MobileRenderer.cpp`, `EP7SceneRenderer.cpp`, `RANMobileEngine.cpp` with `-D__ANDROID__ -Wall -Wextra -Werror`) → **exit 0**, 0 warnings.
- Local unit + authentic Phase 3G/3I tests (`RANMOBILE_REQUIRE_AUTHENTIC_PHASE3G=1 RANMOBILE_REQUIRE_AUTHENTIC_PHASE3I=1`): **exit 0**, `ALL TESTS PASSED!` across sections **1–39**:
  - #36: TGA (`16/24/32`-bpp, RLE, top-down/bottom-up, 16-bpp 1-bit alpha attribute) & baseline `SOF0` JPEG decode + malformed/progressive-`SOF2` rejection.
  - #37: `MobileCamera` LH `LookAtLH` / `PerspectiveFovLH`, 6-plane Gribb-Hartmann frustum culling, `MobileRenderer` 3D mesh registry, `DrawMesh3D` culling stats, and `OnSurfaceLost()`/`OnSurfaceCreated()` GPU restoration.
  - #38: `EP7WorldParser` negative/corrupt boundary checks, multi-touch `ScenePreview` 1-finger orbit + 2-finger pinch/pan, and design-space `MobileHUD` `InGame` rendering.
  - #39: `63/63` Phase 3I manifest SHA-256 verification, `login.wld` (`3,098,590/3,098,590` B exact EOF), `innerzone_01.{wld,wld0,Lev}` (`1,190,775` / `5,808,480` / `12,654` B exact EOF), `12/12` `.pie`/`.pis` pieces, `12/12` Phase 3I textures, staged end-to-end 3D scene load (`140` terrain batches, `579` piece instances, `49` lights), multi-pass `ScenePreview` + `InGame` rendering, `OnSurfaceLost`/`OnSurfaceCreated` recovery, and standalone `login.wld` 3D load/render.
- Android CI: GitHub Actions Run **`45894091238`** (`job 140163582802`, commit `21f510092e5a1fb81bce7c36bcb20d17e4304056`) → **`success`** across Linux C++17 tests, NDK `r26d` `arm64-v8a` `libranmobile.so` (**2,933,235 B**), and Gradle `assembleDebug`/`assembleRelease`/`bundleRelease` (**2,341,433 B**).
- Not performed / still unverified: on-device or emulator Android execution, `.chf`/`.cps`/`.bin` skinned character rendering, live server login, and original `.exe` execution.



---

## 11. Phase 3J verification (2026-11-26, authentic character pipeline)

### 11.1 Local host build (verified in this sandbox)

```
g++ 12.2.0, Linux x86-64
g++ -std=c++17 -Wall -Wextra -Werror   # zero warnings, all 8 new sources + overlay + scene
```

Flags match the CI workflow's existing host build exactly.

### 11.2 Local host tests

| Target | Result |
|---|---|
| `RANMobile/Tests/test_phase3j.cpp` | **263 checks, 0 failures** |
| same, `-fsanitize=address,undefined`, `detect_leaks=1` | **clean, no reports** |
| `RANMobile/Tests/test_main.cpp` (Phase 3G–3I regression) | compiles clean with the 8 new sources linked; runtime stops at the **pre-existing** `assert(tpStats.found)` (line 186) because `external/ep7_release/thaipack_original` does not exist in this sandbox — unchanged behaviour, not a Phase 3J regression |

Coverage details: see `PHASE3J_CHARACTER_ANIMATION.md` §9. The suite includes `.rcc`
archive tests: case-insensitive lookup, stored entries, in-memory ZipCrypto decryption with
the client password `"tkqwlf"` feeding the real `.cps`/`.chf` parsers, CRC-corruption
rejection, and a truncation/byte-corruption sweep of the archive itself.

### 11.3 Robustness sweep (part of the suite)

Every truncated prefix of all six ABI fixtures, plus four byte values at every single byte
offset, fed through all five parsers (`skeleton`, `skinned mesh`, `.chf`, `.cps`, `.bin`,
`.cfg`). No crash, no sanitizer report.

### 11.4 CI steps added (not yet executed — no CI run from this sandbox)

New workflow steps in `.github/workflows/build-ranmobile-android.yml`:

1. `Build & Run Phase 3J Character Tests` — builds and runs the suite with
   `-Wall -Wextra -Werror`, writes `phase3j_test_output.log`.
2. `Verify Authentic EP7 Character Assets` — builds
   `RANMobile/Tools/verify_ep7_character_assets.cpp` and runs it against
   `external/ep7_release/client_original`, writing `phase3j_authentic_assets.log`. The tool
   walks loose files **and** opens every `.rcc`/`.zip` it finds, because the authentic
   character data lives inside `SkinObject.rcc` / `Animation.rcc` (see BLOCKERS.md B9
   addendum). The tool's exit code is part of the contract: `0` = every authentic entry
   parsed, `1` = at least one failed (fails the step), `2` = nothing authentic found (loud
   warning, step still green because an absent client is not a parser defect). The counts,
   the exit code and every failure are also emitted as check-run annotations, so the verdict
   is readable from the PR page even though the uploaded log needs credentials.
3. Both logs are added to the job summary and to the `linux-test-failure-log` artifact.

The existing 33-source host build now includes the 8 Phase 3J sources.

### 11.5 Explicitly NOT verified by Phase 3J

* No authentic `.chf/.cps/.x/.bin/.cfg` file was parsed (B9). The verification tool is the
  only path to that evidence and has not been run on real data.
* No character was rendered on screen.
* No GPU skinning path exists to test.
* Android arm64 build, `assembleDebug` / `assembleRelease` / `bundleRelease`: **CI-only**
  (B1), not executed here.
* No physical Android device was used.
* No mobile performance measurement was taken.

---

## 12. CI isolation & authentic-asset outcome model — 2026-11-26

### 12.1 Workflow structure (three independent jobs)

`.github/workflows/build-ranmobile-android.yml` previously ran everything in **one job**.
The authentic-asset step exits non-zero when real data cannot be parsed, so a coverage gap
in an unimplemented format also skipped the core C++17 tests, the test summary, the log
artifact and the whole Android build. That is fixed by splitting into three jobs with **no
`needs:` between them**:

| Job | Contents | Client data needed | Blocks CI |
|---|---|---|---|
| `core-tests` | `test_main.cpp` suite + Phase 3J character suite | no | **yes** |
| `authentic-assets` | `verify_ep7_character_assets` against the real client | yes (~2.5 GB) | on `FAIL` only |
| `android-arm64` | NDK arm64 lib + `assembleDebug/Release`, `bundleRelease` | no | yes |

Because the jobs are independent, the workflow's overall status is the AND of three real
statuses: a core failure can never be masked, and an asset verdict cannot stop core tests.

### 12.2 Outcome model

`verify_ep7_character_assets` classifies every entry as **PASS / FAIL / UNSUPPORTED** and
returns one of four exit codes:

| Exit | Outcome | Step behaviour |
|---|---|---|
| 0 | PASS | `::notice`, job green |
| 1 | FAIL | `::error` + `exit 1` — **job fails** |
| 2 | UNSUPPORTED | `::warning` + step summary table; job green (B11 coverage gap) |
| 3 | NOT FOUND | `::warning`; job green (nothing to verify is not a defect) |

`UNSUPPORTED` is never counted as parsed. `continue-on-error` is **not used anywhere**: the
non-blocking decision for exit 2 is an explicit `case` in the step, so the reasoning is
visible in the log.

### 12.3 Verified commands (run 2026-11-26, Ubuntu sandbox, g++ 12)

```bash
# Phase 3J suite (276 checks)
g++ -std=c++17 -Wall -Wextra -Werror \
  RANMobile/Tests/test_phase3j.cpp \
  RANMobile/Data/Importer/EP7CharacterFile.cpp RANMobile/Data/Importer/EP7SkinPiece.cpp \
  RANMobile/Data/Importer/EP7Skeleton.cpp      RANMobile/Data/Importer/EP7XFile.cpp \
  RANMobile/Data/Importer/EP7SkinnedMesh.cpp   RANMobile/Data/Importer/EP7Animation.cpp \
  RANMobile/Data/Importer/EP7RccArchive.cpp    RANMobile/Engine/Scene/EP7CharacterScene.cpp \
  RANMobile/Engine/Camera/MobileCamera.cpp     RANMobile/Engine/Renderer/MobileRenderer.cpp \
  RANMobile/Engine/Core/Logger.cpp -o t && ./t

# Sanitizers
g++ -std=c++17 -g -fsanitize=address,undefined <same sources> -o t_asan
LD_PRELOAD=$(gcc -print-file-name=libasan.so) ASAN_OPTIONS=detect_leaks=1 ./t_asan

# Authentic-asset probe against any extracted client root
g++ -std=c++17 -Wall -Wextra -Werror \
  RANMobile/Data/Importer/EP7Animation.cpp RANMobile/Data/Importer/EP7CharacterFile.cpp \
  RANMobile/Data/Importer/EP7RccArchive.cpp RANMobile/Data/Importer/EP7Skeleton.cpp \
  RANMobile/Data/Importer/EP7SkinPiece.cpp RANMobile/Data/Importer/EP7SkinnedMesh.cpp \
  RANMobile/Data/Importer/EP7XFile.cpp RANMobile/Tools/verify_ep7_character_assets.cpp \
  -o verify && ./verify <extracted-client-root>; echo "exit=$?"
```

Outcome matrix confirmed locally with synthetic clients:

| Client root | Probe exit | Expected |
|---|---|---|
| all assets parse | 0 | PASS |
| `SANIMCONINFO` 0x0114 only | 2 | UNSUPPORTED |
| corrupted pack entry | 1 | FAIL |
| empty directory | 3 | NOT FOUND |

Core-failure handling was also reproduced verbatim (`test_main.cpp:186` assertion, exit 134):
the step's shell propagates the failure (`exit $TEST_EXIT`) and emits `::error::`.

---

## 13. Phase 3J follow-up — `.cps` trace-point regression (`SVERTEXINFLU::VERSION`)

| Item | Requirement | Result |
|---|---|---|
| Unit regression | `.cps` trace record with tag `0x0101` and `numBone = 2` parses structurally | PASS |
| Field fidelity | `index`, `position[3]`, `normal[3]`, `boneCount`, `bones[2]`, `weights[2]` all exact | PASS |
| Boundary | `effectCount` and `m_dwFlag` reached ⇒ cursor landed on the structure, not the over-stated skip | PASS |
| Negative gating | tag flipped to `0x0100` leaves `influence.valid == false` (engine's unknown-version branch) | PASS |
| Defect reproduction | reverting the constant to `0x0100` fails 4 checks in the new test | PASS (reproduced) |
| Full 3J suite | build `-Wall -Wextra -Werror` + run | PASS — **294 checks, 0 failures** (was 276) |
| Sanitizers | ASan + UBSan + LSan on the full suite | PASS — clean, exit 0 |
| Core suite compile | workflow command, `-Werror` | PASS — exit 0 |
| Core suite run | `test_main.cpp:186` external-data assertion | exit 134 — **pre-existing, unchanged** (no authentic data in sandbox) |
| Authentic-data verification | re-run probe against real assets | **NOT RUN** — assets unreachable here (`gh release download` → `EOF`) |

### What this test does and does not establish

The fixture is an **ABI fixture**, written by hand to the layout recovered from
`SVERTEXINFLU::SaveFile` / `LoadFile` — it is not game data. It establishes that the reader
honours the engine's real version tag and consumes the body structurally. It does **not**
establish that authentic `.cps` files now parse end-to-end; that requires the CI
authentic-asset job with the release asset present, which cannot be executed in this sandbox.

### Check counts by stage

| Stage | Checks | Failures |
|---|---|---|
| Before this change | 276 | 0 |
| After adding `TestSkinPieceTraceInfluence` | **294** | **0** |
| Old constant restored (control) | 283 | 4 |

The control run confirms the new test is sensitive to the defect rather than passing vacuously.

### 13.1 Executed validation against authentic assets (CI run `45917078958`, commit `802f0c0`)

| Item | Result |
|---|---|
| `Core C++17 tests (blocking)` job | **success** |
| `Android ARM64 build (lib + APK/AAB)` job | **success** |
| `Authentic EP7 asset verification` job | failure (probe exit 1 — 3,679 FAILs remain) |
| `cps: truncated trace name` | **2,664 → 0** |
| `cps: effect count exceeds sanity limit` | **438 → 0** |
| `cps: truncated trace vertex influence` | **58 → 0** |
| `cps: truncated effect count` | **5 → 0** |
| Total FAIL reasons | **6,812 → 3,679** (−3,133; reconciles exactly, see PROGRESS_REVIEW.md) |
| `cps` entries now parsing cleanly | the trace section is reached and completed for every `.cps` that is not gate-rejected |

The asset job still fails, and must continue to: 3,679 genuine FAILs remain (B13 3,138 + B15 88
+ `.chf` 243 + skeleton 210). It must not be made green by relaxing a check.

Read the probe's **aggregated reason table** for comparisons, not the raw annotation list — the
~10-annotation cap moves which reasons are visible between runs (UNSUPPORTED reasons now appear
that were previously crowded out).

---

## 14. Phase 3J follow-up — `.cps` legacy versions 0x0101–0x0104 (B13 / B15)

Source basis: `DxSkinPiece::LoadPiece` has seven branches and a terminal error path
(`DxSkinPieceContainer.cpp:374/459/544/629/712/783/859`, `else` at `:934-943`).

### Regression coverage added (`TestSkinPieceLegacyVersions`)

| Item | Requirement | Result |
|---|---|---|
| 0x0104 | parses; materials present; trace name from file; ends at effect count | PASS |
| 0x0104 trailing flag | a flag DWORD appended to the file is **not** consumed (`flag == 0`) | PASS |
| 0x0103 | parses; **no** material list; traced names; no trailing flag | PASS |
| 0x0102 | parses; **10** fixed slots; names `IMAGE01`/`IMAGE11`; `syntheticName` set | PASS |
| 0x0101 | parses; **8** fixed slots; same name table | PASS |
| COL_ERR slots | slots with `m_dwIndex == COL_ERR` produce no trace point (2 of 4 kept) | PASS |
| slot count is load-bearing | 10-slot body read as 0x0101 fails; 8-slot body read as 0x0102 fails | PASS |
| version gate | `0x0100` / `0x0108` / `0x0200` still rejected | PASS |
| control: `hasTrailingFlag >= 0x0104` | 3 failures reproduce the old defect | PASS (reproduced) |
| control: 10 slots for both legacy versions | 4 failures | PASS (reproduced) |
| control: pre-fix reader | 8+ failures across the legacy section | PASS (reproduced) |

### Executed local results

| Stage | Checks | Failures |
|---|---|---|
| before this change | 294 | 0 |
| after | **339** | **0** |

ASan + UBSan + LSan: clean, exit 0. Core suite compile (`-Werror`): exit 0; run stops at the
pre-existing `test_main.cpp:186` external-data assertion (exit 134), unchanged. Probe compiles
clean under `-Werror`.

### Executed CI results (run `45923517031`, commit `a591320`)

| Job | Result |
|---|---|
| `Core C++17 tests (blocking)` | **success** |
| `Android ARM64 build (lib + APK/AAB)` | **success** |
| `Authentic EP7 asset verification` | failure — correct; genuine FAILs remain |

Pack entries (the only measure that counts, from the probe's summary line):

| | before `1e36e89` | after `a591320` | Δ |
|---|---|---|---|
| parsed | 6,350 | **6,479** | **+129** |
| failed | 3,679 | **3,550** | **−129** |
| unsupported | 1,631 | 1,631 | 0 |
| total | 11,660 | 11,660 | 0 |

Reason-level change, all `.cps`:

| Reason | before | after |
|---|---|---|
| `cps: unsupported .cps version` | 3,138 | 3,097 (**−41**) |
| `cps: truncated piece flag` | 88 | **0** (**−88**) |
| `.chf` / skeleton / `.bin` / `.cfg` / `.x` | unchanged | unchanged |

41 + 88 = 129 = Δparsed = Δfailed. **Authentic improvement is confirmed by CI**, not inferred.

### What is deliberately not claimed

The residual 3,097 `.cps` failures are files whose version DWORD is outside `0x0101..0x0107`.
The engine rejects those too (same terminal `else`), so this is correct behaviour, not a
remaining bug in the branch logic. No speculative version branch was added. Confirming whether
any are legitimate would need the files themselves, which are unobtainable in this sandbox.

---

## 15. Phase 3J follow-up — `SVERTEXINFLU` `dwSize` accounting (B14)

Source: `SVERTEXINFLU::SaveFile`, `DxSkinMeshContainer9.cpp:59-92` (copy at
`SMeshContainer.cpp:52-85`); reader `:94-127`.

### Regression coverage (`TestSkinPieceInfluenceSizeAccounting`)

| Item | Requirement | Result |
|---|---|---|
| `numBone == 0` | `dwSize` and body agree; record parses; reaches `effectCount` | PASS |
| `numBone == COL_ERR` | `dwSize` under-states (wrapped `0x18` vs `0x20` body); record parses | PASS |
| `numBone` 0..6 | every legal count round-trips, `index`/`boneCount` exact, lands on `effectCount` and `m_dwFlag` | PASS |
| mismatch tag | `dwSize` still honoured as the skip distance (engine parity) | PASS |
| fixture fidelity | `PieceCpsWithInfluence` reproduces the writer's arithmetic before printing it | PASS |

### Controls — the tests are load-bearing

| Control | Injected change | Result |
|---|---|---|
| A | trust `dwSize` unconditionally on a match | many failures |
| B | trust `dwSize` when it looks plausible | many failures |
| C | drop the `COL_ERR` special case | many failures |
| D | add a `dwSize`-vs-body consistency check | **31 failures**, incl. `numBone == 0` and `COL_ERR` |

Control D is the guard rail that matters: a consistency check is the most likely future
"hardening" and the suite now rejects it, because the writer is wrong rather than the reader.

### Executed local results

| Stage | Checks | Failures |
|---|---|---|
| before | 339 | 0 |
| after | **398** | **0** |

ASan + UBSan + LSan: clean, exit 0. Core suite compile (`-Werror`): exit 0; run stops at the
pre-existing `test_main.cpp:186` external-data assertion (exit 134), unchanged.

### Executed CI results (run `45926387844`, commit `80a6336`)

| Job | Result |
|---|---|
| `Core C++17 tests (blocking)` | **success** |
| `Android ARM64 build (lib + APK/AAB)` | **success** |
| `Authentic EP7 asset verification` | failure — correct; genuine FAILs remain |

| Pack entries | `6ee1b02` (before) | `80a6336` (after) |
|---|---|---|
| parsed | 6,479 | 6,479 |
| failed | 3,550 | 3,550 |
| unsupported | 1,631 | 1,631 |

Identical, as expected for a test-only change. **No authentic-asset improvement is claimed and
none was predicted** — B14 does not affect parsing. The value of this run is the negative
result: adding the tests did not perturb behaviour.

### Production code

Unchanged. This task modified **only** `RANMobile/Tests/test_phase3j.cpp`. RANMobile has no
`.cps` writer, and the reader already ignores `dwSize` on a version match, so there was nothing
to correct.

---

## 16. Phase 3J follow-up — B11 "Anim" container investigation

Investigation only. **No production code was modified; no parser was written.** The suite was run
to confirm no regression.

| Item | Result |
|---|---|
| Branch / HEAD | `arena/42c5ac0c-ran-online-ep7` @ `73d6d6d`, clean tree |
| Production code changed | **none** |
| C++ suite (ASan + UBSan + LSan) | **398 checks, 0 failures**, exit 0 |
| Regression introduced | none |

### Finding

The `"Anim"` magic is the first four bytes of the 128-byte basestream type string
`"AnimContainer"` (`SAnimation.cpp:22`, written by `SerialFile.cpp:57-66`). The format is
`SAnimContainer`, and it is **already fully implemented** in `EP7Animation.cpp:267`.

Source-accurate fixture, parsed through the real reader:

| Input | Result |
|---|---|
| `"AnimContainer"` + `0x0102`, up-body then main list | `valid=1`, `up=1`, `main=1`, bone names and keys recovered |
| same bytes through the probe's magic pre-check | classified **UNSUPPORTED**, never parsed |

### Root of the misclassification

`verify_ep7_character_assets.cpp:242-259` rejects any `.bin` whose first four bytes are printable,
on the stated premise that *"the client's own containers start with a small version DWORD"*. That
premise is inverted — the version DWORD is at offset `0x80`, after a 128-byte ASCII type string.
The check runs before `ParseContainer` and therefore suppresses the parser for exactly the files
it understands.

This is a **tool-only** false negative: `grep isprint` matches only those two lines;
`EP7AssetImporter` and the loader path contain no such heuristic.

### Why the existing suite could not catch it

All 8 basestream fixtures zero-fill the 128-byte type field. Per `SAnimContainer::LoadAnimFromFile`
the type string is not consulted on load, so the fixtures pass while never exercising a real
header. **Recommended addition (separate task):** a fixture with a genuine `"AnimContainer"` type
string and one with a genuinely foreign string, so both classification branches are covered.

### Not established

The split of the 6,228 reported `.bin` UNSUPPORTED entries. Determining how many are well-formed
`SAnimContainer` files versus genuinely foreign data requires one run of a corrected classifier
against authentic assets, which are unavailable in this sandbox.


> Preserved-branch B11 evidence: the following append was copied from
> commits 80f932b/cf4e03c on arena/1491d572-ran-online-ep7. Its CI results
> describe that source branch, NOT a newly integrated PR #5 head. Target CI
> must be rerun after integration. No Phase 3B–3I implementation is copied.


## 2026-11-27 — B11 verifier regression execution

- [x] Test the actual verifier translation unit, not a duplicate classifier.
- [x] Real `AnimContainer` string in 128-byte type field, NUL terminator, nonzero
  padding after NUL, version DWORD at offset 128, payload at 132.
- [x] Supported versions 0x0100/0101/0102, named zero-key track in 0x0102,
  printable prefix accepted when parsing succeeds. ABI fixtures, not game data.
- [x] Every strict truncation, excessive list count, unsupported track version,
  foreign type with otherwise parsable body, unterminated type, all-zero type,
  and headerless data cannot falsely pass.
- [x] Recognized unsupported container version counted separately from malformed
  header, unrecognized type and recognized-container parser failure.
- [x] Existing cfg unsupported/chf/cps/skeleton failure/x skeleton success and
  extension/path classifiers remain exercised.
- [x] `bash RANMobile/Tests/run_ep7_verifier_tests.sh`: 590/0 normal; 590/0 with
  ASan+UBSan+integrated LSan (`detect_leaks=1`), exit 0.
- [x] Mutation (old ASCII gate restored) fails 373/590 checks, exit 1.
- [x] Existing Phase 3J: 398/0. Core compiles; local data-dependent execution
  stops at missing ThaiPack assertion; no assertion is disabled.
- [ ] Post-fix remote core + verifier + sanitizer tests, Android ARM64 lib/APK/AAB,
  and authentic release classification: results to be appended after CI.

Baseline pack totals: 6,479 parsed / 3,550 failed / 1,631 unsupported. Compare
pack totals separately from loose totals; the historical 6,228 printable-prefix
figure includes more than the pack-only unsupported count. Do not equate it with
valid AnimContainers. Retain full authentic verifier log as CI artifact.


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


---

## 16. Phase 3J B12 — Authentic Animation Container (`SAnimContainer` / `.bin`) Investigation

- [x] Production `RANMobile/Data/Importer/EP7Animation.{h,cpp}` and B11 `RANMobile/Tools/verify_ep7_character_assets.cpp` verified `git diff` clean vs B11 commit `328e3d730ae37ac9f0fd276e0a723094acbe027a`.
- [x] Read-only inspector unit & sanitizer suite (`bash RANMobile/Tests/run_ep7_animation_inspector_tests.sh`): **5,574 checks, 0 failures** in both `-O2` normal build and `-fsanitize=address,undefined` (`ASAN_OPTIONS=detect_leaks=1`) build.
- [x] B11 verifier regression suite (`bash RANMobile/Tests/run_ep7_verifier_tests.sh`): **590 checks, 0 failures** normal and ASan+UBSan+LSan.
- [x] Existing Phase 3J suite (`test_phase3j.cpp`): **398 checks, 0 failures**.
- [x] Remote CI workflow `Phase 3J B12 Authentic Animation Container Investigation`:
  - `Retrieve verified EP7 engine animation sources`: **success** — all 19 `Lib_Engine` LFS source files verified against SHA-256 OIDs (`matheomedrana/Ran-Online-EP7-Complete-Files` @ `f661fbf89612cc77f95a7c9b935ff06b22bc55e3`).
  - `Inspect authentic ran-ep7-full-v1 animation containers`: **success** — `6,228` `data/animation/*.bin` files surveyed (`982` parsed / `4,037` unsupported / `1,209` parser-rejected / `0` invalid / `0` unrecognized), `6,228 / 6,228` (`384,339` tracks) verified to zero-trailing-byte `EXACT_EOF` under `SAnimationSaveLoad.cpp` read-only shadow spec, post-run asset SHA-256 immutability `YES`.
- [x] Remote CI workflow `Build RANMobile Android (ARM64) & Verify EP7 Release`:
  - `Core C++17 tests (blocking)`: **success**
  - `Android ARM64 build (lib + APK/AAB)`: **success**
  - `Authentic EP7 asset verification`: **expected non-blocking diagnostic failure (exit 1)** unchanged from B11 (`982` loose `.bin` parsed / `4,037` unsupported / `1,209` parser-rejected).

---

## 17. Phase 3J B13 — Authentic Animation Track `0x0100` & 80-Byte `SMatrixKey` Fix

- [x] Strict C++17 host build (`g++ -std=c++17 -O2 -Wall -Wextra -Werror`) of `RANMobile/Tests/test_phase3j.cpp`: **952 checks, 0 failures** (swept 7 ABI fixtures across truncations and 4x single-byte corruptions, including `AnimationBin0100()`).
- [x] Sanitizer build (`-fsanitize=address,undefined -fno-omit-frame-pointer`, `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`, `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`) of `RANMobile/Tests/test_phase3j.cpp`: **952 checks, 0 failures**.
- [x] B11/B13 verifier regression & sanitizer suite (`bash RANMobile/Tests/run_ep7_verifier_tests.sh`): **1,571 checks, 0 failures** in both normal `-O2` and `ASan + UBSan + integrated LSan` builds (covers `0x0100` `pos`/`rot`/`scale`/`80B matrix` keys with non-zero 12B `align(16)` padding, empty bone names, byte-by-byte truncations `0..size-1`, and `0x0103`/`0x0104`/`0x0200` unsupported classification).
- [x] B12/B13 animation container inspector unit & sanitizer suite (`bash RANMobile/Tests/run_ep7_animation_inspector_tests.sh`): **7,035 checks, 0 failures** in both normal `-O2` and `ASan + UBSan + integrated LSan` builds.
- [x] Local verification on 36 authentic `ran-ep7-full-v1` `.bin` samples (`verify_ep7_character_assets` + `inspect_ep7_animation_containers`): **20 parsed (`12` `0x0100` + `4` `0x0101` + `4` `0x0102`, `trailing_bytes=0 : 20 files`), 16 unsupported-version (`8` `0x0103` + `7` `0x0104` + `1` `0x0200`), 0 parser-rejected**.
- [x] Remote CI `Phase 3J B12 Authentic Animation Container Investigation` on commit `1e11d2ec7f77bf938cdefa0bda6668a5f2a85b48` ([run `45990886385`](https://github.com/famoh01/ran-online-ep7/actions/runs/45990886385), check run `140478981523`, report blob `a7710f879d895aa37c221ef49e217de851852464`): **success** — `6,228` `data/animation/*.bin` files surveyed:
  - `1,210 x 0x0100:parsed` (`63,226` `0x0100` tracks, `1,205` files with `m_cMatrixKeys > 0`, `22` files with empty `m_strBone`)
  - `907 x 0x0101:parsed` (`71,783` `0x0101` tracks)
  - `74 x 0x0102:parsed` (`5,284` `0x0102` tracks)
  - **`trailing_bytes=0 : 2,191 files` (`100.00%` `EXACT_EOF` across all `2,191` parsed files)**
  - **`parser-rejected = 0`** (down from `1,209`)
  - `unsupported-version = 4,037` (`1,533` `0x0103` + `2,495` `0x0104` + `9` `0x0200`)
- [x] Remote CI `Build RANMobile Android (ARM64) & Verify EP7 Release` on commit `1e11d2ec7f77bf938cdefa0bda6668a5f2a85b48` ([run `45990886364`](https://github.com/famoh01/ran-online-ep7/actions/runs/45990886364)):
  - `Core C++17 tests (blocking)` (job `140478134000`): **success**
  - `Android ARM64 build (lib + APK/AAB)` (job `140478133579`): **success** (`libranmobile-arm64-v8a` `2,302,838` B; `ranmobile-android-packages` `1,988,407` B)
  - `Authentic EP7 asset verification (diagnostic)` (job `140478133913`): **`2,191 parsed, 4,037 unsupported-version, 0 invalid-header, 0 unrecognized-type, 0 parser-rejected; total=6,228`** (overall job remains expected diagnostic exit `1` due to pre-existing `SkinObject.rcc` `.chf`/`.cps`/`.abf` gaps: `6,479 parsed, 3,550 failed, 1,631 unsupported`).
- [ ] Not claimed / still pending separate tasks: `0x0103`/`0x0104`/`0x0200` `.bin` containers (`4,037` files), `.cfg` `0x0108`–`0x0115`/`0x0200` metadata, `SkinObject.rcc` `.chf`/`.cps`/`.abf` gaps, physical Android device/emulator execution, and live server gameplay testing.

---

## 18. Phase 3J B14 — Authentic Animation `0x0103` Container & Track Implementation

- [x] Strict C++17 host build (`g++ -std=c++17 -O2 -Wall -Wextra -Werror`) of `RANMobile/Tests/test_phase3j.cpp`: **1,499 checks, 0 failures** (swept 8 ABI fixtures across truncations and 4x single-byte corruptions, including `AnimationBin0103()`, `0x0103` `upBody` + `dwGarbageValue` + `main` lists, `rotateKeys`/`dwGarbage1`/`positionKeys`/`quatPosKeys`/`matrixKeys` 80B/`dwGarbage2`/`scaleKeys`, `EP7AnimationPlayer::ApplyToSkeleton`, invalid counts/lengths, and `0x0100`/`0x0101`/`0x0102` regressions).
- [x] Sanitizer build (`-fsanitize=address,undefined -fno-omit-frame-pointer`, `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`, `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`) of `RANMobile/Tests/test_phase3j.cpp`: **1,499 checks, 0 failures**.
- [x] B11/B14 verifier regression & sanitizer suite (`bash RANMobile/Tests/run_ep7_verifier_tests.sh`): **2,784 checks, 0 failures** in both normal `-O2` and `ASan + UBSan + integrated LSan` builds (covers `0x0103` container/track parsing, every strict prefix truncation `0..size-1` of `0x0103`, and `0x0104`/`0x0200` unsupported classification).
- [x] B12/B14 animation container inspector unit & sanitizer suite (`bash RANMobile/Tests/run_ep7_animation_inspector_tests.sh`): **8,303 checks, 0 failures** in both normal `-O2` and `ASan + UBSan + integrated LSan` builds.
- [x] Local verification on 36 authentic `ran-ep7-full-v1` `.bin` samples (`verify_ep7_character_assets` + `inspect_ep7_animation_containers`): **28 parsed (`12` `0x0100` + `4` `0x0101` + `4` `0x0102` + `8` `0x0103`, `891` tracks, `trailing_bytes=0 : 28 files`), 8 unsupported-version (`7` `0x0104` + `1` `0x0200`), 0 parser-rejected**.
- [x] Remote CI `Phase 3J B12 Authentic Animation Container Investigation` on commit `63c8c805ff66c3c89a3e8143757efe11c4c29a9a` ([run `45995540399`](https://github.com/famoh01/ran-online-ep7/actions/runs/45995540399), check run `140495005201`, JSON report blob `0ef25dcc3a2e6de8abd8b9632f2813cfed724716`): **success** — `6,228` `data/animation/*.bin` files surveyed:
  - `1,210 x 0x0100:parsed` (`63,226` `0x0100` tracks)
  - `907 x 0x0101:parsed` (`71,783` `0x0101` tracks)
  - `74 x 0x0102:parsed` (`5,284` `0x0102` tracks)
  - **`1,533 x 0x0103:parsed` (`114,210` `0x0103` tracks; `1,532` w/ `quatPosKeys` + `1` zero-track; `dwGarbageValue == 0x0103 + numUpBody + numMain` in `1,533 / 1,533` files)**
  - **`trailing_bytes=0 : 3,724 files` (`100.00%` `EXACT_EOF` across all `3,724` parsed files / `254,503` tracks)**
  - **`parser-rejected = 0`**
  - `unsupported-version = 2,504` (`2,495` `0x0104` + `9` `0x0200`, down from `4,037`)
- [x] Remote CI `Build RANMobile Android (ARM64) & Verify EP7 Release` on commit `63c8c805ff66c3c89a3e8143757efe11c4c29a9a` ([run `45995539967`](https://github.com/famoh01/ran-online-ep7/actions/runs/45995539967)):
  - `Core C++17 tests (blocking)` (job `140493888315`): **success**
  - `Android ARM64 build (lib + APK/AAB)` (job `140493888339`): **success** (`libranmobile-arm64-v8a` `2,303,142` B; `ranmobile-android-packages` `1,989,193` B)
  - `Authentic EP7 asset verification (diagnostic)` (job `140493888068`): **`3,724 parsed, 2,504 unsupported-version, 0 invalid-header, 0 unrecognized-type, 0 parser-rejected; total=6,228`** (overall job remains expected diagnostic exit `1` due to pre-existing `SkinObject.rcc` `.chf`/`.cps`/`.abf` gaps: `6,479 parsed, 3,550 failed, 1,631 unsupported`).
- [ ] Not claimed / still pending separate tasks: `0x0104`/`0x0200` `.bin` outer containers (`2,504` files), `.cfg` `0x0108`–`0x0115`/`0x0200` metadata, `SkinObject.rcc` `.chf`/`.cps`/`.abf` gaps, physical Android device/emulator execution, and live server gameplay testing.

---

## 19. Phase 3J B15 — Authentic Animation Container `0x0104` Implementation

- [x] Strict C++17 host build (`g++ -std=c++17 -O2 -Wall -Wextra -Werror`) of `RANMobile/Tests/test_phase3j.cpp`: **2,084 checks, 0 failures** (swept 9 ABI fixtures across truncations and 4x single-byte corruptions, including `AnimationBin0104()`, `main` + `upBody1` + `upBody2` ordering, all 8 empty/populated list subsets, `2048 + 2048 == 4096` combined `upBody` limit, `2048 + 2049 == 4097` overflow rejection, malformed counts, exact EOF + trailing data, `0x0100`–`0x0103` regressions, and `0x0200` rejection).
- [x] Sanitizer build (`-fsanitize=address,undefined -fno-omit-frame-pointer`, `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`, `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`) of `RANMobile/Tests/test_phase3j.cpp`: **2,084 checks, 0 failures**.
- [x] B11/B15 verifier regression & sanitizer suite (`bash RANMobile/Tests/run_ep7_verifier_tests.sh`): **3,755 checks, 0 failures** in both normal `-O2` and `ASan + UBSan + integrated LSan` builds (covers `0x0104` `main`+`upBody1`+`upBody2` parsing, every strict prefix truncation `0..size-1`, malformed count rejection, and `0x0200` unsupported classification).
- [x] B12/B15 animation container inspector unit & sanitizer suite (`bash RANMobile/Tests/run_ep7_animation_inspector_tests.sh`): **9,702 checks, 0 failures** in both normal `-O2` and `ASan + UBSan + integrated LSan` builds (covers `0x0104` exact EOF, trailing-byte measurement, `upBody2` rejection pinpointing, and directory survey immutability).
- [x] Local verification on 36 authentic `ran-ep7-full-v1` `.bin` samples (`verify_ep7_character_assets` + `inspect_ep7_animation_containers`): **35 parsed (`12` `0x0100` + `4` `0x0101` + `4` `0x0102` + `8` `0x0103` + `7` `0x0104`, `943` tracks, `trailing_bytes=0 : 35 files`), 1 unsupported-version (`0x0200`), 0 parser-rejected**.
- [x] Remote CI `Phase 3J B12 Authentic Animation Container Investigation` on commit `263ef9cf7ae0e355b1363c0a615415ecaac291a6` ([run `45999738400`](https://github.com/famoh01/ran-online-ep7/actions/runs/45999738400), check run `140508852646`, JSON report blob `bd9250ca49297e09aa8322f2e5d44768ac47869a`): **success** — `6,228` `data/animation/*.bin` files surveyed:
  - `1,210 x 0x0100:parsed` (`63,226` `0x0100` tracks)
  - `907 x 0x0101:parsed` (`71,783` `0x0101` tracks)
  - `74 x 0x0102:parsed` (`5,284` `0x0102` tracks)
  - `1,533 x 0x0103:parsed` (`114,210` `0x0103` tracks)
  - **`2,495 x 0x0104:parsed` (`129,379` `0x0103` tracks; `2,491` non-empty `main`, `810` non-empty `upBody1`, `628` non-empty `upBody2`; `3` zero-track)**
  - **`trailing_bytes=0 : 6,219 files` (`100.00%` `EXACT_EOF` across all `6,219` parsed files / `383,882` tracks)**
  - **`parser-rejected = 0`**
  - `unsupported-version = 9` (`9` `0x0200` `EMBYTECRYPT_BIN`, down from `2,504`)
  - `.cfg` cross-check on `2,495` `0x0104` `.bin`: `21` pair with supported `.cfg` `0x0104`–`0x0107`, `2,464` pair with unsupported `.cfg` `0x0110`–`0x0115` (`UNSUPPORTED`), `10` have no `.cfg` stem.
- [x] Remote CI `Build RANMobile Android (ARM64) & Verify EP7 Release` on commit `263ef9cf7ae0e355b1363c0a615415ecaac291a6` ([run `45999738432`](https://github.com/famoh01/ran-online-ep7/actions/runs/45999738432)):
  - `Core C++17 tests (blocking)` (job `140508021895`): **success**
  - `Android ARM64 build (lib + APK/AAB)` (job `140508021781`): **success** (`libranmobile-arm64-v8a` `libranmobile.so` `2,304,051` B; `ranmobile-android-packages` `assembleDebug` + `assembleRelease` + `bundleRelease` `1,989,679` B)
  - `Authentic EP7 asset verification (diagnostic)` (job `140508021418`): **`6,219 parsed, 9 unsupported-version, 0 invalid-header, 0 unrecognized-type, 0 parser-rejected; total=6,228`** (overall job remains expected diagnostic exit `1` due to pre-existing `SkinObject.rcc` `.chf`/`.cps`/`.abf` gaps: `6,479 parsed, 3,550 failed, 1,631 unsupported`).
- [ ] Not claimed / still pending separate tasks: `0x0200` `EMBYTECRYPT_BIN` `.bin` outer containers (`9` files), `.cfg` `0x0108`–`0x0115`/`0x0200` metadata, `SkinObject.rcc` `.chf`/`.cps`/`.abf` gaps, physical Android device/emulator execution, and live server gameplay testing.





---

## 20. Phase 3J B16 — Authentic `SANIMCONINFO` `.cfg` `0x0108`–`0x0112`/`0x0114`/`0x0115` Implementation

- [x] Strict C++17 host build (`g++ -std=c++17 -O2 -Wall -Wextra -Werror`) of `RANMobile/Tests/test_phase3j.cpp`: **5,686 checks, 0 failures** (swept 11 ABI fixtures across truncations and 4x single-byte corruptions, including per-version minimal parses `0x0108`–`0x0115`, rich `0x0108`/`0x0110`/`0x0111`/`0x0115` fixtures with effects + `DxAniScale`, 9 malformed mutations with exact rejection substrings, sound-block truncations, `mainType` 33/34 boundaries, and `0x0101`–`0x0107` regressions).
- [x] Sanitizer build (`-fsanitize=address,undefined -fno-omit-frame-pointer`, `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`, `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`) of `RANMobile/Tests/test_phase3j.cpp`: **5,686 checks, 0 failures**.
- [x] B11/B16 verifier regression & sanitizer suite (`bash RANMobile/Tests/run_ep7_verifier_tests.sh`): **3,757 checks, 0 failures** in both normal `-O2` and `ASan + UBSan + integrated LSan` builds (covers the `0x0108` minimal-body `Pass` classification and the `0x0113` `UNSUPPORTED` classification).
- [x] B12/B16 animation container inspector unit & sanitizer suite (`bash RANMobile/Tests/run_ep7_animation_inspector_tests.sh`): **13,423 checks, 0 failures** ×2 configurations in both normal `-O2` and `ASan + UBSan + integrated LSan` builds (covers the B16 `.cfg` survey aggregation: outcomes, trailing sizes, effect masks, rejection reasons, JSON emission).
- [x] Remote CI `Phase 3J B12 Authentic Animation Container Investigation` on commit `d123d931592057d19dfc3c0fa5df4a07741f3a55` ([run `46010599537`](https://github.com/famoh01/ran-online-ep7/actions/runs/46010599537), check run `140544798789`): **success**.
- [x] Remote CI survey (`Build RANMobile Android (ARM64) & Verify EP7 Release`, `b12-authentic-anim-survey` job, check run `140545691201`, text report blob `13f4dee0f2e41532ead15ecc7f260039bdb00cf1`): **success** — `6,734` indexed `.cfg` surveyed:
  - `cfg version 0x0102 : 5, 0x0103 : 5, 0x0104 : 414, 0x0105 : 221, 0x0106 : 1095, 0x0107 : 1091` (pre-B16 paths unchanged)
  - **`B16 layouts (0x0108..0x0115): attempted=3903 parsed=3903 exact-eof=1055 with-trailing=2848` (rejections `0`)**
  - `168 x 0x0108:exact-eof, 17 x 0x0109:trailing-bytes(104B), 1081 x 0x0110:trailing-bytes(90/117/125/133/197B), 887 x 0x0111:exact-eof, 183 x 0x0112:trailing-bytes(90/170/250/330/490B), 1002 x 0x0114:trailing-bytes(115/195/203/275/291/379/467/555B), 565 x 0x0115:trailing-bytes(39/127/215/303/391/479/567B)`
  - Effect masks only `Single` (`0x0001`), `Ghosting` (`0x0002`), `Single|Ghosting` (`0x0003`): `783` effect-bearing files
  - `.bin` side unchanged: `6,219` parsed (`100%` `EXACT_EOF`), `9 x 0x0200` unsupported, `parser-rejected = 0`, asset SHA-256 unchanged after inspection: YES
- [x] Remote CI `Build RANMobile Android (ARM64) & Verify EP7 Release` on commit `d123d931592057d19dfc3c0fa5df4a07741f3a55` ([run `46010599353`](https://github.com/famoh01/ran-online-ep7/actions/runs/46010599353)):
  - `Core C++17 tests (blocking)` (job `140544798259`): **success**
  - `Android ARM64 build (lib + APK/AAB)` (job `140544798206`): **success** (`libranmobile-arm64-v8a` artifact `2,355,338` B; `ranmobile-android-packages` artifact `2,024,876` B)
  - `Authentic EP7 asset verification (diagnostic)` (job `140544797658`): expected diagnostic exit `1` (pre-existing `SkinObject.rcc` `.chf`/`.cps`/`.abf` pack gaps: `3,550` failed entries, unchanged from B11–B15).
- [ ] Not claimed / still pending separate tasks: `0x0200` `EMBYTECRYPT_BIN` `.bin` outer containers (`9` files), `0x0200` `EMBYTECRYPT_CFG` encrypted-`.cfg` path, `0x0113` (authentic `UNSUPPORTED`, no `case` in `LoadFile`), `SkinObject.rcc` `.chf`/`.cps`/`.abf` gaps, physical Android device/emulator execution, and live server gameplay testing.

---

## 21. Phase 3J B17 — Authentic Animation Runtime Integration & Skeleton Evaluation

- [x] Remote branch/HEAD/ancestry/PR state verified before work (stale local metadata repaired via plain `git reset origin/arena/00fd13f9-ran-online-ep7`; no main modification).
- [x] Original-source investigation (SAnimation.cpp SetTime/SetMatrix/SetPosQuat/SetMix/ConvertMatToQuat, DxSkinAniControl.cpp SetAnimation/FrameMove/UpdateTime/SELECTANI/DEFAULTANI/SELECTSKILLANI, DxBoneCollector.cpp UpdateBones/ResetBone, DxAniBoneScale.cpp, DxSkinChar.cpp creation, DxSkinMesh9_CPU.cpp) mapped to mobile files with documented differences; recorded in `ANIMATION_CONTAINER_INVESTIGATION.md` §18 and `PHASE3J_CHARACTER_ANIMATION.md` §14.
- [x] Open source question resolved: `SAnimation::ConvertMatToQuat` (SAnimation.cpp:1326–1356) converts matrix-only tracks to quatPos keys and **deletes the matrix keys**; tracks with quatPos keys keep matrix keys; every `Load_*` runs it (1459/1511/1568/1135).
- [x] Skeleton: bind-pose SQT at bind time, `ResetToRestPose` = `DxBoneCollector::ResetBone` mirror (flags=0/scale=1/mix=id/CUR=PREV=ORIG), `UpdateBones` recursion (firstChild/nextSibling) with BLENDING lerp/slerp PREV→CUR by weight, ADD mix multiply, aniScale rows.
- [x] Track evaluation: matrix-key direct write (SetMatrix memcpy parity), quatPos lower-bound snap + bFirst CUR/PREV, S/R/T fmod-window branch with rotate-w negation; track→bone binding with missing-bone skip; EMBODY routing (DEFAULT = upBody then main).
- [x] Pose: local → propagate → global (row-vector `local · parent`); world root from instance pos/yaw/scale; reset-to-rest every evaluation (no cumulative/double application — suite asserts the rest-pose restore contract).
- [x] Playback: start (SetAnimation: backup clock + `m_fTime = STime`), stop/end (`animationEnded` from `bEndAnim`), elapsed update (unit-anikey 4800/s), loop wrap vs non-loop end window (ETime − UNITTIME·2) + EMANI_ENDFREEZE pin, switching with `m_fWeight = 0` → +7/s blend-in, same-node re-select keeps playing, DoShockMix (mainType 11, deterministic, documented).
- [x] Runtime matrix→quatPos conversion (`EP7AnimationReader::ConvertMatToQuat`) with re-compression identity test (decompress→compress reproduces stored words) + NaN→identity and hemisphere canonicalization hardening tests.
- [x] Engine quality: dt clamp (non-finite/≤0 → 0, >0.25 → 0.25), no blocking loads in render, no per-frame allocations (scratch buffers; measured alloc-stability over 2,000 frames), stable orientation, extensible control API for touch/UI phases.
- [x] Rendering: real assets only — per-piece submission advances the skinned base pointer by `pieceVertexOffsets[slot]` (piece-local indexing); pieces failing bounds are skipped, never substituted; no fabricated geometry.
- [x] `test_phase3j`: **5,732 checks, 0 failures** (up from 5,686) — normal `-Wall -Wextra -Werror` + `ASan/UBSan/LSan` clean; CI-exact compile lines verified locally.
- [x] Regression: verifier suite **3,757 / 0**, inspector suite **13,423 / 0 ×2**, all sanitizers clean (B11–B16 behavior unchanged).
- [x] Performance measured on synthetic scale (62 bones, 4×3,000 verts, 2,000 frames, host sandbox, NOT device): spawn 0.055 ms; update avg 0.671 ms/frame; skinning dominates (~53 ns/vertex); pose pipeline < 3 µs. Numbers recorded in `PHASE3J_CHARACTER_ANIMATION.md` §14.8.
- [x] Docs updated non-destructively: `ARCHITECTURE.md`, `PROGRESS_REVIEW.md`, `ANIMATION_CONTAINER_INVESTIGATION.md` (§18), `TESTING_CHECKLIST.md` (§21), `BLOCKERS.md`, dedicated B17 report `PHASE3J_CHARACTER_ANIMATION.md` §14.
- [ ] **Device/emulator run: NOT TESTED ON DEVICE** — no Android device or emulator execution exists for B17 (sandbox blocker B1); CI compile ≠ device proof.

---

## 22. Phase 3J B18 — Authentic GPU Skinning, Bone Palette & Texture Binding

How to run (host, headless — the GPU path validates without a GL context; a GLES3 stub header is
only needed for the `-D__ANDROID__` compile check):

```bash
# phase3j suite (includes B18): expect 5,821 checks, 0 failures
g++ -std=c++17 -Wall -Wextra -Werror \
  RANMobile/Engine/Core/Logger.cpp RANMobile/Engine/Renderer/MobileRenderer.cpp \
  RANMobile/Engine/Camera/MobileCamera.cpp RANMobile/Engine/Scene/EP7CharacterScene.cpp \
  RANMobile/Engine/Animation/EP7SkinAniControl.cpp RANMobile/Data/Importer/EP7Animation.cpp \
  RANMobile/Data/Importer/EP7CharacterFile.cpp RANMobile/Data/Importer/EP7RccArchive.cpp \
  RANMobile/Data/Importer/EP7Skeleton.cpp RANMobile/Data/Importer/EP7SkinPiece.cpp \
  RANMobile/Data/Importer/EP7SkinnedMesh.cpp RANMobile/Data/Importer/EP7TextureDecoder.cpp \
  RANMobile/Data/Importer/EP7XFile.cpp RANMobile/Tests/test_phase3j.cpp \
  -lpthread -o phase3j && ./phase3j
# ASan+UBSan+LSan: same line with -fsanitize=address,undefined -g -O1; ASAN_OPTIONS=detect_leaks=1
# Android-path compile check (stub GLES3/gl3.h): -D__ANDROID__ -I<stub> over MobileRenderer.cpp +
#   RANMobileEngine.cpp
```

- [x] GPU skin streams: influence equivalence vs `SkinVerticesCPU` (per-bone scan order, cap 4,
      OOB vertex refs skipped, zero-weight passthrough) on the character fixture.
- [x] Palette semantics: manual 90° Head pose through `BuildBonePalette` + the exact shader
      formula (per-influence row-vector transform, w-divide, renormalize windows) — posed vertex
      verified to move; offset×combined parity asserted.
- [x] Overflow/capacity: palette > 256 entries rejected (logged, never approximated); 6 influences
      on 1 vertex → cap-4, stream is exactly 4 slots/vertex (ASan-verified no OOB reads).
- [x] Headless renderer API: counters, unknown handle, face-range OOB, release/double-release.
- [x] TGA decode: type 2/10 24/32 bpp (BGR order), colour-mapped type 1, greyscale type 3, origin
      bit, invalid type rejected; `.mtf` DecryptMTF round-trip (in-test encrypted container).
- [x] Scene texture binding: ladder (`face.dds` → `face.mtf`), lowercase cache, unknown subset →
      id 0 (flat colour, not a failure), `Clear` releases + second `Clear` safe.
- [x] Regression: verifier 3,757/0 + sanitizers; inspector 13,423/0 ×2 + sanitizers; phase3j
      5,821/0 + sanitizers; full-engine compile (44 TUs) clean; Android-path compile clean.
- [ ] **GPU render on a live GL context / device: NOT TESTED ON DEVICE** — headless validation +
      GLES3-stub compile only (sandbox blocker B1). CI compile ≠ device proof.
- [ ] Authentic-texture CI evidence: the `authentic-verification` job now reports shipped-texture
      decode rates and per-piece texture resolution for the first valid `.chf`; review its log on
      the B18 CI run (adds evidence, does not change the exit-code model).
