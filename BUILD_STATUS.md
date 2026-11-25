# BUILD_STATUS.md — สถานะการ Build และ Test

| Target | Environment | Status | Latest Run / Evidence |
|---|---|---|---|
| `app-debug.apk` (ARM64) | GitHub Actions (`ubuntu-latest` + NDK r26b) | **PASS** | Run `45627265319` (`c138f15` / `72bd7f3` ใน `apk-latest`) |
| **Renderer Test (Physical Device)** | Android 2400x1080 | **FAIL (จอดำ)** | หน้า InGame ดำสนิท ไม่มี Map (`login.wld`) / Character (`s01_01.chf`) แสดงผล |
| **Server Connection** | `127.0.0.1:5001` | **HARNESS LOCAL ONLY** | `LIVE EP7 SERVER ENDPOINT NOT AVAILABLE` · `PROTOCOL NOT VERIFIED` |
| `ep7-server-db-verify.yml` | GitHub Actions (`windows-latest` + LocalDB) | **PASS** | Run `45556715374` (`RanUser`, `RanGame1`, `RanShop`, `RanLog` `.bak` ผ่าน) |
| `dep_scan_cli` | GitHub Actions (`ubuntu-latest`) | **PASS** | Run `45354762099` (`s01_01.chf` 38/38, `login.wld` 61/67) |
| `ranmobile_tests` | Sandbox `g++` | **PARTIAL** | Compile ผ่าน `-Wall -Wextra -Werror`, ขาด `thaipack_original` ใน sandbox |
| Original Win32 Server | Sandbox / Linux | **BLOCKED** | ต้องใช้ Windows + MSVC + MSSQL |
