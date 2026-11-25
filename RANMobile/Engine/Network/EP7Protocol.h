#pragma once

#include <cstdint>
#include <cstring>

namespace RANMobile {

// Verified directly from netclientLib/s_NetGlobal.h
constexpr int32_t EP7_NET_MSG_BASE = 0;
constexpr int32_t EP7_NET_MSG_LOBBY = EP7_NET_MSG_BASE + 1013;

constexpr int32_t EP7_NET_MSG_HEARTBEAT_CLIENT_REQ = 160;
constexpr int32_t EP7_NET_MSG_HEARTBEAT_CLIENT_ANS = 161;

constexpr int32_t EP7_NET_MSG_LOGIN_2 = EP7_NET_MSG_LOBBY + 107;
constexpr int32_t EP7_NET_MSG_LOGIN_FB = EP7_NET_MSG_LOBBY + 108;
constexpr int32_t EP7_NET_MSG_REQ_CHA_BINFO = EP7_NET_MSG_LOBBY + 302;
constexpr int32_t EP7_NET_MSG_CHA_BAINFO = EP7_NET_MSG_LOBBY + 306;

enum EP7LoginFbSub : uint16_t {
    EP7_LOGIN_FB_SUB_OK        = 0,
    EP7_LOGIN_FB_SUB_FAIL      = 1,
    EP7_LOGIN_FB_SUB_INCORRECT = 5
};

constexpr size_t EP7_USR_ID_LENGTH = 20;
constexpr size_t EP7_USR_PASS_LENGTH = 20;
constexpr size_t EP7_USR_RAND_PASS_LENGTH = 6;
constexpr size_t EP7_ENCRYPT_KEY = 12;

#pragma pack(push, 1)
struct EP7NetMsgGeneric {
    uint32_t dwSize = 0;
    int32_t nType = 0;
};

struct EP7NetLoginData {
    EP7NetMsgGeneric nmg;
    int32_t nChannel = 0;
    char szRandomPassword[EP7_USR_RAND_PASS_LENGTH + 1]{};
    char szPassword[EP7_USR_PASS_LENGTH + 1]{};
    char szUserid[EP7_USR_ID_LENGTH + 1]{};
    char szEnCrypt[EP7_ENCRYPT_KEY + 1]{};

    EP7NetLoginData() {
        nmg.dwSize = sizeof(EP7NetLoginData);
        nmg.nType = EP7_NET_MSG_LOGIN_2;
        nChannel = 0;
        std::memset(szRandomPassword, 0, sizeof(szRandomPassword));
        std::memset(szPassword, 0, sizeof(szPassword));
        std::memset(szUserid, 0, sizeof(szUserid));
        std::memset(szEnCrypt, 0, sizeof(szEnCrypt));
    }
};

struct EP7NetLoginFeedback {
    EP7NetMsgGeneric nmg;
    uint16_t nResult = EP7_LOGIN_FB_SUB_FAIL;
    int32_t nChaRemain = 0;

    EP7NetLoginFeedback() {
        nmg.dwSize = sizeof(EP7NetLoginFeedback);
        nmg.nType = EP7_NET_MSG_LOGIN_FB;
    }
};
#pragma pack(pop)

} // namespace RANMobile
