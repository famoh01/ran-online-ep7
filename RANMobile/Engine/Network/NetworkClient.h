#pragma once

#include "EP7Protocol.h"
#include <string>
#include <vector>
#include <cstdint>
#include <functional>

namespace RANMobile {

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting
};

class NetworkClient {
public:
    ~NetworkClient();

    bool Connect(const std::string& host, uint16_t port);
    void Disconnect();
    void Poll(float deltaTime);

    bool SendPacket(int32_t msgType, const void* data, uint32_t size);
    ConnectionState GetState() const { return m_state; }
    bool IsConnected() const { return m_state == ConnectionState::Connected; }

    void SetOnPacketCallback(std::function<void(int32_t, const uint8_t*, uint32_t)> cb) {
        m_onPacket = std::move(cb);
    }

private:
    int m_socket = -1;
    ConnectionState m_state = ConnectionState::Disconnected;
    std::string m_host;
    uint16_t m_port = 0;
    float m_heartbeatTimer = 0.0f;
    std::function<void(int32_t, const uint8_t*, uint32_t)> m_onPacket;
};

} // namespace RANMobile
