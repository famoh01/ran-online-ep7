# ARCHITECTURE.md — RAN ONLINE EP7 FAM HAWAII MOBILE

## 1. Design Principles
- **Zero Server/DB Changes**: Connects to existing EP7 Login, Session, Agent, and Field servers using the exact EP7 packet protocol (`NET_MSG_GENERIC`).
- **Zero Game Data Fabrication**: All Monsters, NPCs, Maps, Items, Skills, Quests, and Character Classes are loaded strictly from existing EP7 data files via `RANMobileDataConverter` and validated by `DataValidator`.
- **Server Display Name**: `FAM HAWAII` is used as the UI display name on the Login and Server Selection screens, decoupled from raw server IP/Port configuration.
- **Cross-Platform C++17 Core**: `RANMobile/` compiles both natively on Linux (for fast unit/integration testing) and on Android ARM64 via Android NDK + CMake + OpenGL ES 3.0.

## 2. Folder Structure
```
RANMobile/
├── Engine/
│   ├── Core/         (RANMobileEngine, Logger, Time, Memory)
│   ├── Renderer/     (OpenGL ES 3.0 / Portable Render Pipeline, Frustum Culling, LOD, GraphicsSettings)
│   ├── Resource/     (ResourceManager, Async Asset Streaming, Object Pooling)
│   ├── Input/        (InputManager, Multi-touch state)
│   ├── Touch/        (TouchSystem, Gesture recognizer)
│   ├── Camera/       (MobileCamera: ThirdPerson, Free, Target, Combat, Pinch Zoom)
│   ├── Animation/    (Skeletal & Keyframe Animation Adapter)
│   ├── Audio/        (AudioManager: OpenSL ES / Portable stub)
│   └── Network/      (POSIX Socket Transport, PacketReader, PacketWriter, SessionManager, Reconnect, Heartbeat)
├── Client/
│   ├── Login/        (AuthenticationManager, EP7 Login Adapter)
│   ├── Character/    (CharacterSelectManager, EP7 Character Data Adapter)
│   ├── Gameplay/     (GameWorldClient, PlayerController)
│   ├── Combat/       (CombatController, TargetManager)
│   ├── Inventory/    (InventoryManager, Potion/Item Use)
│   ├── Skills/       (SkillManager, Cooldowns)
│   ├── Quest/        (QuestManager)
│   └── Chat/         (ChatManager: Normal, Whisper, Party, Guild, System, UTF-8 Thai)
├── UI/
│   ├── Login/        (LoginScreen — "RAN ONLINE EP7" / "FAM HAWAII")
│   ├── HUD/          (MobileHUD: Top Status, MiniMap, Left Joystick, Right Combat, Bottom Bar)
│   ├── Buttons/      (MobileButton, MobileSkillButton, Cooldown & Hold support)
│   ├── Joystick/     (VirtualJoystick: Multi-touch, Deadzone, Walk/Run)
│   ├── Inventory/    (InventoryUI)
│   ├── Skills/       (SkillUI)
│   ├── Quest/        (QuestUI)
│   ├── Map/          (MapUI)
│   └── Settings/     (SettingsUI: Low/Medium/High/Ultra, 30/45/60/90 FPS, Layout)
├── Data/
│   ├── Importer/     (EP7 Raw Format Readers)
│   ├── Converter/    (RANMobileDataConverter)
│   ├── Validator/    (DataValidator & DATA VALIDATION REPORT)
│   └── Loader/       (GameDataLoader)
├── Android/          (CMakeLists.txt, AndroidManifest.xml, build.gradle, MainActivity.java, native_bridge.cpp)
├── Config/           (server_config.json, graphics_defaults.json)
├── Tools/            (converter_cli.cpp)
├── Tests/            (test_main.cpp — Comprehensive Unit & Integration Tests)
├── CMakeLists.txt
├── README.md
└── BUILD_ANDROID.md
```

---

## Phase 3J addition (2026-11-26) — Character / skeleton / animation layer

New directories and files. Nothing existing was moved or replaced.

```
RANMobile/
├── Data/Importer/
│   ├── EP7ByteFormat.h      (header-only) bounds-checked basestream + compbyte
│   ├── EP7RccArchive.{h,cpp}  .rcc pack reader (ZIP central dir, inflate, ZipCrypto)
│   ├── EP7Math.h            (header-only) Vec3/Quat/Mat4, D3DX row-vector order
│   ├── EP7XFile.{h,cpp}     .x text tokenizer (rejects binary encodings)
│   ├── EP7Skeleton.{h,cpp}  .x Frame hierarchy -> bones, pose accumulation
│   ├── EP7SkinPiece.{h,cpp} .cps 0x0104..0x0107
│   ├── EP7CharacterFile.{h,cpp}  .chf 0x0100..0x0102
│   ├── EP7SkinnedMesh.{h,cpp}    Mesh + SkinWeights + materials + CPU skinning
│   └── EP7Animation.{h,cpp}      .bin 0x0100..0x0102, .cfg 0x0107, player
├── Engine/Scene/
│   └── EP7CharacterScene.{h,cpp} loader, instances, pose eval, AABB culling, diagnostics
├── UI/Character/
│   └── CharacterPreviewOverlay.{h,cpp}  touch preview + diagnostics overlay
└── Tools/
    └── verify_ep7_character_assets.cpp  authentic-asset probe (CI)
```

### Layering rules this layer follows

0. **Pack-aware asset access.** Authentic character data lives in `SkinObject.rcc` /
   `Animation.rcc` (`GLOGIC::InitFile`), not on disk, so `EP7RccArchive` is the byte source
   for anything that reads `.chf/.cps/.x/.bin/.cfg`.
1. **Parsers never touch I/O.** Every reader takes bytes + a `bool compByteEncoded`, or an
   `ep7byte::Stream`. `EP7CharacterLoader` resolves names through a
   `bool (*)(void* userData, const std::string&, std::vector<uint8_t>&)` callback, so Android
   `AAssetManager`, a pack archive and test fixtures are interchangeable.
2. **One frustum implementation.** `EP7CharacterScene::CullInstances` feeds instance AABBs to
   the existing `MobileRenderer::IsBoxInFrustum` instead of duplicating view-distance logic.
3. **One camera.** The preview overlay drives the existing `MobileCamera` API
   (`OnDrag`/`OnPinch`/`SetMode`/`SetDistanceBounds`); there is no second camera type.
4. **One UI primitive set.** The overlay draws through `MobileRenderer::DrawUIQuad` /
   `DrawUIText`, the same calls `MobileHUD` uses, so no new GL state is introduced.
5. **No data aliasing.** Each character instance owns its own `EP7Skeleton`; the shared
   `EP7CharacterAsset` holds immutable parsed data only.
6. **No fabricated fallbacks.** A character instance cannot exist without a successfully
   parsed `.chf` + skeleton + at least one piece. There is no placeholder path.

### Draw-path contract for the 3D renderer (PR #4)

Per frame, after the world pass is prepared:

```
characterScene.Update(dt);                                  // animation + CPU pose
characterScene.CullInstances(camera, &renderer);            // sets inFrustum + bounds
for (instance : characterScene.Instances())
    if (instance->poseValid && instance->inFrustum)
        for (slot : pieces)
            draw(instance->skinnedPositions + pieceVertexOffsets[slot],
                 instance->skinnedNormals,
                 piece.mesh.faces,
                 piece.mesh.subsets[i].texture);
```

A GPU skinning path can replace the vertex upload by sending `BuildBonePalette()` output as a
palette uniform; the mesh already carries per-vertex blend data in its `SkinWeights`.

---

## 2026-11-26 — Verification boundaries in CI

The character pipeline now has an explicit *verification* layer with its own outcome model,
kept separate from the runtime path:

```
external/ep7_release/client_original/         (git-ignored, ~2.5 GB, release asset)
        │
        ├── SkinObject.rcc ─┐
        └── Animation.rcc ──┤   EP7RccArchive (ZIP + deflate + ZipCrypto "tkqwlf" + CRC-32)
                            ▼
              verify_ep7_character_assets      ← tool, not runtime code
                            │
             ┌──────────────┼───────────────┐
          PASS           UNSUPPORTED      FAIL
        (parsed)      (no definition       (reader
                       in this repo)       defect)
             │              │               │
          exit 0         exit 2          exit 1
```

Rules this layer enforces:

1. **`UNSUPPORTED` is a first-class outcome, not an error and not a pass.** It is produced
   only when the basestream type really is `SANIMCONINFO` *and* the version is outside
   `0x0101`–`0x0107`. Anything else — wrong type, missing header, corrupt entry — is `FAIL`.
2. **Verification can never be mistaken for runtime support.** Nothing in the runtime path
   consults the probe; the probe only reads and reports.
3. **CI keeps the boundary.** `core-tests` and `android-arm64` need no client data; only the
   isolated `authentic-assets` job downloads it. A format coverage gap therefore cannot gate
   the build, and a build failure cannot be excused by one.

For the record of *why* `0x0108`–`0x0115` stay unimplemented, see `BLOCKERS.md` B11: the
matching constants in the engine belong to the landscape (`DxLandMan`) family, whose
`FILEMARK` is `"LAND.MAN"`, so reusing them for animation would fabricate a layout.

---

## Appendix — `.cps` trace-point record: `SVERTEXINFLU` (Phase 3J follow-up)

Clarifies the `.cps` layout note earlier in this document, which lists a trace as
"string name, then `SVERTEXINFLU::LoadFile`" but does not pin the nested record's version tag.

**`SVERTEXINFLU::VERSION` is `0x0101`.** It is defined identically in two translation units —
`enginelib/Meshs/DxSkinMeshContainer9.cpp:36` and `enginelib/Meshs/SMeshContainer.cpp:29` —
and is used symmetrically: `SaveFile` (`:59`) writes it as the record tag, `LoadFile` (`:94` /
`SMeshContainer.cpp:95`) compares the tag it reads against it. A reader that compares against a
different value silently takes the unknown-version branch for every trace.

On-disk record, exactly as produced by `SVERTEXINFLU::SaveFile` and consumed structurally by
`LoadFile` on a version match (little-endian, no padding):

```
DWORD   dwVer        == 0x0101 (SVERTEXINFLU::VERSION)
DWORD   dwSize       // body byte count — the writer over-states this, see below
DWORD   m_dwIndex
float   m_vVector[3]
float   m_vNormal[3]
DWORD   m_dwNumBone  // 0xFFFFFFFF (COL_ERR) means "no influence"; MAX_BONE is 6
DWORD   m_pBone[numBone]     // present only when 0 < numBone <= MAX_BONE
float   m_pWeights[numBone]  // present only when 0 < numBone <= MAX_BONE
```

Structural body sizes: `32` bytes for `numBone == 0` or `COL_ERR`, otherwise `32 + 8*numBone`
(40 / 48 / 64 bytes for `numBone` = 1 / 2 / 6).

**Caveat on `dwSize`.** `SaveFile` adds the bone and weight arrays twice — once in the initial
`dwSize` expression and once inside the `if (m_dwNumBone > 0 && m_dwNumBone != COL_ERR)` block —
so `dwSize` reports `32 + 16*numBone` while the record actually occupies `32 + 8*numBone`.
The engine never notices because `LoadFile` uses `dwSize` **only** on the unknown-version path.
A reader that matches the version must consume the body structurally, and one that does not
match must skip `dwSize` (mirroring the engine) rather than assuming the value is exact.

`RANMobile/Data/Importer/EP7SkinPiece.cpp` (`ReadVertexInfluence`) implements exactly this:
structural consume on a `0x0101` match, `Skip(blockSize)` otherwise. The nested block is then
followed by the effect list (`DWORD count`, each `DWORD TypeID` + effect payload) and
`m_dwFlag`, which is what makes a mis-consumed influence visible as a desync on the *next*
trace's name rather than at the influence itself.

---

## Appendix — `.cps` version branches and the legacy trace layouts

The `.cps` container is not one layout with a version tag; `DxSkinPiece::LoadPiece`
(`enginelib/Meshs/DxSkinPieceContainer.cpp`) is seven separate branches plus a terminal `else`
that raises *"Can't Load Skin object"* and returns `E_FAIL` (`:934-943`). A reader must
dispatch on the version **and** change its field set accordingly.

### Branch map

| Version | Branch | `m_dwMaterialNum` + materials | Trace section | `m_dwFlag` |
|---|---|---|---|---|
| 0x0107 | `:374` | yes | count + name + `SVERTEXINFLU` | read |
| 0x0106 | `:459` | yes | count + name + `SVERTEXINFLU` | read |
| 0x0105 | `:544` | yes | count + name + `SVERTEXINFLU` | read |
| 0x0104 | `:629` | yes | count + name + `SVERTEXINFLU` | **not read** |
| 0x0103 | `:712` | **no** | count + name + `SVERTEXINFLU` | **not read** |
| 0x0102 | `:783` | **no** | **10 fixed slots**, no names, no count | **not read** |
| 0x0101 | `:859` | **no** | **8 fixed slots**, no names, no count | **not read** |

`m_emWeaponWhereBack` is stored on disk only for 0x0106/0x0107 (`:398-399`, `:483-484`). Every
other branch assigns it `EMPEACE_WEAPON_RSLOT` without reading (`:626`, `:709`, `:780`, `:857`,
`:931`).

### The fixed-slot trace layout (0x0101 / 0x0102)

No count, no name strings — just N consecutive `SVERTEXINFLU` records:

```
0x0101:  8 x SVERTEXINFLU      // for ( int i=0; i<8;  i++ ) ...  :891
0x0102: 10 x SVERTEXINFLU      // for ( int i=0; i<10; i++ ) ...  :815
DWORD  effectCount
```

Only the first four slots can produce a trace point. The branches test
`sTraceArray[k].m_dwIndex != COL_ERR` for k = 0..3 and name the survivors from the compile-time
table in `DxSkinPieceContainer.h:22-25`:

```c
#define STRACE_IMAGE01 ("IMAGE01")   // slot 0
#define STRACE_IMAGE02 ("IMAGE02")   // slot 1
#define STRACE_IMAGE11 ("IMAGE11")   // slot 2
#define STRACE_IMAGE12 ("IMAGE12")   // slot 3
```

`STRACOR::STRACOR` copies that name with `StringCchCopy ( m_szName, STRACE_NSIZE = 32, ... )`,
so it is truncated to 31 characters — harmless for these eight-character literals. Slots 4..7
(0x0101) and 4..9 (0x0102) are read for alignment and discarded.

Consequence for a reader: those names are **not file data**. The mobile reader exposes them
but marks the trace `syntheticName = true`, so no caller can mistake `IMAGE01` for a
dependency name recovered from the asset.

### Why `m_dwFlag` is absent below 0x0105

The constructor initialises `m_dwFlag(0L)` (`:192`). Branches before 0x0105 never read it from
the file; they only accumulate `m_dwFlag |= pEffChar->GetFlag()` per successfully loaded effect
(`:703`, `:774`, `:850`, `:925`). A reader that insists on a trailing DWORD for 0x0104 or older
consumes the next record's bytes — or, at end of file, reports a complete file as
`truncated piece flag`. That was the B15 defect: 88 authentic 0x0104 files were being rejected
for a field their version does not store.

## Appendix — competing `DxSkinPiece::VERSION` definition

`enginelib/Meshs/ReplaceContainer.cpp:19` defines `DxSkinPiece::VERSION = 0x00000100` and a
`LoadPiece` that only accepts that one version. It is **not** the `.cps` loader: the file
defines the `DxReplaceContainer` family used by `DxLandMan`/`DxPieceContainer` (via
`DxReplaceContainer.h`), and `enginelib.vcxproj:1526` compiles `Meshs\DxSkinPieceContainer.cpp`
into the character path. The authoritative `.cps` loader is `DxSkinPieceContainer.cpp`, with
`VERSION = 0x00000107` (`:158`) and the seven branches above. Do not take a version bound from
`ReplaceContainer.cpp`.

---

## Appendix — `SVERTEXINFLU` `dwSize` is unreliable (B14), and why the reader ignores it

The `dwSize` DWORD that follows the `SVERTEXINFLU` tag is **not** a trustworthy body length. It
is hand-computed by `SVERTEXINFLU::SaveFile`
(`enginelib/Meshs/DxSkinMeshContainer9.cpp:59-92`; byte-identical copy at
`enginelib/Meshs/SMeshContainer.cpp:52-85`) and the computation counts the bone and weight
arrays twice while the body writes them once.

| `m_dwNumBone` | declared `dwSize` | actual body | delta |
|---|---|---|---|
| 0 | `0x20` | `0x20` | 0 |
| 1..6 | `0x20 + 16n` | `0x20 + 8n` | `+8n` (over) |
| `COL_ERR` (`0xFFFFFFFF`) | `0x18` (wrapped) | `0x20` | `−8` (under) |

Body layout actually written after the `dwSize` field:

```
DWORD  m_dwIndex              //  4
float  m_vVector[3]           // 12
float  m_vNormal[3]           // 12
DWORD  m_dwNumBone            //  4       = 32 bytes so far
DWORD  m_pBone[numBone]       // 4n      only when 0 < numBone <= MAX_BONE(6)
float  m_pWeights[numBone]    // 4n      only when 0 < numBone <= MAX_BONE(6)
```

So the body is `32` bytes for `numBone == 0` and for `COL_ERR`, and `32 + 8n` otherwise.

### The rule the reader follows

Mirroring `SVERTEXINFLU::LoadFile` (`:94-127`):

- **tag matches `SVERTEXINFLU::VERSION` (0x0101)** → consume the body **structurally**; never
  consult `dwSize`. A structural read cannot be displaced by a wrong size.
- **tag does not match** → skip exactly `dwSize` bytes, because that is what `LoadFile` does
  (`SetOffSet(GetfTell() + dwSize)`, `:126`). This is the *only* place the field is
  authoritative, incorrect or not.

Do **not** add a "consistency check" comparing `dwSize` against the bytes consumed. The writer
is wrong, not the reader: such a check rejects valid data at `numBone == 0` and at `COL_ERR`,
and at every other count alike. `TestSkinPieceInfluenceSizeAccounting` fails 31 checks if this
check is introduced.

### Contrast with `SMATERIAL_PIECE`, whose size *is* reliable

`SMATERIAL_PIECE::SaveFile` (`DxSkinPieceContainer.cpp:109-122`) brackets its body with
`BeginBlock`/`EndBlock`. `EndBlock` (`enginelib/Common/SerialFile.cpp:189-208`) back-patches the
size from the measured positions:

```cpp
long nEndPos = GetfTell();
DWORD dwTotalSize = DWORD ( ( nEndPos - m_nMarkPos[m_dwBlockMarkPOS] ) - sizeof(DWORD) );
```

Measured, therefore exact. That is why the material block can be skipped safely by its size and
the influence record cannot. When reading a block, establish which of the two conventions
applies — measured (`BeginBlock`/`EndBlock`) or predicted (hand-summed). Only the former may be
trusted for a skip.

---

## Appendix — basestream file-type header: why every EP7 file starts with ASCII

This corrects an assumption embedded in the verification tool (see `BLOCKERS.md` B11).

### The header is a 128-byte string, not a numeric magic

```cpp
// enginelib/Common/SerialFile.cpp:57-66
void CSerialFile::WriteFileType ()
{
    fwrite ( &m_szFileType, sizeof(char), FILETYPESIZE, m_FileStream );  // 128 bytes
    fwrite ( &m_FileID, sizeof(DWORD), 1, m_FileStream );                // version
    m_DefaultOffSet = ftell ( m_FileStream );                            // = 132
}
```

`FILETYPESIZE` is `128` (`enginelib/Common/basestream.h:16`). `SetFileType`
(`SerialFile.cpp:34-38`) copies a caller-supplied string into that field, so the first bytes of
every container are that string's first bytes:

| Type string | Set at | First 4 bytes on disk |
|---|---|---|
| `"AnimContainer"` | `SAnimation.cpp:22`, used `:1583` | `A n i m` |
| `"SANIMCONINFO"` | `DxSkinAniManSaveLoad.cpp:188` | `S A N I` |
| `"LAND.MAN"` | `DxLandManSaveLoad.cpp:31` | `L A N D` |
| `"EFF_PROPGROUP"` | `DxEffSinglePropGMan.cpp:596` | `E F F _` |

So an EP7 basestream **always** begins with printable ASCII. Observed layout:

```
0x00 .. 0x7F : file-type string, NUL-padded to 128 bytes   (FILETYPESIZE)
0x80 .. 0x83 : version / file id DWORD                     (m_FileID)
0x84 ..      : payload                                     (GetfTell() == 0 here)
```

This is why `EP7ByteFormat.h` models `kFileTypeSize = 128` and
`kHeaderSize = 132`, and why `GetfTell()`/`SetOffSet()` are relative to offset 132.

### The type string is usually *not* validated

`SAnimContainer::LoadAnimFromFile` (`SAnimation.cpp:1754-1776`) reads the type into
`szFileType` and then **ignores it** — dispatch is on `dwVersion` alone. A `strcmp` against the
type appears only in the landscape loader (`DxLandManSaveLoad.cpp:733`). Consequently a file with
a blanked or foreign type string but a valid version still loads in the original engine.

Two consequences for this codebase:

1. **Tests that zero-fill the header cannot detect a type-string problem.** All 8 basestream
   fixtures in `test_phase3j.cpp` do exactly that. A fixture carrying a real `"AnimContainer"`
   string is needed to exercise the header path faithfully.
2. **Do not infer "foreign format" from printable leading bytes.** It is the normal case. To
   decide whether a buffer is a known container, read the 128-byte type plus the version at
   `0x80` and attempt the parse — not the other way round.

The `.cfg` reader (`ParseInfo`) *does* validate the family and reports *"not a SANIMCONINFO
file"* for a genuine mismatch; that behaviour is correct and should be the model for `.bin`.


> Preserved-branch B11 evidence: the following append was copied from
> commits 80f932b/cf4e03c on arena/1491d572-ran-online-ep7. Its CI results
> describe that source branch, NOT a newly integrated PR #5 head. Target CI
> must be rerun after integration. No Phase 3B–3I implementation is copied.


## 2026-11-27 — B11 verifier-only classification boundary

The tool's printable-ASCII rejection is removed. Source references:
`SAnimation.cpp:22,1573-1735`, `basestream.h:16`, `SerialFile.cpp:34-65`.
SAnimContainer is a 128-byte NUL-terminated `AnimContainer` type field followed
by the version DWORD at byte 128, then payload at byte 132. Trailing type-field
padding need not be zero. The original loader dispatches on the version; the
mobile production parser is unchanged and does not enforce type identity.

The verifier now adds explicit type/header validation before calling that parser,
without a headerless fallback. PASS requires successful parsing. Only the parser's
explicit unsupported-container-version error yields UNSUPPORTED. Truncated header,
unrecognized type, and recognized-header parser rejection remain FAIL with distinct
diagnostics. Unrecognized type is not proof of foreign data versus corruption;
parser rejection is not proof of malformed data versus a reader limitation.
`.bin` counters are split by diagnostic bucket and pack/loose scope. Other format
classifiers and all runtime code are untouched. This tool is not a complete
semantic validator: successful parsing does not prove animation playback/rendering.


### 2026-11-27 — B11 post-correction CI evidence (final results)

Implementation commit `80f932b287ebb2c5144014b8eb59f897efe1fc74` was tested in
[CI run 45937494300](https://github.com/famoh01/ran-online-ep7/actions/runs/45937494300).
Core C++ tests with downloaded release data PASSED; Phase 3J PASSED; verifier
590/0 normal and 590/0 ASan+UBSan+integrated LSan PASSED. Android ARM64 native
library and debug/release APK + release AAB PASSED. The authentic verifier ran
against the extracted release and correctly remained FAIL (exit 1), not a green
coverage claim. Full log artifact: `phase3j-authentic-assets-log`; core/sanitizer
logs and Android outputs are also uploaded.

#### Exact comparison: same pack baseline, separate loose .bin result

| Scope / metric | Before | After | Delta |
|---|---:|---:|---:|
| Pack parsed | 6,479 | 6,479 | 0 |
| Pack failed | 3,550 | 3,550 | 0 |
| Pack unsupported | 1,631 | 1,631 | 0 |
| Loose .bin parsed | 0 | 982 | +982 |
| Loose .bin failed (parser-rejected after recognized header) | 0 | 1,209 | +1,209 |
| Loose .bin unsupported | 6,228 (incorrect printable gate) | 4,037 (explicit version rejection) | -2,191 |
| Loose .bin total | 6,228 | 6,228 | 0 |

After correction: **0 invalid-header**, **0 unrecognized-type** among these 6,228.
All have the recognized header, but only **982 parse successfully**. The 4,037
unsupported outer container versions are **0x0103: 1,533**, **0x0104: 2,495**,
**0x0200: 9**. All 1,209 recognized-header parser rejections report the existing
parser diagnostic `unsupported animation version` (track-level, NOT the explicit
outer-container-version error). They remain FAIL/parser-rejected; no assertion is
made that the original files are corrupt or that every such failure is a parser
bug. No parser implementation is changed to force them green.

**Critical scope finding:** every .bin candidate in this release scan is a loose
file under `data/animation`; **zero .bin entries were encountered inside packs**.
That is why the pack verdict/counts remain unchanged. This does not prove no pack
in any EP7 distribution contains .bin files; it describes this run only. The
historical 6,228 figure must not be presented as a pack-only count or as 6,228
valid animations. Header recognition/parse success also does not prove gameplay
or rendered animation correctness. The old binUnsupported summary counter was
not incremented; the before figure here is the actual emitted UNSUPPORTED outcome
count, not that erroneous zero-valued counter.

Remaining blockers: unsupported versions above; 1,209 parser rejections requiring
separate investigation; pre-existing pack .chf/.cps/.abf/.cfg coverage gaps; and
PR #5 head belonging to another session branch. The verifier correction is done
on `arena/1491d572-ran-online-ep7`; PR #5 remains OPEN/DRAFT, unmerged, with its
head unchanged. This task does not authorize changing runtime/parser behavior.
