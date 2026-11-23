# ORIGINAL_SOURCE_RECOVERY_REPORT.md — RAN EP7 (FAM HAWAII MOBILE)

1. **Release Assets**: 4 assets `FOUND` in GitHub Release `ran-ep7-full-v1` (via `gh release view`).
2. **Download URLs**: Listed in `ORIGINAL_RELEASE_MANIFEST.md` and `original_release_assets.json`.
3. **File Sizes**: `1572864000`, `1010562339`, `49791515`, `1359185` bytes.
4. **SHA256**: `NOT AVAILABLE` (download blocked before file bytes could be hashed).
5. **Download Status**: `BLOCKED` — `release-assets.githubusercontent.com` is outside the sandbox outbound allowlist (`EOF`).
6. **Extraction Status**: `BLOCKED` (pending manual upload of original archives into `external/ep7_release/original/`).
7. **Client / Server / Database / Thai Pack / Game Data**: `FOUND` in C++ source (`RanClientLib/`, `enginelib/`, `netclientLib/`, `LoginServer/`, `AgentServer/`, `FieldServer/`); binary archives `BLOCKED`.
8. **Asset Integration Status**: `READY` (`EP7AssetImporter`, `RANMobileDataConverter`, `DataValidator`, `GameDataLoader` implemented in `RANMobile/Data/`).
9. **Blocked Files**: All 4 release zip/part files (`RAN-EP7-Client-TH.zip.part00`, `part01`, `RAN-EP7-ServerDBTools.zip`, `RAN-EP7-ThaiPack-FamHawaii.zip`).
10. **Remaining Work**: User must upload or place the original release files into `external/ep7_release/original/` on a machine with `release-assets.githubusercontent.com` access, then run `DataValidator` and `RANMobileDataConverter`.
