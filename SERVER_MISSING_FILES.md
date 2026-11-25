# SERVER_MISSING_FILES.md — รายการไฟล์ Server/Database ที่ขาด

1. **MSSQL Database Backups (`.bak` & `.sql`)**: `DATA NOT FOUND IN SOURCE` ใน `famoh01/ran-online-ep7` แต่พบใน `matheomedrana/Ran-Online-EP7-Complete-Files` (`Ran Online Database/` — Git LFS):
   - `RanGame1.bak` / `RanGame1.sql`
   - `RanLog.bak` / `RanLog.sql`
   - `RanShop.bak` / `RanShop.sql`
   - `RanUser.bak` / `RanUser.sql`
2. **Server Runtime Configs (`.cfg` / `.ini` สำหรับ Login/Agent/Field/Session)**: `DATA NOT FOUND IN SOURCE` ใน C++ Repo
3. **DirectX 9 SDK (Summer 2004 / June 2010)**: อาจจำเป็นสำหรับบางโปรเจกต์ที่อ้างอิง `d3dx9.h` บน `windows-latest` (รอผลจาก `build-ep7-windows-server.yml`)
