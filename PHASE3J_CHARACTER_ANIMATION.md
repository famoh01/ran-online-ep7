# PHASE3J_CHARACTER_ANIMATION.md — Authentic EP7 Character Model, Skeleton & Animation Rendering

**Date:** 2026-11-26
**Phase:** 3J (character / skeleton / animation rendering foundation)
**Branch:** `arena/42c5ac0c-ran-online-ep7` (branched from `main` @ `47f3c6d6`)
**Status:** **Foundation implemented and verified on the host; NOT wired into a 3D draw pass on this branch.**

> This document is deliberately blunt. Section 12 lists exactly what is *not* done. Nothing
> here claims that an authentic EP7 character has been rendered on screen: no authentic
> `.chf/.cps/.x/.bin/.cfg` bytes exist in this sandbox (see B2), so the parsers are verified
> against ABI fixtures plus a real-asset verification tool that runs in CI where the
> original client archive is extracted.

---

## 1. Scope and honest position

Phase 3J asks for authentic EP7 character loading, skeleton evaluation, animation sampling,
skinning and a mobile rendering foundation. What is delivered:

| Area | Delivered | Evidence |
|---|---|---|
| Binary serialisation layer (`basestream`) | Yes | `Data/Importer/EP7ByteFormat.h` |
| Math layer (row-major D3DX semantics) | Yes | `Data/Importer/EP7Math.h` |
| `.x` text tokenizer | Yes | `Data/Importer/EP7XFile.{h,cpp}` |
| Skeleton (`Frame` / `FrameTransformMatrix`) | Yes | `Data/Importer/EP7Skeleton.{h,cpp}` |
| `.cps` skin-piece container | Yes (through the trace list) | `Data/Importer/EP7SkinPiece.{h,cpp}` |
| `.chf` character definition | Yes | `Data/Importer/EP7CharacterFile.{h,cpp}` |
| Skinned mesh (`Mesh` + `SkinWeights`) | Yes | `Data/Importer/EP7SkinnedMesh.{h,cpp}` |
| Animation container `.bin` | Yes (0x0100–0x0102) | `Data/Importer/EP7Animation.{h,cpp}` |
| Animation metadata `.cfg` | Yes (0x0107) | `Data/Importer/EP7Animation.{h,cpp}` |
| Animation player / frame sampling | Yes | `EP7AnimationPlayer` |
| CPU skinning | Yes | `SkinVerticesCPU`, `BuildBonePalette` |
| GPU skinning | **No** | requires the GLES3 3D pass that lives in PR #4 |
| Scene integration (instances, lifecycle, culling) | Yes | `Engine/Scene/EP7CharacterScene.{h,cpp}` |
| Touch UX overlay + diagnostics | Yes | `UI/Character/CharacterPreviewOverlay.{h,cpp}` |
| Real-asset verification tool | Yes | `Tools/verify_ep7_character_assets.cpp` |
| `.rcc` pack reader (inflate + ZipCrypto) | Yes | `Data/Importer/EP7RccArchive.{h,cpp}` |

---

## 2. Repository state this phase was built on

Verified with `git`, not assumed:

```
main                                         47f3c6d6  (this branch's base)
origin/arena/be1e147d-ran-online-ep7         76b2c1c3  PR #4, +33 commits, Draft
origin/arena/075e1725-ran-online-ep7         7b355b3c  PR #3, Draft
```

* `git merge-base --is-ancestor HEAD origin/arena/be1e147d-…` → **true**: PR #4 is a clean
  descendant of this branch's base, so a Phase 3J branch layered on it would carry no
  conflicts.
* PR #3 (7b355b3c) and PR #4 (76b2c1c3) are **untouched** by this phase.
* `EP7SceneRenderer`, `EP7WorldParser`, `EP7AssetPipeline`, `MobileUIKit` exist **only** on
  PR #4's branch — not here. This phase therefore does not modify, and cannot extend,
  `EP7SceneRenderer`; it exposes the exact hooks that layer needs (see §8).

---

## 3. Original-source evidence (what each parser is derived from)

Every structure below was read out of the original client/server tree in this repository.
No structure was invented.

### 3.0 Where the authentic character bytes actually live (`SkinObject.rcc`, `Animation.rcc`)

This is the most consequential discovery of the phase, and it invalidated the first version
of the verification tool.

`GLOGIC::InitFile` (`enginelib/G-Logic/GLogic.cpp:40-62`) does **not** read character assets
from the file system:

```
strANIMATION_ZIPFILE = DxSkinAniMan::GetPath()  + "Animation.rcc";
strSKINOBJ_ZIPFILE   = DxSkinObject::GetPath()  + "SkinObject.rcc";
CUnzipper::LOADFILE_RCC(strANIMATION_ZIPFILE);
CUnzipper::LOADFILE_RCC(strSKINOBJ_ZIPFILE);
```

and every character load goes through
`GLOGIC::openfile_basestream(bENGLIB_ZIPFILE, zipName, fullPath, fileName)` →
`CUnzipper::UnzipToMemory` (`enginelib/Common/Unzipper.cpp:569`). Three consequences:

1. A directory walk of an extracted client finds **zero** character assets — they are all
   inside two ZIP archives. A verification tool that only walks the tree would report
   "0 candidates" and look like success.
2. Entry lookup is **case-insensitive** and keyed on `zipName + entryName`
   (`CUnzipper::LOADFILE_RCC` / `GOTOFILEPOS` both `std::transform(..., tolower)`).
3. Encrypted entries use **traditional ZipCrypto with the password `"tkqwlf"`**
   (`unzOpenCurrentFilePassword(m_uzFile, "tkqwlf")`).

`EP7RccArchive` therefore implements exactly that subset: central-directory parsing, stored
and deflate entries (a self-contained canonical-Huffman inflater), ZipCrypto decryption with
the client password, and CRC-32 verification on every extraction. It is a reader, not a
general ZIP library: no zip64, no multi-disk, no AES, no writing — and every offset is
bounds-checked.

### 3.1 Serialisation (`enginelib/Common/`)

| Fact | Source |
|---|---|
| Asset = 128-byte file-type block + `DWORD` file ID, then payload | `SerialFile.cpp:ReadFileType`, `SerialMemory.cpp:ReadFileType`, `basestream.h:FILETYPESIZE` |
| `GetfTell()/SetOffSet()` are payload-relative | `SerialFile.cpp:SetOffSet/GetfTell` |
| `operator<<(std::string)` = `DWORD(strlen+1)` **and** writes the NUL | `SerialFile.cpp:460` |
| `operator>>(std::string)` reads `len` bytes, 0-terminated buffer | `SerialFile.cpp:609` |
| `BeginBlock()/EndBlock()` back-patch a block-size `DWORD` | `SerialFile.cpp:BeginBlock/EndBlock` |
| `m_bencode` applies the compbyte permutation to every block | `CompByte.cpp` (256-entry table, 1:1) |
| Short reads historically zero-filled silently | `CSerialMemory::read` |

`EP7ByteFormat.h` reproduces these semantics **with bounds checking**: every read reports
failure instead of poisoning downstream values, and the compbyte table is copied verbatim
from `CompByte.cpp` (verified programmatically to be a permutation of 0–255).

### 3.2 Skeleton (`enginelib/Meshs/DxBoneCollector.cpp`)

`DxSkeleton::LoadBoneTrans` (lines 336–470), reproduced exactly:

* a synthetic root bone `"__ROOT"` is created before enumerating (line 300), and **all**
  top-level `Frame` objects become its children;
* a `Frame` with no instance name is dropped (`if (!szFrameName) goto e_Exit`);
* `FrameTransformMatrix` → `matRot = matRotOrig = *pmatNew`;
* the body part is inherited from the parent, and a child named exactly
  `"Bip01_Spine1"` switches the whole subtree to `EMBODY_UPBODY`;
* `DxBoneTrans::AddBoneTrans` inserts so the **first** child stays the head of the sibling
  list — reproduced in `EP7Skeleton::RebuildLinks()`.

`DxSkeleton::UpdateBones` is mirrored in `EP7Skeleton::UpdateBonesRecursive`: when the pose
comes from SQT, the local matrix is rebuilt as `RotationQuaternion(q)` with the per-axis
scale multiplied into columns 0–2 and the translation in row 3, then
`combined = Multiply(local, parentCombined)` (D3DX row-vector order).

### 3.3 Character definition (`enginelib/Meshs/DxSkinCharData.cpp:349`)

`DxSkinCharData::LoadFile` → `LOAD_Ver101` / `LOAD_Ver100`. The 0x0101/0x0102 layout is
reproduced field by field (scale, piece slots, skeleton name, animation list, bounding box,
optional world-object flag, optional height/radius, trailing flag). Absent optional tails are
treated as "file simply ends" — the original returns `TRUE` there, so rejecting them would
be *less* faithful. Version 0x0100 uses the fixed 260-byte skeleton field and 128-byte
animation names.

### 3.4 Skin piece (`enginelib/Meshs/DxSkinPieceContainer.cpp:356`)

`DxSkinPiece::LoadFile` gates on version 0x0104–0x0107, then reads refCount, `.x` file name,
skeleton name, piece type, weapon-back slot (0x0106+), mesh name, and a material array.
`SMATERIAL_PIECE::LoadFile` (line 124) reads **both** `dwVERSION` and `dwBLOCKSIZE`
unconditionally; the block size is the body length back-patched by `EndBlock`. Unknown
material versions are skipped by that block size instead of being reinterpreted.

### 3.5 Skinned mesh (`enginelib/Meshs/DxSkinMeshLoad.cpp`, `DxSkinMesh9*.cpp`)

* `DxSkinMesh::LoadMesh` uses `D3DXLoadSkinMeshFromXof` and names the container from the
  `Mesh` instance name, falling back to the parent frame name (line 452).
* `DxSkinMesh9_HLSL.cpp:96-107` proves the runtime vertex layout carries **four** blend
  weights and four blend indices, i.e. the assets stay within 4 influences/vertex.
* `DxSkinMesh9_CPU.cpp:625` proves the palette rule
  `g_pBoneMatrices[i] = pBoneOffsetMatrices[i] * ppBoneMatrixPtrs[i]`.
* `DxSkinMesh9::SetupBoneMatrixPointersOnMesh` returns `E_FAIL` when `FindBone` misses — so
  a `SkinWeights` block naming an unknown bone is a hard error, not a warning.
* `xskinexp/ExportXFile.cpp:2823` proves the exporter attaches every `SkinWeights` object to
  the **Mesh** (`pParent->AddDataObject`), which is why the reader treats skin data as a
  child of `Mesh` (sibling placement is also accepted for foreign files).

The one thing that genuinely cannot be reproduced from source is D3DX's *inference* of the
vertex declaration from the declared byte size. The reader therefore recovers the stride
from the contiguous float run the file records and only decodes the canonical 12/24/32-byte
layouts; anything else is reported as `EP7VertexLayout::Unknown` with `geometryDecoded`
false rather than guessed.

### 3.6 Animation (`enginelib/Meshs/SAnimation.cpp`, `DxSkinAniManSaveLoad.cpp`)

* `SAnimContainer::SaveToFile` (line 1573) → `SetFileType(szFILETYPE, VERSION)`, so the
  container version lives in the **header file ID**, and each `SAnimation` track begins with
  its own version `DWORD`.
* `SAnimation::Load` (0x0102) order: bone name, rotate keys, position keys, garbage DWORD,
  quatpos keys, garbage DWORD, matrix keys, scale keys.
* `SAnimation::SaveToFile` (line 1353) proves the two interleaved "garbage" DWORDs are
  `m_strBone.length()` and `m_strBone.length() + VERSION`.
* `SMatrixKey` holds a `D3DXMATRIXA16`, which is `__declspec(align(16))`; on the 32-bit
  client `sizeof(SMatrixKey)` is **80**, i.e. 12 bytes of padding follow `dwTime`.
* `SANIMCONINFO::SaveFile` streams members individually. `m_sStrikeEff[ACF_STRIKE=9]` is a
  fixed array read regardless of the declared count, and `SChaSoundData` is written with
  `WriteBuffer(&m_ChaSoundData, sizeof(SChaSoundData))` — 386 bytes of meaningful data with
  no trailing struct padding.
* `DecompressionQuaternion` (SAnimation.cpp:52) subtracts 32767 from **all four** 16-bit
  halves, including `w`. That quirk is reproduced, not "fixed".
* `DxSkinAniControl.cpp` defines `UNITANIKEY_PERSEC 4800`, and `ANIMCONTNODE::FrameMove`
  both clamps to `[m_dwSTime, m_dwETime]` and reports `bEndAnim` as soon as a non-looping
  clip enters its last two `m_UNITTIME` intervals.

---

## 4. Implemented parsers

### 4.1 `EP7ByteFormat.h` — bounds-checked `ep7byte::Stream`
`ReadFileType`, all scalar reads, `ReadString` (len-prefixed, NUL-terminated),
`ReadFixedString`, `ReadBool32`, `CanReadElements` (overflow-safe element pre-check),
`Skip/Seek/Remaining/Tell/Fail`. Compbyte decode applies to every read when the asset is
encoded.

### 4.2 `EP7XFile.{h,cpp}` — `.x` text tokenizer
Handles the D3DX text grammar subset actually used by skeleton and mesh files. **Important
correction found during testing:** every `.x` file — text *and* binary — starts with
`"xof "`. `IsBinaryX()` therefore inspects the format field at offset 8 (`bin`/`bzib`/`tzip`)
instead of the magic, and `IsTextX()` accepts `txt`. Rejecting on the magic alone would have
rejected every real text `.x`.

### 4.3 `EP7Skeleton.{h,cpp}` — bone hierarchy
Produces `bones[]` with `parent/firstChild/nextSibling`, `localRest`, `local`, `combined`,
SQT fields and `EP7BodyPart`. `PaletteOrder()` is the depth-first order matching
`UpdateBones` recursion. Malformed input (missing brace, wrong float count, no named frames,
binary `.x`) is rejected with a message; `BoneCount()` includes the synthesised `__ROOT`.

### 4.4 `EP7SkinPiece.{h,cpp}` — `.cps`
Versions 0x0104–0x0107; strings, piece type, weapon-back slot (0x0106+), mesh name, material
records (version + block size, texture, flags, zBias → `* 0.00001f`), trace points with
`SVERTEXINFLU` version gating (unknown version skipped by size, as the original does) and the
declared effect count. Effect *payloads* are not parsed; `effectsPresent` records that the
list is non-empty so nothing is silently ignored.

### 4.5 `EP7CharacterFile.{h,cpp}` — `.chf`
0x0100/0x0101/0x0102 as described in §3.3. Piece count above `PIECE_SIZE` (13) is rejected
before any loop, exactly like `if (dwPieceSize > PIECE_SIZE) return FALSE;`.

### 4.6 `EP7SkinnedMesh.{h,cpp}` — skinned mesh
Faces, vertex stream (stride recovered, canonical layouts decoded), `SkinWeights`
(bone name = first body element — the D3D template has **no instance name**), material list
collapsed into contiguous per-material subsets with texture names. Validation: face indices,
skin vertex indices, index/weight count agreement, weights in `[0,1]`, unknown bone names.
Skinning: `BuildBonePalette` (offset × combined) and `SkinVerticesCPU` (≤4 influences,
weight renormalisation, normal transform).

### 4.7 `EP7Animation.{h,cpp}` — `.bin`, `.cfg`, player
Track parsing for 0x0100/0x0101/0x0102 with per-track version; `BindToSkeleton` resolves bone
names (missing bone → `boneIndex == -1`, which is reported, never silently bound);
`EP7AnimationPlayer` implements the 4800-keys/second clock, clamping, looping wrap, the
two-unit early-end rule and the freeze-at-end path. Sampling: matrix keys snap to
`LowerBound`, quatpos keys are dequantised then slerped, explicit keys compose `S·R·T`.

---

## 5. Scene integration

`Engine/Scene/EP7CharacterScene.{h,cpp}`:

* `EP7CharacterLoader::LoadCharacter` — parses `.chf`, resolves the skeleton, every named
  piece, its `.cps`, its `.x` mesh (bound against the skeleton), and each animation's `.bin`
  + `.cfg`. Assets are resolved through a caller-supplied callback so Android
  `AAssetManager`, a pack archive and tests all plug in without the loader knowing about the
  file system.
* Cross-checks: a `.cps` whose skeleton name disagrees with the `.chf` is rejected, and a
  `.cps` whose piece type disagrees with the `.chf` slot is rejected. Both correspond to
  `FG_BONEFILE_CHECK` / slot ordering in the original.
* `Spawn/Destroy/Clear/Find` — instance lifecycle; each instance owns its own
  `EP7Skeleton` copy so poses never alias.
* `Update(dt)` — advances animation clocks, resets to the bind pose, applies the clip,
  accumulates world transforms, skins every piece.
* `UpdateBounds` / `CullInstances(camera, renderer)` — world AABB from the authentic `.chf`
  bounding box (or height/radius, or the `DxSkinCharData` defaults) fed to
  `MobileRenderer::IsBoxInFrustum`, so there is exactly **one** frustum implementation.
* `GetDiagnostics()` — measured counts only: characters, visible characters, bones, animated
  bones, vertices, triangles, draw calls, clips, decoded pieces, CPU skin ms, FPS, and an
  `authenticAssetsOnly` flag that goes false if any instance lacks a real asset.

**No placeholder character can be spawned**: `RegisterAsset` refuses anything whose `.chf`
did not parse or whose skeleton is invalid.

---

## 6. Mobile UX

`UI/Character/CharacterPreviewOverlay.{h,cpp}` — a character-inspection layer, not a game
menu. It adds no gameplay actions and no new animation states.

* drag on the character strip → orbit (drives the existing `MobileCamera::OnDrag`)
* pinch → zoom (`MobileCamera::OnPinch`, ratio accumulated per frame)
* double-tap or `RESET VIEW` → fixed three-quarter preview camera
* `◀ / ▶` → continuous orbit for one-handed use
* clip chips → play an animation **by its authentic `.chf` name**; the chip list is built
  from the loaded asset, so no invented clip can appear
* `DIAG` → toggles the measured diagnostics panel (FPS, skin ms, bones, animated bones,
  vertices, triangles, draws, clip count, decoded geometry, authentic-asset flag)
* status band + red error banner for load failures

Layout is expressed in the renderer's design space: the interactive character strip is the
central 40% of the screen, chips occupy the left column above the joystick band, and the
bottom-left rotate cluster sits outboard of both. Nothing is drawn over the character.

---

## 7. Performance

Measured, on this host only:

| Measurement | Value | Environment |
|---|---|---|
| Phase 3J suite wall time | 0.585 s total (includes 6-fixture truncation + 4× byte-corruption sweep) | g++ 12.2.0, x86-64 Linux, single core, no optimisation flags |
| Suite under ASan + UBSan + LSan | clean, no reports | same |
| CPU skinning of 2 pieces × 4 vertices | sub-microsecond, reported per instance as `lastSkinMs` | same |

**Not measured and therefore not claimed:** skinning throughput on ARM64, real vertex
counts, GPU palette upload cost, texture memory, character load time, frame-time stability.
Those require authentic assets (B2) and a device; `EP7CharacterDiagnostics::cpuSkinMs` is
wired to provide them the moment real data exists.

The GPU-vs-CPU decision is documented rather than assumed: the authentic HLSL path
(`DxSkinMesh9_HLSL.cpp`) shows the palette is uploaded as shader constants and skinned on
the GPU, so GPU skinning is the intended mobile path. It is **not implemented here** because
this branch has no 3D world pass to attach a skinning program to. Interleaving would mean
building a second renderer, which the "do not replace existing engine architecture" rule
forbids. CPU skinning exists as the reference/small-count path and as the oracle for tests.

---

## 8. Hooks PR #4 (Phase 3I) needs

The integration is intentionally one-directional: Phase 3J owns data + pose evaluation,
Phase 3I owns GL state. To draw characters, `EP7SceneRenderer` needs:

1. `EP7CharacterScene::Update(dt)` once per frame before the world pass.
2. `EP7CharacterScene::CullInstances(camera, &renderer)` to refresh `inFrustum`/bounds.
3. For each instance with `poseValid && inFrustum`: upload `skinnedPositions` (and
   `skinnedNormals`) as the POSITION/NORMAL streams, use the per-piece `texture` from
   `EP7MeshSubset`, and `glDrawElements` per subset — indices are already in
   `EP7SkinnedMesh::faces`.
4. Optionally replace steps 3's CPU upload with a palette uniform
   (`BuildBonePalette` output) once a skinning vertex program exists.

None of these require changes to Phase 3J files.

---

## 9. Tests

`RANMobile/Tests/test_phase3j.cpp` — **263 checks, 0 failures**, and identical results under
`-fsanitize=address,undefined` with leak detection.

**Fixture policy:** the buffers are hand-written ABI fixtures that follow the layouts
recovered above. They are *not* game data, they are never presented as a rendered character,
and every fixture is labelled as such in the file header.

| Section | Covers |
|---|---|
| `x tokenizer` | text/binary discrimination (real `xof 0303bin` header), empty input |
| `skeleton` | hierarchy, sibling head-insert, UPBODY propagation, palette order, combined transforms, malformed/truncated/binary rejection |
| `skinned mesh` | layout detection, stride, positions/normals/UVs, bone binding, element-order corruption, out-of-range skin index, out-of-range face index, unknown bone name, missing mesh, empty buffer, bind-pose identity, weight renormalisation |
| `.cps` | versions 0x0104–0x0107 gate, all fields, old-version rejection, truncation, empty |
| `.rcc` archive | case-insensitive lookup, stored entries, in-memory ZipCrypto decrypt with `"tkqwlf"` that then feeds the real `.cps`/`.chf` parsers, CRC corruption rejection, missing entry, garbage/empty input, plus a truncation and byte-corruption sweep of the archive |
| `.chf` | 0x0102 full parse, defaults, unknown version, `pieceCount > 13`, truncation, empty |
| animation | `.bin` 0x0102 track, bone binding, `MaxKeyTime`, `.cfg` 0x0107 fields + loop flag, looping wrap, 4800-unit clock, two-unit early end, truncation, wrong version |
| scene integration | asset load through the resolver, cross-file consistency, spawn/pose/skin, clip-by-name, animation actually moves vertices, measured diagnostics, AABB culling, destroy, and **refusal to spawn from an unloadable asset** |
| math | D3DX multiplication order, transform, quaternion decompression quirk, slerp midpoint |
| robustness sweep | every truncated prefix of all 6 fixtures and 4 values × every byte offset, through all 5 parsers — must never crash |

---

## 10. Real-asset verification (CI)

`RANMobile/Tools/verify_ep7_character_assets.cpp` walks an extracted client tree, runs every
loose `.chf/.cps/.skt/.x/.bin/.cfg` through the real parsers, then opens every `.rcc`/`.zip`
it finds and runs the same parsers on the character entries inside (§3.0). It prints
per-entry success or the parser's error string and a summary. It is wired into the CI workflow (after the
original client archive is extracted to `external/ep7_release/client_original`) and its log
is uploaded and summarised. If the directory is missing it prints "nothing verified" instead
of reporting success.

**Result in this sandbox: no authentic archives exist (B2/B9), so nothing is claimed about
real-asset compatibility.** The tool was exercised end-to-end against a synthetic
client-shaped tree (`SkinObject.rcc` + `Animation.rcc`) built from the ABI fixtures in §9:
all 7 entries parsed (`chf 1/1`, `cps 2/2`, `x 1 skeleton + 1 mesh`, `bin 1/1`, `cfg 1/1`),
and it was checked against a real Python-generated ZIP using both stored and deflated
entries plus a CRC-corruption case.

The tool exits **0** when authentic character entries were found and every one parsed,
**1** when entries were found but at least one failed to parse or could not be extracted,
and **2** when nothing authentic was found at all. CI treats those three outcomes
differently — see §11 — so a green run can never be mistaken for "real data verified" when
the client tree was simply absent. An entry that cannot be decompressed counts as a failed
candidate rather than disappearing from the tally; that specific bug was found while
corrupting a test archive, where a pack with one unreadable entry still reported
"parsed 1 of 1" and exited 0.

That end-to-end run caught a real classification bug worth recording: a skinned mesh `.x`
also begins with `Frame` objects, so "does a skeleton parser accept it?" is **not** a valid
skeleton test — the mesh reader must be tried first (presence of `SkinWeights` is the
discriminator).

---

## 11. Build & CI

* `RANMobile/CMakeLists.txt`: 8 new sources added to `RANMOBILE_SOURCES`, plus two host
  targets — `ranmobile_phase3j_tests` and `verify_ep7_character_assets`.
* `.github/workflows/build-ranmobile-android.yml`: the 8 sources joined the existing
  `-Wall -Wextra -Werror` host build; two new steps build/run the Phase 3J suite and the
  authentic-asset probe; their logs go into the job summary and the failure artifact.
* Every new file compiles clean with `g++ -std=c++17 -Wall -Wextra -Werror`.
* Android ARM64 NDK and `assembleDebug/assembleRelease/bundleRelease` remain **CI-only**
  (no NDK/SDK/Gradle in this sandbox — B1). Those jobs are unchanged in shape, so the new
  sources are compiled for arm64-v8a by the same CMake path.

---

## 12. What is NOT done (explicit non-claims)

1. **No authentic character has been rendered.** No `.chf/.cps/.x/.bin/.cfg` bytes and no
   `SkinObject.rcc`/`Animation.rcc` archives exist here (B2/B9).
2. **GPU skinning is not implemented** on this branch (§7).
3. **No `EP7SceneRenderer`/`MobileRenderer` draw path for characters.** PR #4's renderer is
   not on this branch; §8 defines the hooks instead.
4. **Texture binding for materials is not done.** `.cps` texture names are parsed and kept;
   decoding/uploading is Phase 3I/3K work.
5. **Attachments and effect payloads are not implemented.** `.cps` trace points are parsed;
   `DxEffAniData` effect bodies are skipped and flagged (`effectsPresent`).
6. **Animation subset only.** 0x0100/0x0101/0x0102 tracks and 0x0107 metadata; `.cfg`
   versions 0x0101–0x0106 and effect/`DxAniScale` tails are not parsed.
7. **No blending between upper/lower body** beyond playing the clip's own UPBODY tracks;
   `ANIMCONTNODE`'s cross-fade logic is not reproduced.
8. **No physical-device testing.** No APK was installed or run on hardware.
9. **No performance numbers on mobile.** See §7.
10. **`.x` binary (`bin`/`bzib`/`tzip`) files are rejected**, not decoded.
11. **Vertex declarations beyond the 12/24/32-byte layouts are not decoded** (reported as
    `Unknown`).
11b. **The `.rcc` reader does not handle zip64, multi-disk archives, AES encryption or any
    compression method other than stored/deflate.** If the shipped packs use any of those,
    extraction fails loudly rather than guessing.
12. **`DxSkinPieceContainer` 0x0104/0x0105 trace/effect ordering** is implemented from the
    unified read path; only the 0x0106/0x0107 weapon-slot difference is version-gated. Files
    written by older exporters remain unverified until the probe runs on real data.

---

## 13. Blockers added

* **B9** — authentic EP7 character assets unavailable in the sandbox; real-data verification
  is CI-only (`verify_ep7_character_assets`).
* **B10** — no 3D draw path on this branch, so GPU skinning cannot be integrated here
  without duplicating PR #4's renderer.

Details in `BLOCKERS.md` (Phase 3J update section).

---

## 14. Phase 3J B17 — Authentic Animation Runtime Integration & Skeleton Evaluation (`arena/00fd13f9-ep7`)

**Status: the authentic EP7 animation pipeline is connected end-to-end in the mobile runtime —
`.cfg` (SANIMCONINFO) → `.bin` (SAnimContainer) → SAnimation tracks → skeleton (DxSkeleton
equivalent) → DxSkinAniControl-equivalent state machine → bone/pose evaluation → character model →
renderer. Parsing success is now playback success: the chain evaluates authentic poses every frame.**

### 14.1 What was built (chain mapping)

| Authentic source | Mobile runtime | Evidence |
|---|---|---|
| `SANIMCONINFO::LoadFile_*` (.cfg) | `EP7AnimationReader::ParseInfo` (B16, unchanged) | B16 survey 3,903/3,903 |
| `SAnimContainer::Load` (.bin 0x0100–0x0104) | `EP7AnimationReader::ParseContainer` (B12–B15, unchanged) | byte-level reader untouched |
| `SAnimation::Load_*` + `ConvertMatToQuat` | `EP7AnimationReader::BindToSkeleton` + **NEW `ConvertMatToQuat`** | SAnimation.cpp:1326, 1459, 1511, 1568 |
| `DxBoneCollector` skeleton + `DxBoneTrans` SQT state | `EP7Skeleton` (bind pose, `MatrixToSQT` at bind, `ResetToRestPose` = `ResetBone` mirror) | DxBoneCollector.cpp `ResetBone`: flags=0/scale=1/mix=id/CUR=PREV=ORIG |
| `DxSkinAniControl` + `DxAniControl` + `DxAniControlCORE` | **NEW `EP7SkinAniControl.{h,cpp}`** (registry, SELECTANI/DEFAULTANI/SELECTSKILLANI semantics, FrameMove, UpdateTime, UpdateMix, DoShockMix) | DxSkinAniControl.cpp:155/523/584/742/753/771/843 |
| `SANIMCONINFO::UpdateTime` / `SAnimation::SetTime` | `EP7AnimationPlayer::ApplyToSkeleton(sk, bFirst, emBody)` (B17 rework) | SAnimation.cpp:480–1236, SAnimContainer::UpdateTime:1236 |
| `DxAniScaleMAIN::UpdateBoneScale` | `EP7AnimationPlayer::ApplyBoneScale` + control tail (`UpdateTime` NON-const applies bone scale exactly like the original's tail call) | DxAniBoneScale.cpp:5–48, DxSkinAniControl.cpp:194 |
| `SAnimation::SetMix` (latent raw-time bug) | `EP7AnimationPlayer::ApplyMix` + `UpdateMix` | SAnimation.cpp:387; documented, not silently "fixed" |
| `DxSkeleton::UpdateBones` | `EP7Skeleton::UpdateBones(worldRoot, blendWeight)` (BLENDING lerp/slerp PREV→CUR, ADD quat multiply, aniScale rows, firstChild/nextSibling recursion) | DxBoneCollector.cpp USE_ANI_QUATERNION path |
| `DxSkinMesh9_CPU` skinning | `BuildBonePalette` + `SkinVerticesCPU` (palette = offsetMatrix · matCombined) | DxSkinMesh9_CPU.cpp |
| `DxSkinChar::SetCharDataData` creation | `EP7CharacterScene::Spawn`: register every valid clip (`LoadAnimation`+`ClassifyAnimation`) → `DEFAULTANI` (nodes[0]) → animated immediately | DxSkinChar.cpp:242–254 |
| renderer 3D pass | `MobileRenderer::BeginScene3D/DrawSkinnedMesh/EndScene3D` (GLES2-compatible shaders, orbit camera, one draw per piece) | new; UI state preserved |

### 14.2 Resolved source question — `SAnimation::ConvertMatToQuat`

`SAnimation.cpp:1326–1356` (USE_ANI_QUATERNION): if the track has **no** quatPos keys and has matrix
keys, every matrix key is decomposed (`D3DXMatrixToSQT` + `CompressionQuaternion`, SAnimation.cpp:27)
into an `SQuatPosKey`, and **the matrix keys are then freed** (`m_cMatrixKeys = 0; SAFE_DELETE_ARRAY`)
— the Korean comment states the translation: "after converting, the matrices are deleted". Every
`SAnimation::Load_*` variant calls it right after reading a track (SAnimation.cpp:1459, 1511, 1568)
and the qualified-reader path does too (SAnimation.cpp:1135). The `!m_pQuatPosKeys` guard means a
track that carries BOTH quatPos and matrix keys keeps its matrix keys — and `SAnimation::SetTime`
dispatches matrix keys first (SAnimation.cpp:499), so those still animate through the direct-memcpy
`SetMatrix` branch (SAnimation.cpp:359, no weight involvement).

Mobile mirror: `EP7AnimationReader::ConvertMatToQuat` (called by the character loader after
`BindToSkeleton`) implements the same transform — matrix-only tracks become quatPos tracks and drop
their matrix keys; tracks with quatPos keys keep them. The byte-level reader stays untouched (this
is the runtime load transform, not a parser change). `ep7math::CompressionQuaternion` reproduces the
original pack (hemisphere canonicalization, `(DWORD)(v*0xffff)` truncation) with one documented
hardening: non-finite quaternion input compresses the identity and pre-truncation values clamp to
[0,1] so a malformed matrix key can never wrap the DWORD cast.

### 14.3 Playback semantics implemented (source-confirmed only)

* **Clock:** keys are unit-anikey (`kUnitAniKeyPerSec = 4800`); the control advances
  `m_fTime += elapsed * 4800` (DxSkinAniControl.cpp:742 comment parity via `EP7AnimationPlayer::Advance`).
* **Windows:** non-loop end > `m_dwETime − m_UNITTIME*2` freezes at the end window; loop wraps to
  `m_dwSTime`; `EMANI_ENDFREEZE` (flag 0x0001) pins the end; `wDivFrame` stop points recorded but
  gameplay always continues (documented `bContinue`).
* **Selection:** `SELECTANI(name|M,S)` → `SetAnimation`: same node + no reset → FALSE but the caller
  reports success (a re-select keeps playing); every successful selection sets `m_fWeight = 0`
  (DxSkinAniControl.cpp:516/562/598) and `FrameMove` ramps it +7/s clamped to 1 — the blend-in.
  `DEFAULTANI` = nodes[0]; missing `SELECTSKILLANI` target → `downBody.endAnim = TRUE` + fallback
  nodes[0] (DxSkinAniControl.cpp:588–596).
* **Creation auto-plays:** `DxSkinChar::SetCharDataData` (DxSkinChar.cpp:242) always selects
  (requested type → `AN_PLACID` → `DEFAULTANI`) — there is no "bind pose with nothing playing"
  state for a character that owns animations. `EP7CharacterScene::Spawn` mirrors this; the bind-pose
  path only exists for characters with zero loadable clips.
* **Switching:** the previous node's clock/clip/end state is preserved in a backup control
  (`sAniControlBACKUP`, DxSkinAniControl.cpp:160–164).
* **EMBODY routing:** `SAnimContainer::UpdateTime` (SAnimation.cpp:1236) `emBody` switch — DEFAULT
  applies upBody tracks then main tracks; UPBODY/DOWNBODY apply their own list. B-first flags follow
  `DxAniControlCORE::UpdateTime` (upBody+layer first with `bFirst=TRUE`... per control pair).
* **Upper/lower mixing:** `DoShockMix` selects the first mainType-11 clip (deterministic, documented)
  and mixes through `q_i · inv(q₀)` quaternion composition with the raw-`fGlobalTime` sampling the
  original uses (SAnimation.cpp:387 latent bug reproduced, not silently altered).

### 14.4 Engine-quality requirements (§7)

* Frame-rate-independent update: the engine clamps non-finite/≤0 dt to 0 and >0.25 s to 0.25 s
  (documented divergence: the original trusts `fElapsedTime`).
* No blocking loads on the render thread: assets load through the loader before `Spawn`; the render
  path only consumes evaluated buffers.
* No per-frame allocations: the pose path reuses instance-owned scratch buffers (palette/pos/norm)
  and geometric-growth VBO/EBO; measured alloc-stability over 2,000 frames (§14.8).
* Stable orientation & pause/resume: instance yaw/scale compose the world root each frame; the dt
  clamp freezes motion (not time-jumps) across suspensions.
* Extensibility: `EP7SkinAniControl` exposes SELECTANI/DEFAULTANI/SELECTSKILLANI/DoShockMix for the
  upcoming touch/UI phases; `CharacterPreviewOverlay` already compiles against the new scene API and
  stays unwired until those phases (B17 scope excludes Login/UI work).

### 14.5 Rendering with real assets (§6)

`RANMobileEngine::Render` (CharacterPreview state) submits only authentic, successfully parsed
geometry: per visible/posed instance, per loaded piece with faces, `DrawSkinnedMesh` receives the
piece's own skinned vertex window (`pieceVertexOffsets[slot]` base-pointer advance — the concatenated
buffer is indexed piece-locally exactly like the pieces' own `mesh.faces`). A piece that fails the
bounds check is skipped; geometry is never substituted or fabricated. Textures are not bound yet
(`textureId = 0`, default gray material) — `.dds/.tga` decode/upload remains future work; the
authentic subset/texture names are parsed and recorded.

### 14.6 B11–B16 compatibility (§5)

* Byte-level `.bin` reader and `.cfg` reader untouched (0x0113 stays UNSUPPORTED; 0x0200
  EMBYTECRYPT stays not-implemented; trailing bytes never reinterpreted).
* Suites re-run on the final code: phase3j **5,732 checks / 0 failures** (up from 5,686), verifier
  **3,757 / 0**, inspector **13,423 / 0 ×2**, all also under `ASan+UBSan+LSan`.
* CI-exact compile (`-std=c++17 -Wall -Wextra -Werror`) of the phase3j line and the full core test
  line both clean.

### 14.7 Test coverage (§8 categories)

`TestB17RuntimeSemantics` (new) + reworked `TestCharacterScene`: cfg/bin load (B11–B16, unchanged),
track→bone binding, hierarchy accumulation, bind pose (now via a no-clip resolution of the same
.chf), local/global matrices, frame progression (`CurrentTime`), pos/rot/scale keys, playback start,
re-select semantics, loop wrap vs non-loop end reporting (`animationEnded`), switching with weight
blend-in, matrix→quatPos conversion (time/scale/pos/quat round-trip + re-compression identity),
compression hardening (NaN → identity, hemisphere canonicalization), rest-pose restore contract
(no cumulative application), OOB bone indices, malformed refs, NaN/inf guards (B12–B15 suites),
B11–B16 regressions. All fixtures remain synthetic and labeled test-generated in-source.

### 14.8 Performance (§10, measured; synthetic scale, not device)

Bench methodology: 62-bone chain skeleton + 4 pieces × 3,000 vertices (12,000 total), 2,000 frames,
`-O2`, host sandbox (x86_64) — **synthetic**, recorded as engineering ceiling, NOT device numbers:

| Stage | Cost |
|---|---|
| Spawn (register clips + DEFAULTANI + first pose) | 0.055 ms |
| `scene.Update` (FrameMove + EvaluatePose) avg | 0.671 ms/frame |
| — `ResetToRestPose` | 0.0003 ms |
| — control `UpdateTime` (SQT pose write) | < 0.0001 ms |
| — control `UpdateMix` | < 0.0001 ms |
| — `skeleton.UpdateBones` (62 bones) | 0.0013 ms |
| — `BuildBonePalette` (62) | 0.0007 ms |
| — `SkinVerticesCPU` (3,000 verts) | 0.160 ms (~53 ns/vertex) |
| Scratch allocations across 2,000 frames | none (all capacities identical; output min=max=36,000 floats) |

Skinning dominates (~99.8%); the pose/state-machine pipeline is negligible at authentic skeleton
sizes. No optimization was applied (§10: correctness first); a typical EP7 character (4–8k vertices)
sits well under 0.5 ms/frame on this reference path.

### 14.9 Device status (§9)

**NOT TESTED ON DEVICE.** No Android device or emulator execution exists for B17 (sandbox blockers
B1). CI compile + unit/integration/sanitizer evidence is not device proof; the Android workflow's
`android-arm64` job compiles the runtime (`libranmobile-arm64-v8a`) but does not execute it.

### 14.10 Explicit non-claims / remaining work

1. GPU skinning: the renderer draws the CPU-evaluated reference path; the vertex-shader palette
   binding (B10 remainder) is still open.
2. Textures/materials: not bound (names recorded); lighting is a hardcoded directional in the
   character shader.
3. `SQUAD` interpolation is approximated by the equivalent slerp (documented in-source); the
   original's `D3DXQuaternionSquadSetup` tangents are not reproduced.
4. Effect payloads (`DxEffAniSingle` etc.) and trace-point-driven effects are not rendered.
5. Attachments (`Bip01_R/L_Hand matCombined`) are not exposed yet.
6. 0x0200 EMBYTECRYPT `.bin` and `SkinObject.rcc` pack gaps (`3,550` entries) remain unsupported
   (unchanged blockers; BLOCKERS.md updated).

### 14.11 Final report — the 14 B17 items

1. **Branch/PR state verified first** (stale local metadata repaired via plain `git reset origin/…`;
   main untouched; PR #7 kept OPEN+Draft; PR #5 untouched).
2. **Original-source investigation mapped before implementation** (`SAnimation.cpp`
   SetTime/SetMatrix/SetPosQuat/SetMix/ConvertMatToQuat, `DxSkinAniControl.cpp`,
   `DxBoneCollector.cpp`, `DxAniBoneScale.cpp`, `DxSkinChar.cpp`, `DxSkinMesh9_NORMAL/_CPU.cpp`) —
   table in §14.1 and ACI §18; no format or behavior guessed.
3. **Skeleton evaluation** — bind SQT, `ResetToRestPose` = `ResetBone` mirror (CUR=PREV=ORIG),
   `UpdateBones` recursion with BLENDING/ADD/aniScale; CI evidence check-run confirms
   `resetbone_cur_prev_to_orig: True`, `updatebones_blending_lerp/mix_add/scale_rows: True/True/True`.
4. **Track evaluation** — matrix-key direct write (SetMatrix memcpy parity, no weight), quatPos
   lower-bound snap with bFirst CUR/PREV, S/R/T fmod windows, EMBODY routing; CI evidence:
   `settime_matrix_first: True`, `settime_writes_matrot_only_for_matrix: True`.
5. **Pose evaluation** — local → propagate → global with world root; reset-to-rest before every
   evaluation (no cumulative application; suite asserts the contract).
6. **Playback/control** — SetAnimation backup clock, weight-0 blend-in (+7/s), loop wrap /
   end-window freeze / ENDFREEZE, same-node re-select semantics, skill-miss fallback, DoShockMix
   documented; creation auto-plays (DxSkinChar parity).
7. **Runtime matrix→quatPos conversion** — `ConvertMatToQuat` mirror with re-compression identity
   test; `CompressionQuaternion` bit-exact pack with NaN hardening.
8. **Rendering integration** — per-piece skinned submission with piece-local base pointer
   (bug from the first wiring found and fixed), no fabricated geometry, textures recorded not bound.
9. **Engine quality** — dt clamp, no blocking loads, zero per-frame reallocation (measured),
   stable orientation, extensible control API.
10. **Performance (§10)** — measured on synthetic scale (62 bones / 12,000 verts / 2,000 frames):
    update 0.671 ms/frame avg; skinning dominates (~53 ns/vertex); pose pipeline < 3 µs; no
    optimization applied (correctness first).
11. **Tests (§8)** — phase3j 5,732 / 0 (up from 5,686) incl. new B17 runtime-semantics section;
    verifier 3,757 / 0; inspector 13,423 / 0 ×2; all ASan+UBSan+LSan clean.
12. **B11–B16 compatibility** — byte readers untouched; 0x0113 UNSUPPORTED; 0x0200 not claimed;
    trailing bytes recorded; suites re-run green on the final code.
13. **Device status — NOT TESTED ON DEVICE** (no device/emulator; CI compile + suites ≠ device
    proof). Runtime evidence = suite-verified pose/skin/benchmark execution on host.
14. **CI evidence** — run [`46029586709`](https://github.com/famoh01/ran-online-ep7/actions/runs/46029586709):
    `Core C++17 tests` success, `Android ARM64 build` success, `b12-engine-anim-sources` success
    (24 verified LFS files), **`b17-runtime-semantics-evidence` success** (USE_ANI_QUATERNION=True;
    SetTime matrix branch writes matRot, SQT untouched=True; UpdateBones blending/mix/scale
    True/True/True; ResetBone CUR/PREV=ORIG True; CPU palette offset×bone True), `b12-authentic-anim-survey`
    success; `Authentic EP7 asset verification (diagnostic)` failure = the long-documented expected
    exit-1 (3,550 SkinObject.rcc pack gaps, unchanged). Investigation run
    [`46029586756`](https://github.com/famoh01/ran-online-ep7/actions/runs/46029586756) success.
    Completion claim basis: source verification (CI check-run facts) + integration tests + runtime
    execution — explicitly not device proof.

A workflow fix rode along (`97926ac`): the a4dba2e LFS list named `DxSkinMesh9_CPU.{h,cpp}`, which
the upstream backup repo does not contain (the CPU path lives in `DxSkinMesh9_NORMAL.*` there; the
`_CPU` split exists only in the GDrive engine drop) — substituted and re-verified all 24 paths via
the contents API.

---

## 15. Phase 3J B18 — Authentic GPU Skinning, Bone Palette & Character Rendering Integration (`arena/00fd13f9-ran-online-ep7`)

**Status: the verified B17 CPU skinning pipeline now has a complete GPU (GLES3) twin built to the
original's `D3DINDEXEDHLSLVS` convention, plus an authentic texture decode/bind path
(`.mtf` → DDS/TGA → GL). The engine render loop prefers the GPU path per piece with the B17 CPU
path as fallback. Every number in this section comes from authentic-source verification or from
explicitly labeled synthetic fixtures. NOT TESTED ON DEVICE.**

### 15.1 Sources inspected (original engine, verified before implementation)

| Question | Source file | Finding reproduced in B18 |
|---|---|---|
| GPU skinning method | `DxSkinMesh9.cpp:18` | `m_SkinningMethod = D3DINDEXEDHLSLVS` — the HLSL indexed-palette path is the shipped method, not the CPU path |
| Vertex declaration (HLSL) | `DxSkinMesh9_HLSL.cpp:95-106` | position + normal + uv + `FLOAT4` blend weights + `FLOAT4` blend indices; `CurNumBones = NumInfl − 1` |
| Palette composition | `DxSkinMesh9_HLSL.cpp:220-229` | `mWorldMatrixArray` = `offsetMatrix × bone.matCombined` per attribute group, uploaded via `SetMatrixArray` — same formula the CPU path uses (`DxSkinMesh9_CPU.cpp` / `BuildBonePalette`, B17) |
| Palette count semantics | `DxSkinMesh9_HLSL.cpp` | palette entries = skin-bone count per piece (attributes grouped per bone), not skeleton-total |
| Light direction | `DxSkinMesh9_HLSL.cpp` (`lhtDir`) | `normalize(0,1,-1)` — B18 changes BOTH mobile fragment shaders from the invented `(0.4,0.8,0.45)` to this authentic constant |
| Height bias | `DxSkinMesh9_HLSL.cpp` (`_42 += m_fHeight`) | documented; the mobile world pass already composes its own model matrix, so the bias is applied via the model matrix where a piece height exists (no separate shader constant added in B18) |
| Texture container split | `MemoryTexture.cpp:115-123` | `IsEncrypt` `.mtf` → `EncryptTexture` writes a 12-byte header (`TEXTURE_DDS`/`TEXTURE_TGA` type + per-byte `+0x03` then `^0x47` body transform) |
| Texture load call | `TextureManager.cpp:1148` (`LoadTexture`) | name-resolution order: exact file → stem-variant; mirrored by the B18 resolver ladder |
| Material → piece texture names | `DxSkinPieceContainer.cpp` `SMATERIAL_PIECE::LoadTexture` | `.cps` material texture name is the primary lookup key; `.x` `MeshMaterialList` subset names are the fallback (both implemented) |
| Piece indexing | `DxPiece.cpp:478`, `DxCharPart.cpp` `SetPart` | per-piece draw with piece-local vertex ranges (mobile parity: `pieceVertexOffsets[slot]`, B17) — B18 keeps per-subset `faceStart` draw ranges instead of re-uploading |

### 15.2 What was built

1. **GPU skin streams** (`EP7SkinnedMesh::BuildGpuSkinStreams`): influence construction is
   **identical to `SkinVerticesCPU`** (per-bone vertex-list scan order, cap = `MaxInfluencesPerVertex`
   = 4, out-of-range vertex references skipped, no-influence vertices keep zero weights → shader
   bind-passthrough). Output: `boneIndices` + `boneWeights` (4 floats/vertex).
2. **TGA decoder** (`EP7TextureDecoder::DecodeTGA`): uncompressed type 2 + RLE type 10 (24-bit BGR /
   32-bit BGRA), colour-mapped type 1 (24/32-bit map), greyscale type 3; origin bit `0x20` honoured;
   ≤4096×4096; anything else returns `invalid` — never guessed. `DecryptMTF` (B18 pre-existing)
   unchanged: 12-B header, per-byte `+0x03` then `^0x47`, `TEXTURE_DDS`/`TEXTURE_TGA` types only
   (per `MemoryTexture EncryptTexture`).
3. **GLES3 GPU skinning** (`MobileRenderer`): one 64-byte-stride interleaved buffer per piece
   (pos3+normal3+uv2+boneIdx4+boneWt4 — the authentic HLSL declaration), static VBO/EBO uploaded
   once per piece; per-frame data is ONLY the palette (offset×combined, row-major floats) written
   into a shared 1024x1 RGBA32F palette texture (B19 live-GLES correction - the emulator's GL
   translator demotes std140 UBO arrays to default uniform arrays, driver error "Vertex shader
   active uniforms exceed GL_MAX_VERTEX_UNIFORM_VECTORS (256)", so the B18 16 KB UBO could never
   link there; texelFetch x = bone*4+i reproduces the std140 mat4 layout bit-identically, so the
   skinning math is unchanged). The vertex shader
   reproduces `SkinVerticesCPU` semantics exactly: per-influence row-vector transform with w-divide,
   translation-dropped normals, zero-sum → bind passthrough, `|sum−1|>1e-4` → renormalize.
   `DrawGpuSkinnedMesh(handle, palette, count, textureId, color, alphaBlend, faceStart, faceCount)`
   draws a subset range by byte offset — no re-upload per subset. Invalid handle / palette null /
   palette > 256 / face range OOB → logged error + failure counter + `false`; a missing texture
   (`textureId == 0`) draws the flat material colour and is NOT a failure.
4. **Authentic texture binding** (`EP7CharacterScene`): resolver ladder exact name → `stem+".mtf"`
   → `name+".mtf"` (TextureManager order), lowercase cache, `.mtf` → `DecryptMTF` (undecryptable
   skipped, never fabricated), `DDS ` magic → `DecodeDDS`, `.mtf` fileType 1 → `DecodeTGA`, plain
   file → TGA-then-DDS. `.cps` material texture names are the primary keys, `.x`
   `MeshMaterialList` subset names the fallback; synthetic `material_` prefixed names are rejected.
   `FLAG_ALPHATEXTURE (0x0001)` drives alpha blending on the draw.
5. **Scene/engine integration**: `SetRenderer` wiring, lazy per-piece GPU upload
   (`EnsurePieceGpuHandle`, upload-once, scene-owned, released in `Clear`), per-instance
   `piecePalettes` snapshot written by `EvaluatePose` each frame, per-subset GPU draw in the
   engine render loop with the B17 CPU fallback kept per piece, `Clear()` releases GPU resources
   (safe to call twice).

### 15.3 Palette semantics (source-verified)

`palette[b] = offsetMatrix(b) × matCombined(b)` where `matCombined` comes from the B17 skeleton
evaluation (`UpdateBones`, world-root composed) — the same values the CPU path (`SkinVerticesCPU`)
consumes, so GPU and CPU results agree by construction. Row-major float storage fed into
column-major GLSL `mat4` preserves the original's row-vector (`v·M`) semantics for the shader's
`M·v`. Palette count = per-piece skin-bone count; pieces whose palette exceeds 256 are skipped
with a logged error — never approximated.

### 15.4 Tests (§11)

`test_phase3j`: **5,821 checks, 0 failures** (B17: 5,732; +89 B18 checks), `-Wall -Wextra -Werror`
plus **ASan+UBSan+LSan clean**. B18 coverage:
- TGA decode: origins (top-left/bottom-left), RLE, greyscale, invalid type → rejected; BGR order verified.
- GPU streams vs `SkinVerticesCPU` influence equivalence on the character fixture, including a
  manual 90° Head-bone pose through `BuildBonePalette` + the exact shader formula (posed vertex moves).
- Influence rules: 6 bones on 1 vertex → cap-4 order preserved, influences 5-6 dropped (stream is
  exactly 4 slots/vertex); OOB vertex refs skipped; invalid mesh → invalid streams.
- Headless renderer API: counters, unknown handle rejected, palette 257 rejected, face-range OOB
  rejected, release/double-release safe.
- Scene texture binding: in-test `.mtf` container (fileType 1 + encrypted TGA body), `DecryptMTF`
  round-trip, ladder resolution `face.dds` → `face.mtf`, lowercase cache, unknown subset → id 0,
  `Clear` releases + second `Clear` safe.
- Regression: verifier suite **3,757/0** + sanitizers; inspector suite **13,423/0 ×2** + sanitizers;
  full-engine compile (44 TUs) `-Wall -Wextra -Werror` clean; Android-path renderer/engine compile
  clean against a GLES3 stub (`-D__ANDROID__`).

### 15.5 Performance (§12 — synthetic fixture, host sandbox, NOT device)

62 bones / 3,000 vertices × 4 pieces, `-O2`, headless (no GL):
- `BuildGpuSkinStreams` (load-time, once per piece): 0.675 ms total ≈ **56 ns/vertex**.
- `BuildBonePalette` (per frame): 0.0008 ms (62 bones); palette payload **3,968 B/frame** of the
  64 KB texture (B19: palette transported as a 1024x1 RGBA32F texture).
- `DrawGpuSkinnedMesh` CPU-side validation (headless; on device replaced by the real GL draw):
  0.006 µs/draw, 20,000/20,000 accepted.
- Per-frame uploads: palette only (one `glBufferSubData` per draw ≤ 3,968 B) — no per-frame
  geometry allocation (B17 alloc-stability result carries over).

### 15.6 Authentic-data evidence path (§9)

`verify_ep7_character_assets` gained an additive B18 evidence block (the B11–B16 outcome model and
exit codes are untouched): shipped textures (`.mtf`/`.dds`) are classified (decrypt/decode rates),
pack entries are cached (160 MB cap), and the first valid `.chf` is loaded through the full
character loader to report, per piece: vertex/tri/skin-bone counts, `gpuStreams` validity, influenced
vertex counts, max influences, rest-pose palette size, and every material texture name resolved
through the real ladder with decoded dimensions. This runs in the `authentic-verification` CI job
against `external/ep7_release` when present.

### 15.7 Device status (§10)

**NOT TESTED ON DEVICE.** No Android device or emulator execution exists for B18 (sandbox blocker
B1). APK compile ≠ device proof. The GPU path requires an ES 3.0 context
(`GL_MAX_TEXTURE_SIZE ≥ 1024` is the requirement the texture-palette transport relies on — met by every ES 3.0 device, which guarantees ≥ 2048); GLES2-only devices
fall back to the B17 CPU path.

### 15.8 Explicit non-claims / documented divergences

- **NOT TESTED ON DEVICE** (§15.7). The GPU path is verified by unit tests + headless validation +
  Android-path compile against a GLES3 stub — not by a live GL context or device render.
- **TGA decode is a native implementation.** The original decodes inside `LoadImageData` with
  D3DX assistance; no D3DX exists here, so B18 implements the TGA spec surface the engine's
  textures use (types 1/2/3/10, 24/32 bpp). Unsupported TGA variants return invalid, never guessed.
- **DDS support is the pre-existing subset** (DXT1/DXT3/DXT5 + uncompressed RGBA8/BGRA8); other
  fourCCs return invalid and are reported, never approximated.
- **`subsetIndex ↔ EP7MaterialPiece` 1:1** is assumed from identical iteration order (source
  verified for both sides); a runtime count check remains TODO and is listed, not silently assumed.
- **`glGetUniformLocation` is queried per draw** (no program-uniform cache yet) — a known,
  documented inefficiency on the GPU path, not a correctness issue.
- The light-direction change `(0.4,0.8,0.45) → normalize(0,1,-1)` in both fragment shaders is
  intentional (authentic `lhtDir`); previously rendered screenshots from B17 are not comparable.
- **0x0113 `.cfg` remains UNSUPPORTED; `.bin` 0x0200 EMBYTECRYPT remains NOT implemented** — no
  parser was touched in B18; all B11–B16 suites re-run green (§15.4).
- **No fabricated assets**: every texture/vertex/bone number shown in evidence comes from
  authentic pack entries or from fixtures explicitly labeled synthetic.

### 15.9 Files changed (B18)

`RANMobile/Data/Importer/EP7SkinnedMesh.{h,cpp}` (GPU streams), `EP7TextureDecoder.{h,cpp}`
(DecodeTGA), `Engine/Renderer/MobileRenderer.{h,cpp}` (GPU path + authentic light), 
`Engine/Scene/EP7CharacterScene.{h,cpp}` (renderer/texture/palette glue + GPU release),
`Engine/Core/RANMobileEngine.cpp` (GPU-preferred render loop), `Tests/test_phase3j.cpp`
(+89 checks), `Tests/run_ep7_verifier_tests.sh` (source list for the tool's new includes),
`Tools/verify_ep7_character_assets.cpp` (B18 evidence block), `.github/workflows/
build-ranmobile-android.yml` (phase3j compile list + verifier tool compile list gained
`EP7TextureDecoder.cpp` and the scene/renderer sources).

---

## 16. Phase 3J B19 — Live GLES3 GPU Verification, Subset/Material Semantics & Resource Lifecycle (`arena/00fd13f9-ran-online-ep7`)

**Status: the B18 GPU path is now verified against a live GLES3 implementation (Android emulator,
SwiftShader, via a CI-booted AVD running the selftest harness inside the debug APK), the B18
subset→material assumption is resolved to the authentic AttribId semantics, the texture sampling
state is corrected to the original's trilinear-mip/WRAP configuration, and three GPU-resource
lifecycle defects are fixed. Physical-device testing remains NOT PERFORMED.**

### 16.1 Sources inspected (verified before changing anything)

| Question | Source file | Finding |
|---|---|---|
| Which material does a subset bind? | `DxSkinMesh9_CPU.cpp:584-587` | The draw loop iterates attribute groups and binds `pMaterialEx[pAttribTable[iAttrib].AttribId]` — the material index is the **AttribId of the attribute group**, not the group's position. Subsets may be out of order or share one material. |
| Texture sampling state | `DxSkinMesh9_HLSL.cpp:324-325` (and `:486-487`, `:880-881`) | Skin draws set `D3DSAMP_MINFILTER = D3DTEXF_LINEAR` **and** `D3DSAMP_MIPFILTER = D3DTEXF_LINEAR` — trilinear over mipmaps. |
| Mip generation | `TextureManager.cpp:212-217` | `D3DXCreateTextureFromFileInMemoryEx(..., MipLevels=D3DX_DEFAULT, ..., MipFilter=D3DX_FILTER_BOX|D3DX_FILTER_MIRROR, ...)` — a full mip chain is generated for every piece texture. |
| Texture addressing | D3D9 defaults (no `SetSamplerState` for address modes found) | `D3DTADDRESS_WRAP` (U/V) — not CLAMP. |
| `.cps` material array | `DxCharPart.cpp SetPart` | `SMATERIAL_PIECE[m_dwMaterialNum]` copies verbatim — the array is indexed by material id, confirming the AttribId-based lookup. |

### 16.2 What was fixed (each with tests)

1. **Subset→material semantics (B18 TODO resolved, §15.8 item closed).**
   `EP7CharacterScene::GetPieceTextureId` now resolves the texture through
   `mesh.subsets[subsetIndex].materialIndex` (the AttribId), falling back to the `.x`
   MeshMaterialList name. New `ValidatePieceSubsets(asset, slot)` checks every subset's face
   range against the index buffer and its materialIndex against both material arrays; the
   engine render loop calls it before the GPU path and falls back to the B17 CPU path on
   mismatch (logged, never guessed). Tests: out-of-order AttribIds resolve the correct `.cps`
   material; materialIndex outside both arrays is rejected with a precise detail string; face
   ranges outside the index buffer are rejected; out-of-range subset indices return 0.
2. **Authentic texture sampling (§6).** `UploadRGBA8Texture` now generates mipmaps and samples
   `GL_LINEAR_MIPMAP_LINEAR`/`GL_LINEAR` with `GL_REPEAT` wrap (GLES3 supports both on NPOT
   textures), matching the D3DX mip chain + HLSL sampler states + D3D9 WRAP default. The font
   atlas path (separate texture, LINEAR, no mips) is unchanged — documented intentionally.
3. **Stale GPU handles after context loss (§7).** `EP7CharacterScene::ReleaseGpuResources` now
   resets every registered piece's `gpuMeshHandle` (previously a rotation/restore re-entered the
   render loop with handles whose GL objects no longer exist). The Android bridge calls
   `RANMobileEngine::OnGpuContextLost()` on `APP_CMD_TERM_WINDOW` and `EnsureGpuContext()` on
   re-entry, so the renderer rebuilds programs/VAO/palette-texture and the scene re-uploads lazily.
4. **Headless GPU-registry leak (§7).** `MobileRenderer::Shutdown` freed the per-mesh
   `GpuMeshGlState` heap objects only under `__ANDROID__`; the cleanup now runs on every build
   via `MobileRenderer::ReleaseGlResources()` (headless builds delete the registry objects; GL
   builds delete the GL objects too). Renderer reuse after release is preserved.
5. **Alpha-flag indexing (§5 ride-along).** The engine loop read `info.materials[s]` for the
   per-subset alpha flag with the same wrong positional assumption; it now uses
   `subsets[s].materialIndex`.

### 16.3 Live GLES3 verification harness (`GpuSelfTest`)

`RANMobile/Engine/Renderer/GpuSelfTest.{h,cpp}` runs the following suite against the CURRENT GL
context (launched inside the app via the intent extra `selftest=gpu`; headless builds return
`SKIPPED` and never call GL):

1. **Environment report**: `GL_RENDERER`/`GL_VERSION`/`GL_SHADING_LANGUAGE_VERSION`,
   `MAX_UNIFORM_BLOCK_SIZE`, `MAX_TEXTURE_SIZE`.
2. **Shader compile/link**: `GpuSkinProgramReady()` — the programs were compiled and linked by
   the live driver inside `MobileRenderer::Initialize` (driver info-log diagnostics already
   wired); the palette-transport requirement (GL_MAX_TEXTURE_SIZE >= 1024) is asserted.
3. **CPU vs GPU skinning (transform feedback)**: the production vertex shader gains an inert
   `out vec3 vSkinnedPos` capture point, declared for transform feedback at link time
   (`vSkinnedPos`+`vNormal`, interleaved). The harness draws SYNTHETIC quad fixtures (bind pose
   and an animated 90° pose through `BuildBonePalette`) with `GL_RASTERIZER_DISCARD`, captures
   the skinned positions/normals from the SAME program that renders, and compares against
   `SkinVerticesCPU` (tolerances 5e-4 position / 5e-3 normal — float32 vs float32, MAD rounding).
   This validates bone transform order, offset×combined composition, weight blending, zero-weight
   passthrough and w-division on real GPU execution.
4. **Pixel-level rasterization**: flat-colour draw (also proves zero-weight passthrough),
   2×2-texture draw (bilinear centre + exact texel), alpha blending (0.5 red over blue
   background), and subset draw offsets (two subsets, red/green, per-half pixel checks) — all
   with an NDC view-projection override (`SetViewProjectionOverride`, diagnostic) and expected
   colours computed from the shader's own lighting constant (authentic `lhtDir` → lit=0.35 for
   n·l≤0). Tolerance ≤10/255.
5. **GL resource lifecycle**: invalid handle, palette-257 overflow and face-range OOB draws
   rejected; double release safe; registry counts correct.
6. **Authentic character render** (when EP7 files are bundled): loads one original `.chf`
   through `EP7CharacterLoader` + the scene (textures via the production ladder), evaluates the
   pose, validates subsets, uploads/draws every piece through the production GPU path, counts
   resolved/missing textures, renders 256×256 and writes a PPM screenshot + per-piece stats
   (subset/material counts, gpuHandle, palette size, validation verdict) into the verdict JSON.
   SYNTHETIC fixtures in sections 3-4 are labeled SYNTHETIC; section 6 uses only original
   client files — nothing is fabricated.

The verdict is written to `<internalDataPath>/gpu_selftest.json` (+ PPM) by
`native_bridge.cpp`, which then finishes the activity — the normal game path is untouched.

### 16.4 Android emulator verification (CI)

New workflow job `android-emulator-gpu-verification` (ubuntu-latest):

- downloads the original EP7 client release (`ran-ep7-full-v1`), runs
  `verify_ep7_character_assets --bundle-plan` to extract ONE authentic character's dependency
  set (chf + cps + x + textures + manifest) from the packs,
- copies the set into the APK assets (`assets/ep7auth/`), builds the debug APK (arm64-v8a +
  **x86_64** — the second ABI exists so the emulator can install it),
- boots an x86_64 AVD (API 30) with `-gpu swiftshader_indirect` under KVM — a real Android GLES3
  stack — installs the APK, starts the activity with `--es selftest gpu`,
- polls the verdict JSON via `run-as cat`, records renderer/version/verdict in the step summary,
  uploads JSON + PPM + logcat as the `android-emulator-gpu-selftest` artifact, and fails the job
  unless the harness verdict is PASS.

**Device classification (honest):** SwiftShader-in-emulator is a REAL GLES3 implementation and a
real Android runtime — but it is an EMULATOR with a software GPU. This is NOT a physical-device
test: **NOT TESTED ON DEVICE** remains true for B19. CI compile of an APK was never treated as
rendering proof; the emulator harness is.

### 16.5 Tests (§9/§11)

- `test_phase3j`: **5,846 checks, 0 failures** (B18: 5,821; +25 B19 checks) — subset semantics,
  validation rejections, handle reset, double release, headless Shutdown leak fix — all with
  ASan+UBSan+LSan clean (the Shutdown-leak fix is ASan-verified).
- Verifier suite **3,757/0** + sanitizers; inspector suite **13,423/0 ×2** + sanitizers.
- Full-engine compile: all 44 engine TUs `-Wall -Wextra -Werror` clean (headless).
- Android-path compile: `MobileRenderer.cpp` / `GpuSelfTest.cpp` / `RANMobileEngine.cpp` /
  `EP7CharacterScene.cpp` clean under `-D__ANDROID__` against a GLES3 stub (extended with
  transform-feedback/TF-buffer/`glReadPixels` declarations). `native_bridge.cpp` is compiled by
  the CI gradle/NDK build (headless cannot provide the full Android header set — documented).

### 16.6 Explicit non-claims / remaining work

- **NOT TESTED ON DEVICE.** The emulator harness is real GLES3 execution but software-rendered
  (SwiftShader); no physical device or emulator-with-hardware-GPU run exists.
- GPU-vs-CPU skinning equivalence is verified on small SYNTHETIC fixtures plus the authentic
  character render; the authentic character's CPU/GPU vertex-level comparison happens through
  the same transform-feedback path only when its fixtures are used (the authentic render itself
  is verified at pixel-count/statistics level, not per-vertex).
- The B18 "glGetUniformLocation per draw" inefficiency remains (documented, non-blocking).
- Context-loss handling covers window loss/restore via the bridge; EGL context-lost callbacks
  (`eglMakeCurrent` returning EGL_CONTEXT_LOST mid-frame) are not separately handled — the
  term/init cycle is the supported path.
- If the emulator job fails for infrastructure reasons (AVD download/boot), the job is red and
  the evidence gap is reported — it is never treated as a rendering PASS.

### 16.7 Files changed (B19)

`RANMobile/Engine/Renderer/GpuSelfTest.{h,cpp}` (new harness), `MobileRenderer.{h,cpp}`
(TF capture point, ReleaseGlResources, authentic sampling, VP override, source accessors),
`Engine/Scene/EP7CharacterScene.{h,cpp}` (subset validation, AttribId resolution, handle reset),
`Engine/Core/RANMobileEngine.{h,cpp}` (OnGpuContextLost/EnsureGpuContext),
`Android/native_bridge.cpp` (selftest mode + context-loss wiring),
`Android/app/build.gradle` (x86_64 ABI), `CMakeLists.txt` (harness in sources),
`Tests/test_phase3j.cpp` (+25 checks), `Tools/verify_ep7_character_assets.cpp` (--bundle-plan),
`.github/workflows/build-ranmobile-android.yml` (emulator job).

### 16.8 B19 CI validation — live GLES3 verdict PASS (2026-11-28)

The emulator job (x86_64 AVD, API 30, SwiftShader GLES 3.0) first required a CI fix chain —
each blocker isolated from the previous run's evidence, never guessed:

1. `834dc2a`/`d084815`/`380a267` — bundle-tool path: 160 MB cache cap exhausted before any
   `.chf` (metadata files now bypass it); output dir never created; OOB `asset->pieces[slot]`
   on unloaded assets (243 × "unsupported .chf version" left `pieces` empty) segfaulted the
   bundle walk; the B18 texture-extraction hook sat behind the `IsCharacterAssetName` filter
   so `.mtf`/`.dds` never reached the cache.
2. `826b291`/`a613084` — job budget: bundle fast path (`--bundle-plan` walks metadata +
   textures only) + single-ABI emulator build (`-PemulatorOnly`) + 75-min cap + bounded
   device-online poll (`adb wait-for-device` blocks forever on a dead emulator).
3. `c566790` — `ANDROID_AVD_HOME` pinned: newer `avdmanager` writes
   `~/.config/.android/avd`, the emulator searches `~/.android/avd`; "Unknown AVD name".
4. `9c53b43`-era — verdict transport: the app emits `GPUSELFTEST_JSON` logcat chunks; the
   snapshot is taken while adb is alive and reassembled host-side, so device-side adbd
   wedges (observed twice) cannot lose the verdict; one bounded relaunch recovery cycle.
5. `d7512d7`/`ebf34c3` — harness capture redesign: transform feedback is unusable on this
   stack (the TF-session draw fails with `GL_INVALID_OPERATION` because the production draw
   path's `glBindVertexArray` clobbers the TF binding; with `GL_RASTERIZER_DISCARD` the
   translator captures nothing). The §3 CPU-vs-GPU comparison now transports the production
   vertex shader's `vSkinnedPos`/`vNormal` through the raster path: production VS source
   verbatim + one appended `gl_PointSize`, FS encodes both as 16-bit hi/lo-split RGBA8
   (quantization 1.2e-4 / 3.1e-5, far below the 5e-4/5e-3 tolerances), palette sampled from
   the texture the production draw path uploaded. One `GL_POINTS` draw + one 1×1 readback
   per probed vertex (≤48 × 3 passes) into an offscreen RGBA8 FBO (ES 3.0 required-
   renderable; the on-screen surface silently quantized to 5/6-bit-class channels).
6. `cec1514` — the FBO probe hang: `DrawGpuSkinnedMesh` leaves texture unit 1 active with
   the palette bound; binding the probe's color target on the still-active unit replaced
   the palette binding AND attached that texture to the framebuffer, so the probe sampled
   its own FBO attachment — a texture feedback loop (undefined behavior; the translator
   hangs on it). The color target now binds on unit 0; unit 1 keeps the palette throughout.

**Final verdict (run `46127248810`, sha `cec1514`): PASS — all 14 tests.** GL renderer
"Android Emulator OpenGL ES Translator (Google SwiftShader)", GLES 3.0, GLSL ES 3.00,
`surfaceBits=[8,8,8,0]`. CPU-vs-GPU skinning: bind pose and animated pose both
`maxPosDiff=6.10e-05`, `maxNrmDiff=1.53e-05` (tol 5e-4/5e-3), 4/4 vertices probed, zero GL
errors, FBO complete. Rasterization: flat, bilinear-center, texel-red, alpha-blend and both
subset-offset pixel tests pass. Lifecycle rejection tests pass. `authentic_character_render`
and `authentic_texture_gpu` are verdict-neutral SKIPPED (§16.6 — documented B11 `.chf` gap;
nothing fabricated). §3/§4 evidence therefore comes from the production shader path on a
live GLES3 driver — an emulator with a software rasterizer, **not device proof**.

### 16.9 B20 — authentic `.chf` versions 0x0103+ implemented from CI-collected binary evidence (2026-11-28)

**Objective.** The mobile parser accepted only 0x0100–0x0102 while `SkinObject.rcc` ships
newer versions (B11: 243 × "unsupported .chf version"). Implement 0x0103+ support backed
only by authentic evidence — no guessed fields, no fabricated offsets.

**Evidence channel.** The sandbox cannot download the release pack (egress blocked), so the
authentic bytes were collected through CI annotations from the pack CI already downloads:

* Round 1 (run `46131351370`, sha `d364882`) — version census + payload-head dumps
  (offset 132, ≤188 B, ≤3 size-distinct examples per version):
  `chf-census: 0x0104 x33; 0x0106 x83; 0x0107 x81; 0x0108 x3; 0x0200 x43` (sum = 243 ✓).
  Heads prove the newer files carry **no leading scale**: the version DWORD is followed
  directly by the piece count, then standard `[BOOL present][len+bytes]` piece slots with
  null-terminated names (`m_face04.cps`, `CUN0503_m.cps`, …).
* Round 2 (run `46133690603`, sha `c5713f3`) — per-version smallest-example tails
  (last 160 B) + full dump of the smallest unsupported file. The dump confirmed the
  0x0200 class is a different container: its 128-B type block reads `default` (not
  `DxSkinCharData`) and the payload is high-entropy (encrypted), like the documented
  `EMBYTECRYPT` family.

**Format solved (byte-exact reconstruction, no guessing).** Four smallest examples —
`NPC_ark.chf` 0x0104/309 B, `event_Athena.chf` 0x0106/311 B, `npc_police_1.chf`
0x0107/307 B, `NPC_hangar.chf` 0x0108/319 B — were reconstructed in Python against one
layout with exact total-size matches and identical 160-B tail windows:

```
[132-B header][version u32]
[pieceCount u32 (per-file, observed 10..13)]  // no scale field since 0x0103
repeat: [BOOL present] + [len u32 + name bytes incl NUL]   // present only
[string skeleton] [animCount u32] repeat: [string anim]
trailer block, total size selects the variant:
  60 B (0x0104 sample): scale f32; 1;            h f32; r f32; 0; 1; vMax 3f; vMin 3f; 1; 0; 0
  64 B (0x0106/07):     scale f32; 0; 1;         h f32; r f32; 0; 1; vMax 3f; vMin 3f; 1; 0; 0
  72 B (0x0108 sample): scale f32; 0; 1;         h f32; r f32; 0; 1; vMax 3f; vMin 3f; 1; 0; + (1; 0;) + 1; 0; 0
```

Every sample satisfies `vMax == (radius, height, radius)` / `vMin == (-radius, 0, -radius)`
(the exporter writes the `SetRadiusHeight`-derived box) and the trailing `(1, 0, 0)` run maps
naturally to `hasWorldObj=1 / worldObj=0` in the 0x0102 field order. The parser fills the
fields positionally and sanity-checks the flag DWORDs as booleans only; it does not hard-assert
the geometry relation. Version policy: **0x0103 admitted** (no authentic sample; the sibling
`DxSkinPieceContainer` evolution 0x0102→0x0103 only made fixed arrays count-prefixed, which
SkinCharData already is at 0x0102 — the size-adaptive block covers the 0x0104-observed
variants), **0x0105 rejected** (shipped by no authentic pack, no known layout), **0x0200
rejected** (undeciphered container class, 43 files, trees/props — not player-character data).

**Implementation** (`RANMobile/Data/Importer/EP7CharacterFile.{h,cpp}`): `LoadVer0103Plus()`
+ version gate 0x0103..0x0108 minus 0x0105; `ReadFlag32()` boolean sanity; error string now
`unsupported .chf version (expected 0x0100..0x0104 or 0x0106..0x0108)`; census keying in the
verify tool unchanged (records `info.version` before the gate). The 0x0100–0x0102 paths are
byte-for-byte untouched.

**Tests** (phase3j suite, +1,875 checks): four byte-exact authentic fixtures (front rebuilt
from the parsed model, trailer block = authentic bytes, and each built file must end with the
authentic 160-B dump window — a `memcmp` round-trip proving front/block join, sizes and slot
placement `pieces[1]`/`pieces[8]`); field asserts with exact float bit patterns
(0x423347ae=44.82, 0x413b0a3d=11.69, 0x3f333333=0.7, 0x40a0f5c3=5.03, 0x3fc51eb8=1.54,
0x3f8ccccd=1.1, 0x4198147b=19.01, 0x40e00000=7, 0x419228f6=18.27); 0x0103 admitted /
0x0105+0x0109+0x0200 rejected with the census-matched error; pieceCount 14 rejected;
truncations (1–8 B cuts, mid-payload cuts, header-only); flag DWORD 2 rejected at all seven
flag positions; 3-anim/0-anim legality; a 0x0103 file laid out like 0x0102 fails cleanly
(scale bits read as piece count → PIECE_SIZE reject); three authentic fixtures added to the
truncation + single-byte corruption sweep.

**Gates:** phase3j **7,721/0** (was 5,846); ASan+UBSan+LSan clean (`detect_leaks=1`);
verify tool `-Werror` clean + end-to-end PASS on an authentic-layout 0x0104 file inside a
synthetic `.rcc`; Android-path `-D__ANDROID__ -Werror` clean (MobileRenderer, RANMobileEngine,
EP7CharacterScene, EP7CharacterFile). CI validation of the authentic packs: see §16.10.


### 16.10 B20 rounds 2b–6 — 0x0103+ trailer fully decoded; value-adaptive block; opaque extension sections (2026-11-27..28)

**Why the rounds continued.** §16.9 shipped the first parser from four reconstructed files, but
the authentic-assets CI run (`46138779929`, verdict 8260/3400) showed the remaining admitted-version
failures concentrate in larger files: after the flag-split fix (`82e74b8`), the payload-failure
census (run `46141210307`) counted 50 failing `.chf` in rem-size classes — 88 ×32, 184 ×2, 260 ×6,
290 ×1, 312 ×1, 454 ×2, 462 ×1, 478 ×1 — plus 4 × "trailer flag out of range" (rem = bytes left
after the reconstructed front).

**Evidence-channel hardening.** GitHub keeps only the first 10 warning annotations per step
(confirmed empirically when round 3's tails were silently dropped): the census moved onto one
aggregated line, evidence is emitted FIRST in budget order (fail census → per-class tails → full
dump → version census), and dumps are split across two `if: always()` steps with separate tags
(`chf-fail-dump`, `-b`, `-c`).

**Rounds 3b/4 dumps (runs `46143484256`, `46145908605`).** A round-3 dump was wasted on a 0x0200
file because version rejections were recorded as payload failures — fixed by filtering. The
smallest admitted-version failure, `mob_raikan_7.chf` (0x0107, 312 B), arrived complete: front
ends at 224, standard 64-B block, then a **24-byte extension section** `(0, 1, 256, 4, 0, 0)`.

**Round 5 per-class dumps (run `46147778604`).** Three complete files:

* `TrickBoomLow.chf` (0x0107, 511 B): the dword after the scale is an **arbitrary float
  (12.5f)**, not a flag — the four "flag out of range" rejects were mislabeled; both leading
  dwords are raw values.
* `GBOX02.chf` (0x0107, 414 B): standard 64-B block + **120-byte prop-geometry extension**
  (arbitrary floats, -1.0f, counts 84/2/4, dword 256).
* `m_cos_wing04.chf` (0x0104, 3955 B, first 1128 B): the 16-dword block shape attests **at
  0x0104** (the 60-B shape is not version-exclusive); its extension is **5 × 40-byte
  bone-attach records** `[3 floats][0 u32][16-byte bone name][strlen][15]`
  (`Bip01_L_Thigh`, `Bip01_R_Calf`, `Bip01_L_Calf`, …).

**Final format (round 6, commit `efde0c0`).**

```
block shapes (value-adaptive — both attest under 0x0104 AND 0x0106+):
  15 dwords (60 B): [scale f32][Y raw][h f32][r f32][0][1][vMax 3f][vMin 3f][t0 t1 more]
  16 dwords (64 B): as 60 B plus [X raw] between Y and h
  0x0108 (72 B):    as 64 B plus two flag DWORDs (1, 0) between vMin and the run
discrimination: X-slot dword 0/1 selects the 16-dword shape (a real height float never
has bit pattern 0/1); an exact 60-B block forces the 15-dword shape.  Y/X are never
validated (12.5f attests in Y).
[t0 t1 more]: t0/t1 map to the world-obj booleans (1, 0 attested); more=1 switches an
OPAQUE EXTENSION SECTION running to EOF: 8 B / 24 B minimal markers, 120 B prop
geometry, 200 B = 5 x 40 B bone-attach records.  Skipped positionally — the content
includes 256/4 DWORDs and arbitrary floats that must NOT be flag-validated — and
recorded (hasExtensionSections / extensionBytes).
```

`LoadVer0103Plus()` was rewritten around this; stray bytes after a more=0 block are a hard
error (the extension must be switched on by more=1).

**Tests** (+641 → **8,362**): TrickBoom raw-Y fixture (h=25.7 / r=3.88 exact bit patterns);
24-B ext fixture proving the 256/4 DWORDs are not flag-validated; 2 × 40-B bone-record fixture
(ext=80); 120-B GBOX ext fixture (ext=120, h=20 / r=7); 15-dword + ext fixture; trailing-bytes
reject; flag-garbage offsets updated for raw Y/X; the 0x0103 fixture moved to the 15-dword shape.

**Verify tool:** dump priority repointed to the post-decode unknown classes (`trailing bytes
after .chf trailer block`, `.chf world flag out of range`, `.chf bbox flag out of range`);
tails capped at 128 B (annotation message limit).

**Gates:** phase3j **8,362/0**; ASan+UBSan+LSan clean; verify tool `-Werror` clean; synthetic
negative fixtures reject with unchanged reasons; authentic-layout 0x0104-in-.rcc end-to-end
PASS. CI validation: TESTING_CHECKLIST §24.
