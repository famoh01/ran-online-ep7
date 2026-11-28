# Phase 3J B12 — Authentic EP7 Animation Container (`SAnimContainer` / `.bin`) Investigation

- **Date:** 2026-11-27
- **Branch:** `arena/00fd13f9-ran-online-ep7` (stacked Draft PR #7 -> `arena/42c5ac0c-ran-online-ep7` -> Draft PR #5 -> `main`)
- **Scope:** Read-only forensic source + authentic-asset investigation. **Zero production importer (`RANMobile/Data/Importer/EP7Animation.{h,cpp}`) or B11 verifier (`RANMobile/Tools/verify_ep7_character_assets.cpp`) behavior was modified.**

---

## 1. Executive Summary

Following the Phase 3J B11 `.bin` verifier correction, authentic release verification against `ran-ep7-full-v1` (`RAN-EP7-Client-TH.zip`) classified the `6,228` loose `.bin` files under `data/animation/` into:

- **`982` parsed** (`0x0100`: `1`, `0x0101`: `907`, `0x0102`: `74`)
- **`4,037` unsupported outer container version** (`0x0103`: `1,533`, `0x0104`: `2,495`, `0x0200`: `9`)
- **`1,209` parser-rejected** after a recognized `AnimContainer` header (`0x0100`: `1,209`)
- **`0` invalid-header**, **`0` unrecognized-type**, and **`0` `.bin` entries inside `.rcc` packs**

This B12 investigation retrieved the complete EP7 engine animation serialization sources from the verified backup repository (`matheomedrana/Ran-Online-EP7-Complete-Files` @ `f661fbf89612cc77f95a7c9b935ff06b22bc55e3`) and executed a read-only forensic inspection + source-derived shadow verification across **all `6,228` authentic `data/animation/*.bin` files (`384,339` bone tracks)** in CI run [`45982141864`](https://github.com/famoh01/ran-online-ep7/actions/runs/45982141864).

### Key Conclusions

1. **Every single one of the `6,228` authentic `data/animation/*.bin` files (`100.00%`) is a valid, uncorrupted `SAnimContainer` binary that parses to zero-trailing-byte `EXACT_EOF` under the authoritative `Lib_Engine/Meshs/SAnimationSaveLoad.cpp` specification.** None of the `1,209` `parser-rejected` files are malformed assets.
2. **Exact root cause of all `1,209` `parser-rejected` files:**
   - All `1,209 / 1,209` files have outer container version `0x0100` (`numMain >= 1`) and store per-bone `SAnimation` tracks with **`dwVersion = 0x0100`**.
   - `EP7AnimationReader::ParseContainer()` (`EP7Animation.cpp:330-335`) accepts outer container `0x0100`, so the single `0`-track file `data/animation/a_mob_wdm_stay01.bin` (`136` bytes) succeeds without entering `ReadBoneTrack()`.
   - Every one of the remaining `1,209` `0x0100` files has `numMain >= 1`, enters `ReadBoneTrack()` at **byte offset `136`** (`list="main"`, `trackIndex=0`), reads `trackVer = 0x0100`, and is immediately rejected because `ReadBoneTrack()` only implements `0x0102` and `0x0101` (`EP7Animation.cpp:18-205`), failing with `"unsupported animation version"`.
3. **Critical latent parser defect discovered in `EP7Animation.cpp` (`SMatrixKey` 68-byte read vs 80-byte on-disk stride):**
   - On 32-bit Windows MSVC (`_MSC_VER`, x86), `SMatrixKey` (` { DWORD dwTime; D3DXMATRIXA16 mat; } `) has `sizeof(SMatrixKey) == 80` because `D3DXMATRIXA16` carries `__declspec(align(16))`, inserting **12 bytes of uninitialized heap padding at struct bytes `[4..15]`** before the 64-byte `4x4` float matrix at `[16..79]`.
   - `EP7AnimationReader` correctly defines `kMatrixKeySize = 80` and checks `s.CanReadElements(count, kMatrixKeySize)`, **but its read loops (`EP7Animation.cpp:88-104` for `0x0102` and `166-178` for `0x0101`) read `ReadU32(time)` + `16 * ReadF32(mat)` = `68` bytes without `s.Skip(12)`!**
   - This reads 12 bytes of Win32 heap garbage into `mat[0..2]` and shifts the stream cursor backward by `12 * m_cMatrixKeys` bytes.
   - Why this defect did not trigger on the `982` currently accepted files: `0` of those `982` `0x0101`/`0x0102` files have `m_cMatrixKeys > 0` (all `979` non-empty accepted files had converted matrices to `SQuatPosKey` via `SAnimation::ConvertMatToQuat()`).
   - Why this defect was masked in synthetic unit tests: in `RANMobile/Tests/test_phase3j.cpp:406-434` (`AnimationBin`) and `1033-1047` (`TestAnimation`), `AnimationBin()` explicitly wrote a real 80-byte `SMatrixKey` (with 12 zero padding bytes at `:429` and identity matrix ending in `0.0f, 0.0f, 1.0f`), and `ParseContainer()` misread the final three floats (`0.0f, 0.0f, 1.0f`) as `scaleCount = 0` and `mainTrackCount = 0` (`upBody` came first in `0x0102`, `main` had 0 tracks!), returning `clip.valid == true` with 12 unread trailing bytes while `clip.upBodyTracks[0].matrixKeys[0].mat[0]` silently became `0.0f` instead of `1.0f`!
   - **Blast radius on `0x0100`:** **`1,205` of `1,210` (`99.59%`)** authentic `0x0100` files have `m_cMatrixKeys > 0`. Adding `SAnimation::Load_0100` without first fixing the 12-byte `SMatrixKey` alignment-padding skip would corrupt or desynchronize `1,205` authentic files.
4. **Complete specifications verified for all unsupported outer versions (`4,037` files):**
   - **`0x0103` (`1,533` files):** unencrypted; `upBody` list (`SAnimation` `0x0103`), **`DWORD dwGarbageValue` (`4` B; equals `0x0103 + numUpBody + numMain` in `1,533 / 1,533` files)**, `main` list (`SAnimation` `0x0103`).
   - **`0x0104` (`2,495` files):** unencrypted; **three** sequential track lists (`SAnimation` `0x0103`): `main` list (`2,491` non-empty), `upBody` list #1 (`810` non-empty), and `upBody` list #2 (`628` non-empty; both `upBody` lists append to `m_listAniUPBODY` in `SAnimContainer::Load_0104`).
   - **`0x0200` (`9` files):** 132-byte header is plaintext; payload `[132..EOF)` is byte-encoded with **`EMBYTECRYPT_BIN` (`ARRAY_BIN_VAR1`, 256-byte substitution table)**; decoded payload has the exact `0x0102` container layout (`upBody` list, `main` list) containing `SAnimation` `0x0103` tracks.
   - **Per-bone track `0x0103` (`244,046` tracks across `0x0103`/`0x0104`/`0x0200` containers):** differs from `0x0102` ONLY in the placement of the two `DWORD` obfuscation words (`dwGarbage1 = m_strBone.length()` moves **before** `posKeys`; `dwGarbage2 = m_strBone.length() + 0x0103` moves **after** `matrixKeys`). All keyframe struct layouts and quaternion compression (`QUATCOMP`) are 100% unchanged across `0x0100`–`0x0200`.

---

## 2. Verified Baseline & B11 Reconciliation

| Metric | `data/animation/` Scope (matches B11 verifier) | Non-Animation Loose `.bin` Outside `data/animation/` | Tree-Wide Loose `.bin` Total | `.rcc` Pack `.bin` Entries |
|---|---:|---:|---:|---:|
| Total `.bin` files | **6,228** | 357 | 6,585 | **0** |
| `parsed` (unmodified `EP7AnimationReader`) | **982** | 0 | 982 | 0 |
| `unsupported-version` (`0x0103`, `0x0104`, `0x0200`) | **4,037** | 0 | 4,037 | 0 |
| `parser-rejected` (`AnimContainer` header, parser failed) | **1,209** | 0 | 1,209 | 0 |
| `invalid-header` (`< 132` B or unterminated type string) | **0** | 34 | 34 | 0 |
| `unrecognized-type` (132B header, type `!= "AnimContainer"`) | **0** | 323 | 323 | 0 |

- Every `data/animation/*.bin` bucket (`982 + 4,037 + 1,209 + 0 + 0 = 6,228`) reconciles to the exact integer with B11 (`verify_ep7_character_assets.cpp`).
- The `357` non-animation `.bin` files outside `data/animation/` are root patch metadata (`cVer.bin`, `cFileList.bin`) and `data/glogic/*.bin` / `data/glogicserver/*.bin` logic tables (`verify_ep7_character_assets.cpp` properly scopes loose `.bin` scanning to `data/animation/`).
- All `6,228` `AnimContainer` 128-byte `szFileType` headers are strictly zero-padded (`zero_padded = 6,228`, `nonzero_trailing_bytes = 0`).

---

## 3. Authoritative Source Evidence Provenance

Two engine source trees were cross-checked:

1. **Current repository (`famoh01/ran-online-ep7`):**
   - `enginelib/Meshs/SAnimation.h` (`SAnimation::VERSION = 0x0102`, `SAnimContainer::VERSION = 0x0102`)
   - `enginelib/Meshs/SAnimation.cpp` (implements `SAnimation::Load_0100`, `Load_0101`, `Load_0102`, and `SAnimContainer::Load_0100`, `Load_0101`, `Load_0102` at lines `1184`–`1560`)
   - `enginelib/Meshs/DxAniKeys.h` (`UNITANIKEY_PERSEC = 4800`, `SPositionKey`, `SRotateKey`, `SScaleKey`, `SMatrixKey`, `SQuatPosKey`)
2. **Verified full EP7 engine backup repository (`matheomedrana/Ran-Online-EP7-Complete-Files` @ commit `f661fbf89612cc77f95a7c9b935ff06b22bc55e3`, identical tree to Google Drive `Ran-Online-Episode-7-main.zip` SHA-256 `9c11d62d9f2b12b8869a49c42bf6c55c063abdb8a60ac611a1b53b165fd2256c`):**
   - Retrieved via targeted Git LFS fetch in `.github/workflows/phase3j_b12_animation_investigation.yml` (`b12-engine-anim-sources`, check run `140448637476`), with every file verified byte-for-byte against its committed Git LFS SHA-256 pointer:

| Backup Repo Path | Bytes | Verified LFS SHA-256 | Key Contents |
|---|---:|---|---|
| `Lib_Engine/Meshs/SAnimation.h` | 5,420 | `90c9a3673bddeeb9739c25b19b2e07c55228f1b819b2b770f943102607119002` | `SAnimation::VERSION = 0x0103`; `SAnimContainer::VERSION = 0x0200`, `VERSION_ENCODE = 0x0200` |
| `Lib_Engine/Meshs/SAnimationSaveLoad.cpp` | 18,473 | `e07686104ca1bbbfb431c63c8e79a954b4fb366ef36b048205e2a776d5da644b` | Full `SAnimation::Load_0100`–`0103` + `SaveToFile` (`0x0103`); `SAnimContainer::Load_0100`–`0104`, `Load_0200`, `SaveToFile` (`0x0200` + `EMBYTECRYPT_BIN`) |
| `Lib_Engine/Meshs/SAnimation.cpp` | 34,515 | `187bd5e515df14df5ba2c46109e249897be4a009749996d544be47503981b48e` | `SetTime`, `SetMix`, `ConvertMatToQuat`, `UpdateTime`, `UpdateMix`, `.x` loader |
| `Lib_Engine/Meshs/SAnimationInfo.h` | 7,377 | `c877c09767b54a8364a5b214ed58e0d8c0f246db40a0e004f8015772d5027f75` | `SANIMCONINFO::VERSION = 0x0200`, `ACF_*` flags, `AN_TYPE_SIZE = 34`, `AN_SUB_00_SIZE = 100` |
| `Lib_Engine/Meshs/SAnimationInfoSaveLoad.cpp` | 13,516 | `279a50708cd9778b9703cf47b470c7484f36e34f38411f90f457dc3444da62e2` | `SANIMCONINFO::Load_0101`–`Load_0115`, `Load_0200` (`EMBYTECRYPT_CFG`) |
| `Lib_Engine/Meshs/DxAniKeys.h` | 6,931 | `d3020d82c6f1b8bf822301573026243b2421265c9615035a5a19833d20961ca4` | `UNITANIKEY_PERSEC = 4800`, `SPositionKey`, `SRotateKey`, `SScaleKey`, `SMatrixKey`, `SQuatPosKey`, `QUATCOMP` |
| `Lib_Engine/Meshs/DxAniBoneScale.h` | 1,913 | `85358bf4b6f3b025551a0bf2b51343299a9e1a36b8da4221bf8f4f5c79e904d2` | `DxAniScale` / `DxAniScaleMAIN` |
| `Lib_Engine/Meshs/DxAniBoneScale.cpp` | 6,284 | `be5b21a5b9496e900cfde171ab27bc5bc987a77b933688890878a5d81948aa7e` | `.cfg` bone-scale table serialization |
| `Lib_Engine/Meshs/DxSkinAniMan.cpp` | 16,177 | `d9b0b123b165c7ab56395b2069b274d35e54079b80be3540a57f976610968696` | `LoadAnimContainer` pairing `.cfg` + `.bin` |
| `Lib_Engine/Meshs/DxSkinAniControl.cpp` | 27,236 | `b782abdc0328cb5b79c64b0c0153a548155a96db338a02e88a675a66466d3230` | Runtime playback, upper/lower body mixing |
| `Lib_Engine/Common/basestream.h` | 5,625 | `6021a56bd3bc6fd60d5044932bb04016b162f6d9da143949c1cf36b007b58565` | `FILETYPESIZE = 128`, `m_FileID` |
| `Lib_Engine/Common/SerialFile.cpp` | 13,895 | `21564a020e3df749c54f93e7c71fdca72652a7a3fd87d396608b4984499b5276` | `ReadFileType`, `SetEncodeType`, `BYTECRYPT::byte_decode`, `operator>>(std::string&)` |
| `Lib_Engine/Common/ByteCrypt.h` | 1,601 | `29f1a10e00eb108a3de765e54cf3111c670f8b67d1b069f92bc5e28d18adfc15` | `EMBYTECRYPT_BIN = 19`, `EMBYTECRYPT_CFG = 17` |
| `Lib_Engine/Common/ByteCrypt.cpp` | 5,593 | `5081906dd78a1b19aa1fd7a1a8c17946f0e5c8e35ef82b19a2de521b55c92b0e` | `byte_encode` / `byte_decode` 256-byte bijective table inversion |
| `Lib_Engine/Common/ByteCryptDefVer1.h` | 32,493 | `fe42183b4a0715bf6a3cd147f951c487c0c1c3e2bc175d5e109fd8886e909120` | `ARRAY_BIN_VAR1[256]` and `ARRAY_CFG_VAR1[256]` |

---

## 4. Authentic Release Archive Provenance

All authentic asset measurements were performed read-only against `famoh01/ran-online-ep7` GitHub Release `ran-ep7-full-v1`:

| Release Artifact | SHA-256 |
|---|---|
| `RAN-EP7-Client-TH.zip.part00` | `a68df3ae793b7301ccf5cc91d28613c0dee0b72015a8e38d06195559f6612356` |
| `RAN-EP7-Client-TH.zip.part01` | `a0351ea3720f2065c847eaf226e3438f36637cee7c96c547e6c8fcd098a8f992` |
| Reassembled `RAN-EP7-Client-TH.zip` | `6866e84b46e7ea081988f94532c505dd1219c3540081c7ba00beaba40a64c4e3` |
| `data/animation/animation.rcc` (`6,734` `.cfg` entries, `0` `.bin` entries) | `116bb1a5e4743ebac07ed8c368b985b1f76a0720562a6bc9284ee6a82aa1aced` |
| `data/skinobject/skinobject.rcc` (`0` `.bin` entries) | `448dc74477c52baa223e4289aa0b7ed4dcb1f6c3fee6e87a10aedafe83aa6941` |

`inspect_ep7_animation_containers` recorded the pre-run SHA-256 of every sampled loose `.bin` file and re-computed its SHA-256 at exit (`sample_hashes_unchanged_after_run: true`).

---

## 5. Complete `SAnimContainer` Outer Header & Container Version Specification

Source: `Lib_Engine/Meshs/SAnimationSaveLoad.cpp:299-555` and `Lib_Engine/Common/SerialFile.cpp:48-68, 219-244`.

### 5.1 132-Byte `CSerialFile` Header (`[0..131]`, Always Unencrypted)

| Byte Offset | Size | C++ Type / Field | Value in Authentic `data/animation/*.bin` |
|---:|---:|---|---|
| `0` | `128` | `char szFileType[128]` | `"AnimContainer\0"` (`41 6e 69 6d 43 6f 6e 74 61 69 6e 65 72 00`) + `114` zero bytes (`0x00`) in all `6,228 / 6,228` files |
| `128` | `4` | `DWORD dwVersion` (`m_FileID`) | Little-endian `uint32_t`: `0x0100`, `0x0101`, `0x0102`, `0x0103`, `0x0104`, or `0x0200` |

Note on `SAnimContainer::LoadAnimFromFile` (`SAnimationSaveLoad.cpp:307-313`):
- `CSerialFile::OpenFile(FOT_READ, ...)` reads the 132-byte header in plaintext (`ReadFileType()`), setting `m_DefaultOffSet = 132`.
- Immediately after `GetFileType(szFileType, FILETYPESIZE, dwVersion)`, the engine checks:
  ```cpp
  if ( dwVersion >= VERSION_ENCODE ) // VERSION_ENCODE = 0x0200
      SFile.SetEncodeType( EMBYTECRYPT_BIN );
  ```
- Therefore bytes `[0..131]` are **never** encrypted even for `0x0200`; `EMBYTECRYPT_BIN` applies strictly to bytes `[132..EOF)`.

### 5.2 Outer Container Version Table (`[132..EOF)`)

Each track list is serialized as `DWORD dwCount` followed by `dwCount` consecutive `SAnimation` track records:

| Outer `dwVersion` | Files in `ran-ep7-full-v1` | Payload Encoding `[132..EOF)` | Exact Sequential Payload Layout (`SAnimationSaveLoad.cpp`) | Track `dwVER` Observed in Authentic Assets | Current `EP7AnimationReader` Status |
|---|---:|---|---|---|---|
| `0x0100` | **1,210** | Plaintext | `u32 numMain; SAnimation main[numMain];` (`upBody` is empty) (`Load_0100`, `:328-346`) | `0x0100` (`63,226` tracks across `1,209` files; `1` file has `numMain = 0`) | **Broken for non-empty files:** outer `0x0100` accepted, but `ReadBoneTrack` lacks `0x0100` -> `1` parsed (`0` tracks), **`1,209` parser-rejected** |
| `0x0101` | **907** | Plaintext | `u32 numMain; SAnimation main[numMain];`<br>`u32 numUpBody; SAnimation upBody[numUpBody];` (`Load_0101`, `:348-378`) | `0x0101` (`71,783` tracks) | Supported (`907` parsed; latent `SMatrixKey` bug not hit because `m_cMatrixKeys == 0` in all `907`) |
| `0x0102` | **74** | Plaintext | `u32 numUpBody; SAnimation upBody[numUpBody];`<br>`u32 numMain; SAnimation main[numMain];` (`Load_0102`, `:380-410`) | `0x0102` (`5,284` tracks) | Supported (`74` parsed; `m_cMatrixKeys == 0` in all `74`) |
| `0x0103` | **1,533** | Plaintext | `u32 numUpBody; SAnimation upBody[numUpBody];`<br>**`u32 dwGarbageValue;`**<br>`u32 numMain; SAnimation main[numMain];` (`Load_0103`, `:412-449`) | `0x0103` (`114,210` tracks) | Unsupported (`1,533` unsupported-version) |
| `0x0104` | **2,495** | Plaintext | `u32 numMain; SAnimation main[numMain];`<br>`u32 numUpBody1; SAnimation upBody1[numUpBody1];`<br>`u32 numUpBody2; SAnimation upBody2[numUpBody2];` (`Load_0104`, `:451-486`) | `0x0103` (`129,379` tracks) | Unsupported (`2,495` unsupported-version) |
| `0x0200` | **9** | **`EMBYTECRYPT_BIN`** (`ARRAY_BIN_VAR1[256]`) | After byte-table decode:<br>`u32 numUpBody; SAnimation upBody[numUpBody];`<br>`u32 numMain; SAnimation main[numMain];` (`Load_0200`, `:488-519`, `SaveToFile`, `:521-555`) | `0x0103` (`457` tracks) | Unsupported (`9` unsupported-version) |

### 5.3 Verified Structural Invariants on `0x0103`, `0x0104`, and `0x0200`

1. **`0x0103` Inter-List `dwGarbageValue` (`SAnimationSaveLoad.cpp:427`):**
   - `Load_0103` reads and discards a single `DWORD dwGarbageValue` between `m_listAniUPBODY` and `m_listAnimation`.
   - Across all **`1,533 / 1,533` (`100%`)** authentic `0x0103` `.bin` files in `ran-ep7-full-v1`, the writer populated that word using the exact formula `dwGarbageValue == 0x0103 + numUpBody + numMain` (e.g. `a_event_pierrot.bin`: `upBody = 14`, `main = 21`, `dwGarbageValue = 0x0126 = 0x0103 + 35`). Consistent with `Load_0103`, a reader must consume and ignore this `uint32_t` without enforcing the writer's formula.
2. **`0x0104` Three-List Semantics (`SAnimationSaveLoad.cpp:451-486`):**
   - `Load_0104` reads three sequential counted track lists:
     - List 0 (`dwSize` at `:459`) -> `m_listAnimation` (`mainTracks`), non-empty in **`2,491 / 2,495`** files.
     - List 1 (`dwSize` at `:467`) -> `m_listAniUPBODY` (`upBodyTracks`), non-empty in **`810 / 2,495`** files.
     - List 2 (`dwSize` at `:475`) -> **also** `m_listAniUPBODY.push_back(pAnimation)` (`upBodyTracks`), non-empty in **`628 / 2,495`** files (`4` files have `0, 0, 0`, e.g. `144`-byte `a_item_lovely_step_bone.bin`).
   - Because `628` production `0x0104` files store real upper-body tracks in List 2, both List 1 and List 2 must append into `clip.upBodyTracks`.
3. **`0x0200` `EMBYTECRYPT_BIN` Lookup Table (`ByteCryptDefVer1.h` `ARRAY_BIN_VAR1`):**
   - `BYTECRYPT::byte_encode` replaces each plaintext byte `p` with `ARRAY_BIN_VAR1[p]`; `BYTECRYPT::byte_decode` builds the inverse 256-byte bijection `DEC[ARRAY_BIN_VAR1[i]] = i` and maps each ciphertext byte `c` at `[132..EOF)` to `DEC[c]`.
   - Applying `DEC` to all `9 / 9` authentic `0x0200` `.bin` files (`a.bin`, `a2.bin`, `budots.bin`, etc.) yields standard `Load_0200` (`upBody`, `main`) streams with `SAnimation` `0x0103` tracks reaching exact EOF on all `9` files.

---

## 6. Complete `SAnimation` Per-Bone Track & Keyframe Specification

Source: `Lib_Engine/Meshs/SAnimationSaveLoad.cpp:15-297`, `Lib_Engine/Meshs/DxAniKeys.h:1-215`, `Lib_Engine/Common/SerialFile.cpp:433-443, 582-597`.

### 6.1 Keyframe Struct Binary Layouts (Unchanged Across `0x0100`–`0x0103`)

Time units across all versions use `UNITANIKEY_PERSEC = 4800` ticks/second (`DxAniKeys.h`).

| Engine Struct (`DxAniKeys.h`) | On-Disk Size | Exact Little-Endian Field Layout | Notes |
|---|---:|---|---|
| `SPositionKey` | **`16` B** | `u32 dwTime` (`[0..3]`), `f32 vPos[3]` (`[4..15]`) | Unchanged |
| `SRotateKey` | **`20` B** | `u32 dwTime` (`[0..3]`), `f32 quatRotate[4]` (`x, y, z, w` at `[4..19]`) | DirectX `D3DXQUATERNION` `(x,y,z,w)` |
| `SScaleKey` | **`16` B** | `u32 dwTime` (`[0..3]`), `f32 vScale[3]` (`[4..15]`) | Unchanged |
| `SQuatPosKey` | **`36` B** | `u32 m_dwTime` (`[0..3]`), `f32 m_vScale[3]` (`[4..15]`), `f32 m_vPos[3]` (`[16..27]`), `u32 m_vQuatComp.dwFirst` (`[28..31]`), `u32 m_vQuatComp.dwSecond` (`[32..35]`) | `QUATCOMP` codec (`32767.f` / `32768` / `65535.f` + sign bit `0x80000000` for `w`) is identical in both engine trees and `EP7AnimationReader::DecompressQuaternion` |
| `SMatrixKey` | **`80` B** | `u32 dwTime` (`[0..3]`), **`u8 _align16_pad[12]` (`[4..15]`)**, `f32 mat[16]` (`[16..79]`, row-major `D3DXMATRIXA16`) | **`D3DXMATRIXA16` has `align(16)` on Win32 MSVC**, placing `mat` at offset `16` and making `sizeof(SMatrixKey) == 80` |

### 6.2 `CSerialFile` String Encoding & Empty Bone Name Edge Case

- `CSerialFile::operator<<(const std::string& str)` (`SerialFile.cpp:438-440`) writes `DWORD(str.length() + 1)` followed by `str.length() + 1` bytes including the trailing `'\0'`.
- An empty `m_strBone` (`""`) is serialized on disk as `01 00 00 00 00` (`len = 1`, payload `"\0"`).
- **Authentic asset edge case:** **`22` authentic `0x0100` `.bin` files** contain at least one `SAnimation` track whose `m_strBone` is empty (`""`). `SAnimationSaveLoad.cpp` accepts empty `m_strBone` (`SFile >> m_strBone` without non-empty rejection, skipping bone binding only at runtime if `m_pSkeleton->FindBone` returns `NULL`). `EP7ByteFormat.h`'s `Stream::ReadString` already strips trailing `'\0'` and returns `true` on `""`, so `ReadBoneTrack` must **not** reject `boneName.empty()`.

### 6.3 Per-Bone `SAnimation` Track Version Table (`0x0100`–`0x0103`)

Every `SAnimation` record begins with `DWORD dwVersion`. Each key array is serialized as `DWORD count` followed (if `count > 0`) by `count * sizeof(KeyStruct)` raw bytes (`SFile.WriteBuffer` / `SFile.ReadBuffer`):

| Track `dwVersion` | Source Function (`SAnimationSaveLoad.cpp`) | Exact Field Order After `DWORD dwVersion` | Authentic Track Count in `ran-ep7-full-v1` |
|---|---|---|---:|
| **`0x0100`** | `SAnimation::Load_0100` (`:15-50`) | 1. `posKeys` (`u32 cPos` + `16 * cPos` B)<br>2. `rotKeys` (`u32 cRot` + `20 * cRot` B)<br>3. `scaleKeys` (`u32 cScale` + `16 * cScale` B)<br>4. `matrixKeys` (`u32 cMat` + **`80 * cMat` B**)<br>5. `std::string m_strBone` (`u32 len` + `len` B)<br>*(Engine calls `ConvertMatToQuat()` after load)* | **63,226** (in `1,209` `0x0100` files) |
| **`0x0101`** | `SAnimation::Load_0101` (`:52-95`) | 1. `posKeys` (`16` B/key)<br>2. `rotKeys` (`20` B/key)<br>3. `scaleKeys` (`16` B/key)<br>4. `matrixKeys` (**`80` B/key**)<br>5. `quatPosKeys` (`36` B/key)<br>6. `std::string m_strBone` | **71,783** (in `907` `0x0101` files) |
| **`0x0102`** | `SAnimation::Load_0102` (`:97-152`) | 1. `std::string m_strBone`<br>2. `rotKeys` (`20` B/key)<br>3. `posKeys` (`16` B/key)<br>4. **`u32 dwGarbage1`**<br>5. `quatPosKeys` (`36` B/key)<br>6. **`u32 dwGarbage2`**<br>7. `matrixKeys` (**`80` B/key**)<br>8. `scaleKeys` (`16` B/key) | **5,284** (in `74` `0x0102` files) |
| **`0x0103`** | `SAnimation::Load_0103` (`:154-209`) & `SaveToFile` (`:236-297`) | 1. `std::string m_strBone`<br>2. `rotKeys` (`20` B/key)<br>3. **`u32 dwGarbage1`** *(written as `m_strBone.length()`)*<br>4. `posKeys` (`16` B/key)<br>5. `quatPosKeys` (`36` B/key)<br>6. `matrixKeys` (**`80` B/key**)<br>7. **`u32 dwGarbage2`** *(written as `m_strBone.length() + 0x0103`)*<br>8. `scaleKeys` (`16` B/key) | **244,046** (`114,210` in `0x0103`, `129,379` in `0x0104`, `457` in `0x0200`) |

Look at the exact difference between track `0x0102` and track `0x0103`:
- No struct sizes, key semantics, or compression changed.
- Only the two `DWORD` obfuscation fields moved:
  - `dwGarbage1` moved from **after `posKeys`** (`0x0102`) to **before `posKeys`** (`0x0103`).
  - `dwGarbage2` moved from **before `matrixKeys`** (`0x0102`) to **after `matrixKeys`** (`0x0103`).

---

## 7. Root-Cause Dissection of the `1,209` `parser-rejected` Files

Across all `1,209` files in `data/animation/` that B11 classified as `recognized container parser rejection`:

- **100% (`1,209 / 1,209`) share the exact same root cause and failure site:**
  ```text
  1209 x 0x0100 | list=main | idx=0 | byte_offset=136 | trackVer=0x0100 | err=unsupported animation version
  ```
- **Why `1` `0x0100` file (`data/animation/a_mob_wdm_stay01.bin`, `136` B) passed in B11 while `1,209` failed:**
  - `a_mob_wdm_stay01.bin` (`sha256 = 2b1bb00cb510ff91ccde93e1072ab9c562e85512ee6aeeb362849c501f6047d5`) has bytes `[128..135] = 00 01 00 00 00 00 00 00` (`outerVersion = 0x0100`, `numMain = 0`).
  - `EP7AnimationReader::ParseContainer()` (`EP7Animation.cpp:330-335`) handles `version == 0x0100` by calling `ReadTrackList(stream, clip.mainTracks)`. When `count == 0`, the loop runs zero times and never calls `ReadBoneTrack()`.
  - All other `1,209` `0x0100` files have `numMain >= 1` and immediately invoke `ReadBoneTrack()` at offset `136`, where `version == 0x0100` hits the terminal failure (`EP7Animation.cpp:203-205`).
- **Historical reason `0x0100` uses `SMatrixKey` (`1,205 / 1,210` files):**
  - As shown in `SAnimation.cpp:1241-1265` (`SAnimation::ConvertMatToQuat`), `#define USE_ANI_QUATERNION` was introduced starting with `0x0101` to decompose `80`-byte `SMatrixKey` arrays into `36`-byte `SQuatPosKey` arrays (`m_cMatrixKeys = 0L; SAFE_DELETE_ARRAY(m_pMatrixKeys);`).
  - Pre-`0x0101` (`0x0100`) `.bin` files predated `SQuatPosKey` and serialized raw `SMatrixKey` arrays directly; `SAnimation::Load_0100` (`SAnimationSaveLoad.cpp:45`) calls `ConvertMatToQuat()` on load.

---

## 8. Defects & Hazards Discovered in Existing `RANMobile/Data/Importer/EP7Animation.{h,cpp}`

Per Phase 3J B12 scope rules, production `EP7Animation.{h,cpp}` was left **100% untouched** during this investigation. Auditing `EP7Animation.{h,cpp}` against `SAnimationSaveLoad.cpp` and authentic `.bin` bytes uncovered three distinct issues that any future implementation PR must address:

### 8.1 Critical Defect #1 — `SMatrixKey` 68-Byte Read vs 80-Byte On-Disk Stride (`EP7Animation.cpp:88-104`, `166-178`)

In `EP7Animation.cpp:88-104` (`0x0102`) and `166-178` (`0x0101`):

```cpp
// EP7Animation.h: kMatrixKeySize = 80 (u32 time + 12B align16 pad + 16 floats)

// EP7Animation.cpp:94-104 (and identically at 168-178):
if (!s.CanReadElements(count, EP7AnimationReader::kMatrixKeySize)) { ... }
out.matrixKeys.resize(count);
for (uint32_t i = 0; i < count; ++i) {
    if (!s.ReadU32(out.matrixKeys[i].time)) return false; // reads 4 bytes [0..3]
    // BUG: MISSING s.Skip(12) for Win32 D3DXMATRIXA16 16-byte alignment padding [4..15]!
    for (int k = 0; k < 16; ++k) {
        if (!s.ReadF32(out.matrixKeys[i].mat[k])) return false; // reads [4..67] instead of [16..79]!
    }
}
```

- **Binary proof on authentic `0x0100` sample `data/animation/a_gbox_stay.bin` (`sha256 = 81576bcf...e85d`, `10,918` B):**
  - Track 0 (`"Bip01"`, `122` matrix keys) starts its first `SMatrixKey` at byte offset `156`:
    - `[156..159]`: `00 00 00 00` -> `dwTime = 0`
    - `[160..171]`: `68 80 15 03 2d 32 33 54 31 33 3a 34` -> **12 bytes of uninitialized Win32 heap padding**
    - `[172..235]`: 16 IEEE-754 floats forming a valid orthonormal `4x4` rigid-body transform (`row0 = (-0.000, 0.506, -0.863, 0.0)`, `row1 = (-0.000, 0.863, 0.506, 0.0)`, `row2 = (1.000, 0.000, 0.000, 0.0)`, `row3 = (-0.300, 0.036, 93.758, 1.0)`).
  - Skipping `[160..171]` (`stride = 80`) lands cleanly on key 1 (`dwTime = 160` at offset `236`) and reaches exact EOF at byte `10,918`. Reading `68` bytes without `s.Skip(12)` ingests heap garbage into `mat[0..2]` and desynchronizes the stream by `12 * 122 = 1,464` bytes on Track 0 alone.
- **Why existing synthetic unit test `TestAnimation()` (`RANMobile/Tests/test_phase3j.cpp:406-434, 1033-1047`) missed this bug:**
  - `AnimationBin()` (`test_phase3j.cpp:429`) explicitly wrote an **80-byte** `SMatrixKey` (`time`, `12` zero padding bytes, and `16` identity matrix floats ending in `0.0f, 0.0f, 1.0f`).
  - `ParseContainer()` read `68` bytes (leaving the last `12` bytes `00 00 00 00 | 00 00 00 00 | 00 00 80 3f` unconsumed), then read the next `u32` (`0x00000000`) as `scaleCount = 0`, and the next `u32` (`0x00000000`) as `mainTrackCount = 0`, exiting with `12` unread bytes (`00 00 80 3f` + real `scaleCount=0` + real `mainCount=0`) and `clip.upBodyTracks[0].matrixKeys[0].mat[0] == 0.0f` instead of `1.0f` — which `TestAnimation()` never asserted!

### 8.2 Defect #2 — Outer `0x0100` Accepted Without `SAnimation::Load_0100` in `ReadBoneTrack`

- `EP7Animation.cpp:330` accepts `version == 0x0100`, while `ReadBoneTrack` (`EP7Animation.cpp:18-205`) rejects `version == 0x0100`, causing `0`-track `0x0100` files to pass and all `1,209` non-empty `0x0100` files to fail.

### 8.3 Defect #3 — Unchecked `fileType` and Missing `unsupported` Flag on `EP7AnimClip`

- Unlike `EP7AnimationReader::ParseInfo()` (`EP7Animation.cpp:432-449`), which checks `IsSanimconinfoType(info.fileType)` and sets `info.unsupported = true` on unimplemented versions, `ParseContainer()` (`EP7Animation.cpp:268-348`) ignores `fileType` and has no `bool unsupported = false` field on `EP7AnimClip`.

---

## 9. Authentic Byte-Level Hex Dissections Across All 6 Outer Versions

All byte offsets below are 0-based file offsets from authentic `ran-ep7-full-v1` samples (all start with `[0..15] = 416e696d436f6e7461696e6572000000` `"AnimContainer\0"` and zeros through `[127]`):

### 9.1 `0x0100` Zero-Track Container (`parsed`) — `data/animation/a_mob_wdm_stay01.bin`
- **Size:** `136` B | **SHA-256:** `2b1bb00cb510ff91ccde93e1072ab9c562e85512ee6aeeb362849c501f6047d5`
- `[128..131] = 00 01 00 00` (`outerVersion = 0x0100`)
- `[132..135] = 00 00 00 00` (`numMain = 0`) -> **EOF at `136`**.

### 9.2 `0x0100` Matrix-Keyed Container (`parser-rejected`) — `data/animation/a_gum_mix.bin`
- **Size:** `9,444` B | **SHA-256:** `373dd54902deec3aed3a22ce2c6d449468e35da5d8319fe4a944218b347fad7d`
- `[128..131] = 00 01 00 00` (`outerVersion = 0x0100`)
- `[132..135] = 0f 00 00 00` (`numMain = 15`)
- `[136..139] = 00 01 00 00` (`track[0].dwVersion = 0x0100` — **exact rejection offset of `EP7AnimationReader`**)
- `[140..151] = 00 00 00 00 | 00 00 00 00 | 00 00 00 00` (`cPos = 0`, `cRot = 0`, `cScale = 0`)
- `[152..155] = 08 00 00 00` (`cMatrix = 8` -> `8 * 80 = 640` bytes `[156..795]`, each key `[time(4B) + pad(12B) + mat4x4(64B)]`)
- `[796..811]`: `std::string m_strBone` (`"Bip01 Spine1\0"`), followed by tracks `1..14` -> **exact EOF at `9,444`**.

### 9.3 `0x0101` Container (`parsed`) — `data/animation/101_5f_enemy_boss_attack.bin`
- **Size:** `78,491` B | **SHA-256:** `380df2d3b29e9e91beab1948b6a0f2acf477842ed04f7ad65a0a709de41e254e`
- `[128..131] = 01 01 00 00` (`outerVersion = 0x0101`)
- `[132..135] = 50 00 00 00` (`numMain = 80`)
- `[136..139] = 01 01 00 00` (`track[0].dwVersion = 0x0101`)
- `[140..159]`: `cPos=0`, `cRot=0`, `cScale=0`, `cMatrix=0`, `cQuatPos = 0x1A (26)` (`26 * 36 = 936` B), then `m_strBone`, ..., `numUpBody` -> **exact EOF at `78,491`**.

### 9.4 `0x0102` Container (`parsed`) — `data/animation/a_trap_atk.bin`
- **Size:** `1,011` B | **SHA-256:** `558c5b884ab511f85b1d915460c67b54bd47efe5c1a24f829ff9f1b2bea0d7a9`
- `[128..131] = 02 01 00 00` (`outerVersion = 0x0102`)
- `[132..135] = 00 00 00 00` (`numUpBody = 0`), `[136..139] = 01 00 00 00` (`numMain = 1`)
- `[140..154] = 02 01 00 00 | 07 00 00 00 | 42 6f 6e 65 30 31 00` (`trackVer = 0x0102`, `m_strBone = "Bone01\0"`)
- `[155..170] = 00 00 00 00 (cRot=0) | 00 00 00 00 (cPos=0) | 06 00 00 00 (g1=6) | 17 00 00 00 (cQuatPos=23)` -> **exact EOF at `1,011`**.

### 9.5 `0x0103` Container (`unsupported-version`) — `data/animation/a_as_tiger_weapon_01_stay.bin` & `a_event_pierrot.bin`
- **`a_as_tiger_weapon_01_stay.bin`** (`5,996` B, `sha256 = efbf1ab4e51e946a803aed6d438692a70bff12f1f7d5083e16a8a2d61d1fa1e0`):
  - `[128..131] = 03 01 00 00` (`outerVersion = 0x0103`)
  - `[132..135] = 00 00 00 00` (`numUpBody = 0`)
  - `[136..139] = 07 01 00 00` (**inter-list `dwGarbageValue = 0x0107 == 0x0103 + 0 + 4`**)
  - `[140..143] = 04 00 00 00` (`numMain = 4`)
  - `[144..183]`: `trackVer = 0x0103`, `m_strBone = "tiger_bow_01_rota02\0"` (`len = 20`), `cRot = 0`, `dwGarbage1 = 0x13 (19)`, `cPos = 0`, `cQuatPos = 0x3E (62)` -> **exact EOF at `5,996`**.
- **`a_event_pierrot.bin`** (`76,261` B, `sha256 = 206ee569db0d48b460f8e53243966a2c2929d04e09a93039ccbd08eb846d26cf`):
  - `numUpBody = 14` (`[132..135] = 0e 00 00 00`), followed by 14 `0x0103` upper-body tracks (`"Bip01_Spine1\0"`, ...), then `dwGarbageValue = 0x0126` (`0x0103 + 14 + 21`), then `numMain = 21` -> **exact EOF at `76,261`**.

### 9.6 `0x0104` Container (`unsupported-version`) — `data/animation/a_w_mc_ks_weapon_01_stay.bin`
- **Size:** `260` B | **SHA-256:** `327f9d1ac8cfd65f5101c6c68eba875b45310356e8463b802275ec7eed615848`
- `[128..131] = 04 01 00 00` (`outerVersion = 0x0104`)
- `[132..135] = 01 00 00 00` (`numMain = 1`)
- `[136..251]`: 1 `SAnimation` `0x0103` track (`"Bone043\0"`, `cRot=0`, `g1=7`, `cPos=0`, `cQuatPos=2` (`72` B), `cMat=0`, `g2=0x010A`, `cScale=0` = `116` B)
- `[252..255] = 00 00 00 00` (`numUpBody1 = 0`), `[256..259] = 00 00 00 00` (`numUpBody2 = 0`) -> **exact EOF at `260`**.

### 9.7 `0x0200` Encrypted Container (`unsupported-version`) — `data/animation/budots.bin`
- **Size:** `171,912` B | **SHA-256:** `04c1a05221a719a41237a02b08eddcc6cbf1c0c185724d0a19b2ba7bcbef98d1`
- `[128..131] = 00 02 00 00` (`outerVersion = 0x0200 >= VERSION_ENCODE`)
- Raw ciphertext `[132..160]`: `45 00 00 00 e2 f0 00 00 88 00 00 00 df bd 09 6b eb 22 3a 09 bd 46 94 eb 00`
- Decoded via `EMBYTECRYPT_BIN` (`ARRAY_BIN_VAR1` inverse):
  `22 00 00 00 | 03 01 00 00 | 0d 00 00 00 | 42 69 70 30 31 5f 53 70 69 6e 65 31 00`
  (`numUpBody = 34`, `track[0].dwVersion = 0x0103`, `m_strBone = "Bip01_Spine1\0"`, ..., `numMain = 21`) -> **exact EOF at `171,912`**.

---

## 10. Full `6,228`-File Read-Only Shadow Verification Results

Executed in GitHub Actions run [`45982141864`](https://github.com/famoh01/ran-online-ep7/actions/runs/45982141864) (`b12-authentic-anim-survey`, check run `140449383184`; `b12_anim_survey.json` SHA-256 `34df29de8d3a33132c931f2f8a138d83342bfa4314a41555a41a119cb634e38a`):

| Outer Version | Authentic Files | Shadow Spec `EXACT_EOF` (`trailing_bytes == 0`) | Total Bone Tracks | Track Version | Files w/ `m_cMatrixKeys > 0` | Files w/ `m_cQuatPosKeys > 0` | Empty/0-Track Files |
|---|---:|---:|---:|---|---:|---:|---:|
| `0x0100` | `1,210` | **`1,210 / 1,210` (`100%`)** | `63,226` | `0x0100` | **`1,205`** | `0` | `1` (`0` tracks) + `4` (`pos`/`rot`/`scale` only) |
| `0x0101` | `907` | **`907 / 907` (`100%`)** | `71,783` | `0x0101` | `0` | `905` | `2` |
| `0x0102` | `74` | **`74 / 74` (`100%`)** | `5,284` | `0x0102` | `0` | `74` | `0` |
| `0x0103` | `1,533` | **`1,533 / 1,533` (`100%`)** | `114,210` | `0x0103` | `0` | `1,532` | `1` |
| `0x0104` | `2,495` | **`2,495 / 2,495` (`100%`)** | `129,379` | `0x0103` | `0` | `2,492` | `3` |
| `0x0200` | `9` | **`9 / 9` (`100%`)** | `457` | `0x0103` | `0` | `9` | `0` |
| **TOTAL** | **`6,228`** | **`6,228 / 6,228` (`100.00%`)** | **`384,339`** | `0x0100`–`0x0103` | **`1,205`** | **`5,012`** | **`7` zero-key/empty** |

---

## 11. Companion `.cfg` (`SANIMCONINFO`) Cross-Version Dependency Matrix

`DxSkinAniMan::LoadAnimContainer` (`DxSkinAniMan.cpp:562-598`) pairs each `data/animation/<stem>.bin` with `animation.rcc#<stem>.cfg` (`SANIMCONINFO`, carrying `m_szSkeletion`, `m_dwSTime`, `m_dwETime`, `m_UNITTIME`, `m_dwFlag` / `ACF_LOOP`, strike frames, sound data, animation effects, and bone scales). Currently `EP7AnimationReader::ParseInfo()` implements `.cfg` versions **`0x0101`–`0x0107`**.

Cross-tabulating all `6,228` `.bin` files against the `6,734` `.cfg` entries in `animation.rcc`:

| `.bin` Outer Version | Total `.bin` | Paired w/ Currently Supported `.cfg` (`0x0103`–`0x0107`) | Paired w/ Newer `.cfg` (`0x0108`–`0x0115`) | No Matching `.cfg` Stem | Exact `.cfg` Version Breakdown |
|---|---:|---:|---:|---:|---|
| `0x0100` | `1,210` | **`1,208` (`99.8%`)** | `1` | `1` | `0x0103`: 5, `0x0104`: 231, `0x0105`: 145, `0x0106`: 824, `0x0107`: 3, `0x0110`: 1, `no-cfg`: 1 |
| `0x0101` | `907` | **`895` (`98.7%`)** | `12` | `0` | `0x0104`: 30, `0x0105`: 2, `0x0106`: 25, `0x0107`: 838, `0x0108`: 3, `0x0110`: 9 |
| `0x0102` | `74` | **`74` (`100.0%`)** | `0` | `0` | `0x0106`: 10, `0x0107`: 64 |
| `0x0103` | `1,533` | `161` (`10.5%`) | **`1,349` (`88.0%`)** | `23` | `0x0107`: 161, `0x0108`: 164, `0x0109`: 17, `0x0110`: 1,056, `0x0111`: 110, `0x0112`: 1, `0x0114`: 1, `no-cfg`: 23 |
| `0x0104` | `2,495` | `21` (`0.8%`) | **`2,464` (`98.8%`)** | `10` | `0x0104`: 1, `0x0105`: 2, `0x0106`: 15, `0x0107`: 3, `0x0110`: 12, `0x0111`: 714, `0x0112`: 177, `0x0114`: 996, `0x0115`: 565, `no-cfg`: 10 |
| `0x0200` | `9` | **`8` (`88.9%`)** | `0` | `1` | `0x0107`: 8, `no-cfg`: 1 |

**Architectural implication:**
- Fixing `SMatrixKey` + adding `SAnimation::Load_0100` immediately yields **`1,208` complete `.bin` + `.cfg` pairs** because `99.8%` of `0x0100` `.bin` files already pair with supported `.cfg` `0x0103`–`0x0107`.
- By contrast, `88.0%` of `0x0103` `.bin` files and `98.8%` of `0x0104` `.bin` files pair with `.cfg` `0x0108`–`0x0115` (whose full reader specifications are already retrieved and verified in `Lib_Engine/Meshs/SAnimationInfoSaveLoad.cpp`, `Load_0108`–`Load_0115` + `Load_0200` `EMBYTECRYPT_CFG`). Thus `.bin` `0x0103`/`0x0104` support and `.cfg` `0x0108`–`0x0115` support are natural companion stages for end-to-end runtime playback.

---

## 12. Read-Only Forensic Tooling & Reproducibility Commands

Added in this B12 investigation (without touching any production importer or B11 verifier file):
- `RANMobile/Tools/inspect_ep7_animation_containers.cpp` — standalone read-only C++17 forensic inspector with built-in RFC 6234 SHA-256 self-test, unmodified `EP7AnimationReader` bucket & rejection-offset tracer, full `SAnimationSaveLoad.cpp` read-only shadow verifier (`0x0100`–`0x0104` + `0x0200`/`EMBYTECRYPT_BIN`), `.cfg` stem cross-tabulator, strict ASCII/UTF-8 JSON output, and post-run SHA-256 immutability verification.
- `RANMobile/Tests/test_ep7_animation_inspector.cpp` + `RANMobile/Tests/run_ep7_animation_inspector_tests.sh` — `5,574` unit checks covering RFC 6234 SHA-256 vectors, strict JSON escaping, `0x0100`–`0x0200` container/track layouts, `SMatrixKey` 80B-vs-68B discrepancy detection, and directory survey immutability under both `-O2` and `-fsanitize=address,undefined` (`detect_leaks=1`).
- `.github/workflows/phase3j_b12_animation_investigation.yml` — reproducible read-only CI workflow verifying all 19 `Lib_Engine` LFS sources and surveying `ran-ep7-full-v1`.

```bash
# 1. Run B12 inspector unit & sanitizer tests (5,574 checks, normal + ASan+UBSan+LSan)
bash RANMobile/Tests/run_ep7_animation_inspector_tests.sh

# 2. Run B11 verifier regression tests (590 checks, normal + ASan+UBSan+LSan, untouched)
bash RANMobile/Tests/run_ep7_verifier_tests.sh

# 3. Build & run read-only forensic survey against an extracted EP7 client root
g++ -std=c++17 -Wall -Wextra -Werror -O2 \
  RANMobile/Data/Importer/EP7Animation.cpp \
  RANMobile/Data/Importer/EP7RccArchive.cpp \
  RANMobile/Data/Importer/EP7Skeleton.cpp \
  RANMobile/Data/Importer/EP7XFile.cpp \
  RANMobile/Tools/inspect_ep7_animation_containers.cpp \
  -o inspect_ep7_animation_containers
./inspect_ep7_animation_containers external/ep7_release/client_original \
  --json b12_anim_survey.json --samples-tsv b12_samples.tsv
```

---

## 13. Staged Future Implementation Proposal & Risk Analysis

> **Scope Boundary:** No parser or runtime changes were made in B12. Each stage below is designed as a small, independently testable, source-backed follow-up PR with explicit regression gates and expected authentic-asset verifier deltas.

### Stage 1 (Highest Priority / Bugfix + `1,209` FAIL Elimination) — Fix `SMatrixKey` 80-Byte Alignment Padding + Implement `SAnimation::Load_0100` + Enforce Header/Unsupported Contract
- **Scope (`EP7Animation.{h,cpp}` + `test_phase3j.cpp`):**
  1. **Fix `SMatrixKey` read loop** in `ReadBoneTrack` (`EP7Animation.cpp:94-104` and `168-178`): call `s.Skip(12)` after `s.ReadU32(out.matrixKeys[i].time)` before reading the `16` floats (`4 + 12 + 64 = 80 == kMatrixKeySize`), and add explicit `matrixKeys[0].mat[0] == 1.0f` / `mat[15] == 1.0f` + exact-EOF assertions to `TestAnimation()` (`test_phase3j.cpp:1033-1047`).
  2. **Implement `SAnimation::Load_0100`** (`version == 0x0100` in `ReadBoneTrack`, per `SAnimationSaveLoad.cpp:15-50`): `posKeys` (`16`B), `rotKeys` (`20`B), `scaleKeys` (`16`B), `matrixKeys` (`80`B with `s.Skip(12)`), `ReadString(out.boneName, kMaxNameLen)` (allowing `boneName.empty()` for the `22` authentic empty-bone files). `EP7AnimationPlayer::ApplyToSkeleton()` (`EP7Animation.cpp:669-680`) already samples `track.matrixKeys` directly (`SAnimation::SetMatrix`), so `0x0100` matrix-keyed tracks require no player-side plumbing changes unless optional `ConvertMatToQuat` memory compression is desired later.
  3. **Header & `unsupported` contract parity with `EP7AnimInfo`:** check `hdr.fileType == "AnimContainer"` inside `ParseContainer()` and add `bool unsupported = false` to `EP7AnimClip` for outer versions outside the implemented set.
- **Projected `verify_ep7_character_assets` loose `.bin` impact:**
  - `loose .bin parsed`: `982` -> **`2,191` (`+1,209`)**
  - `loose .bin parser-rejected (FAIL)`: `1,209` -> **`0` (`-1,209`)**
  - `loose .bin unsupported`: `4,037` (unchanged)
  - Unlocks `1,208` complete `.bin` + `.cfg` clips immediately.
- **Risk:** Low, provided `SMatrixKey` `s.Skip(12)` and `SampleBone` `SMatrixKey` evaluation / `ConvertMatToQuat` decomposition are tested together and `boneName.empty()` is permitted.

### Stage 2 (Unencrypted EP7 Expansion — `+4,028` `.bin` Files) — Implement `SAnimation::Load_0103` and `SAnimContainer::Load_0103` / `Load_0104`
- **Scope (`EP7Animation.{h,cpp}` + `verify_ep7_character_assets.cpp` version constant):**
  1. Add `version == 0x0103` to `ReadBoneTrack` (`SAnimationSaveLoad.cpp:154-209`: `boneName`, `rotKeys`, `u32 garbage1`, `posKeys`, `quatPosKeys`, `matrixKeys` (`80`B), `u32 garbage2`, `scaleKeys`).
  2. Add outer `hdr.version == 0x0103` (`upBodyTracks`, `u32 dwGarbageValue`, `mainTracks`) and `hdr.version == 0x0104` (`mainTracks`, `upBodyTracks` list 1, `upBodyTracks` list 2 appended to `upBodyTracks`).
- **Projected `verify_ep7_character_assets` loose `.bin` impact:**
  - `loose .bin parsed`: `2,191` -> **`6,219` (`+4,028`)**
  - `loose .bin unsupported`: `4,037` -> **`9` (`-4,028`, leaving only `0x0200`)**
  - `loose .bin failed`: **`0`**

### Stage 3 (`ByteCrypt` `0x0200` Container Support — Final `+9` `.bin` Files -> `6,228 / 6,228`)
- **Scope (`EP7ByteFormat.h` + `EP7Animation.cpp`):**
  1. Add a shared, table-driven `ByteCryptDecode(uint8_t*, size_t, ByteCryptKind)` helper in `EP7ByteFormat.h` (reusable across `EMBYTECRYPT_BIN`, `EMBYTECRYPT_CFG`, `EMBYTECRYPT_PIECE`, `EMBYTECRYPT_CONTAINER`, etc., from verified `ByteCryptDefVer1.h`).
  2. When `hdr.version == 0x0200` in `ParseContainer`, decode a copy of `[132..EOF)` with `EMBYTECRYPT_BIN` and parse `upBodyTracks` + `mainTracks` (`Load_0200`).
- **Projected `verify_ep7_character_assets` loose `.bin` impact:**
  - `loose .bin parsed`: `6,219` -> **`6,228 / 6,228` (`100.00%`)**, `0` unsupported, `0` failed.

### Companion Stage 4 (Recommended Alongside Stages 2–3) — `SANIMCONINFO` `.cfg` `0x0108`–`0x0115` (+ `0x0200` `EMBYTECRYPT_CFG`)
- Because `3,813` of the `4,028` `0x0103`/`0x0104` `.bin` files pair with `.cfg` versions `0x0108`–`0x0115` in `animation.rcc`, implementing `SANIMCONINFO::Load_0108`–`Load_0115` from the already-verified `Lib_Engine/Meshs/SAnimationInfoSaveLoad.cpp` (`sha256 = 279a5070...62e2`) will simultaneously eliminate the `.cfg` `UNSUPPORTED` entries inside `animation.rcc` and supply full `startTime`/`endTime`/`unitTime`/`ACF_LOOP` metadata for all `6,228` animations.

---

## 14. Phase 3J B13 Implementation & Authentic Validation Update (`0x0100` Track + 80-Byte `SMatrixKey` Fix)

Implemented in commit `1e11d2ec7f77bf938cdefa0bda6668a5f2a85b48` on branch `arena/00fd13f9-ran-online-ep7` (Draft PR #7):

### 14.1 Production Parser Changes (`RANMobile/Data/Importer/EP7Animation.{h,cpp}`)
1. **Fixed `SMatrixKey` 80-byte on-disk stride across `0x0100`, `0x0101`, and `0x0102`:**
   - Added `EP7AnimationReader::kMatrixKeyAlignPadSize = 12` with `static_assert(sizeof(uint32_t) + kMatrixKeyAlignPadSize + 16 * sizeof(float) == kMatrixKeySize)` (`80` bytes total, matching `enginelib/Meshs/DxAniKeys.h:70-88` + `LegacyDX_Include/d3dx9math.h:411,416` `D3DXMATRIXA16` `__declspec(align(16))`).
   - Consolidated key-channel readers (`ReadPositionKeys`, `ReadRotateKeys`, `ReadScaleKeys`, `ReadQuatPosKeys`, `ReadMatrixKeys`) with `kMaxKeysPerTrack = 200000` and `CanReadElements` overflow/bounds guards. `ReadMatrixKeys` reads `u32 time` (`[0..3]`), skips `12` bytes of Win32 `align(16)` struct padding (`[4..15]`) via `s.Skip(12)` without reading uninitialized padding bytes into `mat`, and reads `16` little-endian `float`s (`[16..79]`).
2. **Implemented authentic `SAnimation::Load_0100` (`version == 0x0100` in `ReadBoneTrack`):**
   - Exact source field order (`enginelib/Meshs/SAnimation.cpp:1419-1462`, `Lib_Engine/Meshs/SAnimationSaveLoad.cpp:15-50`): `positionKeys` (`16` B/key), `rotateKeys` (`20` B/key), `scaleKeys` (`16` B/key), `matrixKeys` (`80` B/key), `m_strBone` (`s.ReadString(out.boneName, kMaxNameLen)`), with `quatPosKeys` empty and empty bone names (`""`) preserved cleanly (`BindToSkeleton` resolves `""` to `boneIndex == -1`, matching `DxSkeleton::FindBone("") == NULL`).
   - Outer container versions `0x0103`, `0x0104`, and `0x0200` and inner track version `0x0103` remain intentionally unimplemented in B13 and explicitly rejected.
3. **Synchronized B12 inspector trace (`RANMobile/Tools/inspect_ep7_animation_containers.cpp`):**
   - Updated the reader-cursor trace to match B13 `EP7AnimationReader` (`0x0100`, `0x0101`, `0x0102` tracks + `80`-byte `SMatrixKey`) so `acceptedTrailingBytes` directly audits exact-EOF consumption of the production parser across all parsed authentic files.

### 14.2 Authentic `ran-ep7-full-v1` Validation Results (CI Runs `45990886364` & `45990886385`)
Verified against all `6,228` authentic `data/animation/*.bin` files in `ran-ep7-full-v1` via both `verify_ep7_character_assets` (run [`45990886364`](https://github.com/famoh01/ran-online-ep7/actions/runs/45990886364), job `140478133913`) and `inspect_ep7_animation_containers` (run [`45990886385`](https://github.com/famoh01/ran-online-ep7/actions/runs/45990886385), check run `140478981523`, report blob `a7710f879d895aa37c221ef49e217de851852464`):

| Metric (`data/animation/*.bin`, `total = 6,228`) | Pre-B13 (B11/B12 Baseline) | Post-B13 (Actual Verified) | Delta |
|---|---:|---:|---:|
| **`0x0100` parsed (`63,226` tracks, `1,205` w/ matrix keys, `22` w/ empty bone)** | `1` | **`1,210 / 1,210` (`100.00%`)** | **`+1,209`** |
| **`0x0101` parsed (`71,783` tracks)** | `907` | **`907 / 907` (`100.00%`)** | `0` |
| **`0x0102` parsed (`5,284` tracks)** | `74` | **`74 / 74` (`100.00%`)** | `0` |
| **Total loose `.bin` parsed** | `982` | **`2,191`** | **`+1,209`** |
| **Parsed files reaching `EXACT_EOF` (`trailing_bytes == 0`)** | `982 / 982` | **`2,191 / 2,191` (`100.00%`)** | **`+1,209`** |
| **Loose `.bin` `parser-rejected` (FAIL)** | `1,209` | **`0`** | **`-1,209`** |
| **Loose `.bin` `unsupported-version` (`0x0103`: `1,533`, `0x0104`: `2,495`, `0x0200`: `9`)** | `4,037` | **`4,037`** | `0` |
| **Loose `.bin` `invalid-header` / `unrecognized-type`** | `0` / `0` | **`0` / `0`** | `0` |
| **Pack entries (`SkinObject.rcc` + `Animation.rcc`)** | `6,479` ok / `3,550` fail / `1,631` unsup | **`6,479` ok / `3,550` fail / `1,631` unsup** | `0` |

### 14.3 Regression Suites & Android ARM64 Build Verification
- **Phase 3J character/animation suite (`test_phase3j.cpp`):** **`952` checks, `0` failures** (up from `398`) under both `-O2 -Wall -Wextra -Werror` and `-fsanitize=address,undefined` (`detect_leaks=1`).
- **B11 verifier regression suite (`run_ep7_verifier_tests.sh`):** **`1,571` checks, `0` failures** (up from `590`) under normal and `ASan + UBSan + LSan`.
- **B12/B13 inspector regression suite (`run_ep7_animation_inspector_tests.sh`):** **`7,035` checks, `0` failures** (up from `5,574`) under normal and `ASan + UBSan + LSan`.
- **CI `Core C++17 tests (blocking)` (run `45990886364`, job `140478134000`):** **success** (including data-dependent `ranmobile_tests`, `ranmobile_phase3j_tests`, and verifier sanitizer suite).
- **CI `Android ARM64 build (lib + APK/AAB)` (run `45990886364`, job `140478133579`):** **success** — `libranmobile-arm64-v8a` (`2,302,838` B, artifact `15193164290`) and `ranmobile-android-packages` (`assembleDebug` + `assembleRelease` + `bundleRelease`, `1,988,407` B, artifact `15194155403`).

---

## 15. Phase 3J B14 Implementation & Authentic Validation Update (`SAnimContainer` `0x0103` + `SAnimation` `0x0103`)

Implemented in commit `63c8c805ff66c3c89a3e8143757efe11c4c29a9a` on branch `arena/00fd13f9-ran-online-ep7` (Draft PR #7):

### 15.1 Production Parser Changes (`RANMobile/Data/Importer/EP7Animation.cpp`)
1. **Implemented authentic outer container `SAnimContainer::Load_0103` (`hdr.version == 0x0103` in `ParseContainer`):**
   - Exact source field order (`Lib_Engine/Meshs/SAnimationSaveLoad.cpp:412-442`, `SaveToFile :644-668`):
     - `128`-byte `"AnimContainer"` header + `u32(0x0103)`
     - `upBody` track list (`u32 numUpBody` with `kMaxTracks = 4096` guard, followed by `numUpBody` `SAnimation` tracks)
     - `uint32_t garbageValue = 0; s.ReadU32(garbageValue);` (in authentic EP7 writer output `dwGarbageValue = VERSION + m_listAniUPBODY.size() + m_listAnimation.size()`, verified `1,533 / 1,533` across `ran-ep7-full-v1`; original `Load_0103` consumes `SFile >> dwGarbageValue;` without rejecting non-formula values)
     - `main` track list (`u32 numMain` with `kMaxTracks = 4096` guard, followed by `numMain` `SAnimation` tracks)
2. **Implemented authentic inner track `SAnimation::Load_0103` (`version == 0x0103` in `ReadBoneTrack`):**
   - Exact source field order (`Lib_Engine/Meshs/SAnimationSaveLoad.cpp:192-248`, `SaveToFile :250-293`):
     - `u32(0x0103)`
     - `m_strBone` (`s.ReadString(out.boneName, kMaxNameLen)`, preserving valid empty `""` bone names)
     - `rotateKeys` (`ReadRotateKeys`, `20` B/key)
     - `uint32_t garbage1 = 0; s.ReadU32(garbage1);` (writer emits `m_strBone.length()`; `Load_0103` consumes `DWORD dwGarbage; SFile >> dwGarbage;`)
     - `positionKeys` (`ReadPositionKeys`, `16` B/key)
     - `quatPosKeys` (`ReadQuatPosKeys`, `36` B/key with `QUATCOMP` `dwFirst`/`dwSecond`)
     - `matrixKeys` (`ReadMatrixKeys`, `80` B/key including `kMatrixKeyAlignPadSize = 12` `D3DXMATRIXA16` `align(16)` skip)
     - `uint32_t garbage2 = 0; s.ReadU32(garbage2);` (writer emits `m_strBone.length() + VERSION`; `Load_0103` consumes `SFile >> dwGarbage;`)
     - `scaleKeys` (`ReadScaleKeys`, `16` B/key)
   - Outer container versions `0x0104` and `0x0200` remain intentionally unimplemented in B14 and explicitly report `"unsupported animation container version 0x... (implemented: 0x0100, 0x0101, 0x0102, 0x0103)"`.
3. **Synchronized B12/B14 inspector trace (`RANMobile/Tools/inspect_ep7_animation_containers.cpp`):**
   - Updated the production-reader cursor trace to match B14 `EP7AnimationReader` (`0x0100`–`0x0103` outer containers + `0x0100`–`0x0103` inner tracks) so `acceptedTrailingBytes` directly audits exact-EOF consumption across all `3,724` parsed authentic files.

### 15.2 Authentic `ran-ep7-full-v1` Validation Results (CI Runs `45995539967` & `45995540399`)
Verified against all `6,228` authentic `data/animation/*.bin` files in `ran-ep7-full-v1` via both `verify_ep7_character_assets` (run [`45995539967`](https://github.com/famoh01/ran-online-ep7/actions/runs/45995539967), job `140493888068`) and `inspect_ep7_animation_containers` (run [`45995540399`](https://github.com/famoh01/ran-online-ep7/actions/runs/45995540399), check run `140495005201`, JSON report blob `0ef25dcc3a2e6de8abd8b9632f2813cfed724716` / SHA-256 `a5f0cd58130b4d36147ba1a97503fa58b5520a1e60353af39e9707c08769fbd5`):

| Metric (`data/animation/*.bin`, `total = 6,228`) | Pre-B14 (B13 Verified) | Post-B14 (Actual Verified) | Delta |
|---|---:|---:|---:|
| **`0x0100` parsed (`63,226` tracks)** | `1,210 / 1,210` | **`1,210 / 1,210` (`100.00%`)** | `0` |
| **`0x0101` parsed (`71,783` tracks)** | `907 / 907` | **`907 / 907` (`100.00%`)** | `0` |
| **`0x0102` parsed (`5,284` tracks)** | `74 / 74` | **`74 / 74` (`100.00%`)** | `0` |
| **`0x0103` parsed (`114,210` tracks, `1,532` w/ `quatPosKeys` + `1` zero-track)** | `0 / 1,533` | **`1,533 / 1,533` (`100.00%`)** | **`+1,533`** |
| **Total loose `.bin` parsed (`254,503` tracks)** | `2,191` | **`3,724`** | **`+1,533`** |
| **Parsed files reaching `EXACT_EOF` (`trailing_bytes == 0`)** | `2,191 / 2,191` | **`3,724 / 3,724` (`100.00%`)** | **`+1,533`** |
| **Loose `.bin` `parser-rejected` (FAIL)** | `0` | **`0`** | `0` |
| **Loose `.bin` `unsupported-version` (`0x0104`: `2,495`, `0x0200`: `9`)** | `4,037` | **`2,504`** | **`-1,533`** |
| **Loose `.bin` `invalid-header` / `unrecognized-type`** | `0` / `0` | **`0` / `0`** | `0` |
| **Pack entries (`SkinObject.rcc` + `Animation.rcc`)** | `6,479` ok / `3,550` fail / `1,631` unsup | **`6,479` ok / `3,550` fail / `1,631` unsup** | `0` |

### 15.3 Regression Suites & Android ARM64 Build Verification
- **Phase 3J character/animation suite (`test_phase3j.cpp`):** **`1,499` checks, `0` failures** (up from `952`, sweeping 8 ABI fixtures across truncations and 4x single-byte corruptions, including `AnimationBin0103()`) under both `-O2 -Wall -Wextra -Werror` and `-fsanitize=address,undefined` (`detect_leaks=1`).
- **B11/B14 verifier regression suite (`run_ep7_verifier_tests.sh`):** **`2,784` checks, `0` failures** (up from `1,571`) under normal and `ASan + UBSan + integrated LSan`.
- **B12/B14 inspector regression suite (`run_ep7_animation_inspector_tests.sh`):** **`8,303` checks, `0` failures** (up from `7,035`) under normal and `ASan + UBSan + integrated LSan`.
- **CI `Core C++17 tests (blocking)` (run `45995539967`, job `140493888315`):** **success** (including data-dependent `ranmobile_tests`, `ranmobile_phase3j_tests`, and verifier sanitizer suite).
- **CI `Android ARM64 build (lib + APK/AAB)` (run `45995539967`, job `140493888339`):** **success** — `libranmobile-arm64-v8a` (`2,303,142` B, artifact `15195193779`) and `ranmobile-android-packages` (`assembleDebug` + `assembleRelease` + `bundleRelease`, `1,989,193` B, artifact `15194554934`).

---

## 16. Phase 3J B15 Implementation & Authentic Validation Update (`SAnimContainer` `0x0104`)

Implemented in commit `263ef9cf7ae0e355b1363c0a615415ecaac291a6` on branch `arena/00fd13f9-ran-online-ep7` (Draft PR #7):

### 16.1 Production Parser Changes (`RANMobile/Data/Importer/EP7Animation.cpp`)
1. **Implemented authentic outer container `SAnimContainer::Load_0104` (`hdr.version == 0x0104` in `ParseContainer`):**
   - Verified directly against both `SAnimContainer::Load_0104(CSerialFile&)` and static `SAnimContainer::Load_0104(CSerialFile&, const DxSkeleton*, SANILIST&, SANILIST&)` in `Lib_Engine/Meshs/SAnimationSaveLoad.cpp:444-479, 670-699`:
     - `128`-byte `"AnimContainer"` header + `u32(0x0104)` (plaintext: `0x0104 < VERSION_ENCODE(0x0200)`)
     - List 0 (`mainTracks` / `m_listAnimation`): `u32 numMain` + `numMain` `SAnimation` tracks
     - List 1 (`upBodyTracks` / `m_listAniUPBODY`): `u32 numUpBody1` + `numUpBody1` `SAnimation` tracks
     - List 2 (appended in order to `upBodyTracks` / `m_listAniUPBODY`): `u32 numUpBody2` + `numUpBody2` `SAnimation` tracks (no inter-list garbage `DWORD`)
   - Extended `ReadTrackList` with `bool append = false` and combined overflow-safe track limit enforcement (`count > kMaxBoneTracks || count > kMaxBoneTracks - out.size()`), reusing the verified `ReadBoneTrack` implementation (`0x0100`–`0x0103`).
   - Preserved `0x0100`, `0x0101`, `0x0102`, and `0x0103` behavior unchanged, and preserved explicit rejection of unimplemented `0x0200` (`EMBYTECRYPT_BIN`) with `"unsupported animation container version 0x00000200 (implemented: 0x0100, 0x0101, 0x0102, 0x0103, 0x0104)"`.
2. **Synchronized B12/B15 inspector trace (`RANMobile/Tools/inspect_ep7_animation_containers.cpp`):**
   - Updated `TraceUnmodifiedReaderContainer` and `outerSupported` to include `0x0104` (`main`, `upBody1`, and accumulated `upBody2`) so `acceptedTrailingBytes` directly verifies exact-EOF consumption across all `6,219` plaintext authentic `.bin` files.

### 16.2 Authentic `ran-ep7-full-v1` Validation Results (CI Runs `45999738432` & `45999738400`)
Verified against all `6,228` authentic `data/animation/*.bin` files in `ran-ep7-full-v1` via both `verify_ep7_character_assets` (run [`45999738432`](https://github.com/famoh01/ran-online-ep7/actions/runs/45999738432), job `140508021418`) and `inspect_ep7_animation_containers` (run [`45999738400`](https://github.com/famoh01/ran-online-ep7/actions/runs/45999738400), check run `140508852646`, JSON report blob `bd9250ca49297e09aa8322f2e5d44768ac47869a` / SHA-256 `4e5147d31621177c15c08dafd3ab97e1a2098da2d0f67d33ced42e2cb4fc98cb`):

| Metric (`data/animation/*.bin`, `total = 6,228`) | Pre-B15 (B14 Verified) | Post-B15 (Actual Verified) | Delta |
|---|---:|---:|---:|
| **`0x0100` parsed (`63,226` tracks)** | `1,210 / 1,210` | **`1,210 / 1,210` (`100.00%`)** | `0` |
| **`0x0101` parsed (`71,783` tracks)** | `907 / 907` | **`907 / 907` (`100.00%`)** | `0` |
| **`0x0102` parsed (`5,284` tracks)** | `74 / 74` | **`74 / 74` (`100.00%`)** | `0` |
| **`0x0103` parsed (`114,210` tracks)** | `1,533 / 1,533` | **`1,533 / 1,533` (`100.00%`)** | `0` |
| **`0x0104` parsed (`129,379` tracks: `2,491` non-empty `main`, `810` `upBody1`, `628` `upBody2`; `3` zero-track)** | `0 / 2,495` | **`2,495 / 2,495` (`100.00%`)** | **`+2,495`** |
| **Total loose `.bin` parsed (`383,882` tracks; `100.00%` of plaintext `.bin`)** | `3,724` | **`6,219`** | **`+2,495`** |
| **Parsed files reaching `EXACT_EOF` (`trailing_bytes == 0`)** | `3,724 / 3,724` | **`6,219 / 6,219` (`100.00%`)** | **`+2,495`** |
| **Loose `.bin` `parser-rejected` (FAIL)** | `0` | **`0`** | `0` |
| **Loose `.bin` `unsupported-version` (`0x0200` `EMBYTECRYPT_BIN` only)** | `2,504` | **`9`** | **`-2,495`** |
| **Loose `.bin` `invalid-header` / `unrecognized-type`** | `0` / `0` | **`0` / `0`** | `0` |
| **Pack entries (`SkinObject.rcc` + `Animation.rcc`)** | `6,479` ok / `3,550` fail / `1,631` unsup | **`6,479` ok / `3,550` fail / `1,631` unsup** | `0` |

- **Actual `.cfg` (`SANIMCONINFO`) compatibility for the `2,495` `0x0104` `.bin` files:**
  - **`21` (`0.84%`)** pair with currently supported `.cfg` `0x0101`–`0x0107` (`1` `0x0104`, `2` `0x0105`, `15` `0x0106`, `3` `0x0107`), increasing total supported `.bin`+`.cfg` pairs from `2,338` to **`2,359` (`+21`)**.
  - **`2,464` (`98.76%`)** pair with currently **unsupported** `.cfg` versions `0x0110`–`0x0115` (`12` `0x0110`, `714` `0x0111`, `177` `0x0112`, `996` `0x0114`, `565` `0x0115`), which remain explicitly classified `UNSUPPORTED` in `EP7AnimationReader::ParseInfo`.
  - **`10` (`0.40%`)** have no matching `.cfg` stem.

### 16.3 Regression Suites & Android ARM64 Build Verification
- **Phase 3J character/animation suite (`test_phase3j.cpp`):** **`2,084` checks, `0` failures** (up from `1,499`, sweeping 9 ABI fixtures across truncations and 4x single-byte corruptions, including `AnimationBin0104()`, all 8 empty/populated `(main, upBody1, upBody2)` combinations, `2048 + 2048 == 4096` combined `upBody` boundary, `2048 + 2049 == 4097` overflow rejection, exact EOF + trailing data, `0x0100`–`0x0103` regressions, and `0x0200` rejection) under both `-O2 -Wall -Wextra -Werror` and `-fsanitize=address,undefined` (`detect_leaks=1`).
- **B11/B15 verifier regression suite (`run_ep7_verifier_tests.sh`):** **`3,755` checks, `0` failures** (up from `2,784`) under normal and `ASan + UBSan + integrated LSan`.
- **B12/B15 inspector regression suite (`run_ep7_animation_inspector_tests.sh`):** **`9,702` checks, `0` failures** (up from `8,303`) under normal and `ASan + UBSan + integrated LSan`.
- **CI `Core C++17 tests (blocking)` (run `45999738432`, job `140508021895`):** **success** (including data-dependent `ranmobile_tests`, `ranmobile_phase3j_tests`, and verifier sanitizer suite).
- **CI `Android ARM64 build (lib + APK/AAB)` (run `45999738432`, job `140508021781`):** **success** — `libranmobile-arm64-v8a` (`libranmobile.so`, `2,304,051` B, artifact `15197203199`) and `ranmobile-android-packages` (`assembleDebug` + `assembleRelease` + `bundleRelease`, `1,989,679` B, artifact `15197378275`).



## 17. Phase 3J B16 Implementation & Authentic Validation Update (`SANIMCONINFO` `.cfg` `0x0108`–`0x0112`/`0x0114`/`0x0115`)

Implemented in commit `d123d931592057d19dfc3c0fa5df4a07741f3a55` on branch `arena/00fd13f9-ran-online-ep7` (Draft PR #7). This closes the `.cfg` half of B11: the companion-metadata versions that `88.0%` of `0x0103` and `98.8%` of `0x0104` `.bin` files pair with (Section 11) are now parsed by the production reader.

### 17.1 Evidence Chain (engine sources finally available)

The blocker recorded in the B11 status review — *"No structure, offset, size or decoder for `0x0108`–`0x0115` … has been written, and none will be until evidence in (1)–(3) exists"* — is resolved by evidence route (1): an authentic EP7-Complete-Files engine drop containing the full `SANIMCONINFO` save/load implementation was retrieved and cross-verified:

1. **Primary source:** backup-repo `matheomedrana/Ran-Online-EP7-Complete-Files` @ `tf661fbf`, `Lib_Engine/Meshs/SAnimationInfoSaveLoad.cpp` (Git LFS blob, SHA-256 `80de6ade…`, fetched as decoded C++ via the backup repo's LFS pointers) — contains the complete authentic dispatch `SANIMCONINFO::LoadFile`:
   - `version >= 0x0200` → `EMBYTECRYPT_CFG` decoding (not implemented here; none observed in the authentic set);
   - `0x0111/0x0112/0x0114/0x0115` → `LoadFile_0111_0112_0114_0115`;
   - `0x0108/0x0109/0x0110` → `LoadFile_0108_0109_0110`;
   - `0x0101`–`0x0107` → individual loaders (already implemented in B12);
   - **no `case 0x0113`** — the authentic switch treats `0x0113` as `ErrorVersion`, so `0x0113` remains explicitly `UNSUPPORTED` here.
   `SAnimationInfo.h` marks both B16 loaders `//official version partial read only` — the official readers stop mid-structure and leave the rest of the file unconsumed. Post-load the authentic code clears `m_dwFlag &= ~ACF_NEWINFO`.
2. **Cross-checks:** the GDrive EP7 drop (`[Lib]__Engine`, `/tmp/ep7-b16-sources/`) carries byte-identical `SAnimationInfo{,.h}`/`SAnimationInfoSaveLoad.cpp`; every sub-structure used by the B16 layouts (`SANIMSTRIKE` 12 B × `ACF_STRIKE=9`, `SChaSoundData` 388 B, `DxAniScale::Load`, `DxEffAniSingle`/`Ghosting`/`Trace`/`FaceOff` `LoadFile` property blobs 464/400, 28, declared-size, 32 B) matches the local `enginelib/` tree already used since B12.

### 17.2 Production Parser Changes (`RANMobile/Data/Importer/EP7Animation.{h,cpp}`)

1. **`ParseInfo` gained the two authentic B16 branches** (pre-B16 explicitly rejected *all* of `0x0108`–`0x0115` as unsupported; `0x0101`–`0x0107` behavior is preserved unchanged):
   - **`0x0108`–`0x0110`** (`LoadFile_0108_0109_0110`): `name[128]`, `skeleton[128]`, `flag`, `UNITTIME`, `STime`, `ETime`, `ETimeOrig`, `MainType`, `SubType`, `SANIMSTRIKE[9]` (108 B), `DivCount` + `DivFrame[8]` (16 B), effect list (`effCount` + entries), `DxAniScale` block, then a `SChaSoundData` (388 B) sound block **last**.
   - **`0x0111`–`0x0112`/`0x0114`/`0x0115`** (`LoadFile_0111_0112_0114_0115`): four `TSTRING`s first (`szCfgFile`/`szBinFile` read and recorded as `cfgFileName`/`binFileName`; `szXFile` → name; `szSkeleton` → skeleton), then `flag`, `STime`, `ETime`, `ETimeOrig`, `UNITTIME`, types, `DivCount`+`DivFrame`, `SANIMSTRIKE[9]`, 388 B sound, effect list, and `DxAniScale` **last**.
2. **Effect entries** are typed per the authentic `DxEffAni*` hierarchy (`TypeID` 0 `Single` / 1 `Ghosting` / 2 `Trace` / 3 `FaceOff`, then per-effect `Ver` + `Size` + property payload; `Single` property 464 B @ `0x0101` / 400 B @ `0x0100`, `Ghosting` 28 B, `Trace` declared-size, `FaceOff` 32 B; unknown `TypeID` and oversize property sizes are rejected with sanity bounds `kMaxCfgEffects = 4096`).
3. **`DxAniScale`** (`u32` mapCount; entry = `TSTRING` + `u32` keyCount + keyCount × (`u32` key + `f32` scale), bounded by `kMaxAniScaleEntries = 4096`).
4. **Partial-read fidelity:** because the official loaders stop early, B16 files legitimately contain bytes the official engine never reads. `EP7AnimInfo` now records `trailingBytes` and `exactEof` per file instead of failing or reinterpreting them; trailing bytes are counted, never parsed (same forensic discipline as B14/B15 `.bin` trailing audits).
5. **`EP7AnimMainType`** extended to the authentic `DxAniKeys.h` enum (`AN_BIKE_A=21` … `AN_REARCAR_A=33`, `AN_NONE=34 = AN_TYPE_SIZE`); main types outside `0..33` are rejected in both B16 branches (`cfg main type out of range`).
6. **`0x0113`** remains an explicit `UNSUPPORTED` classification, matching the authentic switch fall-through (`ErrorVersion`).

### 17.3 Inspector B16 Survey (`RANMobile/Tools/inspect_ep7_animation_containers.cpp`)

The read-only forensic survey now classifies every indexed `.cfg`: `cfg_version_counts` (full `0x0102`–`0x0115` histogram), and for `0x0108`–`0x0115` (≠ `0x0113`) the B16 outcome aggregates `cfg_b16_outcomes` (`<version>:exact-eof` / `<version>:trailing-bytes` / `<version>:rejected`), `cfg_b16_rejection_reasons` (normalized parser error, capped), `cfg_b16_effect_type_masks` (`<version>:mask-0x…` over the parsed effect list), `cfg_b16_trailing_byte_sizes` (`<version>:<n> bytes`), and `cfg_b16_totals` (`attempted`/`parsed`/`exact-eof`/`with-trailing`), plus per-sample `b16_*` JSON fields. The survey remains read-only (`asset SHA-256 unchanged after inspection: YES`).

### 17.4 Authentic `ran-ep7-full-v1` Validation Results (CI Runs `46010599353` & `46010599537`)

Surveyed in GitHub Actions run [`46010599353`](https://github.com/famoh01/ran-online-ep7/actions/runs/46010599353) (`b12-authentic-anim-survey`, check run `140545691201`; text report blob `13f4dee0f2e41532ead15ecc7f260039bdb00cf1`, JSON report blob `1e19f8313a2527e9dfb9de04d7a8242b5e1edb39`) and run [`46010599537`](https://github.com/famoh01/ran-online-ep7/actions/runs/46010599537) (`Inspect authentic ran-ep7-full-v1 animation containers`, check run `140544798789`):

| `.cfg` version | Authentic files | B16 outcome | Notes |
|---|---:|---|---|
| `0x0102` / `0x0103` | `5` / `5` | pre-B16 path (unchanged) | |
| `0x0104` | `414` | pre-B16 path (unchanged) | |
| `0x0105` | `221` | pre-B16 path (unchanged) | |
| `0x0106` | `1,095` | pre-B16 path (unchanged) | |
| `0x0107` | `1,091` | pre-B16 path (unchanged) | |
| `0x0108` | `168` | **`168` parsed, all `exact-eof`** | mask `0x0001`: 30, mask `0x0002`: 1 |
| `0x0109` | `17` | **`17` parsed, all `trailing-bytes` (104 B)** | mask `0x0001`: 2 |
| `0x0110` | `1,081` | **`1,081` parsed, all `trailing-bytes`** | 90 B: 219, 117 B: 846, 125 B: 2, 133 B: 6, 197 B: 8; masks `0x0001`: 258, `0x0002`: 2, `0x0003`: 3 |
| `0x0111` | `887` | **`887` parsed, all `exact-eof`** | masks `0x0001`: 230, `0x0002`: 4, `0x0003`: 2 |
| `0x0112` | `183` | **`183` parsed, all `trailing-bytes`** | 90 B: 157, 170 B: 8, 250 B: 10, 330 B: 4, 490 B: 4; masks `0x0001`: 35, `0x0002`: 2, `0x0003`: 2 |
| `0x0113` | `0` | (absent from the authentic set) | authentic `LoadFile` has no `case 0x0113` |
| `0x0114` | `1,002` | **`1,002` parsed, all `trailing-bytes`** | 115 B: 956, 195 B: 4, 203 B: 18, 275 B: 6, 291 B: 13, 379 B: 3, 467 B: 1, 555 B: 1; masks `0x0001`: 145, `0x0003`: 2 |
| `0x0115` | `565` | **`565` parsed, all `trailing-bytes`** | 39 B: 1, 127 B: 486, 215 B: 30, 303 B: 29, 391 B: 9, 479 B: 9, 567 B: 1; mask `0x0001`: 65 |

- **B16 totals: `attempted=3,903`, `parsed=3,903` (`100.00%`), `exact-eof=1,055`, `with-trailing=2,848`, rejections `0`.** Every B16-layout `.cfg` in the authentic release parses; the `1,631` previously-`UNSUPPORTED` `Animation.rcc` companion-metadata entries are now understood field-by-field.
- **`.bin` side re-confirmed unchanged:** `6,219 / 6,228` parsed (`100%` `EXACT_EOF`), `9` `0x0200` `EMBYTECRYPT_BIN` unsupported, `parser-rejected=0`, header padding `zero-padded=6228`.
- The trailing-byte size modes (`0x0109`:104, `0x0110`:117/90, `0x0112`:90/170, `0x0114`:115, `0x0115`:127/215) are the unread tail the official partial-read loaders leave behind — consistent with the `//official version partial read only` source note; B16 counts and records them per file.
- Effect-type masks across all effect-bearing B16 `.cfg` (`783` files) use only `Single` (bit 0), `Ghosting` (bit 1), and `Single|Ghosting` (`0x0003`) — no `Trace`/`FaceOff` entries occur in the authentic set.

### 17.5 Regression Suites & Android ARM64 Build Verification

- **Phase 3J character/animation suite (`test_phase3j.cpp`):** **`5,686` checks, `0` failures** (up from `2,084`) under both `-O2 -Wall -Wextra -Werror` and `-fsanitize=address,undefined` (`detect_leaks=1`) — including per-version minimal parses `0x0108`–`0x0115`, rich `0x0108`/`0x0110`/`0x0111`/`0x0115` fixtures with effects + `DxAniScale`, 9 malformed mutations (effect-type/sanity/ani-scale/sound-truncation/main-type rejections), every strict prefix truncation, `mainType` `33`/`34` boundaries, and full-prefix single-byte corruption sweeps.
- **B11/B16 verifier regression suite (`run_ep7_verifier_tests.sh`):** **`3,757` checks, `0` failures** (up from `3,755`) under normal and `ASan + UBSan + integrated LSan` (covers the `0x0108` minimal-body `Pass` classification and the `0x0113` `UNSUPPORTED` classification).
- **B12/B16 inspector regression suite (`run_ep7_animation_inspector_tests.sh`):** **`13,423` checks, `0` failures** (up from `9,702`) ×2 configurations under normal and `ASan + UBSan + integrated LSan` (covers the B16 `.cfg` survey aggregation: outcomes, trailing sizes, effect masks, rejection reasons, JSON emission).
- **CI `Core C++17 tests (blocking)` (run `46010599353`, job `140544798259`):** **success**.
- **CI `Android ARM64 build (lib + APK/AAB)` (run `46010599353`, job `140544798206`):** **success** — `libranmobile-arm64-v8a` artifact (`2,355,338` B) and `ranmobile-android-packages` (`assembleDebug` + `assembleRelease` + `bundleRelease`, artifact `2,024,876` B).
- **CI `Authentic EP7 asset verification (diagnostic)` (run `46010599353`, job `140544797658`):** failure — the long-documented expected diagnostic exit `1` from the pre-existing `SkinObject.rcc` `.chf`/`.cps`/`.abf` pack gaps (`3,550` entries, unchanged; out of B16 scope).

---

## 18. Phase 3J B17 — Animation Runtime Integration: Playback Semantics Mapped to the Mobile Runtime

B17 connects the containers investigated in §5–§17 to an actual playback runtime. Everything below
was read from the authentic PC sources (`enginelib/Meshs/`, cp949) before implementation; no
behavior was invented.

### 18.1 `SAnimation` evaluation — the decisive facts

| Original (file:line) | Behavior | Mobile mirror |
|---|---|---|
| `SAnimation::SetTime` (SAnimation.cpp:480) | dispatch order: matrix keys → quatPos keys → else S/R/T composition | `EP7AnimationPlayer::ApplyToSkeleton` same order |
| `SAnimation::SetMatrix` (SAnimation.cpp:359) | `memcpy(pBoneToAnimate->matRot, key.mat, 64)` — direct write, **no weight involvement** | matrix branch writes `bone.local`, `localIsFromSqt=false` |
| `SAnimation::SetPosQuat` (SAnimation.cpp:442) | `fmod` over last key time, lower-bound snap; `bFirst` → CUR else PREV + USE_ANI_BLENDING | same; `bone.scale/position/rotation` vs `prev*` + `useBlending` |
| `SAnimation::SetMix` (SAnimation.cpp:387) | raw `fGlobalTime` lookup (latent bug — blend clock never offsets) | reproduced as-is, documented |
| `SAnimation::ConvertMatToQuat` (SAnimation.cpp:1326) | matrix-only tracks → `SQuatPosKey` via `D3DXMatrixToSQT`+`CompressionQuaternion`, **matrix keys deleted**; tracks with quatPos keys untouched (guard `!m_pQuatPosKeys`); called by every `Load_*` (1459/1511/1568) and the qualified reader (1135) | `EP7AnimationReader::ConvertMatToQuat` — runtime load transform after `BindToSkeleton`; byte reader unchanged |
| `CompressionQuaternion` (SAnimation.cpp:27) | hemisphere canonicalize (w<0 → negate), x/y/z = `(DWORD)((v+1)*0.5*0xffff)` packed hi/lo, w = `(DWORD)(w*0xffff)` | `ep7math::CompressionQuaternion` bit-exact for in-range inputs; NaN → identity + clamp hardening |
| scale/rotate/position branch (SAnimation.cpp:508+) | `fmod(fGlobalTime, lastKeyTime)` window, neighbor lerp; rotate keys negate w after sampling; USE_SQUAD neighbor setup | same windows; squad approximated by slerp (documented) |

### 18.2 `DxSkinAniControl` state machine — the decisive facts

| Original (DxSkinAniControl.cpp:line) | Behavior | Mobile mirror (`EP7SkinAniControl`) |
|---|---|---|
| 155 `SetAnimation` | same node → FALSE; backup clock preserved; `m_fTime = m_dwSTime`; `StartAnim()` | same (backup control, weight untouched on FALSE) |
| 516/562/598 `SELECTANI*` | every successful select sets `m_fWeight = 0` (blend-in) | same |
| 742 `FrameMove` | `m_fWeight += fElapsedTime*7`, clamp 1; ENDFREEZE pins 1 | same ramp |
| 753/771 `FrameMove`/`UpdateTime` | CORE: upBody+layer+downBody+layer clocks; UpdateTime per-pair bFirst | same 4-clock core |
| 194 `UpdateTime` tail | `UpdateBoneScale` after the pose write (per DxAniControl::UpdateTime) | `UpdateTime` non-const, bone-scale tail |
| 523 `DEFAULTANI` | empty registry → `CleanUp`; else nodes[0] | same |
| 584 `SELECTSKILLANI` | missing/OOR target → `downBody.endAnim = TRUE` + fallback nodes[0] when nothing selected | same fallback |
| 814–860 `Render` | pose pipeline `ResetBone → UpdateTime(weight) → UpdateMix → BIGHEAD/BIGHAND → g_fWeight → UpdateBones` | `EvaluatePose`: `ResetToRestPose → UpdateTime → UpdateMix → UpdateBones(worldRoot, weight)` |
| DxSkinChar.cpp:242 creation | always selects an animation (type → AN_PLACID → DEFAULTANI) | `Spawn` auto-plays DEFAULTANI |

### 18.3 Consequence for test/reference semantics

Because creation auto-plays, an animated character never rests in the bind pose. The B17 suite's
bind-pose reference resolves the same `.chf` **without** animation files (no clip → `DEFAULTANI`
cleans up → plain bind pose), replacing the stale pre-B17 assumption that a spawned instance holds
the bind pose until a clip is requested.

### 18.4 Runtime matrix→quatPos conversion and data coverage

The conversion is a runtime-load transform, not a parser change: authentic `.bin` bytes still parse
verbatim (B12–B15 reader untouched, suite re-verified), and the conversion runs after skeleton
binding for every loadable clip, so the playback runtime sees the same track state the original's
`SAnimation` objects hold after `Load_*`. Tracks that already carry quatPos keys keep their matrix
keys (both branches animate; matrix wins per `SetTime` dispatch order).

---

## 19. Phase 3J B18 — GPU Skinning & Texture Binding: Container-Level Findings

B18 did not change any parser (`.bin`/`.cfg`/`.chf`/`.cps`/`.x` readers are byte-identical to B17);
the container-level work is on the texture side, recorded here because it completes the
`.mtf` container story started with `MemoryTexture.cpp` in B11:

* **`.mtf` container**: 12-byte header (`fileType` `TEXTURE_DDS=0` / `TEXTURE_TGA=1`), body
  per-byte `+0x03` then `^0x47` (`EP7TextureDecoder::DecryptMTF`, verified against
  `MemoryTexture.cpp:97-98`). B18 adds the **TGA branch** (`DecodeTGA`: types 1/2/3/10, 24/32 bpp,
  origin bit honoured, ≤4096², else invalid). The DDS branch (`DecodeDDS`: DXT1/DXT3/DXT5 +
  uncompressed RGBA8/BGRA8) is unchanged.
* **Verifier evidence extension** (`verify_ep7_character_assets.cpp`, additive): the
  `authentic-verification` CI job now also classifies shipped textures (`.mtf` decrypt/decode
  rates, `.dds` decode rates) and resolves the first valid character's material texture names
  through the real ladder, reporting decoded dimensions — the same numbers the runtime needs
  (§9 evidence), produced only from authentic pack entries.
* **`.chf`→`.cps`→`.x`→texture chain**: the character loader now builds GPU skin streams at load
  (influence rules identical to `SkinVerticesCPU`) and the scene resolves each piece's texture
  names (`SMATERIAL_PIECE::LoadTexture` parity: `.cps` material names primary, `.x`
  `MeshMaterialList` subset names fallback) against the pack cache.
* **No parser behavior changed**: B11–B16 suites re-run green (verifier 3,757/0, inspector
  13,423/0 ×2); 0x0113 and 0x0200 EMBYTECRYPT remain UNSUPPORTED exactly as before.
