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
