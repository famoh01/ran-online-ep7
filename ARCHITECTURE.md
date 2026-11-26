# ARCHITECTURE.md — RAN ONLINE EP7 (PC → Android mobile port)

> Phase 1 survey — updated on branch `arena/075e1725-ran-online-ep7` (base commit `47f3c6d6bf6e00f62efdb34f313eb7a4311bb26f`, the merge of PR #2).
> This document continues the existing `ARCHITECTURE.md` (mobile engine design) with a verified inventory of the original sources. Nothing in this phase changes game logic, protocol constants, or game data.

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
