# Phase 3C — Original RAN EP7 Asset Rendering & Scene Integration

Date: 2026-11-26 UTC. Active branch: `arena/075e1725-ran-online-ep7`. PR #3 remains **OPEN / DRAFT** and was not merged, closed, overwritten or edited. Implementation commit: `646bc83` (`Implement Phase 3C original asset decoding and scene metadata preview`); Android NDK API 24 input fix: `d2db415`. No isolated branch/new PR was created because this session is fixed to the active branch.

This phase contains real C++ changes in `RANMobile/` only. No original PC source, protocol constant, packet, Server source, DB schema, gameplay logic or recovered archive was modified. No unknown binary/script was executed and no secret was committed.

## 1. Source and asset evidence (read before changing code)

Inventory performed first, on the clean Phase 3B tree at `37d8e9e`:

- `external/ep7_release/thaipack_original`: **730 `.qst`, 309 `.ntk`, 7 `.txt`**; no `.dds`, `.mtf`, `.wld`, `.glmap`, `mapslist.mst` or other model/map/texture file.
- `external/recovery/RanOnlineEP7-Client_paths.txt` (archive path inventory): **0** entries with texture/model/map extensions (`.dds|.mtf|.wld|.mst|.mnsf|.isf|.ssf|.cps|.chf|.ntk|.qst|.tga|.egp`). The client archive contains source/layout only in this checkout.
- Sampled real files: `data/glogic/npcshop.ntk`, `data/glogic/quest/*.qst`, `data/gui/*_th.txt`. The `.ntk`/`.qst` files begin with the original `basestream` 128-byte `"default"` file-type field (`enginelib/Common/basestream.h:151`) followed by a DWORD file ID; the rest is not identified by any recovered loader path in this checkout. They are **not** claimed to be decoded.

Source references used for each implemented format are listed in the classification table. Extensions alone were never used as proof.

### 1.1 Format classification

| Format / data | Status | Evidence | Mobile result |
|---|---|---|---|
| Encrypted `.mtf` container (12-byte header, `TEX_VERSION=0x100`, `+3` then `^0x47`) | **Verified + implemented** | `enginelib/DxCommon/EncryptTextureDef.h`; `enginelib/DxCommon/MemoryTexture.cpp:66-101` | `EP7TextureDecoder::DecryptMTF()` — exact version and exact file-size validation, safe byte transform. |
| DDS header (magic, dims, mip count, pixel format/masks/FourCC) | **Verified + implemented (subset)** | `enginelib/DxCommon/TextureManager.cpp:145-314`; FourCC constants in `LegacyDX_Include/d3d8types.h`, `d3d9types.h`, `ddraw.h` | `InspectDDS()` validates 128-byte header, `ddsMagic`, dimensions 1..4096, depth, pixel-format size, masks and known formats. |
| DDS DXT1 / DXT3 / DXT5 level 0 → RGBA8 | **Verified + implemented** | `TextureContainer::LoadImageData` passes raw DDS bytes to `D3DXCreateTextureFromFileInMemoryEx` and switches on `D3DFMT_DXT1/DXT3/DXT5` (`TextureManager.cpp:203-314`) | Full block decoders including DXT1 1-bit alpha, DXT3 explicit alpha and DXT5 interpolated alpha. Truncated level-0 data is rejected. |
| Uncompressed DDS: A8R8G8B8, X8R8G8B8, R8G8B8, R5G6B5, A1R5G5B5, A4R4G4B4 | **Verified + partially implemented** | Same D3D format switch in `TextureManager.cpp:271-303` | Decoded via generic mask reader. Other original formats (`A8R3G3B2`, `X1R5G5B5`, `X4R4G4B4`, `R3G3B2`, `A8`) are **verified but not implemented**. |
| Mipmaps / volume / cube textures | **Verified but not implemented** | `TextureManager.cpp:147-200` uses `m_dwStage` and cube paths | Header mip count is reported; only level 0 is decoded. Volume/cube data is explicitly rejected, not guessed. |
| Alpha modes `_a`, `_a1`, `_a2` and D3D format rules | **Verified + implemented** | `TextureManager.cpp:246-314` | `AlphaModeFromName()` mirrors the original suffix and format mapping; hard/cutout mode uses a `0.5` alpha-test cutoff in GLES3, soft modes use source-alpha blending. |
| `CSerialFile` header (128-byte file type + DWORD file ID/version) | **Verified + implemented** | `enginelib/Common/SerialFile.cpp:46-65`; `GLLevelFile.cpp:1356-1364` | `EP7SerialFile::Read()` with strict bounds. |
| `compbyte::decode()` byte permutation for encoded serialized data | **Verified + implemented** | `enginelib/Common/CompByte.cpp:10-38`; `SerialFile.cpp:224-242` | `EP7SerialCodec.h` contains the exact 256-byte table copied from source and its inverse. |
| `std::string` payload (DWORD length incl. NUL + bytes) | **Verified + implemented** | `SerialFile.cpp:460-470, 609-624` | `EP7SerialFile::ReadString()` with length and max-size checks. |
| `.glmap` `GLLevelFile` file version `0x0104`, encoded when file ID `>=0x0100` | **Verified + implemented (header only)** | `GLLevelFile.h:231`; `GLLevelFile.cpp:1337-1392` | `EP7LevelHeaderParser` reads and validates version before decoding payload. |
| `SLEVEL_HEAD` `VERSION=0x0101`, WLD filename, map name, brightness, division | **Verified + implemented** | `GLLevelFile.h:118-138`; `GLLevelFile.cpp:629-656` | `EP7LevelHeaderParser` validates block version and size. |
| `mapslist.mst` `[MAP_LIST_INFO] MapKeySize` / `[MAP_LIST] MAP` fields 0..19 | **Verified + implemented (plain-text variant)** | `RanClientLib/G-Logic/GLMapList.cpp:90-162`; separators `,[]\t` | `EP7MapListParser` reads used/file/IDs/field SID/map name/BGM/loading image/instant map with strict row-length checks. |
| Encoded `mapslist.mst` (Rijndael version header or `compbyte` fallback) | **Verified, not implemented** | `IniLoader.cpp:239-253` passes `bDEC`; `StringFile.cpp:71-100,150-180` | Rejected with a diagnostic; no key/version guess. |
| `.wld` geometry (`DxLandMan`) | **Partial evidence, not implemented** | `GLAGLandMan::LoadWldFile` checks `DxLandMan::FILEMARK` and `VERSION` then reads `SLAND_FILEMARK` (`GLAGLandMan.cpp:22-60`) **but no `.wld` file exists in the recovered trees** | Scene geometry, collisions, gates and entities are **not** implemented; no fake map is created. |
| `.ntk` / `.qst` payloads | **Unknown** | Real files exist; no corresponding loader path or matching asset manifest is present in the recovered trees | Not decoded; explicitly out of scope. |
| Thai UI `.txt` strings | **Partial** | Real files exist (`data/gui/*_th.txt`) | Not integrated into renderer text; no invented translations. |

## 2. Implemented code

### New files
- `RANMobile/Data/Importer/EP7SerialCodec.h` — source-exact byte table and inverse decoder.
- `RANMobile/Data/Importer/EP7SerialFile.h/.cpp` — safe `CSerialFile` header/string reader.
- `RANMobile/Data/Importer/EP7SceneData.h/.cpp` — `mapslist.mst` and `.glmap` `SLEVEL_HEAD` parsers plus `EP7SceneIndexBuilder`.
- `RANMobile/Engine/Scene/EP7SceneRenderer.h/.cpp` — render-only integration of original metadata and one original loading texture, when such a file is actually present.

### Changed files
- `RANMobile/Data/Importer/EP7TextureDecoder.h/.cpp` — replaced the over-broad API with source-grounded structs, exact MTF validation, DDS header validation, DXT1/3/5 decode, mask-based uncompressed subset, original alpha-mode rules and bounded allocations. The previous comment claiming general uncompressed RGBA8/BGRA8 support was removed.
- `RANMobile/Engine/Renderer/MobileRenderer.h/.cpp` — keyed asset-texture cache with retained CPU pixels and re-upload after surface recreation, `DrawTexturedQuad` for RGBA8 textures, alpha-test uniform, public safe-area inset setter.
- `RANMobile/Engine/Core/RANMobileEngine.h/.cpp` — `LoadScene()`, `GetSceneStatus()`, `ScenePreview` state, toggleable diagnostics, safe-area forwarding, scene unload on shutdown.
- `RANMobile/Android/native_bridge.cpp` — `DisplayCutout` safe-area read through JNI (API 28+, degrades to zeros), keyboard visibility tracking on focus change/pause/surface loss, JNI cleanup on exception.
- `RANMobile/Tests/test_main.cpp` — test sections 27–30 and fixes to the earlier DDS/MTF fixtures to match the actual original constants.
- `RANMobile/CMakeLists.txt`, `.github/workflows/build-ranmobile-android.yml` — register the three new `.cpp` files.

`EP7AssetPipeline` remained unchanged: the new decoders are called by `EP7SceneRenderer`/parsers, and decoded textures are registered through `MobileRenderer`/`ResourceManager` without changing the existing scanner contract.

### Mobile constraints
- All allocations are bounded by validated header dimensions (max 4096×4096) and actual file size; malformed/truncated data returns an error with no partial texture.
- One CPU RGBA8 copy is retained per cached texture key solely for Android surface recreation; no per-frame decode or allocation.
- Scene metadata scanning is read-only and recursive over the supplied root; no original archive is modified in place.
- No device-side FPS/visual measurement is claimed.

## 3. Scene status

A minimal **real metadata pathway** is implemented: `mapslist.mst` → `.glmap` header → referenced loading texture. `EP7SceneRenderer` draws a clearly labelled **"ORIGINAL EP7 ASSET PREVIEW — NOT A PLAYABLE SCENE"** screen when an original DDS/MTF loading texture is actually found. With the currently recovered files there is no `mapslist.mst`, no `.glmap` and no DDS/MTF, so the real scene index resolves to zero entries and the renderer reports metadata-unavailable; no placeholder scene is invented. `.wld` geometry, entity placement, colliders, gates and gameplay are not implemented.

## 4. Android input / display investigation

- Soft keyboard: focus changes show/hide `InputMethodManager` only on transition, and pause/surface-loss hide it. Hardware ASCII key events remain the only committed text path. An attempt to use `AKeyEvent_getUnicodeChar()` failed to compile against NDK r26d with `ANDROID_PLATFORM=android-24` (`native_bridge.cpp:300:33: use of undeclared identifier 'AKeyEvent_getUnicodeChar'`, CI run `45862660028`); it was removed. Full IME compose/UTF-8 commit remains a blocker, not an implemented feature.
- DisplayCutout: `View.getRootWindowInsets().getDisplayCutout().getSafeInset{Left,Top,Right,Bottom}` is queried on window init and config change and forwarded to `RenderLayout` and `LoginScreen`. Missing API/class/JNI state degrades to zero insets and clears exceptions.
- Orientation: the manifest still hard-locks landscape. The virtual canvas already supports portrait, but no rotation-policy change was made and no portrait device verification is claimed.

## 5. Verification

Local (sandbox):
- Compile: `g++ -std=c++17 -Wall -Wextra -Werror` over all 36 RANMobile sources + tests → **exit 0**; log `external/recovery/phase3c_compile.log`.
- Tests: `./external/recovery/phase3c_tests` → **exit 0**, `ALL TESTS PASSED!`; log `external/recovery/phase3c_test_output.txt`.
- New tests: 27 DDS header/uncompressed decode/alpha rules/malformed header; 28 exact serial table and source-layout synthetic `CSerialFile`; 29 map-list field order and malformed row; 30 synthetic source-layout `.glmap` + DDS fixture through `EP7SceneIndexBuilder` → renderer cache → draw → unload, safe-area setter. The DDS/MTF fixtures are **source-derived synthetic files**, not recovered original binaries; real recovered `.ntk`/`.qst` bytes are classified as unknown above.

CI: see `TESTING_CHECKLIST.md` for the run ID, per-step conclusions and artifact sizes. No device/emulator run is claimed.

## 6. Explicit non-claims

- No original binary texture/map was rendered because none exists in the recovered trees; only source-layout fixtures were decoded and rendered.
- No playable scene, NPC, monster, item, skill, quest, map or fake content was generated.
- No login protocol, Server, DB or gameplay logic was changed; B6/B8 remain open.
- No PR #3 operation was performed.
- STOP after Phase 3C; await the next instruction.
