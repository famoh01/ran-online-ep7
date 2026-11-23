#include "NetworkClient.h"
#include "EP7Protocol.h"
#include "../Core/Logger.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

namespace RANMobile {

NetworkClient::~NetworkClient() {
    Disconnect();
}

bool NetworkClient::Connect(const std::string& host, uint16_t port) {
    Disconnect();
    m_host = host;
    m_port = port;
    m_state = ConnectionState::Connecting;

    m_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket < 0) {
        Logger::Error("NetworkClient", "Failed to create socket");
        m_state = ConnectionState::Disconnected;
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        Logger::Error("NetworkClient", "Invalid server address: " + host);
        ::close(m_socket);
        m_socket = -1;
        m_state = ConnectionState::Disconnected;
        return false;
    }

    if (::connect(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        Logger::Warn("NetworkClient", "Could not connect to EP7 server " + host + ":" + std::to_string(port));
        ::close(m_socket);
        m_socket = -1;
        m_state = ConnectionState::Disconnected;
        return false;
    }

    int flags = ::fcntl(m_socket, F_GETFL, 0);
    ::fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);

    m_state = ConnectionState::Connected;
    Logger::Info("NetworkClient", "Connected to EP7 server " + host + ":" + std::to_string(port));
    return true;
}

void NetworkClient::Disconnect() {
    if (m_socket >= 0) {
        ::close(m_socket);
        m_socket = -1;
    }
    m_state = ConnectionState::Disconnected;
}

void NetworkClient::Poll(float deltaTime) {
    if (m_state != ConnectionState::Connected || m_socket < 0) {
        return;
    }

    m_heartbeatTimer += deltaTime;
    if (m_heartbeatTimer >= 15.0f) {
        m_heartbeatTimer = 0.0f;
        // EP7 Heartbeat packet (verified: NET_MSG_HEARTBEAT_CLIENT_ANS = 161)
        EP7NetMsgGeneric hb{};
        hb.dwSize = sizeof(EP7NetMsgGeneric);
        hb.nType = EP7_NET_MSG_HEARTBEAT_CLIENT_ANS;
        ::send(m_socket, &hb, sizeof(hb), 0);
    }

    uint8_t buffer[4096];
    ssize_t received = ::recv(m_socket, buffer, sizeof(buffer), 0);
    if (received > 0 && static_cast<size_t>(received) >= sizeof(EP7NetMsgGeneric)) {
        const auto* hdr = reinterpret_cast<const EP7NetMsgGeneric*>(buffer);
        if (m_onPacket) {
            m_onPacket(hdr->nType, buffer, static_cast<uint32_t>(received));
        }
    } else if (received == 0) {
        Logger::Warn("NetworkClient", "Server closed connection");
        Disconnect();
    }
}

bool NetworkClient::SendPacket(int32_t msgType, const void* data, uint32_t size) {
    if (m_state != ConnectionState::Connected || m_socket < 0) {
        return false;
    }
    std::vector<uint8_t> packet(sizeof(EP7NetMsgGeneric) + size);
    auto* hdr = reinterpret_cast<EP7NetMsgGeneric*>(packet.data());
    hdr->dwSize = static_cast<uint32_t>(packet.size());
    hdr->nType = msgType;
    if (data && size > 0) {
        std::memcpy(packet.data() + sizeof(EP7NetMsgGeneric), data, size);
    }
    ssize_t sent = ::send(m_socket, packet.data(), packet.size(), 0);
    return sent == static_cast<ssize_t>(packet.size());
}

} // namespace RANMobile
