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
