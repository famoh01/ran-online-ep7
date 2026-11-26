# Phase 3G — Authentic Git LFS Asset Retrieval (2026-11-26)

## Scope and outcome

This phase retrieved **authentic bytes**, not pointers, for a focused sample from the authorized backup repository
`https://github.com/matheomedrana/Ran-Online-EP7-Complete-Files.git` at commit
`f661fbf89612cc77f95a7c9b935ff06b22bc55e3`. All 24 retrieved objects were verified byte-for-byte against the
Git LFS pointer OID and declared size. No recovered `.exe`, `.dll` or `.ocx` was executed. No game logic,
protocol, authentication, rendering path or gameplay data was changed. PR #3 remains **OPEN/DRAFT**; it was not
merged, closed, overwritten or force-pushed.

## Method (authorized standard Git LFS / GitHub only)

Local environment: Git `2.39.5`; `git-lfs` absent; `sudo apt-get update` could not reach the Debian mirrors;
the GitHub LFS batch endpoint returned HTTP 403 “Resource not accessible by integration” with the sandbox token;
`media.githubusercontent.com` and GitHub artifact/log blob hosts failed TLS with EOF/SYSCALL; SSH to
`git@github.com` was reset. Those methods were recorded as limitations and not repeatedly retried.

The successful method used the standard Git LFS client on a GitHub-hosted Ubuntu runner:

1. `sudo apt-get install -y git-lfs` on the runner, `git lfs version`.
2. `GIT_LFS_SKIP_SMUDGE=1 git clone --depth 1` of the backup repository.
3. Because the backup repository’s tracked `.gitattributes` is itself an LFS pointer, local
   `.git/info/attributes` entries were created **only** for the explicitly requested paths.
4. `git lfs pull --include=<explicit paths> --exclude=""`.
5. Each working-tree file was checked for absence of the LFS pointer header, then SHA-256 and byte size were
   compared with `git show HEAD:<path>`.
6. Verified bytes were published as **dangling Git blobs** in `famoh01/ran-online-ep7` through the GitHub
   `git/blobs` REST API and transferred back through `api.github.com`; no commit, branch, tag or release was
   created for the blobs.

Workflow: `.github/workflows/phase3g_focused_lfs_fetch.yml`. Successful retrieval runs:
`45877365090`, `45877502395`, `45877596073`, `45877736996`, `45877854016`, `45878096559`,
`45878225553`, `45878336403`, `45878536743`. Four earlier push-triggered debug runs failed and are recorded
only as diagnostics (`45876890287`, `45877006244`, `45877078973`, `45877240843`).

An isolated Phase 3G branch/PR could not be created in this session; the only tracked additions are the new
retrieval workflow and this report plus append-only documentation sections. No existing file was overwritten.

## Verified authentic objects

Full machine-readable manifest: `external/recovery/phase3g/retrieval_manifest.tsv` (24 rows). Local byte copies:
`external/recovery/phase3g/authentic/`. Every row below was verified as
`SHA-256(file) == LFS OID == reported SHA-256` and `size(file) == pointer size`.

| Repository path | OID / SHA-256 (64 hex) | Bytes | GitHub blob | Run |
|---|---|---:|---|---:|
| `Ran Online Game Client/data/glogic/mapslist.mst` | `cc52d4d0483a7dcd91ced28ac6de783eca0859736ea13bebe5fa7a49cd23488f` | 13,714 | `ec15806974c4c6a0044557be67ed4e9b3884121a` | 45877365090 |
| `Ran Online Game Client/data/map/login.wld` | `16f5c01de514d779a20aad2fd898a4bff5ae4e179e7093525dafc744bd5209b0` | 3,098,590 | `0b883916fa839462a9935fcadd084ed406fdf699` | 45877365090 |
| `(Source Code) .../G-Logic/GLMapList.cpp` | `00018d7466df6c926dbbb457c4575059c28b54f51efd6088a76c09bfc563e648` | 9,068 | `08b260df1ce0a3050194658930fd6e4f3be2e5d7` | 45877502395 |
| `(Source Code) .../G-Logic/GLMapList.h` | `f42dfc2baf5654708c3bf89a0cf737d5da485c7f6182cbb5260d255c19aa957c` | 1,720 | `9e07cf6706e4be7de474f3ba45eaa5c43950bbbd` | 45877502395 |
| `(Source Code) .../G-Logic/GLMapNode.cpp` | `67645e2d6651679c8fc97e0663819cc21c49d8284366d1948b8de203ea79bcbe` | 12,203 | `38343a86dfa1e65cb2d3a234fa3e9b9c1ec98df0` | 45877596073 |
| `(Source Code) .../G-Logic/GLMapNode.h` | `12a6e4050ceac0e2ab9043b17b6f96c162728fd31ad801b260b563c5f192285b` | 5,217 | `757bc0da623a00104d850842b1f00d2a7055bc15` | 45877596073 |
| `(Source Code) .../G-Logic/GLogicData.h` | `a83e748bd3851e9e2039776539285f0373ec4e691e5c252e3db66b3ec2dd282e` | 29,340 | `82472377ddb2e1c11471e8e9d2182d712236fcf8` | 45877596073 |
| `(Source Code) .../Common/basestream.h` | `ee283b84773f5bb6a391ee5522bd05457cd4387bf15fa1dcf94545f2fc5b0959` | 5,625 | `e68844849d39aade3dbd13387258542452b6d082` | 45877736996 |
| `(Source Code) .../Common/SerialFile.cpp` | `602519275cd25301f9b65da4ca204f9952ce3f72e6101e77cf089a77aab559eb` | 13,895 | `8167371b184df0c53ce80e15115fd302dab52b5a` | 45877736996 |
| `(Source Code) .../Common/SerialFile.h` | `22002018fcecb8b12124695ce5e6be538608fca3c2fbf243b1fc1e4582330481` | 4,637 | `7eae6610b16d1b0aa906b63e90c1dfec9583029c` | 45877736996 |
| `(Source Code) .../Common/ByteCrypt.cpp` | `6914d2ae0dca72a7336279ab333c93a7b2ca633c173163d7ef0cf27c3c259d18` | 5,593 | `b1ade732d7f9587e5f1d1ac0245d6dbf9e13861c` | 45877736996 |
| `(Source Code) .../Common/ByteCrypt.h` | `9ca424e50a8bec40437f392611e0abf8badd9cc80ca789ccc8dda7e16f899c02` | 1,601 | `b6edeb263d0786602faf2aadc659a498423a8bd6` | 45877736996 |
| `(Source Code) .../Common/ByteCryptDef.h` | `c647aee9ea900c134b07c7c0cec93d1b8e2994ea6ee692d56a2f03cbdf257bda` | 4,914 | `a2ac382d8b7802e757774587a862652d167353ce` | 45877736996 |
| `(Source Code) .../Common/ByteCryptDefVer2.h` | `2a3c6d8cedc82da7e85e1bac8c168cb23d54e068e257edbec394cd305f49ef01` | 32,475 | `f08874528ead84dcf644129408fa5eeabf7e622d` | 45877736996 |
| `(Source Code) .../Common/ByteCryptDefVer1.h` | `fe42183bcc41cbd332831ea2f86bc345c3e59b941ca293b5ebe4250919c9b120` | 32,493 | `ca945d08ac6e5dbb97347e2e8ee426e4900a0a91` | 45877854016 |
| `Ran Online Game Client/data/glogic/level/innerzone_01.Lev` | `f1738a67987929d0c2e62248f2987b647fc22516f7cc32835645a1fd24a5235c` | 12,654 | `5251258b4d72af414b90f2bea5193cf4e3fed918` | 45878096559 |
| `Ran Online Game Client/textures/gui/loading_032.dds` | `de94e703c2bfefa8a3ae59785d3202a063ae325f3529c4460274080e27245694` | 262,272 | `71624a201985c1481b6af4036e61e54fb8619f5c` | 45878096559 |
| `(Source Code) .../G-Logic/GLLevelFile.cpp` | `62903a6a42ea35cdd9aaad0e33aa354256d9726dec84b76bb948949df395ec4f` | 10,003 | `7c92efbb65d66bb003a97e1f45e1778caf6f510e` | 45878096559 |
| `(Source Code) .../G-Logic/GLLevelFile.h` | `ee870473c5f4271dc43ce2d3b066cae233c04a0ce858c3bc78f9f10350cdf567` | 8,307 | `8d4a28b2da21984c6be9663e303a98bab9ebb9a3` | 45878096559 |
| `(Source Code) .../G-Logic/GLLevelFileSaveLoad.cpp` | `1f943d93e86bc44add1d2e8f050ae544f5ccc8891b1ad516a35c9f03583a78c8` | 14,866 | `93e79ae8d5b98adb041396d934c400099ac2f8ff` | 45878225553 |
| `(Source Code) .../DxOctree/DxLandManSaveLoad.cpp` | `84dbd358b0ad7a588198d2b2a02d1bbb5a060259e38ca76528557a1c39a66a43` | 71,002 | `d16cd56a7150e41d0905a8475562602e47f5e617` | 45878225553 |
| `(Source Code) .../G-Logic/GLLevelHead.cpp` | `99ec24149355a4e720eb0adfb438a60d964f1d377a9cdf14e68aa85a38ee9cd4` | 1,840 | `815a743a92e13def6027d3af8f7469efa4e8c9c7` | 45878336403 |
| `(Source Code) .../G-Logic/GLLevelHead.h` | `77284bc506300a5990809f83ed5cd7203feb84cce9e932137b04466b89b01ee2` | 1,121 | `0e89a344345225576b1bec58ca399d4d82a83bc5` | 45878336403 |
| `(Source Code) .../DxOctree/DxLandMan.h` | `d16a0bd7519946be878a99d3872098afb3fc6b41715912332761d467895df712` | 9,780 | `958cc61bcdf6b9f309de94729af35079d4fb1ed9` | 45878536743 |

`.../` abbreviates
`(Source Code) Ran Online Server Files and Developer Tools/Lib_Client/` or `Lib_Engine/`; the exact path is in
the manifest.

## Authentic map index decode

`external/recovery/phase3g/decode_mapslist.py` implements only the verified read path from the retrieved sources:
`CSerialFile::ReadFileType`/`operator>>`/`read`, `BYTECRYPT::byte_decode` with `ARRAY_MAPSLIST_VAR1`,
`GLMapList::LoadMapsListFile`, and `SMAPNODE_DATA::LOAD` version `0x0203`.

Result:

- Header `GLMAPS_LIST`, file version `0x0200`, crypt mode `EMBYTECRYPT_MAPSLIST`.
- 120 node records; 104 marked used; exactly **13,714 bytes consumed** (the entire verified file).
- All 120 `strFile` values end in `.lev` (case varies), as the original loader expects.
- 62 distinct loading-image names; BGM references are `.ogg` names.
- First entries:
  `innerzone_01.lev | 0/0 | SG_Campus1F | ran07.ogg | loading_032.dds`;
  `Sea_Death.LEV | 1/0 | Halloween Map | scarysong.ogg | loading_057.dds`;
  `w_school_01.Lev | 2/0 | SG_Campus | ran12.ogg | loading_054.dds`.
- Decoded TSV: `external/recovery/phase3g/authentic/mapslist.decoded.tsv`.

This is **not** the old plain-INI variant and not a Rijndael `CStringFile` payload; the earlier Phase 3F
`phase3f_stringfile_decode` harness correctly rejected it, so the binary `GLMAPS_LIST` path was implemented from
the authentic sources instead.

## Referenced LEV, dependency and texture

- Referenced `.lev`: `Ran Online Game Client/data/glogic/level/innerzone_01.Lev`
  (OID `f1738a...5235c`, 12,654 B), retrieved and verified.
- `decode_lev_head.py` reads the authentic file type `glmap`, file version `0x0200`,
  `EMBYTECRYPT_LEVEL_NEW` (`ARRAY_LEVEL_VAR1_NEW`), `SLEVEL_HEAD` version `0x0102`, block size 34,
  consumed bytes 34, `m_strMapName=""`, `m_strWldFile="innerzone_01.wld"`, division 16, bright 2.
- Dependency: `Ran Online Game Client/data/map/innerzone_01.wld` (OID `fe7538cc19c88ee3670119651637837425977302ad8d32707206c39cfc935a0b`, 1,190,775 B)
  and `.../innerzone_01.wld0` (OID `b94ba50abc5741e0e6651c6e5098ed5812e32b0fa6bcb68a1ae9b09c3a6c472f`, 5,808,480 B)
  were **identified from the verified index/LEV**, not downloaded in this focused sample.
- Referenced texture: `Ran Online Game Client/textures/gui/loading_032.dds`
  (OID `de94e7...45694`, 262,272 B), retrieved and verified.

## WLD / LEV compatibility from original source

- Authentic `login.wld`: header `LAND.MAN`, version `0x108`. Original `DxLandMan::IsLandManSupported`
  explicitly accepts `0x0108`; `VERSION_WLD = 0x0200` and `VERSION_ENCODE = 0x0200`, so this file is an older,
  unencoded WLD variant. No byte-crypt key was guessed.
- Authentic `innerzone_01.Lev`: `glmap` `0x0200` with `SLEVEL_HEAD 0x0102` and a `.wld` dependency. The Android
  `EP7LevelHeaderParser` only accepts head version `0x0101`, so the authentic LEV is correctly rejected as
  `UNSUPPORTED GLLEVEL VERSION`; it was not silently coerced.
- The Android scene index rejects the authentic binary `mapslist.mst` with
  `mapslist.mst NOT PLAIN TEXT OR NO USABLE MAP ROWS (encoded mapslist is unsupported)`. No fake plain-text
  manifest was substituted.
- There is still no WLD geometry parser/renderer in RANMobile; no original scene was rendered or claimed.

## Android pipeline checks on verified files only

Local native harnesses compiled with `g++ -std=c++17 -I.` (no Android SDK/NDK available locally):

- `EP7TextureDecoder::InspectDDS` / `DecodeDDS` on the authentic `loading_032.dds`: valid header,
  DXT1 (`fourCC=827611204`), 1024×512, 1 mip, no alpha; decode produced 2,097,152 RGBA bytes, exit 0.
- `EP7SceneIndexBuilder::Build` on the authentic `mapslist.mst`: `valid=NO`, encoded-manifest diagnostic above.
- `EP7LevelHeaderParser::Parse` on the authentic `innerzone_01.Lev`: `valid=0`, `UNSUPPORTED GLLEVEL VERSION`.
- Existing `./external/recovery/phase3d_tests`: exit 0, `ALL TESTS PASSED!` (regression baseline only).
- No APK/AAB build, device/emulator run, live server connection or visual scene test was performed in this phase.

## Limitations and next action

- Local `git-lfs` installation remains unavailable; cloud-runner LFS is the supported retrieval path in this
  environment. GitHub artifact/log blob hosts remain unreachable locally; dangling Git blobs are the verified
  transfer channel.
- Dangling blobs are intentionally not referenced by a commit; GitHub may garbage-collect them later. All
  verified bytes are preserved in `external/recovery/phase3g/authentic/` in this workspace.
- No original 3DS Max/DirectX tool was built and no recovered binary was executed.
- **Exact next action:** with explicit user approval, add a source-derived binary `GLMAPS_LIST` /
  `SLEVEL_HEAD 0x0102` reader to the Android importer behind a no-fallback validation flag, then add focused
  native tests using the verified hashes above before any scene-preview integration. Do not change gameplay,
  authentication, protocol or rendering behavior to fake a scene.

## Artifact index

- `external/recovery/phase3g/retrieval_manifest.tsv` — all 24 verified paths/OIDs/sizes/blobs/runs.
- `external/recovery/phase3g/authentic/` — byte-preserving original files and decoded TSV.
- `external/recovery/phase3g/decode_mapslist.py`, `decode_lev_head.py` — source-derived decoders.
- `external/recovery/phase3g/android_check/` — local verification harnesses and root fixture.
- `.github/workflows/phase3g_focused_lfs_fetch.yml` — reproducible focused retrieval workflow.
