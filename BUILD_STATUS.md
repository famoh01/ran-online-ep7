# BUILD_STATUS.md — สถานะการ Build และ Test

| Target | Environment | Status | Latest Run / Evidence |
|---|---|---|---|
| `app-debug.apk` (ARM64) | GitHub Actions (`ubuntu-latest` + NDK r26b) | **PASS** | Run `45555636244` (`apk-latest` `1,461,970` bytes) |
| `ep7-server-db-verify.yml` | GitHub Actions (`windows-latest` + LocalDB) | **PENDING CI** | Restore/Header Check `.bak` (`RanUser`, `RanGame1`, `RanShop`, `RanLog`) |
| `dep_scan_cli` | GitHub Actions (`ubuntu-latest`) | **PASS** | Run `45354762099` (`s01_01.chf` 38/38, `login.wld` 61/67) |
| `ranmobile_tests` | Sandbox `g++` | **PARTIAL** | Compile ผ่าน `-Wall -Wextra -Werror`, ขาด `thaipack_original` ใน sandbox |
| Original Win32 Server | Sandbox / Linux | **BLOCKED** | ต้องใช้ Windows + MSVC + MSSQL |
