## 0. Sources collected and verification status

| Source | What was actually retrieved | Status |
|---|---|---|
| Main repo `famoh01/ran-online-ep7` (this checkout) | Full C++ PC client/server source, solution files, libraries, `RANMobile/` Android client, 5 GitHub Actions workflows | Available, surveyed |
| Existing PRs | PR #1 and PR #2 both MERGED. PR #2 (`arena/6c988b52-ran-online-ep7`) added `RANMobileEngine`, GLES3 renderer + Thai font atlas, `LoginScreen`, HUD, Android APK/AAB workflows and reports | Available, surveyed (continuation point) |
| GitHub Release `ran-ep7-full-v1` | 4 assets listed via `gh release view`: `RAN-EP7-Client-TH.zip.part00` (1,572,864,000 B), `.part01` (1,010,562,339 B), `RAN-EP7-ServerDBTools.zip` (49,791,515 B), `RAN-EP7-ThaiPack-FamHawaii.zip` (1,359,185 B) | Metadata only: `release-assets.githubusercontent.com` unreachable from this sandbox (SSL/SYSCALL); byte download blocked |
| Google Drive folder `1Y3VOeAeBzSbaB_o9oWLN16qD29b8Ro1m` | 12 files listed via Drive connector, including `RAN-EP7-Client-TH.00.zip` (1,572,864,000 B), `RAN-EP7-Client-TH.001.zip` (1,010,562,339 B), `RAN-EP7-ServerDBTools.zip` (49,791,515 B), two `RAN-EP7-ThaiPack-FamHawaii.zip` copies (1,359,185 / 2,266,660 B), `RanOnlineEP7-Client.zip` (22,378,796 B), `RanOnlineEP7-Server.zip` (19,230,299 B), `Ran-Online-Episode-7-main.zip` (6,538,502 B), Thai-text updates/tutorials | Thai pack was imported and validated in earlier work; the two >1 GB client archives exceed the connector's 100 MB download cap. `external/` is absent in this checkout (not persisted / ignored) |
| Backup repo `matheomedrana/Ran-Online-EP7-Complete-Files` | Cloned metadata (60,457 tracked paths): `Ran Online Game Client/` (`.dds` ×15,449, `.cps` ×11,357, `.x` ×3,888, `.chf` ×1,152, `.qst` ×882, `.ntk` ×318, `data/gui/*.xml`, `data/map/login.wld`), `Ran Online Database/` (`RanUser/RanGame1/RanLog/RanShop` `.sql` + `.bak`), compiled server + `cfg/ServerLogin.cfg`, `ServerAgent.cfg`, `ServerField.cfg`, `ServerSession.cfg`, `Filter.cfg`, `IPFilter.cfg` | Tree surveyed; every large file is a Git LFS pointer (~130 B). LFS batch API returned HTTP 403 and `media.githubusercontent.com` is unreachable, so no bytes were retrieved |
| Release `apk-latest` | `app-debug.apk` (1,510,336 B), produced by CI on a GitHub Actions runner (Android SDK present there) | Metadata only |

Conclusion of collection: source code and file *layout* are complete enough for architecture work; original binary assets, DB dumps and a local Android toolchain are not accessible here (see `BLOCKERS.md`).

## 1. Client / server boundary in the original PC game

The PC solution (`Smtm.sln`, `Smtm2022.sln`) separates at the process/library level:

- **Client process**: `Basic/` (MFC `CBasicApp`/`CBasicWnd`, `WinMain` equivalent `InitInstance`), `RanClientLib/` (stage machine: `DxGlobalStage.cpp`, `DxLobyStage.cpp`, `DxGameStage.cpp`), `RanClientUILib/` (all screens/controls), `EngineUILib/`, `enginelib/`, `EngineSoundLib/`, `netclientLib/`.
- **Server processes**: `LoginServer/`, `SessionServer/`, `AgentServer/`, `FieldServer/`, `GameServer/`, `BoardServer/`, `ServerController/`, `ServerManager/`, `ServerMessanger/`; shared logic in `RanLogic/`, `RanLogicServer/`, `netserverLib/`. DB access is ODBC/MSODBC via `netserverLib/s_COdbcEnv.*`, `s_COdbcGame*.cpp`; Windows IOCP/Winsock2 and `.cfg` files drive runtime configuration.
- **Shared data/protocol**: `RanClientLib/G-Logic/` holds the common game-logic structs and message IDs (`GLContrlMsg.h`), while `netclientLib/s_NetGlobal.h` + `s_NetMsg*.h` define `NET_MSG_GENERIC` packet IDs/structures used by both sides. Client login calls are in `netclientLib/s_NetClientMsgLogin.cpp`; mobile protocol mirror is `RANMobile/Engine/Network/EP7Protocol.h` (`LOGIN_2 = 1120`, `HEARTBEAT_CLIENT_ANS = 161`).
- **Support libraries in-repo**: `=DX_Lib`, `=DLL`, `=MAX*SDK`, `=TbbLib`, `=ZLIB`, `boost/`, `Vorbislib/`, `=DaumCrypt/`, `LegacyDX_Include/`, `ShimInclude/`.

Game content is *data-driven* from the original client files, not from code: maps (`.wld`/`.lev`), crow/NPC (`.mcf`, `.ntk`), items (`.isf`), skills (`.ssf`), quests (`.qst`), meshes (`.x`, `.cps`, `.chf`), textures (`.dds`, `.mtf`), animation configs (`.cfg`) inside `.rcc` archives. The mobile converters/validators in `RANMobile/Data/` read these formats; no new game entities are permitted.

## 2. Engine / graphics / UI technology

| Layer | Original PC | Mobile implementation already in repo |
|---|---|---|
| Windowing | Win32 + MFC (`Basic/Basic.cpp`, `CBasicApp::InitInstance`, `CWinApp`) | JNI `android_main` in `RANMobile/Android/native_bridge.cpp`, `AndroidManifest.xml` (landscape, GLES 3.0), Gradle project under `RANMobile/Android/` |
| Renderer | Direct3D 9 / D3DX (`IDirect3DDevice9`, `D3DX*`) in `enginelib/DxCommon`, `DxCommon9`, `DxEffect`, `DxOctree`, `dxframe`, `Meshs`, `TextTexture` | `RANMobile/Engine/Renderer/MobileRenderer.cpp` (OpenGL ES 3.0 + portable fallback, stb_truetype Thai font atlas), frustum culling/LOD/graphics settings |
| Input | Win32 messages, DirectInput-style mouse/keyboard and edit boxes (`enginelib/DxCommon/DxEditBox.*`) | `RANMobile/Engine/Input`, `Engine/Touch/TouchSystem`, `UI/Joystick/VirtualJoystick`, `UI/Buttons/MobileButton` |
| Audio | DirectSound (`EngineSoundLib/`) | `RANMobile/Engine/Audio/AudioManager` (OpenSL ES / portable stub) |
| Network | Winsock2 + async select in `netclientLib/s_NetClient.*` | `RANMobile/Engine/Network/NetworkClient.cpp` (POSIX sockets) + `EP7Protocol.h` compatibility constants |
| UI framework | `EngineUILib/GUInterface` base controls + `RanClientUILib/Interface` screens, driven by `InnerInterface.cpp` / `OuterInterface.cpp` | `RANMobile/UI/` (`Login`, `HUD`, `Buttons`, `Joystick`, `Inventory`, `Skills`, `Quest`, `Map`, `Chat`, `Settings`) |
| Build | Visual Studio `.vcproj`/`.vcxproj`, 40+ projects | CMake + Android NDK (`RANMobile/CMakeLists.txt`), workflows `build-ranmobile-android.yml`, `build-ep7-windows-server.yml`, `original-asset-import.yml`, `phase1-complete-files-audit.yml`, `build.yml` |

## 3. Login / Character Select / Main UI — exact files

**PC originals (all verified present):**
- Login screen: `RanClientUILib/Interface/LoginPage.cpp/.h` (ID/PW edit boxes, login button, `pNetClient->SndLogin(...)` / `ThaiSndLogin(...)`), `OuterInterface.cpp/.h`, `SelectServerPage.cpp`, `SelectServerChannel.cpp/.h`.
- Character select/create: `RanClientLib/DxLobyStage.cpp/.h` (stage state, `SCHARINFO_LOBBY`, `MAX_SELECTCHAR`), `RanClientUILib/Interface/SelectCharacterPage.cpp/.h`, `SelectCharacterCard.cpp/.h`, `SelectCharacterInfoPage.cpp/.h`, `SelectCharacterLeftPage.cpp/.h`, `SelectClassPage.cpp/.h`, `SelectSchoolPage.cpp/.h`, `SelectSexPage.cpp/.h`, `SelectServerPage.cpp/.h`, `DxCharSham.cpp/.h` (3D preview).
- Main/in-game UI: `RanClientUILib/Interface/InnerInterface.cpp/.h`, `InnerInterfaceGuid.h`, `InventoryPage.cpp/.h`, `BasicChat.cpp/.h`, `QuestMain.cpp/.h`, plus `RanClientUILib/Interface/` controls; 3D stage `RanClientLib/DxGameStage.cpp`.
- Data-driven UI skins: `data/gui/*.xml` and Thai text from the original client archives (backup repo tree confirms these paths).

**Mobile counterparts (from PR #1/#2, already merged):**
- `RANMobile/UI/Login/LoginScreen.cpp/.h` (title "RAN ONLINE EP7" / "FAM HAWAII", username/password, Login/Register/Exit/Settings), `RANMobile/Client/Login/AuthenticationManager.cpp/.h`.
- `RANMobile/Client/Character/CharacterSelectManager.cpp/.h` (character select logic adapter).
- `RANMobile/UI/HUD/MobileHUD.cpp/.h`, `UI/Buttons/`, `UI/Joystick/`, `UI/Inventory/`, `UI/Skills/`, `UI/Quest/`, `UI/Map/`, `UI/Chat/`, `UI/Settings/`.
- `RANMobile/Server/MobileServerHost.cpp/.h` (optional local host, does not modify PC servers).

## 4. Design principles (carried over from the existing document)

- **Zero Server/DB changes**: connects to existing EP7 Login, Session, Agent and Field servers using the exact EP7 packet protocol (`NET_MSG_GENERIC`); mobile-only transport changes.
- **Zero game data fabrication**: all monsters, NPCs, maps, items, skills, quests and classes come from original EP7 data via `RANMobile/Data/` importer/converter/validator.
- **Display-name separation**: "FAM HAWAII" is a UI display string, not a new server or new content.
- **Cross-platform C++17 core**: `RANMobile/` compiles natively on Linux for tests and on Android ARM64 via NDK + CMake + OpenGL ES 3.0.

## 5. Phase 1 conclusion / next phase prerequisites

Architecture is sufficiently mapped to begin Phase 2 (review of PR #1/#2) and later engine work. However, the iron rules require stopping at a blocker: verified blockers (local Android SDK/NDK, original binary assets/DB via blocked hosts, no live server) are recorded in `BLOCKERS.md` with the exact items needed. No Phase 2+ work was started and no Draft PR other than this Phase 1 change was opened.


## 6. Phase 3C asset/scene extension (2026-11-26)

- New read-only parser layer: `RANMobile/Data/Importer/EP7SerialCodec.h` (source-exact `compbyte` table), `EP7SerialFile.*` (128-byte file type + DWORD file ID, `std::string`), `EP7SceneData.*` (`mapslist.mst` fields 0..19 from `GLMapList::LoadMapList`; `.glmap` `SLEVEL_HEAD` from `GLLevelFile.cpp:629-656`).
- `EP7TextureDecoder.*` now decodes the original MTF container and DDS DXT1/3/5 level 0 plus a validated uncompressed mask subset, and reproduces the original `TextureManager.cpp` alpha-mode classification.
- `MobileRenderer` keeps one retained RGBA8 copy per asset key and re-uploads it after `OnSurfaceCreated`; `DrawTexturedQuad` adds an alpha-test cutoff. `RANMobileEngine` owns `EP7SceneRenderer` and exposes `LoadScene()` / `ScenePreview`.
- `EP7SceneRenderer` is deliberately metadata-and-texture-only: it does not parse `.wld` geometry or simulate gameplay. With currently recovered assets the scene index is empty and the UI states that plainly.
- Android bridge applies `DisplayCutout` insets through JNI and tracks soft-keyboard visibility on focus/pause/surface loss; IME UTF-8 commit and rotation-policy changes remain deferred (see `BLOCKERS.md`).

---

## 7. Folder Structure (RANMobile)

RANMobile/
├── Engine/
│   ├── Core/         (RANMobileEngine, Logger, Time, Memory)
│   ├── Renderer/     (OpenGL ES 3.0 / Portable Render Pipeline, Frustum Culling, LOD, GraphicsSettings)
│   ├── Resource/     (ResourceManager, Async Asset Streaming, Object Pooling)
│   ├── Input/        (InputManager, Multi-touch state)
│   ├── Touch/        (TouchSystem, Gesture recognizer)
│   ├── Camera/       (MobileCamera: ThirdPerson, Free, Target, Combat, Pinch Zoom)
│   ├── Animation/    (Skeletal & Keyframe Animation Adapter)
│   ├── Audio/        (AudioManager: OpenSL ES / Portable stub)
│   └── Network/      (POSIX Socket Transport, PacketReader, PacketWriter, SessionManager, Reconnect, Heartbeat)
├── Client/
│   ├── Login/        (AuthenticationManager, EP7 Login Adapter)
│   ├── Character/    (CharacterSelectManager, EP7 Character Data Adapter)
│   ├── Gameplay/     (GameWorldClient, PlayerController)
│   ├── Combat/       (CombatController, TargetManager)
│   ├── Inventory/    (InventoryManager, Potion/Item Use)
│   ├── Skills/       (SkillManager, Cooldowns)
│   ├── Quest/        (QuestManager)
│   └── Chat/         (ChatManager: Normal, Whisper, Party, Guild, System, UTF-8 Thai)
├── UI/
│   ├── Login/        (LoginScreen — "RAN ONLINE EP7" / "FAM HAWAII")
│   ├── HUD/          (MobileHUD: Top Status, MiniMap, Left Joystick, Right Combat, Bottom Bar)
│   ├── Buttons/      (MobileButton, MobileSkillButton, Cooldown & Hold support)
│   ├── Joystick/     (VirtualJoystick: Multi-touch, Deadzone, Walk/Run)
│   ├── Inventory/    (InventoryUI)
│   ├── Skills/       (SkillUI)
│   ├── Quest/        (QuestUI)
│   ├── Map/          (MapUI)
│   └── Settings/     (SettingsUI: Low/Medium/High/Ultra, 30/45/60/90 FPS, Layout)
├── Data/
│   ├── Importer/     (EP7 Raw Format Readers)
│   ├── Converter/    (RANMobileDataConverter)
│   ├── Validator/    (DataValidator & DATA VALIDATION REPORT)
│   └── Loader/       (GameDataLoader)
├── Android/          (CMakeLists.txt, AndroidManifest.xml, build.gradle, MainActivity.java, native_bridge.cpp)
├── Config/           (server_config.json, graphics_defaults.json)
├── Tools/            (converter_cli.cpp)
├── Tests/            (test_main.cpp — Comprehensive Unit & Integration Tests)
├── CMakeLists.txt
├── README.md
└── BUILD_ANDROID.md
```


---

## Phase 3J addition (2026-11-26) — Character / skeleton / animation layer

New directories and files. Nothing existing was moved or replaced.

```
RANMobile/
├── Data/Importer/
│   ├── EP7ByteFormat.h      (header-only) bounds-checked basestream + compbyte
│   ├── EP7RccArchive.{h,cpp}  .rcc pack reader (ZIP central dir, inflate, ZipCrypto)
│   ├── EP7Math.h            (header-only) Vec3/Quat/Mat4, D3DX row-vector order
│   ├── EP7XFile.{h,cpp}     .x text tokenizer (rejects binary encodings)
│   ├── EP7Skeleton.{h,cpp}  .x Frame hierarchy -> bones, pose accumulation
│   ├── EP7SkinPiece.{h,cpp} .cps 0x0104..0x0107
│   ├── EP7CharacterFile.{h,cpp}  .chf 0x0100..0x0102
│   ├── EP7SkinnedMesh.{h,cpp}    Mesh + SkinWeights + materials + CPU skinning
│   └── EP7Animation.{h,cpp}      .bin 0x0100..0x0102, .cfg 0x0107, player
├── Engine/Scene/
│   └── EP7CharacterScene.{h,cpp} loader, instances, pose eval, AABB culling, diagnostics
├── UI/Character/
│   └── CharacterPreviewOverlay.{h,cpp}  touch preview + diagnostics overlay
└── Tools/
    └── verify_ep7_character_assets.cpp  authentic-asset probe (CI)
```

### Layering rules this layer follows

0. **Pack-aware asset access.** Authentic character data lives in `SkinObject.rcc` /
   `Animation.rcc` (`GLOGIC::InitFile`), not on disk, so `EP7RccArchive` is the byte source
   for anything that reads `.chf/.cps/.x/.bin/.cfg`.
1. **Parsers never touch I/O.** Every reader takes bytes + a `bool compByteEncoded`, or an
   `ep7byte::Stream`. `EP7CharacterLoader` resolves names through a
   `bool (*)(void* userData, const std::string&, std::vector<uint8_t>&)` callback, so Android
   `AAssetManager`, a pack archive and test fixtures are interchangeable.
2. **One frustum implementation.** `EP7CharacterScene::CullInstances` feeds instance AABBs to
   the existing `MobileRenderer::IsBoxInFrustum` instead of duplicating view-distance logic.
3. **One camera.** The preview overlay drives the existing `MobileCamera` API
   (`OnDrag`/`OnPinch`/`SetMode`/`SetDistanceBounds`); there is no second camera type.
4. **One UI primitive set.** The overlay draws through `MobileRenderer::DrawUIQuad` /
   `DrawUIText`, the same calls `MobileHUD` uses, so no new GL state is introduced.
5. **No data aliasing.** Each character instance owns its own `EP7Skeleton`; the shared
   `EP7CharacterAsset` holds immutable parsed data only.
6. **No fabricated fallbacks.** A character instance cannot exist without a successfully
   parsed `.chf` + skeleton + at least one piece. There is no placeholder path.

### Draw-path contract for the 3D renderer (PR #4)

Per frame, after the world pass is prepared:

```
characterScene.Update(dt);                                  // animation + CPU pose
characterScene.CullInstances(camera, &renderer);            // sets inFrustum + bounds
for (instance : characterScene.Instances())
    if (instance->poseValid && instance->inFrustum)
        for (slot : pieces)
            draw(instance->skinnedPositions + pieceVertexOffsets[slot],
                 instance->skinnedNormals,
                 piece.mesh.faces,
                 piece.mesh.subsets[i].texture);
```

A GPU skinning path can replace the vertex upload by sending `BuildBonePalette()` output as a
palette uniform; the mesh already carries per-vertex blend data in its `SkinWeights`.

---

## 2026-11-26 — Verification boundaries in CI

The character pipeline now has an explicit *verification* layer with its own outcome model,
kept separate from the runtime path:

```
external/ep7_release/client_original/         (git-ignored, ~2.5 GB, release asset)
        │
        ├── SkinObject.rcc ─┐
        └── Animation.rcc ──┤   EP7RccArchive (ZIP + deflate + ZipCrypto "tkqwlf" + CRC-32)
                            ▼
              verify_ep7_character_assets      ← tool, not runtime code
                            │
             ┌──────────────┼───────────────┐
          PASS           UNSUPPORTED      FAIL
        (parsed)      (no definition       (reader
                       in this repo)       defect)
             │              │               │
          exit 0         exit 2          exit 1
```

Rules this layer enforces:

1. **`UNSUPPORTED` is a first-class outcome, not an error and not a pass.** It is produced
   only when the basestream type really is `SANIMCONINFO` *and* the version is outside
   `0x0101`–`0x0107`. Anything else — wrong type, missing header, corrupt entry — is `FAIL`.
2. **Verification can never be mistaken for runtime support.** Nothing in the runtime path
   consults the probe; the probe only reads and reports.
3. **CI keeps the boundary.** `core-tests` and `android-arm64` need no client data; only the
   isolated `authentic-assets` job downloads it. A format coverage gap therefore cannot gate
   the build, and a build failure cannot be excused by one.

For the record of *why* `0x0108`–`0x0115` stay unimplemented, see `BLOCKERS.md` B11: the
matching constants in the engine belong to the landscape (`DxLandMan`) family, whose
`FILEMARK` is `"LAND.MAN"`, so reusing them for animation would fabricate a layout.

---

## Appendix — `.cps` trace-point record: `SVERTEXINFLU` (Phase 3J follow-up)

Clarifies the `.cps` layout note earlier in this document, which lists a trace as
"string name, then `SVERTEXINFLU::LoadFile`" but does not pin the nested record's version tag.

**`SVERTEXINFLU::VERSION` is `0x0101`.** It is defined identically in two translation units —
`enginelib/Meshs/DxSkinMeshContainer9.cpp:36` and `enginelib/Meshs/SMeshContainer.cpp:29` —
and is used symmetrically: `SaveFile` (`:59`) writes it as the record tag, `LoadFile` (`:94` /
`SMeshContainer.cpp:95`) compares the tag it reads against it. A reader that compares against a
different value silently takes the unknown-version branch for every trace.

On-disk record, exactly as produced by `SVERTEXINFLU::SaveFile` and consumed structurally by
`LoadFile` on a version match (little-endian, no padding):

```
DWORD   dwVer        == 0x0101 (SVERTEXINFLU::VERSION)
DWORD   dwSize       // body byte count — the writer over-states this, see below
DWORD   m_dwIndex
float   m_vVector[3]
float   m_vNormal[3]
DWORD   m_dwNumBone  // 0xFFFFFFFF (COL_ERR) means "no influence"; MAX_BONE is 6
DWORD   m_pBone[numBone]     // present only when 0 < numBone <= MAX_BONE
float   m_pWeights[numBone]  // present only when 0 < numBone <= MAX_BONE
```

Structural body sizes: `32` bytes for `numBone == 0` or `COL_ERR`, otherwise `32 + 8*numBone`
(40 / 48 / 64 bytes for `numBone` = 1 / 2 / 6).

**Caveat on `dwSize`.** `SaveFile` adds the bone and weight arrays twice — once in the initial
`dwSize` expression and once inside the `if (m_dwNumBone > 0 && m_dwNumBone != COL_ERR)` block —
so `dwSize` reports `32 + 16*numBone` while the record actually occupies `32 + 8*numBone`.
The engine never notices because `LoadFile` uses `dwSize` **only** on the unknown-version path.
A reader that matches the version must consume the body structurally, and one that does not
match must skip `dwSize` (mirroring the engine) rather than assuming the value is exact.

`RANMobile/Data/Importer/EP7SkinPiece.cpp` (`ReadVertexInfluence`) implements exactly this:
structural consume on a `0x0101` match, `Skip(blockSize)` otherwise. The nested block is then
followed by the effect list (`DWORD count`, each `DWORD TypeID` + effect payload) and
`m_dwFlag`, which is what makes a mis-consumed influence visible as a desync on the *next*
trace's name rather than at the influence itself.

---

## Appendix — `.cps` version branches and the legacy trace layouts

The `.cps` container is not one layout with a version tag; `DxSkinPiece::LoadPiece`
(`enginelib/Meshs/DxSkinPieceContainer.cpp`) is seven separate branches plus a terminal `else`
that raises *"Can't Load Skin object"* and returns `E_FAIL` (`:934-943`). A reader must
dispatch on the version **and** change its field set accordingly.

### Branch map

| Version | Branch | `m_dwMaterialNum` + materials | Trace section | `m_dwFlag` |
|---|---|---|---|---|
| 0x0107 | `:374` | yes | count + name + `SVERTEXINFLU` | read |
| 0x0106 | `:459` | yes | count + name + `SVERTEXINFLU` | read |
| 0x0105 | `:544` | yes | count + name + `SVERTEXINFLU` | read |
| 0x0104 | `:629` | yes | count + name + `SVERTEXINFLU` | **not read** |
| 0x0103 | `:712` | **no** | count + name + `SVERTEXINFLU` | **not read** |
| 0x0102 | `:783` | **no** | **10 fixed slots**, no names, no count | **not read** |
| 0x0101 | `:859` | **no** | **8 fixed slots**, no names, no count | **not read** |

`m_emWeaponWhereBack` is stored on disk only for 0x0106/0x0107 (`:398-399`, `:483-484`). Every
other branch assigns it `EMPEACE_WEAPON_RSLOT` without reading (`:626`, `:709`, `:780`, `:857`,
`:931`).

### The fixed-slot trace layout (0x0101 / 0x0102)

No count, no name strings — just N consecutive `SVERTEXINFLU` records:

```
0x0101:  8 x SVERTEXINFLU      // for ( int i=0; i<8;  i++ ) ...  :891
0x0102: 10 x SVERTEXINFLU      // for ( int i=0; i<10; i++ ) ...  :815
DWORD  effectCount
```

Only the first four slots can produce a trace point. The branches test
`sTraceArray[k].m_dwIndex != COL_ERR` for k = 0..3 and name the survivors from the compile-time
table in `DxSkinPieceContainer.h:22-25`:

```c
#define STRACE_IMAGE01 ("IMAGE01")   // slot 0
#define STRACE_IMAGE02 ("IMAGE02")   // slot 1
#define STRACE_IMAGE11 ("IMAGE11")   // slot 2
#define STRACE_IMAGE12 ("IMAGE12")   // slot 3
```

`STRACOR::STRACOR` copies that name with `StringCchCopy ( m_szName, STRACE_NSIZE = 32, ... )`,
so it is truncated to 31 characters — harmless for these eight-character literals. Slots 4..7
(0x0101) and 4..9 (0x0102) are read for alignment and discarded.

Consequence for a reader: those names are **not file data**. The mobile reader exposes them
but marks the trace `syntheticName = true`, so no caller can mistake `IMAGE01` for a
dependency name recovered from the asset.

### Why `m_dwFlag` is absent below 0x0105

The constructor initialises `m_dwFlag(0L)` (`:192`). Branches before 0x0105 never read it from
the file; they only accumulate `m_dwFlag |= pEffChar->GetFlag()` per successfully loaded effect
(`:703`, `:774`, `:850`, `:925`). A reader that insists on a trailing DWORD for 0x0104 or older
consumes the next record's bytes — or, at end of file, reports a complete file as
`truncated piece flag`. That was the B15 defect: 88 authentic 0x0104 files were being rejected
for a field their version does not store.

## Appendix — competing `DxSkinPiece::VERSION` definition

`enginelib/Meshs/ReplaceContainer.cpp:19` defines `DxSkinPiece::VERSION = 0x00000100` and a
`LoadPiece` that only accepts that one version. It is **not** the `.cps` loader: the file
defines the `DxReplaceContainer` family used by `DxLandMan`/`DxPieceContainer` (via
`DxReplaceContainer.h`), and `enginelib.vcxproj:1526` compiles `Meshs\DxSkinPieceContainer.cpp`
into the character path. The authoritative `.cps` loader is `DxSkinPieceContainer.cpp`, with
`VERSION = 0x00000107` (`:158`) and the seven branches above. Do not take a version bound from
`ReplaceContainer.cpp`.

---

## Appendix — `SVERTEXINFLU` `dwSize` is unreliable (B14), and why the reader ignores it

The `dwSize` DWORD that follows the `SVERTEXINFLU` tag is **not** a trustworthy body length. It
is hand-computed by `SVERTEXINFLU::SaveFile`
(`enginelib/Meshs/DxSkinMeshContainer9.cpp:59-92`; byte-identical copy at
`enginelib/Meshs/SMeshContainer.cpp:52-85`) and the computation counts the bone and weight
arrays twice while the body writes them once.

| `m_dwNumBone` | declared `dwSize` | actual body | delta |
|---|---|---|---|
| 0 | `0x20` | `0x20` | 0 |
| 1..6 | `0x20 + 16n` | `0x20 + 8n` | `+8n` (over) |
| `COL_ERR` (`0xFFFFFFFF`) | `0x18` (wrapped) | `0x20` | `−8` (under) |

Body layout actually written after the `dwSize` field:

```
DWORD  m_dwIndex              //  4
float  m_vVector[3]           // 12
float  m_vNormal[3]           // 12
DWORD  m_dwNumBone            //  4       = 32 bytes so far
DWORD  m_pBone[numBone]       // 4n      only when 0 < numBone <= MAX_BONE(6)
float  m_pWeights[numBone]    // 4n      only when 0 < numBone <= MAX_BONE(6)
```

So the body is `32` bytes for `numBone == 0` and for `COL_ERR`, and `32 + 8n` otherwise.

### The rule the reader follows

Mirroring `SVERTEXINFLU::LoadFile` (`:94-127`):

- **tag matches `SVERTEXINFLU::VERSION` (0x0101)** → consume the body **structurally**; never
  consult `dwSize`. A structural read cannot be displaced by a wrong size.
- **tag does not match** → skip exactly `dwSize` bytes, because that is what `LoadFile` does
  (`SetOffSet(GetfTell() + dwSize)`, `:126`). This is the *only* place the field is
  authoritative, incorrect or not.

Do **not** add a "consistency check" comparing `dwSize` against the bytes consumed. The writer
is wrong, not the reader: such a check rejects valid data at `numBone == 0` and at `COL_ERR`,
and at every other count alike. `TestSkinPieceInfluenceSizeAccounting` fails 31 checks if this
check is introduced.

### Contrast with `SMATERIAL_PIECE`, whose size *is* reliable

`SMATERIAL_PIECE::SaveFile` (`DxSkinPieceContainer.cpp:109-122`) brackets its body with
`BeginBlock`/`EndBlock`. `EndBlock` (`enginelib/Common/SerialFile.cpp:189-208`) back-patches the
size from the measured positions:

```cpp
long nEndPos = GetfTell();
DWORD dwTotalSize = DWORD ( ( nEndPos - m_nMarkPos[m_dwBlockMarkPOS] ) - sizeof(DWORD) );
```

Measured, therefore exact. That is why the material block can be skipped safely by its size and
the influence record cannot. When reading a block, establish which of the two conventions
applies — measured (`BeginBlock`/`EndBlock`) or predicted (hand-summed). Only the former may be
trusted for a skip.

---

## Appendix — basestream file-type header: why every EP7 file starts with ASCII

This corrects an assumption embedded in the verification tool (see `BLOCKERS.md` B11).

### The header is a 128-byte string, not a numeric magic

```cpp
// enginelib/Common/SerialFile.cpp:57-66
void CSerialFile::WriteFileType ()
{
    fwrite ( &m_szFileType, sizeof(char), FILETYPESIZE, m_FileStream );  // 128 bytes
    fwrite ( &m_FileID, sizeof(DWORD), 1, m_FileStream );                // version
    m_DefaultOffSet = ftell ( m_FileStream );                            // = 132
}
```

`FILETYPESIZE` is `128` (`enginelib/Common/basestream.h:16`). `SetFileType`
(`SerialFile.cpp:34-38`) copies a caller-supplied string into that field, so the first bytes of
every container are that string's first bytes:

| Type string | Set at | First 4 bytes on disk |
|---|---|---|
| `"AnimContainer"` | `SAnimation.cpp:22`, used `:1583` | `A n i m` |
| `"SANIMCONINFO"` | `DxSkinAniManSaveLoad.cpp:188` | `S A N I` |
| `"LAND.MAN"` | `DxLandManSaveLoad.cpp:31` | `L A N D` |
| `"EFF_PROPGROUP"` | `DxEffSinglePropGMan.cpp:596` | `E F F _` |

So an EP7 basestream **always** begins with printable ASCII. Observed layout:

```
0x00 .. 0x7F : file-type string, NUL-padded to 128 bytes   (FILETYPESIZE)
0x80 .. 0x83 : version / file id DWORD                     (m_FileID)
0x84 ..      : payload                                     (GetfTell() == 0 here)
```

This is why `EP7ByteFormat.h` models `kFileTypeSize = 128` and
`kHeaderSize = 132`, and why `GetfTell()`/`SetOffSet()` are relative to offset 132.

### The type string is usually *not* validated

`SAnimContainer::LoadAnimFromFile` (`SAnimation.cpp:1754-1776`) reads the type into
`szFileType` and then **ignores it** — dispatch is on `dwVersion` alone. A `strcmp` against the
type appears only in the landscape loader (`DxLandManSaveLoad.cpp:733`). Consequently a file with
a blanked or foreign type string but a valid version still loads in the original engine.

Two consequences for this codebase:

1. **Tests that zero-fill the header cannot detect a type-string problem.** All 8 basestream
   fixtures in `test_phase3j.cpp` do exactly that. A fixture carrying a real `"AnimContainer"`
   string is needed to exercise the header path faithfully.
2. **Do not infer "foreign format" from printable leading bytes.** It is the normal case. To
   decide whether a buffer is a known container, read the 128-byte type plus the version at
   `0x80` and attempt the parse — not the other way round.

The `.cfg` reader (`ParseInfo`) *does* validate the family and reports *"not a SANIMCONINFO
file"* for a genuine mismatch; that behaviour is correct and should be the model for `.bin`.
