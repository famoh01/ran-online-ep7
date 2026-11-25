# PROJECT_STATE.md — RAN ONLINE EP7 (FAM HAWAII MOBILE)

- **Repository**: `famoh01/ran-online-ep7`
- **Branch**: `arena/6c988b52-ran-online-ep7`
- **Base Commit**: `f520e60`

## สถานะแต่ละระบบตามหลักฐานจริง
| ระบบ | SOURCE VERIFIED | CODE IMPLEMENTED | LOCAL TESTED | DEVICE TESTED | REAL SERVER TESTED | หลักฐาน (ไฟล์ / Commit) |
|---|---|---|---|---|---|---|
| **EP7 Protocol (`LOGIN_2`, `HEARTBEAT`)** | YES | YES | YES | NO | NO | `RANMobile/Engine/Network/EP7Protocol.h`, `netclientLib/s_NetGlobal.h` |
| **RANMobileEngine & ScreenState** | N/A | YES | YES | NO | NO | `RANMobile/Engine/Core/RANMobileEngine.cpp` (`963750d`) |
| **MobileRenderer (GLES3 + Thai Font)** | N/A | YES | YES | YES (UI/Thai) | NO | `RANMobile/Engine/Renderer/MobileRenderer.cpp` (`0e6a77e`) |
| **LoginScreen (FAM HAWAII)** | N/A | YES | YES | YES (UI/Touch), NO (Keyboard on `f520e60`) | NO | `RANMobile/UI/Login/LoginScreen.cpp`, `native_bridge.cpp` (`f520e60`) |
| **MobileHUD / Joystick / Walk-Run / Camera** | YES (Speed/Rules) | YES | YES | NO | NO | `RANMobile/UI/HUD/MobileHUD.cpp`, `VirtualJoystick.cpp` |
| **ThaiPackLoader (`ran-ep7-full-v1`)** | YES | YES | YES (CI) | NO | NO | `RANMobile/Data/Loader/ThaiPackLoader.cpp` |
| **Original Client & ServerDBTools** | YES (Release API) | Pending CI | NO | NO | NO | `.github/workflows/original-asset-import.yml` |
| **Android APK/AAB Build** | N/A | YES | YES (`Run 45271690949`) | `app-debug.apk` | NO | `.github/workflows/build-ranmobile-android.yml` |

## Current Blockers
1. `REAL SERVER TESTED`: ไม่มี Live EP7 Windows Server + MSSQL
2. `RELEASE SIGNING`: ไม่มี Keystore/Passwords ใน Repository
