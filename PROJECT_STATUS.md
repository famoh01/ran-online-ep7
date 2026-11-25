# PROJECT_STATUS.md — FAM HAWAII (RAN ONLINE EP7 MOBILE)

- **Repository**: `famoh01/ran-online-ep7`
- **Branch**: `arena/6c988b52-ran-online-ep7` (PR #2)
- **Head Commit**: `0e711f8`

## สถานะแยกตามหมวด (A–F)
- **A. มีไฟล์จริง**:
  - Original EP7 C++ Source (`enginelib/`, `RanClientLib/`, `RanLogic/`, `RanLogicClient/`, `RanLogicServer/`, `netclientLib/`, `netserverLib/`, `LoginServer/`, `AgentServer/`, `FieldServer/`, `SessionServer/`)
  - Mobile Engine & Android Client (`RANMobile/Engine/`, `RANMobile/Client/`, `RANMobile/UI/`, `RANMobile/Data/`, `RANMobile/Server/MobileServerHost.h/.cpp`, `RANMobile/Android/`)
  - Release `ran-ep7-full-v1` (`RAN-EP7-Client-TH.zip`, `RAN-EP7-ServerDBTools.zip`, `RAN-EP7-ThaiPack-FamHawaii.zip`, `RAN-EP7-ItemText-Thai-Update.zip`)
- **B. พบ Source Code แต่ไม่มี Data ใน Repo**:
  - Map (`.wld`, `.lev`), Monster/NPC/Crow (`.mcf`, `.crowsale`, `.ntk`), Item/Skill/Quest (`.isf`, `.ssf`, `.qst`), Character/Mesh/Texture (`.chf`, `.cps`, `.x`, `.dds`, `.mtf`) — อยู่ใน `RAN-EP7-Client-TH.zip` (6.5 GB บน Release/Cloud ไม่ได้ commit ลง Git)
- **C. พบ Data แต่ยังตรวจสอบไม่ได้ครบ**:
  - `RAN-EP7-Client-TH.zip` และ `RAN-EP7-ServerDBTools.zip` (ตรวจ `login.wld` + `s01_01.chf` ผ่าน `dep_scan_cli` แล้วบน CI Run `45354762099`)
- **D. ยังไม่มีไฟล์**:
  - `.bak` / `.mdf` (MSSQL Database Backup) — `DATA NOT FOUND IN SOURCE` (ต้องตรวจใน Google Drive หรือ `Ran-Online-EP7-Complete-Files` เพิ่มเติม)
  - Release Keystore (`.jks` / passwords) สำหรับ Sign Release APK/AAB
- **E. Build ผ่านแล้ว**:
  - `RANMobile` Android Debug APK (`app-debug.apk`) บน GitHub Actions (`Run 45356254856`, ล่าสุดกำลังรัน `0e711f8`)
  - `dep_scan_cli` บน Linux CI (`Run 45354762099`)
- **F. Build ไม่ผ่าน / ยังไม่รองรับ**:
  - Original Windows Server (`LoginServer`, `AgentServer`, `FieldServer`, `SessionServer`) — ใช้ Win32/IOCP/ODBC ไม่สามารถ Compile บน Linux/Android โดยตรง
  - Local `/tmp/ranmobile_tests` ใน Sandbox ขาด `external/ep7_release/thaipack_original` (ทดสอบเต็มบน GitHub Actions Runner)
