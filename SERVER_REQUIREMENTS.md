# SERVER_REQUIREMENTS.md — RAN ONLINE EP7 (FAM HAWAII)

*หมายเหตุ: สรุปจากการอ่าน Source Code ต้นฉบับใน `famoh01/ran-online-ep7` เท่านั้น (Read-Only)*

## 1. ระบบปฏิบัติการ และฐานข้อมูล
- **ระบบปฏิบัติการ (OS)**: **Windows** (Win32 API, Winsock2, IOCP, MFC)
- **ฐานข้อมูล (Database)**: **Microsoft SQL Server (MSSQL)**
  - **เวอร์ชัน MSSQL**: `NOT FOUND IN SOURCE` (Source เชื่อมผ่าน ODBC DSN)
  - **ไฟล์ฐานข้อมูล (`.bak` / `.mdf`)**: `NOT FOUND IN SOURCE` (ไม่มีไฟล์ Backup ฐานข้อมูลใน Repository)
- **ช่องทางเชื่อมฐานข้อมูลอื่นนอกจาก MSSQL**: **ไม่พบใน Source** (พบเฉพาะ `netserverLib/s_COdbcSupervisor.cpp` ที่ใช้ Windows ODBC `::SQLConnect` / `SQLAllocHandle`)

## 2. รายชื่อ Server Executables (Win32 / 32-bit)
- `LoginServer` (32-bit, เชื่อม DB ผ่าน ODBC)
- `SessionServer` (32-bit, เชื่อม DB ผ่าน ODBC)
- `AgentServer` (32-bit, เชื่อม DB ผ่าน ODBC)
- `FieldServer` (32-bit, เชื่อม DB ผ่าน ODBC)
- `GameServer` (32-bit, เชื่อม DB ผ่าน ODBC)
- `BoardServer` (32-bit)
- `BugTrapServer` (32-bit)
- `ServerController` / `ServerManager` / `ServerMessanger` (32-bit)

## 3. Port และไฟล์ Config
- **Client → LoginServer Port**: `5001` (ตั้งค่าใน `RANMobile/Config/server_config.json`)
- **ไฟล์ Config ฝั่ง Server**: `NOT FOUND IN SOURCE` (ไฟล์ `.cfg` / `.ini` ของ Server ไม่ได้รวมอยู่ใน C++ Source Repo)
