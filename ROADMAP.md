# ROADMAP.md — แผนการทำงาน FAM HAWAII (RAN ONLINE EP7 MOBILE)

- **PHASE 0 (Audit & Status)**: จัดทำ `PROJECT_STATUS.md`, `SOURCE_MANIFEST.csv`, `MISSING_FILES.md`, `BUILD_STATUS.md`, `ROADMAP.md` (**กำลังดำเนินการ**)
- **PHASE 1 (EP7 Data & Manifest)**: ตรวจสอบ `RAN-EP7-Client-TH.zip` / `ServerDBTools.zip` และแหล่งสำรอง (`Ran-Online-EP7-Complete-Files`) เพื่อหา `.bak` และ server `.cfg`
- **PHASE 2 (Mobile Engine)**: เชื่อม Parser จริง (`.wld`, `.chf`, `.cps`, `.mtf`, `.dds`) เข้ากับ `MobileRenderer` (GLES3)
- **PHASE 3 (Android Client)**: เชื่อม Login -> CharacterSelect -> InGame (Map, Monster, Combat, Skill, Inventory, Shop, Chat)
- **PHASE 4 (Server & DB)**: ทดสอบ `MobileServerHost` (ARM64 TCP 5001) และเตรียมเชื่อมต่อ Windows EP7 Server จริง
- **PHASE 5–7 (Integration, Build & Device Test)**: Build APK บน GitHub Actions และส่งลิงก์ให้ทดสอบบนมือถือ Android จริงครบทุกขั้นตอน
