# BUILD_STATUS.md — สถานะการ Build และ Test

| Target | Environment | Status | Latest Run / Evidence |
|---|---|---|---|
| `app-debug.apk` (ARM64) | GitHub Actions (`ubuntu-latest` + NDK r26b) | **PASS** | Run `45653392859` (`22628de` ใน `apk-latest`, `1,510,352` bytes) |
| **Emulator Test (x86_64)** | GitHub Actions (`android-emulator-smoke.yml`) | **FAIL** | Run `45581462611` (`f6abf94`) พังที่ Step 6 `Run Emulator Smoke Test` (ตรวจ log/artifact จาก Sandbox ไม่ได้; workflow ถูกลบแล้ว) |
| **Renderer Test (Physical Device)** | Android 2400x1080 | **FAIL / PARTIAL** | `EP7WorldCharParser` อ่านเฉพาะ Header; ยังไม่แกะ Octree/Bone จริงจาก `DxLandMan`/`DxSkinChar` |
| **Server Connection** | `127.0.0.1:5001` | **HARNESS LOCAL ONLY** | `LIVE EP7 SERVER ENDPOINT NOT AVAILABLE` · `PROTOCOL NOT VERIFIED` |
| **Gameplay Test** | InGame | **NOT TESTED** | รอผลทดสอบจริง |
| `ep7-server-db-verify.yml` | GitHub Actions (`windows-latest` + LocalDB) | **PASS** | Run `45556715374` (`RanUser`, `RanGame1`, `RanShop`, `RanLog` `.bak` ผ่าน) |
| `dep_scan_cli` | GitHub Actions (`ubuntu-latest`) | **PASS** | Run `45354762099` (`s01_01.chf` 38/38, `login.wld` 61/67) |
| `ranmobile_tests` | Sandbox `g++` | **PARTIAL** | Compile ผ่าน `-Wall -Wextra -Werror`, ขาด `thaipack_original` ใน sandbox |
| Original Win32 Server | Sandbox / Linux | **BLOCKED** | ต้องใช้ Windows + MSVC + MSSQL |
