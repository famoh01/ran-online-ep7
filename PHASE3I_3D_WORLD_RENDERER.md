# Phase 3I — Authentic EP7 3D World Parser, Texture Decoders & GLES3 Scene Renderer

Date: 2026-11-26 UTC. Active branch: `arena/be1e147d-ran-online-ep7` (implementation commit `21f510092e5a1fb81bce7c36bcb20d17e4304056`). PR #3 on `arena/075e1725-ran-online-ep7` (`7b355b3`) remains **OPEN / DRAFT** and untouched.

Phase 3I turns `RANMobile` from a 2D UI + loading-screen preview into a real OpenGL ES 3.0 3D EP7 world renderer driven directly by authentic EP7 `.wld`, `.wld0`, `.pie`, `.pis`, `.Lev`, `.dds`, `.tga`, and `.jpg` files — with zero fabricated geometry and zero alterations to original PC source or protocol constants.

---

## 1. Authentic Binary Assets Recovered & Verified (63 / 63)

Using `.github/workflows/phase3g_focused_lfs_fetch.yml` against `matheomedrana/Ran-Online-EP7-Complete-Files`, 63 Git-LFS / source reference objects (maps, pieces, textures, and `enginelib` scene loaders) were fetched and verified SHA-256 + byte-size against `external/recovery/phase3i/retrieval_manifest.tsv` (git-ignored under `external/recovery/phase3i/`):

- **World & Level Binaries**:
  - `innerzone_01.wld` (`1,190,775` bytes, `LAND.MAN` `0x0113`)
  - `innerzone_01.wld0` (`5,808,480` bytes, paired external octree payload)
  - Plus Phase 3G binaries `login.wld` (`3,098,590` bytes, `0x0108`), `innerzone_01.Lev` (`12,654` bytes, `glmap` `0x0200`), `mapslist.mst` (`13,714` bytes), `loading_032.dds` (`1024x512` DXT1)
- **Static & Animated Piece Assets (12 / 12 referenced by `innerzone_01.wld`)**:
  - 7 `.pie` (`e3_bossbd_chair.pie`, `e3_bossbd_desk.pie`, `ep3_1f_desk1.pie`, `ep3_1f_desk2.pie`, `ep3_bossbd_desklight.pie`, `ep3min_consol.pie`, `sg_1f_con_01.pie`)
  - 5 `.pis` (`1sc_1mainup_colum1.pis`, `1sc_1mainup_colum2.pis`, `1sc_1mainup_columside.pis`, `4sc_newdesk1.pis`, `innerzone_light.pis`)
- **Authentic World Textures (12)**:
  - 11 `.dds` (`01city-brick1.dds`, `01city-gazon3.dds`, `1school_in_inwall3.dds`, `1school_in_mainfloor7.dds`, `2fance1_a.dds`, `leaf.dds`, `logo1_a.dds`, `rail.dds`, `scr_001.dds`, `sky_001.dds`, `wall2.dds`)
  - 1 `.tga` (`dark_sky3.tga`)
- **Original Source References**: `enginelib/DxOctree/*`, `DxLand/*`, `DxPiece/*`, `DxCommon/DxFogMan.cpp`, `DxCamAniMan*`, `DxWeatherMan*`, `DxStaticMesh*`, `Lib_Client/G-Logic/{GLLevelFileSaveLoad,GLMobSchedule}*`.

---

## 2. Binary Importers & Texture Decoders

### 2.1 `EP7WorldParser` (`RANMobile/Data/Importer/EP7WorldParser.{h,cpp}`)
Pure C++17 strict binary parser consuming authentic EP7 map/piece/level formats to exact EOF:
- **`.wld` & `.wld0`** (`LAND.MAN` versions `0x0108`–`0x0114`): decodes `SLAND_FILEMARK` + `DXOCTREE` terrain/octree mesh chunks (`VERTEX` pos/normal/uv/D3DCOLOR → RGBA8888, alpha buckets `Opaque`/`HardAlpha`/`SoftAlpha`), `DxAnimationMan` animated frames, `DxCamAniMan` cameras, `DxLightMan` directional/point lights, `DxWeatherMan`, `DxFogMan` (`FOG_PROPERTY 0x0106`–`0x0108`, reading all 7 DWORDs including day/night colors), `DxSkyMan`, `CStaticSoundMan`, `DxLandGateMan`, `DXLANDEFF`, `DXLANDSKINOBJ`, `NavigationMesh` vertices/cells + spawn recommendation, and `DxPieceManager` placements (`NSPIECETREE`). Guards interior/empty octree `FLT_MAX` sentinel bounds (~`3.7e36`) so `worldBounds` stays finite.
- **`.pie` & `.pis`** (`DxPieceEdit` / `DxReplacePiece`): parses `DxStaticMesh` + `DxSingleTexMesh 0x0101` + `DxAABBOctree 0x0100` (`EMBLOCK_02` seeking past trailing collision nodes) and `DxFrame`/`DxMeshes` hierarchies to exact EOF; deduplicates assets via `ResolveAndAttachPieces`.
- **Full `.Lev`** (`glmap` `0x0200`, `EMBYTECRYPT_LEVEL_NEW`): extends `SLEVEL_HEAD` parsing through `SLEVEL_REQUIRE`, `GLMobScheduleMan` (`mobSpawns`), `DxLandGateMan` (`gates`), `DXLANDEFF`, `GLLANDMARK`, and `SLEVEL_ETC_FUNC` to exact EOF (`12,654 / 12,654` bytes on `innerzone_01.Lev`).

### 2.2 `EP7TextureDecoder` (`RANMobile/Data/Importer/EP7TextureDecoder.{h,cpp}`)
Added self-contained C++17 decoders alongside existing DDS (DXT1/DXT3/DXT5/uncompressed) and MTF (`^ 0x47 - 0x03`):
- **`DecodeTGA`**: uncompressed (`type 2/3`) & RLE (`type 10/11`), 8/16/24/32-bpp, color-map (`type 1/9`), bottom-up vs top-down origin flip (`descriptor & 0x20`), and 16-bpp 1-bit alpha attribute awareness (`descriptor & 0x0F`).
- **`DecodeJPEG`**: baseline `SOF0` JFIF/EXIF decoder (DQT, DHT DC/AC tables, 4:4:4 / 4:2:0 / 4:2:2 / grayscale MCU sampling, restart markers, Fast-LLM integer IDCT + BT.601 YCbCr→RGBA8888); explicitly rejects progressive `SOF2` rather than returning corrupt pixels.
- **`DecodeTextureFile`**: magic-byte & extension dispatcher handling `.dds`, `.tga`, `.jpg`/`.jpeg`, and encrypted `.mtf`.
- **`EP7AssetPipeline`**: registered `.wld0`, `.pie`, `.pis`, `.jpg`, `.jpeg` as valid EP7 inventory formats.

---

## 3. Left-Handed GLES3 3D Renderer, Camera & Scene Streaming

### 3.1 `MobileCamera` (`RANMobile/Engine/Camera/MobileCamera.{h,cpp}`)
- Left-handed (`LookAtLH`, `PerspectiveFovLH`) camera matrices matching EP7's D3D9 coordinate system and screen-space clockwise triangle winding (`z_ndc ∈ [-1, 1]`).
- Smooth damped orbit (`OnDrag`), pinch zoom (`OnPinch`), two-finger pan (`OnPan`), `Reset`, `SetProjectionParams`, `SetViewportAspect`, `GetEyePosition`, and 6-plane Gribb-Hartmann frustum extraction (`ExtractFrustumPlanes`).

### 3.2 `MobileRenderer` (`RANMobile/Engine/Renderer/MobileRenderer.{h,cpp}`)
- Dual GLES3 shader pipeline (`kWorldVertexShader` / `kWorldFragmentShader` with `precision highp float` alongside the 2D SDF UI/text pipeline).
- GPU resource registry (`RegisterMesh3D`, `UnregisterMesh3D`, `ClearMeshes3D`, `LoadWorldTexture`) storing CPU-side mesh copies so `OnSurfaceLost()` → `OnSurfaceCreated()` automatically re-uploads 3D VAO/VBO/IBO meshes and world textures across Android EGL surface loss.
- `BeginWorldPass` / `DrawMesh3D` / `EndWorldPass`: `glFrontFace(GL_CW)` preserving D3D9 triangle winding, transposed `D3DXMATRIX` model uploads, directional + up to 4 nearest dynamic point lights, linear distance fog, two-sided culling control, hard-alpha cutout threshold (`0.45`), and 6-plane frustum + closest-point scaled view-distance culling (`m_worldViewScale = 8.0f`, mapping `GraphicsSettings::viewDistance` `75..250` to `600..2000` EP7 units).

### 3.3 `EP7SceneRenderer`, `MobileHUD`, & `RANMobileEngine`
- `EP7SceneRenderer` (`RANMobile/Engine/Scene/EP7SceneRenderer.{h,cpp}`): streams `.wld`/`.wld0`/`.Lev`/`.pis`/`.pie` worlds with case-insensitive recursive lookup and `.mtf` texture fallback, preserves `loading_032.dds` preview fallback when no `.wld` is present (`!HasWorld3D()`), adds `LoadStandaloneWorld` / `SwitchScene` / `ConfigureDefaultCamera`, and renders 4 ordered passes:
  1. Sky dome (`m_world.sky.skyEnable` maps)
  2. Opaque terrain chunks & piece instances
  3. Hard-alpha cutout meshes
  4. Back-to-front sorted soft-alpha meshes (nearest-4 dynamic point lights selected per frame)
- `MobileHUD` (`RANMobile/UI/HUD/MobileHUD.cpp`): migrated drawing from physical pixel bounds to virtual design coordinates (`GetDesignWidth()` / `GetDesignHeight()`), fixing high-DPI scaling.
- `RANMobileEngine` & `native_bridge.cpp`: wired `ScenePreview` & `InGame` 3D rendering, HUD-first touch arbitration with 1-finger orbit + 2-finger pinch/pan camera gestures, `CharacterController::SetPosition` spawn sync, and runtime diagnostics HUD overlay (`DC`/`TRI`/`CULL`).

---

## 4. Verification Summary

| Check | Command / Run | Result |
|---|---|---|
| Local Linux C++17 (`-Wall -Wextra -Werror`) + authentic Phase 3G/3I assets | `RANMOBILE_REQUIRE_AUTHENTIC_PHASE3G=1 RANMOBILE_REQUIRE_AUTHENTIC_PHASE3I=1 ranmobile_tests` | **Exit 0, 0 warnings, ALL TESTS PASSED (tests 1–39)**: `63/63` Phase 3I manifest objects, `login.wld` (`3,098,590/3,098,590` B, `191` batches), `innerzone_01.{wld,wld0,Lev}` exact EOF (`140` batches, `579` pieces, `49` lights, `44` mobs, `10` gates), `12/12` `.pie`/`.pis`, `12/12` textures, multi-pass 3D render + EGL surface recreation |
| Local `__ANDROID__` GLES3 syntax/warning check | `g++ -std=c++17 -Wall -Wextra -Werror -D__ANDROID__ -c MobileRenderer.cpp EP7SceneRenderer.cpp RANMobileEngine.cpp` | **Exit 0, 0 warnings** |
| Android CI (`build-ranmobile-android.yml`) | Run **`45894091238`** (`21f5100`) | **`success`** across all steps: Linux C++17 tests, NDK `r26d` `arm64-v8a` `libranmobile.so` (**2,933,235 B**), `assembleDebug` / `assembleRelease` / `bundleRelease` APKs & AAB (**2,341,433 B**) |

---

## 5. Explicit Non-Claims & Next Phase Scope

- **Not tested on physical Android hardware / emulator in this sandbox** (B1: no local Android SDK/emulator); visual GLES3 pixel verification on device remains pending.
- **Skinned character & animation models (`.chf` / `.cps` / `.abl` / `.bin`)** are not yet rendered inside the 3D scene; this phase delivers static world terrain, `.pie`/`.pis` props, lighting, fog, sky, HUD, and camera/character spawn movement.
- **Live server login / gameplay packets** remain untouched per B3/B6.
