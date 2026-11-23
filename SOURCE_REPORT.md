# SOURCE_REPORT.md — RAN ONLINE EP7 (FAM HAWAII)

## Overview
- **Repository**: `famoh01/ran-online-ep7` (branch `arena/83858354-ran-online-ep7`, base `4affa8fe00a791cb1defbf2eb506b9bfd8dc3ef6`)
- **Release**: `https://github.com/famoh01/ran-online-ep7/releases/tag/ran-ep7-full-v1`
  - Assets listed in release:
    - `RAN-EP7-Client-TH.zip.part00` (1,572,864,000 bytes)
    - `RAN-EP7-Client-TH.zip.part01` (1,010,562,339 bytes)
    - `RAN-EP7-ServerDBTools.zip` (49,791,515 bytes)
    - `RAN-EP7-ThaiPack-FamHawaii.zip` (1,359,185 bytes)
  - *Note*: Direct download of release binary assets (`release-assets.githubusercontent.com`) is blocked by the sandbox network allowlist. However, the complete C++ source code for Client, Engine, Network, and Game Logic is present in the repository.

---

## 1. Client Source Found
- `Basic/` — Main PC client entry point (`WinMain`, `CBasicApp`, `CBasicWnd`).
- `RanClientLib/` — Core client stages and game logic:
  - `DxGlobalStage.cpp/h` — Global stage controller (Login -> Lobby -> Game).
  - `DxLobyStage.cpp/h` — Character selection & creation stage.
  - `DxGameStage.cpp/h` — In-game world stage.
  - `DxCharSham.cpp/h` — Character preview/sham rendering.
  - `RanClientLib/G-Logic/` — Shared client/server game logic (`GLChar.cpp/h`, `GLCharClient.cpp/h`, `GLCrowClient.cpp/h`, `GLGaeaClient.cpp/h`).
- `RanClientUILib/` — PC UI implementation (MFC/DirectX-based UI controls).
- `AutoPatchMan/` — Auto-patcher client.

## 2. Engine Source Found
- `enginelib/` — DirectX 9 3D Engine:
  - `Common/` — Math, file I/O, serializing (`CSerialFile`, `BaseString`, `SUBPATH`).
  - `DxCommon/` & `DxCommon9/` — Direct3D 9 device, textures (`TextureManager`), shaders, lighting, shadows.
  - `DxOctree/` — Map octree & spatial partitioning (`DxLandMan`, `DxOctree`).
  - `Meshs/` — Character/Monster/NPC/Item skeletal & static meshes (`DxSkinChar`, `DxSkinMesh9`, `DxBoneCollector`, `DxAniKeys`).
  - `DxEffect/` — Particle, character, animation, and projection effects.
  - `NaviMesh/` — Navigation mesh for pathfinding.
  - `TextTexture/` — Font & text rendering.
- `EngineSoundLib/` & `SoundSourceManager/` — DirectSound + Ogg Vorbis audio.
- `EngineUILib/` — Base UI framework (`CUIControl`, `CBasicButton`, `CUIEditBox`).
- `RanFileSystem/` — Virtual file system / `.rcc` archive reader.
- `TexEncrypt/` — Texture encryption/decryption.

## 3. Network Client Found
- `netclientLib/` — TCP Winsock client (`CNetClient`, `s_NetGlobal.h`, `MinLzo` compression, `DaumCrypt` / `ByteCrypt` encryption).
- `RanClientLib/G-Logic/GLContrlMsg.h` & `netserverLib/s_NetGlobal.h` — Complete EP7 packet protocol definitions (`NET_MSG_GENERIC`, `NET_MSG_LOGIN_2`, `NET_MSG_CHA_BAINFO`, `NET_MSG_GCTRL_*`).

## 4. Assets & Formats Found in Source
- Models: `.x` (DirectX X-file), `.mxf` (encrypted/binary mesh), `.cps` (character piece), `.chf` (character file), `.abf` (attaching bone file).
- Animations: `.cfg` / `.x` / `.bin` keyframe animation data (`DxAniKeys`).
- Textures: `.dds`, `.tga`, `.bmp`, `.mtf` (encrypted texture).
- Maps: `.wld` (world), `.lev` (level), `.oct` (octree), `.navi` (navigation mesh).
- Sound: `.wav`, `.ogg` (Vorbis).
- UI/Text: `data/gui/` XML/text files, `.ntk` (NPC talk, 318 files in release), `.qst` (Quest, 882 files in release).

## 5. Map Data
- **Status**: **FOUND IN SOURCE (Format & Loader)**; **BINARY ASSETS IN RELEASE** (not downloaded due to sandbox proxy).
- Source definitions: `RanClientLib/G-Logic/GLMapList.h`, `GLMapNode.h`, `enginelib/DxOctree/DxLandMan.h`.

## 6. Monster Data
- **Status**: **FOUND IN SOURCE (Format & Loader)**; **BINARY ASSETS IN RELEASE**.
- Source definitions: `RanClientLib/G-Logic/GLCrowData.h` (`SCROWDATA`, `SCROWBASIC`, `SCROWATTACK`, `SCROWGEN`), `GLCrowClient.h`.

## 7. NPC Data
- **Status**: **FOUND IN SOURCE (Format & Loader)**; **BINARY ASSETS IN RELEASE**.
- Source definitions: `RanClientLib/G-Logic/GLCrowData.h` (NPCs share `GLCrowData` with `EMCROW_NPC`), `RanClientLib/NpcTalk/` (`CNpcDialogue`, `CNpcTalkControl`).

## 8. Item Data
- **Status**: **FOUND IN SOURCE (Format & Loader)**; **BINARY ASSETS IN RELEASE**.
- Source definitions: `RanClientLib/G-Logic/GLItem.h`, `GLItemDef.h`, `GLItemMan.h` (`SITEM`, `SITEMBASIC`, `SITEMSUIT`, `SITEMDRUG`, `SITEMSKILL`).

## 9. Skill Data
- **Status**: **FOUND IN SOURCE (Format & Loader)**; **BINARY ASSETS IN RELEASE**.
- Source definitions: `RanClientLib/G-Logic/GLSkill.h`, `GLSkillDefine.h` (`GLSKILL`, `SKILL::SSKILLBASIC`, `SKILL::CDATA_LVL`).

## 10. Quest Data
- **Status**: **FOUND IN SOURCE (Format & Loader)**; **BINARY ASSETS IN RELEASE**.
- Source definitions: `RanClientLib/G-Logic/GLQuest.h`, `GLQuestMan.h` (`GLQUEST`, `GLQUEST_STEP`).

## 11. Animation & Texture
- **Status**: **FOUND IN SOURCE (Format & Loader)**; **BINARY ASSETS IN RELEASE**.
- Source definitions: `enginelib/Meshs/DxAniKeys.h`, `enginelib/DxCommon/TextureManager.h`.

## 12. What Can Be Used on Android
- Pure C++ game data structures (`SITEM`, `GLSKILL`, `SCROWDATA`, `GLQUEST`, `SMAPNODE`) after removing Windows/MFC types (`DWORD`, `BOOL`, `CString`, `D3DXVECTOR3`).
- Network packet structures (`GLContrlMsg.h`, `s_NetGlobal.h`) — portable POD structs.
- `zlib` / `minilzo` / `ogg-vorbis` compression and audio codecs.

## 13. What Must Be Rewritten
- **Platform & Windowing**: `WinMain` / MFC -> Android `android_main` / `ANativeActivity` / JNI.
- **Renderer**: Direct3D 9 (`IDirect3DDevice9`) -> OpenGL ES 3.0 / Vulkan (`RANMobileEngine/Renderer/`).
- **Audio**: DirectSound -> OpenSL ES / AAudio (`RANMobileEngine/Audio/`).
- **Networking**: Winsock2 (`SOCKET`, `WSAAsyncSelect`) -> POSIX BSD Sockets (`sys/socket.h`) with non-blocking/epoll (`RANMobileEngine/Network/`).
- **Input**: DirectInput / Win32 Mouse & Keyboard -> Android Touch / Multi-Touch, Virtual Joystick, Mobile Camera, Mobile Buttons.
- **UI**: `EngineUILib` / `RanClientUILib` -> Mobile Touch HUD, Login Screen (`FAM HAWAII`), Character Select, Inventory, Quest, Map, Chat, Settings.

## 14. What Must Be Converted
- `.dds` / `.mtf` textures -> ETC2 / ASTC / RGBA8888.
- `.x` / `.mxf` / `.cps` / `.chf` models -> Mobile-optimized binary mesh/skeleton format preserving original IDs, bones, and UVs.
- `.wld` / `.lev` maps -> Mobile terrain/octree chunks preserving original coordinates and gates.
- `.rcc` archives -> Unpacked or indexed mobile asset bundle.

## 15. What Is Missing in Sandbox
- The binary game data files inside `RAN-EP7-Client-TH.zip` (`.rcc`, `data/gui`, `.ntk`, `.qst`, `.dds`, `.x`) could not be downloaded in this sandbox because `release-assets.githubusercontent.com` is outside the outbound allowlist.
- Android SDK / NDK / Gradle are not installed in the sandbox environment (only Linux `g++` is available). Therefore, C++ engine/client/converter/tests will be compiled and verified natively on Linux, and the full Android NDK/Gradle project + `BUILD_ANDROID.md` will be generated ready for Android Studio / NDK build.

## 16. Dependencies Required
- **Android Build**: Android NDK (r25+), CMake 3.22+, Gradle 8.x, OpenGL ES 3.0 (`libGLESv3`, `libEGL`), `libandroid`, `liblog`, `libOpenSLES`.
- **Native/Shared**: C++17 standard library, POSIX sockets (`pthread`), `zlib` (optional for `.rcc`/`.zip`).
