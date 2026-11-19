# คู่มือคอมไพล์ RAN Online EP7 Source Code (ran-online-ep7-main)

> สรุปจากการวิเคราะห์ไฟล์ `ran-online-ep7-main.zip` (43.5 MB) จริงทุกโฟลเดอร์

---

## ⚠️ สรุปสำคัญ: ทำไมคอมไพล์บนระบบคลาวด์ (Linux) ไม่ได้

ซอร์สชุดนี้เป็นโค้ด **Windows เท่านั้น** แบบ 100% คอมไพล์บน Linux/Mac หรือ sandbox คลาวด์ไม่ได้ เพราะ:

| สาเหตุ | รายละเอียด |
|---|---|
| **Visual Studio 2008 (.vcproj)** | Solution `Smtm.sln` เป็น Format Version 10.00 (VS2008) มี 33 โปรเจกต์ / 40 ไฟล์ .vcproj — ไม่มี .vcxproj เลย |
| **MFC (Microsoft Foundation Classes)** | โปรเจกต์หลักทั้งเซิร์ฟเวอร์ (GameServer, LoginServer, FieldServer ฯลฯ) และเครื่องมือ GUI ใช้ `afxwin.h` / `CString` — MFC มีเฉพาะใน Visual Studio บน Windows เท่านั้น (MinGW ไม่มี, Wine คอมไพล์ไม่ได้) |
| **DirectX 9** | Client/Engine ใช้ Direct3D9, DirectInput, DirectSound (header/lib แนบมาในแพ็กเกจแล้ว) |
| **Win32 API / Winsock / ODBC** | โค้ดเครือข่ายและฐานข้อมูลผูกกับ Windows โดยตรง (ใช้ ODBC ผ่าน `sqlext.h`, ไม่ใช่ DB-Library) |

**ต้องคอมไพล์บนเครื่อง Windows เท่านั้น** (ใช้ VM ก็ได้ เช่น VirtualBox/VMware/Hyper-V)

---

## 📦 สิ่งที่อยู่ในแพ็กเกจ (ตรวจสอบแล้วว่าครบ)

- **33 โปรเจกต์** ใน `Smtm.sln`:
  - **Server (ไฟล์ .exe):** GameServer, LoginServer, FieldServer, AgentServer, SessionServer, BoardServer, ServerManager, ServerController, ServerMessanger
  - **Static Library (.lib):** enginelib, RanClientLib, netserverLib, netclientLib, EngineUILib, EngineSoundLib, MfcExLib, minLib, Basic ฯลฯ
  - **Tools:** Emulator, PatchPrimeMan, AutoPatchMan, VersionManager, VerMan, TexEncrypt, NpcAction, MMPC, superpad, xskinexp (ปลั๊กอิน 3ds Max), CrashExplorer, BugTrap
- **Library แนบครบ:** `=DX_Include` + `=DX_Lib/x86` (d3d9.lib, d3dx9.lib, dinput8.lib…), `=Library` (dbghelp, ijl15, lzo2, ogg/vorbis, GameGuard, DaumCrypt), `=boost`, `=ZLIB`, `=TbbLib`
- **Build configurations 44 แบบ** รวม `Release`, `ReleaseD`, `Debug` และรีเจียนต่าง ๆ — มี **`Release_Thailand`** ด้วย (ค่าที่คอมไวน์โค้ดสำหรับเซิร์ฟเวอร์ไทย)
- โค้ดทั้งหมด ~191 MB (หลังแตก zip), 1,344 ไฟล์ .cpp, 3,754 ไฟล์ .h
- คอมเมนต์ในโค้ดและ ReadMe เป็นภาษาเกาหลี (encoding EUC-KR)

### ปัญหาที่ตรวจพบล่วงหน้า (แก้ก่อนคอมไพล์)

1. **โฟลเดอร์ `sql/include` หายไป** — `GameServer.vcproj` อ้างถึง `$(SolutionDir)sql/include` → ให้สร้างโฟลเดอร์ `sql/include` แล้วคัดลอก header ODBC (`sql.h`, `sqlext.h`, `sqltypes.h`) จาก Windows SDK ไปวาง หรือแก้ Additional Include Directories ชี้ไปที่ `C:\Program Files (x86)\Microsoft SDKs\Windows\...\Include`
2. **ไฟล์ SourceSafe ค้างอยู่ 417+39 ไฟล์** (`vssver2.scc`, `*.vspscc`) — VS จะถามเรื่อง source control ทุกครั้ง → ลบทิ้งให้หมด หรือกด "Permanently remove source control bindings" ตอนเปิด solution ครั้งแรก
3. **config `Release_RZ` ไม่มีจริง** — ไฟล์ `.github/workflows/ci.yml` เขียนคอมไพล์ `Release_RZ` แต่ solution ไม่มี config นี้ (CI นั้นใช้ไม่ได้ ข้ามไป)
4. ตามโพสต์ต้นทาง (RaGEZONE) แจ้งว่า **มี error ในโค้ด 2 จุด** แก้ง่าย — มักเป็นเรื่อง header ขาด/สะกดผิด คอยดู error log ตอนคอมไพล์

---

## 🛠️ สิ่งที่ต้องเตรียม

1. **Windows 7 / 10 / 11** (64-bit ก็ได้ เพราะ build เป็น Win32) — VM ใช้ได้
2. **Visual Studio 2008 Professional SP1** ⚠️ ต้องเป็นรุ่น Professional ขึ้นไปเท่านั้น — **Express Edition ไม่มี MFC ใช้ไม่ได้**
3. **Windows SDK** (6.0A ติดมากับ VS2008 หรือ 7.1 แยก) — สำหรับ header ODBC/Win32
4. ~~DirectX SDK~~ — **ไม่ต้องติดตั้งเพิ่ม** เพราะ header/lib แนบมาใน `=DX_Include` และ `=DX_Lib` แล้ว และโปรเจกต์อ้าง path แบบ relative (`$(SolutionDir)`) ไม่ได้ hardcode ที่อยู่ไว้
5. *(สำหรับรันเซิร์ฟเวอร์ ไม่ใช่ตอนคอมไพล์)* SQL Server + สร้าง ODBC DSN + ไฟล์เกม/ฐานข้อมูล EP7

## 📝 ขั้นตอนคอมไพล์

1. แตก zip ไว้ที่ path **ภาษาอังกฤษล้วน ไม่มีช่องว่าง** เช่น `D:\RanSource\ran-online-ep7-main`
2. ลบไฟล์ source control: ค้นหาและลบ `vssver2.scc` และ `*.vspscc` ทั้งหมด
3. สร้างโฟลเดอร์ `sql\include` ข้าง ๆ `Smtm.sln` แล้วคัดลอก `sql.h / sqlext.h / sqltypes.h / sqlucode.h` จาก Windows SDK ไปใส่
4. เปิด `Smtm.sln` ด้วย VS2008 → ถ้าถาม source control ให้เลือก *Permanently remove bindings*
5. เลือก configuration:
   - `Release_Thailand` — เซิร์ฟเวอร์ภาษาไทย
   - `Release` / `Release_Global` — ทั่วไป
   - `Debug` — สำหรับ debug
6. เช็คว่า Code Generation ตรงกันทุกโปรเจกต์ (ReadMe ของเกาหลีแนะนำ `/MTd` สำหรับ Debug)
7. เพิ่ม preprocessor `_WIN32_WINNT=0x0500` ถ้าโปรเจกต์ไหนยังไม่มี (ตาม ReadMe ต้นฉบับ)
8. **Build → Build Solution** — dependency ถูกผูกไว้ใน solution แล้ว (lib พื้นฐาน → enginelib → server)
9. ผลลัพธ์: `.lib` ไปที่ `_dlib\` และ `.exe` ของแต่ละเซิร์ฟเวอร์ในโฟลเดอร์โปรเจกต์ของมัน

## ❗ Error ที่พบบ่อย

- `fatal error C1083: afxwin.h` → ใช้ VS รุ่น Express (ไม่มี MFC) ต้องเปลี่ยนเป็น Professional
- `warning C4819` → ไฟล์มีคอมเมนต์เกาหลี (EUC-KR) บน Windows ภาษาอื่น — เป็นแค่ warning ข้ามได้ หรือแปลงไฟล์เป็น UTF-8 with BOM
- `cannot open file 'd3dx9.lib'` → ตรวจ Additional Library Directories ว่าชี้ `=DX_Lib\x86`
- error แปลก ๆ หลังแก้โค้ด → ลบโฟลเดอร์ `_rbuilddata` แล้ว Rebuild ใหม่
- เปิด solution ไม่ได้ → เช็คว่าไฟล์/โฟลเดอร์ไม่มีชื่อภาษาเกาหลีค้าง (ชุดนี้ตรวจแล้ว **ไม่มี**)

## 🚀 ทางเลือกที่ง่ายกว่า (แนะนำถ้าใช้ Windows 10/11)

ชุมชน RaGEZONE มีการพอร์ตซอร์ส EP7 ไปยัง **Visual Studio 2022** แล้ว คอมไพล์บนเครื่องปัจจุบันได้เลย ไม่ต้องหา VS2008:

- **Juver Ran EP7 (VS 2022)** — build ได้ใน 3–5 นาที ไม่ต้องตั้งค่าเพิ่ม
- **Ye Xiu PH RAN Online x64** — พอร์ตเป็น 64-bit เต็มรูปแบบ (client + server + tools)

ถ้าเป้าหมายคือทำเซิร์ฟเวอร์ส่วนตัวให้รันได้ การใช้ซอร์สพอร์ต VS2022 จะประหยัดเวลากว่ามาก

---

*จัดทำโดยวิเคราะห์จากซอร์สจริง — ถ้าคอมไพล์บน Windows แล้วติด error ตรงไหน ส่ง error log มาให้ช่วยดูได้ครับ*
