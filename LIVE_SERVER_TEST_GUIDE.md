# LIVE_SERVER_TEST_GUIDE.md — RAN ONLINE EP7 (FAM HAWAII MOBILE)

## 1. Server Configuration (`RANMobile/.env.example` / `RANMobile/Config/server_config.json`)
- `SERVER_HOST`: IP of running EP7 LoginServer
- `SERVER_PORT`: `5001` (or configured LoginServer port)
- `EP7_SERVER_DISPLAY_NAME`: `FAM HAWAII`

## 2. Verified EP7 Protocol (`RANMobile/Engine/Network/EP7Protocol.h`)
- `EP7_NET_MSG_HEARTBEAT_CLIENT_ANS`: `161`
- `EP7_NET_MSG_LOGIN_2`: `1120` (`NET_LOGIN_DATA`)

## 3. Notes
- Mobile Client connects to **EP7 Login/Agent/Field Server** only (never directly to MSSQL).
