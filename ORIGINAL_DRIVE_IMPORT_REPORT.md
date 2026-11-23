# ORIGINAL_DRIVE_IMPORT_REPORT.md — RAN ONLINE EP7 (FAM HAWAII MOBILE)

- **Google Drive Folder**: `https://drive.google.com/drive/folders/1Y3VOeAeBzSbaB_o9oWLN16qD29b8Ro1m`

## 1. Files Found in Google Drive
| Filename | File Size (Bytes) | File Type | MD5 Checksum | Status |
|---|---|---|---|---|
| `RAN-EP7-Client-TH.zip.part00` | 1572864000 | `application/x-zip` | `6d59a39e7c5353525230ed5815fb93bf` | `FOUND` / `CLIENT_IMPORT_BLOCKED_BY_CONNECTOR_LIMIT` (>100 MB limit) |
| `RAN-EP7-Client-TH.zip.part01` | 1010562339 | `application/octet-stream` | `de46502fbb30d2e72a78f0de3d3a9383` | `FOUND` / `CLIENT_IMPORT_BLOCKED_BY_CONNECTOR_LIMIT` (>100 MB limit) |
| `RAN-EP7-ServerDBTools.zip` | 49791515 | `application/zip` | `a5af7f8903767cde919c402d8b069639` | `FOUND` / `SERVER_DB_IMPORT_BLOCKED` (retry failed) |
| `RAN-EP7-ThaiPack-FamHawaii.zip` | 1359185 | `application/zip` | `c738eb17ffbf5dfc504cb548bd2105a1` | `FOUND` / `IMPORTED` / `VALIDATED` |

## 2. Files Imported, Extracted & Validated
- `external/ep7_release/original/RAN-EP7-ThaiPack-FamHawaii.zip`
  - **MD5 Verified**: `c738eb17ffbf5dfc504cb548bd2105a1` (exact match)
  - **Extracted to**: `external/ep7_release/thaipack_original/` (preserved)
  - **Total Files Extracted**: `1,046` files (`309` `.ntk`, `730` `.qst`, Thai UI & text files)

## 3. Blocked Data & Technical Reasons
- `CLIENT_IMPORT_BLOCKED_BY_CONNECTOR_LIMIT`: `RAN-EP7-Client-TH.zip.part00` (1.57 GB) and `part01` (1.01 GB) exceed the 100 MB (104,857,600 bytes) cap of `google_drive:download_file`, and no chunked/streaming download tool is available in the connector.
- `SERVER_DB_IMPORT_BLOCKED`: `RAN-EP7-ServerDBTools.zip` (49.79 MB) failed on safe retry (`Could not retrieve the file from the connector`).

## 4. Import Status & Remaining Work
- **Import Status**: `PARTIAL` (`ThaiPack` = `IMPORTED` & `VALIDATED`; `Client-TH` = `BLOCKED`; `ServerDBTools` = `BLOCKED`)
- **Remaining Work**: Provide/download `RAN-EP7-Client-TH.zip.part00/01` and `RAN-EP7-ServerDBTools.zip` locally, extract into `external/ep7_release/client_original/` and `server_original/`, and run `DataValidator`.
