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

`RANMobile/Tests/test_phase3j.cpp` — **216 checks, 0 failures**, and identical results under
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
