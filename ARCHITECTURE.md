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
