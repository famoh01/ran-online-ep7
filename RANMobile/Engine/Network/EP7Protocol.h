#pragma once

#include <cstdint>
#include <cstring>

namespace RANMobile {

// Verified directly from netclientLib/s_NetGlobal.h
constexpr int32_t EP7_NET_MSG_BASE = 0;
constexpr int32_t EP7_NET_MSG_LOBBY = EP7_NET_MSG_BASE + 1013;
constexpr int32_t EP7_NET_MSG_GCTRL = EP7_NET_MSG_BASE + 2011;

constexpr int32_t EP7_NET_MSG_HEARTBEAT_CLIENT_REQ = 160;
constexpr int32_t EP7_NET_MSG_HEARTBEAT_CLIENT_ANS = 161;

constexpr int32_t EP7_NET_MSG_LOGIN_2 = EP7_NET_MSG_LOBBY + 107;
constexpr int32_t EP7_NET_MSG_LOGIN_FB = EP7_NET_MSG_LOBBY + 108;
constexpr int32_t EP7_NET_MSG_REQ_CHA_BINFO = EP7_NET_MSG_LOBBY + 302;
constexpr int32_t EP7_NET_MSG_CHA_BAINFO = EP7_NET_MSG_LOBBY + 306;

constexpr int32_t EP7_NET_MSG_GCTRL_REQ_GATEOUT = EP7_NET_MSG_GCTRL + 131;
constexpr int32_t EP7_NET_MSG_GCTRL_GOTO = EP7_NET_MSG_GCTRL + 142;
constexpr int32_t EP7_NET_MSG_CHAT = EP7_NET_MSG_GCTRL + 100;
constexpr int32_t EP7_NET_MSG_GCTRL_ATTACK = EP7_NET_MSG_GCTRL + 144;
constexpr int32_t EP7_NET_MSG_GCTRL_INVEN_INSERT = EP7_NET_MSG_GCTRL + 200;

enum EP7LoginFbSub : uint16_t {
    EP7_LOGIN_FB_SUB_OK        = 0,
    EP7_LOGIN_FB_SUB_FAIL      = 1,
    EP7_LOGIN_FB_SUB_INCORRECT = 5
};

constexpr size_t EP7_USR_ID_LENGTH = 20;
constexpr size_t EP7_USR_PASS_LENGTH = 20;
constexpr size_t EP7_USR_RAND_PASS_LENGTH = 6;
constexpr size_t EP7_ENCRYPT_KEY = 12;
constexpr size_t EP7_MAX_ONESERVERCHAR_NUM = 4;

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

// Verified from netclientLib/s_NetGlobal.h:3227-3251
struct EP7NetChaBbaInfo {
    EP7NetMsgGeneric nmg;
    int32_t nChaSNum = 0;
    int32_t nChaNum[EP7_MAX_ONESERVERCHAR_NUM]{};

    EP7NetChaBbaInfo() {
        nmg.dwSize = sizeof(EP7NetChaBbaInfo);
        nmg.nType = EP7_NET_MSG_CHA_BAINFO;
        std::memset(nChaNum, 0, sizeof(nChaNum));
    }
};

struct EP7NetChaBaInfo {
    EP7NetMsgGeneric nmg;
    int32_t nChaNum = 0;

    EP7NetChaBaInfo() {
        nmg.dwSize = sizeof(EP7NetChaBaInfo);
        nmg.nType = EP7_NET_MSG_REQ_CHA_BINFO;
    }
};

struct EP7Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// Verified from RanClientLib/G-Logic/GLContrlPcMsg.h:247-265
struct EP7NetPcGoto {
    EP7NetMsgGeneric nmg;
    uint32_t dwActState = 0;
    EP7Vec3 vCurPos{};
    EP7Vec3 vTarPos{};

    EP7NetPcGoto() {
        nmg.dwSize = sizeof(EP7NetPcGoto);
        nmg.nType = EP7_NET_MSG_GCTRL_GOTO;
    }
};

struct EP7NativeID {
    uint16_t wMainID = 0;
    uint16_t wSubID = 0;
};

// Verified from RanClientLib/G-Logic/GLContrlCharJoinMsg.h:701-735
struct EP7NetReqGateOut {
    EP7NetMsgGeneric nmg;
    uint32_t dwGateID = 0;
    uint32_t dwGaeaID = 0;

    EP7NetReqGateOut() {
        nmg.dwSize = sizeof(EP7NetReqGateOut);
        nmg.nType = EP7_NET_MSG_GCTRL_REQ_GATEOUT;
    }
};

struct EP7NetReqGateOutFB {
    EP7NetMsgGeneric nmg;
    uint32_t emFB = 0;
    EP7NativeID sMapID{};
    EP7Vec3 vPos{};

    EP7NetReqGateOutFB() {
        nmg.dwSize = sizeof(EP7NetReqGateOutFB);
        nmg.nType = EP7_NET_MSG_GCTRL_REQ_GATEOUT + 1; // NET_MSG_GCTRL_REQ_GATEOUT_FB
    }
};
#pragma pack(pop)

} // namespace RANMobile
