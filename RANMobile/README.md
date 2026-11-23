# RAN ONLINE EP7 — FAM HAWAII MOBILE

Mobile Client and Mobile Engine (`RANMobileEngine`) for **RAN Online EP7** (`FAM HAWAII`), built in C++17 for Android ARM64 (OpenGL ES 3.0) using existing EP7 game data and protocol structures.

## Key Guarantees
- **Server Display Name**: `FAM HAWAII`
- **No New Server / Database**: Connects to existing EP7 servers.
- **No Fabricated Game Data**: Maps, Monsters, NPCs, Items, Skills, and Quests are loaded strictly from EP7 data files and validated by `DataValidator`.
