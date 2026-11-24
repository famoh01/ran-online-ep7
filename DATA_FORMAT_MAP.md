# DATA_FORMAT_MAP.md — RAN ONLINE EP7 (FAM HAWAII MOBILE)

*หมายเหตุ: อ่านและวิเคราะห์จาก Source Code ใน `famoh01/ran-online-ep7` และผลตรวจ `Run 45341746637` เท่านั้น (Read-Only)*

## 1. ตารางวิเคราะห์ 30 นามสกุลหลักจาก Original Client
| นามสกุล | คืออะไร | ไฟล์ Source / ฟังก์ชันที่ Parse | ขึ้นกับ DirectX / Windows | แนวทางพอร์ตไป Android |
|---|---|---|---|---|
| `.dds` | DirectDraw Surface Texture | `enginelib/DxCommon/TextureManager.cpp` | DirectX 9 (`D3DXCreateTextureFromFileEx`) | แปลงเป็น **ETC2 / ASTC** หรือถอดรหัส DXT1/3/5 เป็น RGBA8 |
| `.cps` | Character Piece Skin | `enginelib/Meshs/DxSkinPieceContainer.cpp` (`LoadPiece`) | DirectX 9 Mesh/Material | เขียน C++ Binary Parser ใหม่ -> OpenGL ES VBO |
| `.cfg` | Animation / Effect / Server Config | `enginelib/Meshs/DxAniKeys.cpp`, `1_Server/cfg/*.cfg` | Cross-platform (Text/Binary) | พอร์ต Parser ได้โดยตรง |
| `.bin` | Animation Key / Binary Data | `enginelib/Meshs/SAnimation.cpp` | Windows Binary I/O (`basestream`) | พอร์ต `CSerialFile` เป็น `std::ifstream` |
| `.x` | DirectX `.X` 3D Mesh | `enginelib/Meshs/DxSkinMesh9.cpp` | DirectX 9 (`D3DXLoadMeshHierarchyFromX`) | ต้องแปลงล่วงหน้าเป็น `.obj`/`.glb` หรือ Binary Mesh บน Cloud |
| `.egp` | Effect Group Property | `enginelib/DxEffect/DxEffSinglePropGMan.cpp` (`LoadEffectGProp`) | DirectX 9 | เขียน Parser + Particle Renderer บน GLES3 |
| `.chf` | Character File | `enginelib/Meshs/DxSkinCharData.cpp` (`LoadFile`) | DirectX 9 | พอร์ต Parser -> โหลด `.cps` + `.cfg` |
| `.abl` | Attach Bone Link | `enginelib/Meshs/DxAttBoneLink.cpp` | DirectX 9 | พอร์ต Parser |
| `.qst` | Quest Data | `RanLogic/Quest/GLQuestMan.cpp` | Cross-platform | อ่านได้โดยตรง (1,046 ไฟล์ใน ThaiPack) |
| `.wav` | Audio Waveform | `enginelib/DxSound/DxSoundMan.cpp` | DirectSound | เล่นผ่าน **Oboe / OpenSL ES** บน Android |
| `.mtf` | Encrypted Texture (`.dds` / `.tga` ที่เข้ารหัส) | `enginelib/DxCommon/TextureManager.cpp:138`, `MemoryTexture.cpp:144` | Header: `nVersion`, `nSize`, `nFileType` + XOR (`TEX_XOR_DATA`) & `- TEX_DIFF_DATA` | **พอร์ตได้ 100%** (ถอดรหัสเป็น `.dds`/`.tga` แล้วแปลงเป็น ETC2/ASTC) |
| `.aps` / `.pis` / `.pie` / `.vps` / `.wld0` / `.pie15` / `.enm` | `NOT FOUND AFTER EXTENDED SEARCH` (ค้นหาด้วย `grep -rnE` + `Format\|StringCch\|strcat\|find\|ext` ใน `enginelib/`, `RanClientLib/`, `RanLogic/` แล้วไม่พบ) | - | - | - |
| `.abf` | Attach Bone File | `enginelib/Meshs/DxAttBoneData.cpp` | DirectX 9 | พอร์ต Parser |
| `.pis` | Piece / Skin | `NOT FOUND IN SOURCE` | - | - |
| `.ntk` | NPC Talk Data | `RanLogic/NpcTalk/NpcTalkMan.cpp` | Cross-platform | อ่านได้โดยตรง |
| `.genitem` | Item Drop / GenItem | `RanLogic/Item/GLItemMan.cpp` | Cross-platform | อ่านได้โดยตรง |
| `.lev` | Level / Map Placement | `RanLogic/GLogicData.cpp` (`GLLevelFile::LoadFile`) | Windows Binary I/O | พอร์ต Parser |
| `.pie` | Piece | `NOT FOUND IN SOURCE` | - | - |
| `.effskin_a` | Skin Effect | `enginelib/DxEffect/Char/DxEffChar.cpp` | DirectX 9 | เขียน GLES3 Shader Adapter |
| `.png` / `.tga` / `.jpg` | Standard Image Textures | `TextureManager.cpp` | DirectX 9 (`D3DX`) | โหลดด้วย `stb_image` -> GLES3 |
| `.mmp` | MiniMap Image/Data | `RanClientLib/G-Logic/GLMapList.cpp` | Cross-platform | โหลดเป็น Texture บน GLES3 |
| `.crowsale` | NPC Shop Sale List | `RanLogic/Crow/GLCrowData.cpp` | Cross-platform | อ่านได้โดยตรง |
| `.vps` | Vertex/Pixel Shader หรือ Particle | `NOT FOUND IN SOURCE` | - | - |
| `.wld` | World / Map Geometry | `enginelib/DxLand/DxLandMan.cpp` (`LoadFile`) | DirectX 9 (`DxStaticMesh`) | แปลงเป็น Static Mesh + Lightmap สำหรับ GLES3 |
| `.charset` | Character Set Preset | `RanLogic/Character/GLCharDefine.cpp` | Cross-platform | อ่านได้โดยตรง |
| `.wld0` / `.pie15` / `.enm` | World0 / Piece15 / Enemy | `NOT FOUND IN SOURCE` | - | - |

## 2. การเข้ารหัส / บีบอัด (`FileCrypt.exe`)
- **ยืนยันจากไฟล์จริง**: ใน `RAN-EP7-ServerDBTools.zip` (`4_Tools_Editors/FileCrypt.exe`, `680 KB`, PE32) มีเครื่องมือเข้ารหัส/ถอดรหัสไฟล์อยู่จริง และใน `enginelib/Common/SerialFile.cpp` มีการใช้ `Rijndael` / XOR สำหรับไฟล์บางประเภท
- **สันนิษฐาน**: ไฟล์ข้อมูลเกมหลัก (`.wld`, `.lev`, `.isf`, `.ssf`) ใช้รูปแบบ Binary Serialization ของ `CSerialFile` โดยอาจมีการเข้ารหัสบางไฟล์ด้วย `FileCrypt`

## 3. ชิ้นข้อมูลทดสอบจริง 1 ชุด (Proof-of-Pipeline โดยไม่ต้องมี Server)
- **แผนที่**: `login.wld` (`3,098,590` bytes, อ้างอิงใน `RanClientLib/DxLobyStage.cpp:624`) พร้อม Texture `.dds` / `.tga` ที่อ้างอิงภายใน
- **ตัวละคร**: `s01_01.chf` (อ้างอิงใน `RanClientLib/DxLobyStage.cpp:387`) โหลดผ่าน `DxSkinCharDataContainer::LoadData` -> `DxSkinCharData::LoadFile` (`enginelib/Meshs/DxSkinCharData.cpp`) ซึ่งอ้างอิงไฟล์ `.cps` (Piece), `.x` (Mesh), `.cfg`/`.bin` (Animation), `.abl`/`.abf` (Attach Bone), และ `.dds`/`.mtf` (Texture) ทั้งหมดมี Parser ใน Source จริง (ยกเว้น `.x` ที่ต้องแปลงล่วงหน้าบน Cloud)

## 4. ประเมินขนาดหลังแปลง & วิธีส่งเข้าแอป
- **ขนาดต้นฉบับ**: `data` (2.5 GB) + `textures` (3.8 GB) + `sounds` (205 MB) ≈ **6.5 GB**
- **ประเมินหลังแปลงสำหรับมือถือ (สันนิษฐาน)**: แปลง `.dds` -> **ETC2/ASTC** และ `.wav` -> **OGG/Opus** คาดว่าจะลดขนาดลงเหลือประมาณ **1.8 – 2.5 GB**
- **วิธีส่งเข้าแอป**: ไม่ควรยัดทั้งหมดลง APK (Play Store จำกัด APK/AAB) ควรใช้ **On-demand Asset Download** ตอนเปิดแอปครั้งแรกจาก CDN/Release แบ่งแพ็กตามแผนที่
