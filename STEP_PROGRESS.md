# STEP_PROGRESS.md — FAM HAWAII (RAN ONLINE EP7 MOBILE)

## สถานะล่าสุด (อัปเดตอัตโนมัติ)
- **Branch**: `arena/6c988b52-ran-online-ep7` (PR #2)
- **Android Client (`RANMobile`)**:
  - `app-debug.apk` (ARM64): **PASS** ใน Run `45475236136` (`0e711f8`)
  - `EP7TextureDecoder.h/.cpp` (PHASE 2): เขียน `.mtf` Decryptor (`TEX_XOR_DATA=0x47`, `TEX_DIFF_DATA=0x03`) + `.dds` DXT1 Decoder เสร็จแล้ว
- **Android Server (`MobileServerHost`)**:
  - `MobileServerHost.h/.cpp` (POSIX TCP `0.0.0.0:5001`, `EP7_NET_MSG_LOGIN_2`, local account register/auth): **PASS** ใน Run `45475236136` (ARM64 NDK Build)
  - หมายเหตุ: ทำงานบนโทรศัพท์ Android ได้โดยไม่ต้องใช้ Windows PC สำหรับการทดสอบ Login/Account เบื้องต้น แต่สำหรับการเชื่อมต่อ MSSQL/World เต็มรูปแบบยังต้องเชื่อม Windows EP7 Server จริง
- **PHASE 1 (Data & Source Audit)**:
  - เพิ่ม `.github/workflows/phase1-complete-files-audit.yml` เพื่อดึง `.bak` และ SQL scripts จาก `Ran-Online-EP7-Complete-Files` ผ่าน `git lfs pull` บน Cloud Runner
