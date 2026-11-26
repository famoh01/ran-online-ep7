# Phase 3A — Original EP7 Protocol / Login Connection Audit

Date: 2026-11-26. Branch: `arena/075e1725-ran-online-ep7`, HEAD `714da18fdc90b7a21ca1d4e2efb96bc4d1cbbd1d`. PR #3 remains **OPEN / DRAFT**.

This phase is **audit-only**. No protocol constant, packet structure, client/server source, game logic, DB/data, mobile renderer/input/UI, or original `.vcxproj` was modified. No live server handshake was attempted or claimed.

## 1. Scope and evidence base

Reviewed: `SOURCE_DATA_RECOVERY.md`, `ARCHITECTURE.md`, `PROGRESS_REVIEW.md`, `SERVER_REQUIREMENTS.md`, `BLOCKERS.md`; `RANMobile/` network/login code and native tests; `RANMobile/Engine/Network/EP7Protocol.h`; `netclientLib/s_NetGlobal.h`; `netclientLib`/`netserverLib` login and dispatch sources; original client UI (`RanClientUILib/Interface/LoginPage.cpp`); `Smttm2022.sln`, `Basic/Basic.vcxproj`, `netclientLib/netclientLib.vcxproj`, `netserverLib/netserverLib.vcxproj`; recovered ThaiPack CFG/DBTools outputs under the git-ignored `external/ep7_release/`; and existing native/CI workflows.

Important limit from the recovered material: `SOURCE_DATA_RECOVERY.md:118–120` says the recovered `Server*.cfg` come from a ThaiPack whose `README_TH.txt` describes its VS2022 build as a **different source lineage** from the Thai client package, with compatibility to be tested, not assumed. The exact build flags of the intended live “FamHawaii Ep7” server are therefore not established by these files.

## 2. `LOGIN_2 = 2046` — what the source proves and what it does not

### 2.1 Arithmetic is source-derived

| Input | Evidence | Value |
|---|---|---|
| Region base under `TH_PARAM` | `netclientLib/s_NetGlobal.h:629–630` | `NET_MSG_BASE = 989` |
| Default base (no region flag) | `netclientLib/s_NetGlobal.h:633–634` | `NET_MSG_BASE = 995` |
| Active lobby offset | `netclientLib/s_NetGlobal.h:654–657` | `NET_MSG_LOBBY = NET_MSG_BASE + 950` |
| Superseded lobby offset | `netclientLib/s_NetGlobal.h:637–644` (inside `/* … */`) | `+1013` **commented out** |
| General login ID | `netclientLib/s_NetGlobal.h:723–724` | `NET_MSG_LOGIN_2 = NET_MSG_LOBBY + 107` |

Therefore, **as a compile-time value of the repo's original shared header**:

- `TH_PARAM`: `989 + 950 + 107 = 2046`
- no region flag (e.g. `Release`): `995 + 950 + 107 = 2052`

A read-only parser over the original header reproduced these values (`external/recovery/phase3a_source_audit.py`, output `external/recovery/phase3a_source_audit_output.txt`): it confirms the active offset is `950`, the `+1013` block is commented out, and `2046`/`2052` follow. This is arithmetic from source, **not** a capture of a live packet.

### 2.2 Build flags are source-derived; the live server remains unresolved

- `Basic/Basic.vcxproj:782`, `netclientLib/netclientLib.vcxproj:724`, `netserverLib/netserverLib.vcxproj:576` define `TH_PARAM` for `Release_Thailand|Win32`.
- `Basic/Basic.cpp:71–73` maps `TH_PARAM` to `RANPARAM::emSERVICE_TYPE = EMSERVICE_THAILAND`.
- `Smttm2022.sln:443–444, 514–515, 1029–1030, 1100–1101` map the solution's `Release_Thailand|Win32` to plain `Release|Win32` for `SessionServer`, `LoginServer`, `FieldServer` and `AgentServer` (their EXE projects do not themselves add `TH_PARAM`). By contrast `netclientLib` (`Smttm2022.sln:293–294`) and `netserverLib` (`Smttm2022.sln:731–732`) map to their `Release_Thailand|Win32` configurations, where `TH_PARAM` is defined. The dispatch and login handlers under audit live in `netserverLib`, so the library can be compiled with `TH_PARAM` even when the EXE project is not.
- A portable probe compiled from the original constants produced `TH_PARAM LOGIN_2 = 2046`, `Default LOGIN_2 = 2052` (`external/recovery/protocol_size_probe.cpp` / `protocol_size_probe`).
- Recovered `1_Server/cfg/Server*.cfg` all contain `service_provider 3`; `netclientLib/s_NetGlobal.h:326–338` defines `SP_CHINA = 3`, while the Thai login handler is gated on `SP_THAILAND` (`netserverLib/s_CAgentServerMsgLogin.cpp:15`) and `netserverLib/s_CAgentServer.cpp:239–247` opens the Thai DB only for `SP_THAILAND`. These are recovered template values, not proof of the live deployment.

**Conclusion:** `2046` is a valid source-derived value for a `TH_PARAM`-compiled *generic* login path. It is **not proven** to be the ID accepted by the intended live/FamHawaii server, and the source contains a separate Thailand-specific login path that may be the one a Thai release client actually uses (§3). The authoritative deployed build configuration, or a capture of the live login packet, is still required. Per instruction, no protocol constant was changed on the basis of arithmetic alone.

## 3. Original login paths are distinct — ID *and* payload matter

### 3.1 Generic login (`NET_MSG_LOGIN_2`)

- `netclientLib/s_NetGlobal.h:2048–2054`: `NET_MSG_GENERIC { DWORD dwSize; EMNET_MSG nType; }`, 8 bytes with natural alignment.
- `netclientLib/s_NetGlobal.h:2210–2228`: `NET_LOGIN_DATA` field order is `nmg, nChannel, szRandomPassword[7], szPassword[21], szUserid[21], szEnCrypt[13]`; constructor writes `dwSize = sizeof(NET_LOGIN_DATA)` and `nType = NET_MSG_LOGIN_2`. With default packing this is **76 bytes** (8 + 4 + 7 + 21 + 21 + 13 = 74, padded to 76).
- `netclientLib/s_NetClientMsgLogin.cpp:25–62`: `SndLogin` copies ID/password/random-password/encrypt-key and TEA-encrypts **all four** fields before `Send`.
- `netclientLib/s_NetClient.cpp:954–999`: `Send(char*)` uses `nmg.dwSize` verbatim; outside `NET_STATE_LOGIN` it adds garbage bytes before queueing.
- `netserverLib/s_CAgentServerMsg.cpp:62` dispatches `NET_MSG_LOGIN_2` to `MsgLogIn`.
- `netserverLib/s_CAgentServerMsgLogin.cpp:536–608`: `MsgLogIn` rejects any packet whose `dwSize != sizeof(NET_LOGIN_DATA)` (`560–565`), TEA-decrypts the four fields (`579–582`), and compares the decrypted encrypt key to the server's `m_szEncrypt` (`595`). Mismatch gives `NET_MSG_LOGIN_FB` / `EM_LOGIN_FB_SUB_FAIL`.

Thus a correct numeric ID alone is insufficient: a 74-byte plaintext payload with no TEA fields is rejected by the generic handler's exact-size check even if the ID were changed to 2046.

### 3.2 Thailand login (`THAI_NET_MSG_LOGIN`)

- `netclientLib/s_NetGlobal.h:757`: `THAI_NET_MSG_LOGIN = NET_MSG_LOBBY + 140`; under `TH_PARAM` this is `989 + 950 + 140 = 2079` (the source parser prints 2079).
- `netclientLib/s_NetGlobal.h:2260–2276`: `THAI_NET_LOGIN_DATA` contains only `nmg, nChannel, szPassword[21], szUserid[21]` (**56 bytes** with natural alignment), no random-password and no encrypt field; its constructor sets `THAI_NET_MSG_LOGIN`.
- `RanClientUILib/Interface/LoginPage.cpp:225–246`: `EMSERVICE_THAILAND` calls `ThaiSndLogin`, not `SndLogin`; `Basic/Basic.cpp:71–73` sets that service type under `TH_PARAM`.
- `netclientLib/s_NetClientMsgLogin.cpp:282–311`: `ThaiSndLogin` TEA-encrypts user ID and password only, then calls `Send`.
- `netserverLib/s_CAgentServerMsg.cpp:66` dispatches `THAI_NET_MSG_LOGIN` to `ThaiMsgLogin`.
- `netserverLib/s_CAgentServerMsgLogin.cpp:13–114`: `ThaiMsgLogin` returns immediately unless `m_nServiceProvider == SP_THAILAND` (`15`), decrypts ID/password (`48–49`), checks channel (`55–83`), and queues `CAgentThaiUserCheck` (`105–113`).

This means a `TH_PARAM` **client UI** is expected to send the 2079 Thai packet, while `2046` belongs to the generic client path. Which path the target server accepts cannot be decided from constants alone. No code was changed to force either path.

### 3.3 Handshake and heartbeat constants not used by mobile

- `netclientLib/s_NetGlobal.h:669–675`: version/key messages `100/110/120/130`, `NET_MSG_SND_CRYT_KEY = 140`, `NET_MSG_RANDOM_NUM = 141`; `682–683`: heartbeat request/answer `160/161`; `760–762`: `NET_MSG_REQ_RAND_KEY = LOBBY + 150`, `NET_MSG_SND_ENCRYPT_KEY = LOBBY + 160`.
- `netclientLib/s_NetClientMsg.cpp:70–90, 159–168`: client handles crypt-key/random-number/encrypt-key messages and stores `m_szEncryptKey`.
- `netclientLib/s_NetClientMsg.cpp:281–300`: original heartbeat answer is only sent in response to `NET_MSG_HEARTBEAT_CLIENT_REQ`; in agent state it carries the TEA-encrypted key.
- `netserverLib/s_CAgentServerMsg.cpp:40–56`: agent heartbeat validates the decrypted encrypt key.

## 4. Mobile login compatibility audit (no changes made)

| Aspect | Original source | `RANMobile` implementation | Audit result |
|---|---|---|---|
| Login ID | `2046` under `TH_PARAM` generic; `2079` Thai variant (§2–3) | `EP7Protocol.h:8–16`: `0 + 1013 + 107 = 1120` | Wrong source lineage: `1013` is the commented-out lobby offset; `1120` is not either Thai-branch value derived above. Live acceptance still unknown. |
| Header | `NET_MSG_GENERIC`, 8 bytes, natural alignment (`s_NetGlobal.h:2050–2054`) | `EP7Protocol.h:23–27` `#pragma pack(1)`, still 8 bytes here | Same size for the header; packing divergence matters for the full struct. |
| Login payload | 76 bytes, natural alignment, fields random/password/userid/encrypt (`s_NetGlobal.h:2210–2228`) | `EP7Protocol.h:29–47`: packed struct, 74 bytes; `AuthenticationManager.cpp:20–29` sends only the 66-byte payload | `NetworkClient::SendPacket` (`NetworkClient.cpp:93–106`) prepends an 8-byte header with `dwSize = 74`. Generic server checks for 76 (`s_CAgentServerMsgLogin.cpp:560`); mismatch is rejected if routed there. |
| Credential protection | TEA on ID/password/random/encrypt (`s_NetClientMsgLogin.cpp:25–62`) or ID/password (`282–311`) | `AuthenticationManager.cpp:20–29` copies plaintext with `strncpy`; no TEA, no random password, no encrypt key | Not byte-compatible with either original login payload. |
| Connection/state flow | Login → agent/session transitions, key exchange, character select, game-server handoff | `NetworkClient::Connect` (`NetworkClient.cpp:17–55`) sets `Connected` on TCP connect; no call site for `Connect` in the app (`PROGRESS_REVIEW.md:29`), no login/agent/field state machine, no key exchange | No original login flow is reproduced. `AuthManager` only returns `SendPacket` success; that is a socket-write result, not authentication. |
| Responses/errors | `NET_MSG_LOGIN_FB` with ~20 result codes handled in `RanClientUILib/Interface/OuterInterfaceMsg.cpp:26, 58–177` | `EP7_NET_MSG_LOGIN_FB` is declared in `EP7Protocol.h:16`; no consumer found in `RANMobile/`; `Poll` (`NetworkClient.cpp:80–90`) forwards any received header to an optional callback that the app does not register (`grep SetOnPacketCallback`: only declaration) | Login result, failure reasons, disconnect handling and server-close reasons are not implemented. |
| Timeouts / reconnect | Original client has explicit network states and server-close handling | `ConnectionState::Reconnecting` exists only as an enum (`NetworkClient.h:12–16`); no timeout or reconnect logic; only `recv == 0` disconnects (`NetworkClient.cpp:87–90`) | Missing. |
| Heartbeat | Server-prompted 161 answer, encrypted in agent state (`s_NetClientMsg.cpp:281–300`) | `NetworkClient.cpp:70–78` sends bare 161 every 15 s unsolicited | Unsolicited and without the encrypt field; not equivalent. |
| Config | Winsock client, server host/port from login server list | `RANMobile/Config/server_config.json` and `RANMobile/.env.example` point at `127.0.0.1:5001`; no code reads them in the login path | Local-only placeholder; not the recovered `(FamHawaii)` ports 3141/5000/5104/5103 (`SOURCE_DATA_RECOVERY.md:118`). |
| Test double | Real Login/Session/Agent servers | `RANMobile/Server/MobileServerHost.cpp:17–41, 103–123` keeps an in-memory FNV-1a password map and answers with one byte; native tests at `RANMobile/Tests/test_main.cpp:237–245` exercise only this stub | A placeholder, not the original server. No simulated login success is treated as protocol verification. |

The native test suite additionally `static_assert`s `EP7_NET_MSG_LOGIN_2 == 1120` (`RANMobile/Tests/test_main.cpp:113–115`), i.e. it currently locks in the disputed value as “verified”. Correcting that assertion would require changing the mobile protocol constant, which is explicitly out of scope for this audit phase; therefore no test source was edited.

## 5. Tests and validation run in this phase

1. Read-only source parser: `python3 external/recovery/phase3a_source_audit.py` → exit 0; output in `external/recovery/phase3a_source_audit_output.txt`. It derives 2046/2052/2079 from the original header and reports `mobile equals TH_PARAM LOGIN_2: False`, `mobile uses commented-out lobby offset: True`.
2. Portable constants/layout probe: `g++ -std=c++17 -Wall -Wextra -Werror external/recovery/protocol_size_probe.cpp -o external/recovery/protocol_size_probe` → exit 0; run prints `TH_PARAM LOGIN_2 = 2046`, `Default LOGIN_2 = 2052`, and struct sizes consistent with the 76-byte original login layout and 74-byte packed mobile layout.
3. Existing native tests re-run: `./external/recovery/ranmobile_tests` → exit 0, ending `ALL TESTS PASSED!` (new log `external/recovery/phase3a_test_output.txt`). This covers the pre-existing portable core/stub tests only; it is **not** a live EP7 handshake, not an Android APK build, and it includes the `1120` assertion discussed above.
4. Additional non-strict Windows configuration check dispatched: `build.yml` with `configuration=Release_Thailand` on this branch, run `45850495046`. Status at report time: **in progress / no conclusion yet**. It is a compile check only; even its completion would not verify runtime protocol compatibility. The strict workflow `45841659289` previously verified `Smtm2022.sln`, `LoginServer`, `SessionServer`, `AgentServer`, `FieldServer` with exit 0 and stopped at the pre-existing `GameServer` DirectX C2011 blocker (`BLOCKERS.md` B8). GameServer remains unverified and was not bypassed.

All probe scripts, logs and binaries are under git-ignored `external/recovery/`; they are audit evidence, not source changes.

## 6. Findings, uncertainty, and next decision

**Proven from source:** 2046 is the arithmetic `NET_MSG_LOGIN_2` for a `TH_PARAM` build of the original shared header; the mobile mirror's 1120 comes from a commented-out `+1013` lobby block. The original generic and Thai login packets differ in ID, size, field order and TEA usage; the mobile login payload matches neither. The mobile app lacks handshake, response handling, timeouts/reconnect, and its server is an in-process stub.

**Not proven:** that the live/FamHawaii server was built with `TH_PARAM`, that it dispatches 2046 rather than the Thai 2079 path, or that any captured server accepts either value. No live server or authorized capture was available. The recovered ThaiPack CFG values (`service_provider 3`) and the different-lineage warning prevent treating them as authoritative for the deployed server.

**Needed before any protocol change (request, not action):** the deployed server's build configuration/edition (or a login-packet capture from the intended server), and explicit authorization to change `RANMobile/Engine/Network/EP7Protocol.h`, `AuthenticationManager.cpp`, the login tests, and the mobile handshake in a separate later phase. Until then `BLOCKERS.md` B6 remains open and no protocol value is changed.

**Phase 3A boundary:** no original gameplay protocol/packets/game logic/server behaviour/DB/data were modified; no fake protocol implementation or simulated login success was added; no renderer, input, UI or server modification was started.
