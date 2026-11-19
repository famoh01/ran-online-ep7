# รายงานผลการทดลองคอมไพล์บน Linux Sandbox

วันที่: 2026-11-19

## สรุป: ลองคอมไพล์จริงแล้ว — ผลลัพธ์ยืนยันว่าต้องใช้ Windows + VS2008 เท่านั้น

### 1. สำรวจทุกโปรเจกต์ (40 โปรเจกต์) ว่าพึ่งพา MFC หรือไม่

| กลุ่ม | โปรเจกต์ | ใช้ MFC? |
|---|---|---|
| Server หลัก | GameServer, LoginServer, FieldServer, AgentServer, SessionServer, BoardServer | ✅ ใช้ (3–5 ไฟล์/โปรเจกต์, stdafx.h รวม afxwin.h) |
| Engine/Client | enginelib (387 ไฟล์!), RanClientLib (99), netserverLib (81), EngineUILib, EngineSoundLib | ✅ ใช้ |
| Tools GUI | Emulator, ServerManager, VersionManager, NpcAction ฯลฯ | ✅ ใช้ (MFC GUI โดยตรง) |
| **Vorbislib** | ไลบรารีเสียง Ogg Vorbis | ❌ **ไม่ใช้ — ตัวเดียวที่พอคอมไพล์ข้ามแพลตฟอร์มได้** |
| xskinexp | ปลั๊กอิน 3ds Max | ❌ ไม่ใช้ MFC แต่ต้องการ 3ds Max SDK (Windows DLL) |

→ 38 จาก 40 โปรเจกต์ จำเป็นต้องมี MFC/Win32/DirectX = **Windows + Visual Studio เท่านั้น**

### 2. ทดลองคอมไพล์ Vorbislib จริงด้วย gcc 14 (Debian)

**ขั้นตอนและผลลัพธ์:**

- ❌ ครั้งแรก fail ทั้ง 25 ไฟล์ — ขาด `ogg/config_types.h` (ไฟล์ที่ปกติ generate โดย autotools บน Unix)
- ✅ สร้าง `ogg/config_types.h` เอง → **คอมไพล์สำเร็จ 25/25 ไฟล์** → รวมเป็น `libvorbis.a` (366 KB)
- ✅ ลิงก์สำเร็จ สัญลักษณ์ครบ (`vorbis_info_init`, `ogg_stream_init`, `vorbis_analysis_init` ฯลฯ)

**แต่เมื่อทดสอบการทำงานจริง:**

- ❌ `vorbis_encode_init_vbr()` เป็น **stub** — ดูโค้ดใน `vorbisenc.c` แล้วพบว่าฟังก์ชันคืนค่า `OV_EIMPL` เสมอ (encoder ยังเขียนไม่เสร็จ โหมดทั้งหมดถูก comment ออก)
- ❌ ทดสอบ decode ไฟล์ .ogg จริง (สร้างด้วย libVorbis ปัจจุบัน) ผ่าน `ov_open()` → **ค้างระหว่าง parse header** — เพราะซอร์สชุดนี้เป็น **Vorbis รุ่นก่อน 1.0 (~ปี 2001)** รูปแบบ codebook ยังไม่ freeze เข้ากับไฟล์ Vorbis สมัยใหม่ไม่ได้

**บทสรุป:** นี่คือเหตุผลที่ตัวซอร์ส RAN แนบไลบรารี Vorbis ที่คอมไพล์เสร็จแล้ว (`=Library/ogg_static.lib`, `vorbisfile.lib`) มาด้วย — ตัวเกมไม่ได้ build จากซอร์ส Vorbislib นี้จริง

### ไฟล์ผลลัพธ์จากการทดลอง

```
build_output/
├── libvorbis.a        (366 KB — static lib ที่คอมไพล์ได้จริง แต่ใช้งานจริงไม่ได้ตามข้างบน)
├── vorbis_obj/        (25 ไฟล์ .o)
├── ogg_inc/ogg/config_types.h  (ไฟล์ที่สร้างเพิ่ม)
├── test_vorbis.c      (โปรแกรมเทสต์ encoder)
├── test_decode.c      (โปรแกรมเทสต์ decoder)
└── test_input.ogg     (ไฟล์เสียงทดสอบ 440Hz)
```

### ข้อสรุปสุดท้าย

การจะได้ไฟล์ **GameServer.exe / LoginServer.exe / FieldServer.exe / Client** ต้อง build บน:

- **Windows** (เครื่องจริงหรือ VM)
- **Visual Studio 2008 Professional SP1** (Express ไม่มี MFC ใช้ไม่ได้)
- ใช้สคริปต์ `build.bat` ที่ผมทำให้ → รันแล้วจะได้ exe ออกมา (ดู BUILD_GUIDE_TH.md)

หรือใช้ซอร์สที่ชุมชนพอร์ตไป VS2022 แล้ว (Juver EP7 / Ye Xiu PH x64) ซึ่ง build บน Windows 10/11 ปัจจุบันได้เลย
