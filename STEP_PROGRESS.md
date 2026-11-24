# STEP_PROGRESS.md — FAM HAWAII (RAN ONLINE EP7 MOBILE)

## 1. Build Status
- **Android Client (`app-debug.apk` ARM64 + `libranmobile-arm64-v8a`)**: **PASS** (Run `45486024663`, `d7ed063`)
- **Original Windows EP7 Server (`Smtm2022.sln`)**:
  - `build-ep7-windows-server.yml` Run `45489604587` (`c98a12c`): **กำลังทำงาน** (โพสต์ผล MSBuild PASS/FAIL ลง PR #2)
  - ฐานข้อมูลต้นฉบับ (`RanGame1`, `RanLog`, `RanShop`, `RanUser` `.bak`/`.sql`): พบครบใน `matheomedrana/Ran-Online-EP7-Complete-Files` (`Ran Online Database/`)
- **Phase 1 Complete Files Audit (`Ran-Online-EP7-Complete-Files` Git LFS)**: **PASS** (Run `45477556185`)

## 2. Integration Status
- **Texture Pipeline (`EP7TextureDecoder` + `MobileRenderer`)**:
  - รองรับ `.mtf` Decrypt (`TEX_XOR_DATA=0x47`, `TEX_DIFF_DATA=0x03`) และ `.dds` **DXT1 / DXT3 / DXT5** -> RGBA8
  - เพิ่ม `MobileRenderer::UploadRGBA8Texture`, `MobileRenderer::DrawTexturedQuad` และ `MobileRenderer::DeleteTexture` (GLES3 `glGenTextures` / `glTexImage2D` / `glDrawArrays` / `glDeleteTextures`)
  - เพิ่ม Unit Test ใน `test_main.cpp` (Test 19) และรวมเข้า `CMakeLists.txt` + CI Build แล้ว
- **Model/Map Parsers (`EP7WorldCharParser` + `BinaryReader`)**:
  - ตรวจสอบ Header/Version จาก Source จริง: `.wld` (`0x0108..0x0114`), `.chf` (`0x0100/0x0101`), `.cps` (`0x00000107`)
  - เพิ่ม `BinaryReader` พร้อม Bounds Checking, Length Validation, Error Handling และ Unit Test (Test 20: Truncated/Invalid Header/Invalid Version/Out-of-Bounds/Invalid Length)
  - **ข้อจำกัด**: ยังไม่ได้ทดสอบกับไฟล์ `.wld`/`.chf`/`.cps` ขนาดเต็มบน Android Device (รอเชื่อม Octree/Mesh/Piece เข้า `MobileRenderer`)
- **Server (`MobileServerHost` & Network Audit)**:
  - **Protocol ที่ยืนยันจาก Source จริง (`netclientLib/s_NetGlobal.h`)**:
    - `NET_MSG_HEARTBEAT_CLIENT_REQ` (160) / `NET_MSG_HEARTBEAT_CLIENT_ANS` (161)
    - `NET_MSG_LOGIN_2` (`NET_MSG_LOBBY + 107`), `NET_MSG_LOGIN_FB` (`NET_MSG_LOBBY + 108`), `NET_MSG_LOGIN_FB_SUB_OK_FIELD` (`NET_MSG_LOBBY + 109`)
  - **Protocol ที่ยังไม่ยืนยัน (`PROTOCOL NOT VERIFIED`)**:
    - Character Select/Create, Agent->Field Transfer, Movement, Combat, Skill, Inventory, Shop, Chat (ยังไม่ได้ดึง struct จาก `GLContrlMsg.h` / `s_NetGlobal.h`)
  - `MobileServerHost` รองรับเฉพาะ `EP7_NET_MSG_LOGIN_2` + Local Account (ยัง **ไม่ใช่ Full EP7 Server** — ยังขาด Agent/Field/World/MSSQL)
  - **ข้อมูลที่ยังขาด**: `MSSQL .bak/.mdf` และ `Server .cfg/.ini` (`DATA NOT FOUND IN SOURCE` ใน repo หลัก — รอตรวจจาก `Ran-Online-EP7-Complete-Files` / Google Drive)

## 3. Gameplay Test Status
- **Login / CharacterSelect / Map / Combat / Shop / Chat**: **NOT YET TESTED ON DEVICE** (รอเชื่อม Parser + Real Server)
