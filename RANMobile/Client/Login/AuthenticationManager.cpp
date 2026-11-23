#include "AuthenticationManager.h"
#include "../../Engine/Network/NetworkClient.h"
#include "../../Engine/Network/EP7Protocol.h"
#include "../../Engine/Core/Logger.h"
#include <cstring>
#include <algorithm>

namespace RANMobile {

bool AuthenticationManager::Login(NetworkClient* net, const std::string& username, const std::string& password) {
    if (username.empty() || password.empty()) {
        Logger::Warn("AuthenticationManager", "Username or password is empty");
        return false;
    }
    if (!net || !net->IsConnected()) {
        Logger::Warn("AuthenticationManager", "Not connected to EP7 Login Server (" + s_serverDisplayName + ")");
        return false;
    }

    EP7NetLoginData loginPkt;
    std::strncpy(loginPkt.szUserid, username.c_str(), EP7_USR_ID_LENGTH);
    std::strncpy(loginPkt.szPassword, password.c_str(), EP7_USR_PASS_LENGTH);

    // Send exact EP7 NET_LOGIN_DATA payload
    const uint8_t* payload = reinterpret_cast<const uint8_t*>(&loginPkt) + sizeof(EP7NetMsgGeneric);
    uint32_t payloadSize = sizeof(EP7NetLoginData) - sizeof(EP7NetMsgGeneric);

    Logger::Info("AuthenticationManager", "Sending EP7 NET_MSG_LOGIN_2 for user: " + username + " -> " + s_serverDisplayName);
    return net->SendPacket(EP7_NET_MSG_LOGIN_2, payload, payloadSize);
}

} // namespace RANMobile
