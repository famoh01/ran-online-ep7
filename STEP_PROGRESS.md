# STEP_PROGRESS.md — FAM HAWAII (RAN ONLINE EP7 MOBILE)

## 1. Build Status
- **Android Client (`app-debug.apk` ARM64 + `libranmobile-arm64-v8a`)**: **PASS** (Run `45475236136`, `ranmobile-android-packages` 1.56 MB, `libranmobile-arm64-v8a` 1.30 MB)
- **Android Server (`MobileServerHost` ARM64)**: **PASS** (Compile ผ่านใน Run `45475236136`)
- **Phase 1 Complete Files Audit (`Ran-Online-EP7-Complete-Files` Git LFS)**: **PASS** (Run `45477556185`)

## 2. Integration Status
- **Texture Pipeline (`EP7TextureDecoder`)**: รองรับ `.mtf` Decrypt (`TEX_XOR_DATA=0x47`, `TEX_DIFF_DATA=0x03`) และ `.dds` **DXT1 / DXT3 / DXT5** -> RGBA8 แล้ว (รอเชื่อม UploadTexture เข้า `MobileRenderer` GLES3)
- **Model/Map Parsers (`.wld`, `.chf`, `.cps`)**: ยืนยัน Header/Version จาก Source จริง (`DxSkinPiece::VERSION = 0x00000107`) — กำลังพัฒนา C++17 Binary Parser
- **Server (`MobileServerHost`)**: รองรับเฉพาะ `EP7_NET_MSG_LOGIN_2` + Local Account (ยัง **ไม่ใช่ Full EP7 Server** — ยังขาด Agent/Field/World/MSSQL)

## 3. Gameplay Test Status
- **Login / CharacterSelect / Map / Combat / Shop / Chat**: **NOT YET TESTED ON DEVICE** (รอเชื่อม Parser + Real Server)
