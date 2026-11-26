# Phase 3H — Authentic Binary Map List & LEV Importer

Date: 2026-11-26 UTC. Active branch: `arena/be1e147d-ran-online-ep7` (implementation commit `2ea77ce`). PR #3 on `arena/075e1725-ran-online-ep7` (`7b355b3`) remains **OPEN / DRAFT** and was not merged, closed, overwritten, or force-pushed.

This phase implements source-derived, strict no-fallback Android-compatible C++17 binary readers and SHA-256 manifest verification for the authentic EP7 binary `mapslist.mst` (`GLMAPS_LIST` `0x0200` / `SMAPNODE_DATA` `0x0203`) and authentic `.Lev` (`glmap` `0x0200` / `SLEVEL_HEAD` `0x0102`) formats recovered in Phase 3G. No gameplay logic, network protocol constant, authentication flow, PC source file, or recovered binary asset was altered, and no playable scene or `.wld` geometry renderer is claimed.

---

## 1. Scope & Objectives

Phase 3H bridges the compatibility gap identified in `PHASE3G_AUTHENTIC_LFS_ASSET_RETRIEVAL.md`:
1. Replace the earlier plain-text-INI-only / legacy-`0x0104` default path with a strict binary reader derived strictly from the recovered original C++ sources (`external/recovery/phase3g/source/`) and verified against the authentic Phase 3G binary artifacts (`external/recovery/phase3g/authentic/`).
2. Decode authentic binary `mapslist.mst` (`GLMAPS_LIST` `0x0200`, `EMBYTECRYPT_MAPSLIST`, 120 `SMAPNODE_DATA` `0x0203` records, 104 used, consuming all **13,714 / 13,714 bytes**).
3. Decode authentic `.Lev` header and `SLEVEL_HEAD` (`glmap` `0x0200`, `EMBYTECRYPT_LEVEL_NEW`, `SLEVEL_HEAD` `0x0102`, **34-byte** block, **174 bytes** total header + head consumed, `innerzone_01.wld`, division `16`, bright `2`).
4. Enforce **strict no-fallback validation (`EP7ValidationMode::StrictAuthentic`)** as the default across parsers, scene index builder, scene renderer, and `RANMobileEngine::LoadScene`: refuse plain-text INI, JSON, demo terrain, fabricated entries, silent version downgrades, or unparsed trailing bytes in `GLMAPS_LIST`.
5. Implement pure C++17 FIPS 180-4 SHA-256 and `retrieval_manifest.tsv` verification (`EP7HashVerify`) with zero external dependencies for Android NDK `arm64-v8a` and Linux host tests.
6. Keep the importer strictly decoupled from rendering while preparing metadata inspection interfaces for a future Android scene-preview module.

---

## 2. Recovered Source Rules & Line-Level Traceability

Every decoding constant, struct field order, crypt selector, and bounds check in `RANMobile/Data/Importer/` is traced directly to recovered original source files (verified by SHA-256 against `external/recovery/phase3g/retrieval_manifest.tsv`):

| Rule / Constant | Value / Behavior | Recovered Original Source Citation |
|---|---|---|
| `CSerialFile` header layout | `char m_szFileType[128]` (`FILETYPESIZE=128`) + `DWORD m_FileID` (LE `uint32_t`) | `Lib_Engine/Common/basestream.h:16, 134–144`; `Lib_Engine/Common/SerialFile.cpp:48–57` (`CSerialFile::ReadFileType`) |
| `CSerialFile` payload decode | Byte-by-byte inverse permutation via `BYTECRYPT::byte_decode()` when `m_emCrypt != EMBYTECRYPT_NONE` | `Lib_Engine/Common/SerialFile.cpp:259–277` (`CSerialFile::read`); `Lib_Engine/Common/ByteCrypt.cpp:108–127` |
| `CSerialFile` `std::string` wire layout | `DWORD (str.length() + 1)` followed by NUL-terminated chars | `Lib_Engine/Common/SerialFile.cpp:365–373, 520–533` (`operator<<` / `operator>> std::string`) |
| `CSerialFile` `bool` / `BeginBlock` | 1-byte `bool`; 4-byte `DWORD dwSize` written at block start by `BeginBlock`/`EndBlock` | `Lib_Engine/Common/SerialFile.cpp:193–226, 355–363, 509–518` |
| `GLMapList` file head & version | `_FILEHEAD = "GLMAPS_LIST"`, `VERSION = 0x0200`, `VERSION_ENCODE = 0x0200`, `VERSION_ENCODE_OLD = 0x0100`, `MAPLIST_MAX = 1000` | `Lib_Client/G-Logic/GLMapList.h:18–24`; `Lib_Client/G-Logic/GLMapList.cpp:14` |
| `GLMapList::LoadMapsListFile` crypt rule | `dwFILEVER >= 0x0100` → `EMBYTECRYPT_OLD`; `dwFILEVER >= 0x0200` → `EMBYTECRYPT_MAPSLIST` (`ARRAY_MAPSLIST_VAR1`) | `Lib_Client/G-Logic/GLMapList.cpp:133–138`; `Lib_Engine/Common/ByteCrypt.cpp:47`; `ByteCryptDefVer1.h:387` |
| `SMAPNODE_DATA` record format | `VERSION = 0x0203`; `dwVer`, `bUsed`, `strFile`, `wMainID`, `wSubID`, `dwFieldSID`, 11 zone bools, `strMapName`, `strBGM`, `strLoadingImageName`, 16 feature/block bools | `Lib_Client/G-Logic/GLMapNode.h:15, 20–62`; `Lib_Client/G-Logic/GLMapNode.cpp:128–185` (`SMAPNODE_DATA::LOAD`) |
| Map ID & name limits | `MAXLANDMID = 256`, `MAXLANDSID = 256`, `MAP_NAME_MAX = 16`, uniqueness check on `(sNativeID)` and `strFile` | `Lib_Client/G-Logic/GLMapList.cpp:25–39, 158–200`; `enginelib/DxOctree/DxLandDef.h:14–16`; `GLMapNode.h:15` |
| `GLLevelFile` file ext & version | `FILE_EXT = "glmap"`, `VERSION = 0x0200`, `VERSION_NEW_ENCODE = 0x0200`, `VERSION_BEFORE_ENCODE = 0x0100` | `Lib_Client/G-Logic/GLLevelFile.h:40–47`; `Lib_Client/G-Logic/GLLevelFile.cpp:11` |
| `GLLevelFile::LoadFile` crypt rule | `dwFileVER >= 0x0100` → `EMBYTECRYPT_OLD`; `dwFileVER >= 0x0200` → `EMBYTECRYPT_LEVEL_NEW` (`ARRAY_LEVEL_VAR1_NEW`); dispatches `LOAD_200` | `Lib_Client/G-Logic/GLLevelFileSaveLoad.cpp:179–220`; `ByteCrypt.cpp:69`; `ByteCryptDefVer1.h:24` |
| `SLEVEL_HEAD` `0x0102` vs `0x0101` | `LOAD_0102`: `m_strMapName`, `m_strWldFile`, `m_eDivision`, `m_emBright` (swapped vs `LOAD_0101`: `wldFile, mapName, bright, division`); verifies `dwRead == dwSIZE` | `Lib_Client/G-Logic/GLLevelHead.h:29–46`; `Lib_Client/G-Logic/GLLevelHead.cpp:8–64` |
| `EMDIVISION` & `EMBRIGHT` bounds | `EMDIVISION`: powers of 2 from `QUAD_DIV_01=2` to `QUAD_DIV_10=1024`; `EMBRIGHT`: `0..2` (`BRIGHT_LIGHT`, `BRIGHT_DARK`, `BRIGHT_BOTH`) | `enginelib/G-Logic/GLQuadTree.h:5–17`; `RanClientLib/G-Logic/GLCharDefine.h:698–705` |

---

## 3. ByteCrypt Tables & `CSerialFile` Container Framing

### 3.1 `EP7ByteCryptMode` (`RANMobile/Data/Importer/EP7SerialCodec.h`)
Added `EP7ByteCryptMode` supporting:
- `EP7ByteCryptMode::None` (`0`) — pass-through identity.
- `EP7ByteCryptMode::Old` (`1`) — `ARRAY_OLD` (`ByteCryptDef.h:24`, identical to `enginelib/Common/CompByte.cpp:10–38`).
- `EP7ByteCryptMode::MapsList` (`21`) — `ARRAY_MAPSLIST_VAR1` (`ByteCryptDefVer1.h:387`).
- `EP7ByteCryptMode::LevelNew` (`42`) — `ARRAY_LEVEL_VAR1_NEW` (`ByteCryptDefVer1.h:24`).

Each 256-entry encoding table was copied verbatim from the recovered source headers and checked byte-for-byte with Python against `ByteCryptDef.h` and `ByteCryptDefVer1.h`, plus runtime bijection validation (`EP7Serial::IsValidBijection`). Decoding constructs the exact 256-byte inverse lookup table as `BYTECRYPT::byte_decode()`.

### 3.2 `CSerialFile` Framing & Header Quirks (`RANMobile/Data/Importer/EP7SerialFile.{h,cpp}`)
- Added `EP7SerialFile::ReadHeaderOnly()`, `EP7SerialFile::Read(data, cryptMode)`, `ReadU8()`, `ReadU16()`, `ReadBoolStrict()`, and hardened `ReadString()`.
- **Important authentic header observation**: `basestream` initializes `m_szFileType` to `"default"` (`basestream.h:143`) before `StringCchCopy` writes `"glmap"` (`SerialFile.cpp:38`). Consequently, authentic `innerzone_01.Lev` bytes `0..7` contain `"glmap\0t\0"` (a leftover `'t'` at byte 6 after the first NUL). `ReadHeaderOnly()` stops at the first NUL (`memchr`), rejects missing NUL / empty `fileType` / non-printable ASCII before the first NUL, and tolerates trailing buffer bytes after the first NUL inside the 128-byte slot.
- `ReadString()` rejects `length < 1`, `length > maxLength`, `length > remainingPayload`, missing terminating NUL, and embedded NUL bytes inside the declared length.
- `ReadBoolStrict()` accepts only `0x00` or `0x01`, rejecting any byte `> 1` that would indicate corrupted data or a wrong ByteCrypt key.

---

## 4. Authentic Binary `GLMAPS_LIST` (`mapslist.mst` `0x0200`) Decoder

Implemented in `EP7BinaryMapListParser::Parse()` (`RANMobile/Data/Importer/EP7SceneData.{h,cpp}`):
1. Validates the 132-byte `CSerialFile` header (`fileType == "GLMAPS_LIST"`).
2. In `StrictAuthentic` mode, requires `fileVersion == 0x0200` and decodes the payload with `EP7ByteCryptMode::MapsList`.
3. Reads `dwNum` (`1 <= dwNum <= 1000` per `MAPLIST_MAX`).
4. Parses each `SMAPNODE_DATA` entry (`0x0203` required in `StrictAuthentic`; `0x0100..0x0203` accepted only in `LegacySourceCompat`):
   - `dwVer`, `bUsed`, `strFile`, `sNativeID.wMainID`, `sNativeID.wSubID`, `dwFieldSID`,
   - 11 zone flags (`bPeaceZone` .. `bClubBattleZone`),
   - `strMapName`, `strBGM`, `strLoadingImageName`,
   - 16 flags (`bInstantMap`, `bQBoxEnable`, `bLunchBoxForbid`, `bCPReset`, `bPKMap`, `bUIMapSelect`, `bUIMapInfo`, `bClubPKRecord`, `bOpenPrivateMarket`, `bPartySparring`, `bBlockTaxi`, `bBlockFriendCard`, `bBlockRecall`, `bBlockTeleport`, `bDisableSystemBuffs`, `bBlockHPPotion`).
5. Enforces bounds and safety rules:
   - `wMainID < 256` (`MAXLANDMID`) and `wSubID < 256` (`MAXLANDSID`),
   - `strMapName.size() <= 16` (`MAP_NAME_MAX`), printable ASCII only,
   - `strFile`, `strBGM`, `strLoadingImageName` contain no `/`, `\`, `:`, `..`, or control bytes; non-empty `strFile` ends in `.lev` (case-insensitive) in strict mode,
   - `bUsed` records have unique `(wMainID, wSubID)` and unique case-insensitive `strFile`,
   - **Exact byte consumption check**: rejects any trailing unparsed payload bytes (`offset != view.payload.size()`), and requires `usedRecords > 0`.

### Verified Authentic `mapslist.mst` Result
- File: `external/recovery/phase3g/authentic/mapslist.mst`
- SHA-256: `cc52d4d0483a7dcd91ced28ac6de783eca0859736ea13bebe5fa7a49cd23488f`
- Format / Version: `GLMAPS_LIST` `0x0200`, record version `0x0203`
- Records: **120** total, **104** used, **16** unused
- Exact consumption: **13,714 / 13,714 bytes** (`bytesConsumed == fileBytes`)
- Sample verified rows:
  - Record `[0]`: `used=true`, `file="innerzone_01.lev"`, `id=0/0`, `mapName="SG_Campus1F"`, `bgm="ran07.ogg"`, `loading="loading_032.dds"`
  - Record `[1]`: `used=false`, `file="Sea_Death.LEV"`, `id=1/0`, `mapName="Halloween Map"`
  - Record `[2]`: `used=true`, `file="w_school_01.Lev"`, `id=2/0`, `mapName="SG_Campus"`, `loading="loading_054.dds"`

---

## 5. Authentic `.Lev` (`glmap` `0x0200`) & `SLEVEL_HEAD` (`0x0102`) Decoder

Updated `EP7LevelHeaderParser::Parse()` (`RANMobile/Data/Importer/EP7SceneData.{h,cpp}`):
1. In `StrictAuthentic` mode:
   - Requires `fileType == "glmap"` and `fileVersion == 0x0200`.
   - Decodes payload with `EP7ByteCryptMode::LevelNew`.
   - Reads `headVersion` and `headBlockSize`; requires `headVersion == 0x0102`.
   - Reads fields in exact `SLEVEL_HEAD::LOAD_0102` order: `m_strMapName`, `m_strWldFile`, `m_eDivision`, `m_emBright`.
   - Enforces exact `SLEVEL_HEAD` block size match (`headBlockBytesConsumed == headBlockSize`, mirroring `dwRead == dwSIZE` in `GLLevelHead.cpp:61–62`).
   - Validates `m_strWldFile` is non-empty, path-safe, printable ASCII ending in `.wld` (case-insensitive), `m_eDivision` is a power of 2 in `2..1024`, and `m_emBright` is in `0..2`.
   - Records `bytesConsumed` (header + `SLEVEL_HEAD`) and `remainingPayloadBytes` (unparsed `LOAD_200` subsections after `SLEVEL_HEAD`) without pretending to parse missing `LOAD_200` headers.
2. In explicit `LegacySourceCompat` mode only: preserves compatibility with synthetic `GLLEVEL` `0x0104` / `SLEVEL_HEAD` `0x0101` (`EMBYTECRYPT_OLD`).

### Verified Authentic `innerzone_01.Lev` Result
- File: `external/recovery/phase3g/authentic/innerzone_01.Lev`
- SHA-256: `f1738a67987929d0c2e62248f2987b647fc22516f7cc32835645a1fd24a5235c`
- File Size: **12,654 bytes**
- Header / Head Version: `glmap` `0x0200` / `SLEVEL_HEAD` `0x0102`
- `SLEVEL_HEAD` Block: **34 / 34 bytes** consumed (`174` total bytes including 132-byte `CSerialFile` header and 8-byte `dwVer`/`dwSize`; `12,480` remaining `LOAD_200` subsection bytes explicitly left unparsed)
- Decoded Metadata: `m_strMapName=""`, `m_strWldFile="innerzone_01.wld"`, `m_eDivision=16`, `m_emBright=2`
- Also preserved `mapslist.mst` display name (`"SG_Campus1F"`) in `EP7SceneIndexBuilder::Build` when `SLEVEL_HEAD.m_strMapName` is empty (fixing a bug where the old builder overwrote non-empty `mapslist.mst` names with empty `.Lev` `mapName`).

---

## 6. Strict No-Fallback Validation & SHA-256 Manifest Verification

1. **`EP7ValidationMode::StrictAuthentic` (Default)**:
   - Default parameter on `EP7BinaryMapListParser::Parse`, `EP7LevelHeaderParser::Parse`, `EP7SceneIndexBuilder::Build`, `EP7SceneRenderer::Load`, and `RANMobileEngine::LoadScene`.
   - Never falls back to `EP7MapListParser::Parse` (INI), never coerces `0x0104`/`0x0101` or `GLMAPS_LIST 0x0100`, never invents demo maps or fallback textures, and fails if any existing `.Lev` file on disk fails validation (`invalidLevelHeaders > 0`) or if 0 valid `.Lev` headers resolve (`resolvedLevelHeaders == 0`).
   - `EP7SceneBuildOptions` additionally provides `requireAllReferencedLevels` and `requireReferencedWlds` for complete-client drops, plus `manifestPath` for cryptographic verification.
2. **Case-Insensitive Asset Lookup**:
   - Authentic `mapslist.mst` specifies `"innerzone_01.lev"` while the authentic file on disk is `"innerzone_01.Lev"`. `FindFileCaseInsensitive()` in `EP7SceneData.cpp` and `EP7SceneRenderer.cpp` resolves exact-case first and case-insensitive second on Linux/Android filesystems, while rejecting `/`, `\`, `:`, and `..` traversal attempts.
3. **`EP7HashVerify` (`RANMobile/Data/Importer/EP7HashVerify.{h,cpp}`)**:
   - Pure C++17 FIPS 180-4 SHA-256 implementation (`Sha256Hex`) and TSV manifest parser (`LoadManifest`, `VerifyBytesAgainstExpected`, `VerifyBytesAgainstManifest`).
   - Verified against `external/recovery/phase3g/retrieval_manifest.tsv` (**24 / 24** retrieved LFS objects).
   - `RANMobileEngine::LoadSceneWithManifest()` / `EP7SceneBuildOptions::manifestPath` verifies `mapslist.mst`, every found `.Lev` file, and every found loading image / `.wld` file against the manifest before accepting scene metadata; any single-byte modification fails immediately with exact expected vs actual SHA-256 diagnostics.

---

## 7. Importer Separation & Future Android Scene-Preview Interface

- **Rendering Independence**: `EP7HashVerify`, `EP7SerialCodec`, `EP7SerialFile`, and `EP7SceneData` depend only on standard C++17 headers and `Logger.h` — zero dependency on OpenGL ES, EGL, Android JNI, or `MobileRenderer`.
- **Scene-Preview Integration**: `EP7SceneRenderer` consumes `EP7SceneIndexBuilder::Build` in `StrictAuthentic` mode by default, exposing `GetValidationMode()`, `GetResolvedLevelCount()`, `GetSceneEntryCount()`, `GetIndex()`, and `GetStatus()`, while `RANMobileEngine` exposes `LoadScene(root)`, `LoadScene(root, mode)`, and `LoadSceneWithManifest(root, manifestPath)`.
- **Verified Authentic Staging Check**: Against a clean staged tree containing copies of `mapslist.mst`, `innerzone_01.Lev`, `loading_032.dds`, and `retrieval_manifest.tsv`, `EP7SceneRenderer::Load` resolved `SG_Campus1F` → `innerzone_01.Lev` (`16` div / `2` bright / `innerzone_01.wld`) and decoded `loading_032.dds` (1024×512 DXT1) for preview rendering (`"ORIGINAL ASSET TEXTURE READY: loading_032.dds (1024x512)"`), while accurately recording `missingLevels=103` and `wldFound=false`.

---

## 8. Files Created & Modified

| File | Role |
|---|---|
| `RANMobile/Data/Importer/EP7HashVerify.h`, `.cpp` | Pure C++17 FIPS 180-4 SHA-256 & Phase 3G TSV manifest verifier |
| `RANMobile/Data/Importer/EP7SerialCodec.h` | `EP7ByteCryptMode` (`None`, `Old`, `MapsList`, `LevelNew`) + verified source tables |
| `RANMobile/Data/Importer/EP7SerialFile.h`, `.cpp` | `ReadHeaderOnly`, parameterized `Read`, `ReadU8/U16/U32`, `ReadBoolStrict`, hardened `ReadString` |
| `RANMobile/Data/Importer/EP7SceneData.h`, `.cpp` | `EP7ValidationMode`, `EP7BinaryMapListParser`, updated `EP7LevelHeaderParser`, `EP7SceneBuildOptions`, case-insensitive `EP7SceneIndexBuilder` |
| `RANMobile/Engine/Scene/EP7SceneRenderer.h`, `.cpp` | Strict default loading, `EP7SceneBuildOptions` support, case-insensitive texture lookup, metadata inspection accessors |
| `RANMobile/Engine/Core/RANMobileEngine.h`, `.cpp` | `LoadScene(root, mode)` and `LoadSceneWithManifest(root, manifestPath)` |
| `RANMobile/CMakeLists.txt` | Added `Data/Importer/EP7HashVerify.cpp` to `RANMOBILE_SOURCES` |
| `.github/workflows/build-ranmobile-android.yml` | Added `RANMobile/Data/Importer/EP7HashVerify.cpp` to Linux C++17 test compile |
| `RANMobile/Tests/test_main.cpp` | Updated section 30 (`StrictAuthentic` rejects legacy `0x0104`/INI; explicit `LegacySourceCompat` passes) and added sections 32–35 |
| `PHASE3H_BINARY_MAP_IMPORTER.md`, `SOURCE_DATA_RECOVERY.md`, `BLOCKERS.md`, `TESTING_CHECKLIST.md` | Phase 3H architecture, traceability, blocker, and verification documentation |

---

## 9. Native Build, CI Verification & Test Results

### 9.1 Local Native Compile & Tests (Linux sandbox, `g++ 12.2.0`)
- **Compile**: `g++ -std=c++17 -Wall -Wextra -Werror` across all 37 `RANMobile/` C++ sources + `RANMobile/Tests/test_main.cpp` → **exit 0** (`external/recovery/phase3h_compile.log`, 0 bytes / 0 warnings).
- **Test Run**: `RANMOBILE_REQUIRE_AUTHENTIC_PHASE3G=1 ./external/recovery/phase3h_tests` → **exit 0**, `ALL TESTS PASSED!` (`external/recovery/phase3h_test_output.txt`), plus default run without env var → **exit 0**.
- **Test Coverage (Sections 30 & 32–35)**:
  - **Section 30**: Proves `StrictAuthentic` rejects legacy `GLLEVEL 0x0104` and plain-text INI `mapslist.mst`, while explicit `LegacySourceCompat` succeeds.
  - **Section 32**: Verifies bijection and round-trip encode/decode across `None`, `Old`, `MapsList`, `LevelNew`, plus FIPS 180-4 SHA-256 vectors (`""`, `"abc"`), digest mismatch, and byte-size mismatch rejection.
  - **Section 33**: Source-layout binary `GLMAPS_LIST 0x0200` + `glmap 0x0200 / SLEVEL_HEAD 0x0102` synthetic round-trip, case-insensitive lookup (`zone_a.lev` → `Zone_A.Lev`, `load_a.dds` → `LOAD_A.DDS`), `requireReferencedWlds` rejection, and 1-byte tamper rejection via manifest.
  - **Section 34**: Adversarial / strict no-fallback suite — empty files, truncated headers (`< 132` B), unterminated/non-ASCII `fileType`, plain INI and JSON inputs, unsupported file versions (`0x0100`, `0x0300`), unsupported record version (`0x0202`), invalid boolean bytes (`0x02`, `0x42`), out-of-bounds IDs (`>= 256`), `mapName > 16`, path traversal (`../evil.lev`, `dir/evil.lev`, `../a.wld`), invalid extensions (`.txt`), duplicate IDs/filenames, trailing unparsed payload byte, zero used records, `dwNum > 1000`, malformed strings (0-length, missing NUL, embedded NUL, overflow), `SLEVEL_HEAD` block size mismatch, invalid `EMDIVISION`/`EMBRIGHT`, and missing-`.Lev` scene index rejection.
  - **Section 35**: Authentic Phase 3G verification — validates **24 / 24** entries in `external/recovery/phase3g/retrieval_manifest.tsv`, authentic `mapslist.mst` (`13,714 / 13,714` B, `120` records, `104` used, exact fields and bounds on all 120 rows), authentic `innerzone_01.Lev` (`glmap 0x0200`, `SLEVEL_HEAD 0x0102`, `34/34` B block, `174` B consumed, `12,480` B remaining, `innerzone_01.wld`, `div=16`, `bright=2`), authentic `login.wld` (`LAND.MAN 0x0108`), end-to-end staged `LoadSceneWithManifest` decoding `loading_032.dds` (1024×512), `requireAllReferencedLevels` failure on 1-of-104 staging, and strict rejection when 1 byte of copied `innerzone_01.Lev` or `loading_032.dds` is corrupted.

### 9.2 GitHub Actions CI (`Build RANMobile Android (ARM64) & Verify EP7 Release`)
- **Run ID**: `45882553115` on commit `2ea77ce3f364a7404d86b351e46febaf9399534a` (`arena/be1e147d-ran-online-ep7`), Job `140125196382` (`build-android-arm64`, 3m37s) → **`success`**.
- All steps succeeded: EP7 Release extraction (`ran-ep7-full-v1`), Linux C++17 compile (`-Wall -Wextra -Werror`) and test execution (`test_main.cpp`), Android NDK `r26d` (`android-24`, `arm64-v8a`) CMake build of `libranmobile.so`, Java 17 / Gradle 8.5 `assembleDebug`, `assembleRelease`, and `bundleRelease`.
- Uploaded CI Artifacts: `libranmobile-arm64-v8a` (**2,032,860 B**), `ranmobile-android-packages` (**1,938,104 B**), `linux-test-failure-log` (**1,835 B**, `if: always()` summary upload).
- Note: Because `external/recovery/phase3g/` is git-ignored, CI runs sections 1–34 (including the binary `GLMAPS_LIST 0x0200` / `SLEVEL_HEAD 0x0102` synthetic round-trip and adversarial rejection tests) and prints `[PHASE 3H] Authentic Phase 3G local files absent; manifest checks skipped (CI mode)` for section 35, whereas local execution with `RANMOBILE_REQUIRE_AUTHENTIC_PHASE3G=1` enforces section 35 unconditionally.

---

## 10. Limitations, Unparsed Sections & Explicit Non-Claims

- **Unparsed `LOAD_200` subsections**: After `SLEVEL_HEAD` (`174` B), `innerzone_01.Lev` contains `12,480` bytes belonging to `SLEVEL_REQUIRE`, `GLMobScheduleMan`, `DxLandGateMan`, `DXLANDEFF`, `GLLANDMARK`, and `SLEVEL_ETC_FUNC` (`GLLevelFileSaveLoad.cpp:552–626`). Their Phase 3G headers were not retrieved and are not guessed or decoded here.
- **No `.wld` geometry parser or playable scene**: Only `innerzone_01.wld` is resolved as a metadata string dependency; `innerzone_01.wld`/`.wld0` bytes were not downloaded in Phase 3G, and no 3D map geometry, collision octree, portals, or entities are parsed or rendered.
- **No `EMBYTECRYPT_*2` (`ByteCryptDefVer2.h`) paths**: Only `None`, `Old`, `MapsList` (`VAR1`), and `LevelNew` (`VAR1_NEW`) are enabled because only those modes are referenced by verified authentic files and recovered `GLMapList` / `GLLevelFile` loaders.
- **No local Android toolchain or device execution**: Sandbox lacks SDK/NDK/JDK/Gradle/adb (Blocker B1). Android `arm64-v8a` `.so`, APK, and AAB compilation are verified via GitHub Actions Run `45882553115`, not local build or device/emulator execution.
- **No gameplay, server, or protocol changes**: `EP7Protocol.h` / Blocker B6, live server login (B3), and GameServer DX header conflict (B8) remain completely untouched, and PR #3 remains **OPEN / DRAFT**.
