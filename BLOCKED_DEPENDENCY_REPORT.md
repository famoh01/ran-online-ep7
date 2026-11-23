# BLOCKED_DEPENDENCY_REPORT.md — RAN ONLINE EP7 (FAM HAWAII MOBILE)

## ORIGINAL ASSET
STATUS: BLOCKED
AVAILABLE:
- C++ Game Data Structs & Loaders in repository (`RanClientLib/G-Logic/GLMapList.h`, `GLCrowData.h`, `GLItem.h`, `GLSkill.h`, `GLQuest.h`, `enginelib/Meshs/DxAniKeys.h`, `enginelib/DxCommon/TextureManager.h`)
BLOCKED:
- `RAN-EP7-Client-TH.zip.part00` (1,572,864,000 bytes) — `release-assets.githubusercontent.com` blocked by sandbox outbound allowlist
- `RAN-EP7-Client-TH.zip.part01` (1,010,562,339 bytes) — `release-assets.githubusercontent.com` blocked by sandbox outbound allowlist
- `RAN-EP7-ThaiPack-FamHawaii.zip` (1,359,185 bytes) — `release-assets.githubusercontent.com` blocked by sandbox outbound allowlist
REQUIRED:
- Unpacked EP7 binary data & assets (`.rcc`, `data/gui`, `.ntk`, `.qst`, `mapslist.mst`, `crow.mnsf`, `item.isf`, `skill.ssf`, `quest.lst`, `skinobject/`, `texture/`, `animation/`)

## EP7 SERVER
STATUS: BLOCKED
AVAILABLE:
- Server source & protocol definitions in repository (`LoginServer/`, `SessionServer/`, `AgentServer/`, `FieldServer/`, `netserverLib/`, `netclientLib/s_NetGlobal.h`, `RanClientLib/G-Logic/GLContrlMsg.h`)
BLOCKED:
- `REAL EP7 SERVER TEST = BLOCKED` (no live EP7 Server or MSSQL Database running in this Linux sandbox; server components are Win32/Winsock/ODBC)
REQUIRED:
- Running EP7 Login/Session/Agent/Field Server reachable via TCP/IP for live authentication and gameplay packet testing

## ANDROID
STATUS: BLOCKED
AVAILABLE:
- `RANMobile/CMakeLists.txt` (Android ARM64 + Linux targets)
- `RANMobile/Android/AndroidManifest.xml` (OpenGL ES 3.0, landscape, `com.famhawaii.ranmobile`)
- `RANMobile/Android/native_bridge.cpp` (`android_main` loop)
- `RANMobile/BUILD_ANDROID.md`
BLOCKED:
- `ANDROID BUILD = BLOCKED` (Android SDK, Android NDK r25+, and Gradle are not installed in this sandbox)
REQUIRED:
- Android NDK r25+, Android SDK (API 24+ / targetSdk 34), CMake 3.22+, Gradle 8.x to compile `RANMobile.apk` / `RANMobile.aab` for `arm64-v8a`

---

## CURRENT PROJECT STATUS:
- **ENGINE**: IMPLEMENTED & LOCAL TEST PASS (`RANMobileEngine`, `MobileRenderer`, `ResourceManager`, `TouchSystem`, `MobileCamera`, `AudioManager`, `AnimationManager`)
- **CLIENT**: IMPLEMENTED & LOCAL TEST PASS (`AuthenticationManager`, `CharacterSelectManager`, `CharacterController`, `TargetManager`, `CombatController`, `InventoryManager`, `QuestManager`, `ChatManager`)
- **UI**: IMPLEMENTED & LOCAL TEST PASS (`LoginScreen` ["RAN ONLINE EP7" / "FAM HAWAII"], `VirtualJoystick`, `MobileButton`, `MobileSkillButton`, `MobileHUD`, `SettingsUI`)
- **DATA**: IMPLEMENTED & LOCAL TEST PASS (`EP7DataTypes`, `DataValidator`, `RANMobileDataConverter`, `GameDataLoader` — zero fabricated data)
- **NETWORK**: IMPLEMENTED & LOCAL TEST PASS (`NetworkClient` POSIX socket, `EP7Protocol.h` verified against `netclientLib/s_NetGlobal.h`)
- **ASSET**: BLOCKED (`BLOCKED — ORIGINAL BINARY ASSET NOT ACCESSIBLE`)
- **ANDROID**: BLOCKED (`ANDROID BUILD = BLOCKED` — SDK/NDK not installed in sandbox)
- **SERVER**: BLOCKED (`REAL EP7 SERVER TEST = BLOCKED`)
