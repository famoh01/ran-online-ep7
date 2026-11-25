#include "MobileServerHost.h"
#include "../Engine/Core/Logger.h"
#include "../Engine/Network/EP7Protocol.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>

namespace RANMobile {

MobileServerHost::~MobileServerHost() {
    Stop();
}

uint64_t MobileServerHost::HashPassword(const std::string& password) {
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : password) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

bool MobileServerHost::RegisterAccount(const std::string& username, const std::string& password) {
    if (username.empty() || password.empty()) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_accounts.count(username) > 0) return false;
    m_accounts[username] = HashPassword(password);
    Logger::Info("MobileServerHost", "Registered account: " + username);
    return true;
}

bool MobileServerHost::Authenticate(const std::string& username, const std::string& password) {
    if (username.empty() || password.empty()) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_accounts.find(username);
    if (it == m_accounts.end()) return false;
    return it->second == HashPassword(password);
}

bool MobileServerHost::Start(uint16_t port) {
    if (m_running.load()) return true;
    m_port = port;
    m_listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_listenFd < 0) {
        Logger::Error("MobileServerHost", "Failed to create socket");
        return false;
    }

    int opt = 1;
    ::setsockopt(m_listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(m_port);

    if (::bind(m_listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        Logger::Error("MobileServerHost", "Bind failed on port " + std::to_string(m_port));
        ::close(m_listenFd);
        m_listenFd = -1;
        return false;
    }

    if (::listen(m_listenFd, 8) < 0) {
        Logger::Error("MobileServerHost", "Listen failed");
        ::close(m_listenFd);
        m_listenFd = -1;
        return false;
    }

    m_running.store(true);
    m_thread = std::thread(&MobileServerHost::AcceptLoop, this);
    Logger::Info("MobileServerHost", "Android Server Host listening on 0.0.0.0:" + std::to_string(m_port));
    return true;
}

void MobileServerHost::Stop() {
    if (!m_running.exchange(false)) return;
    if (m_listenFd >= 0) {
        ::shutdown(m_listenFd, SHUT_RDWR);
        ::close(m_listenFd);
        m_listenFd = -1;
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
    Logger::Info("MobileServerHost", "Android Server Host stopped");
}

void MobileServerHost::AcceptLoop() {
    while (m_running.load()) {
        sockaddr_in clientAddr{};
        socklen_t len = sizeof(clientAddr);
        int clientFd = ::accept(m_listenFd, reinterpret_cast<sockaddr*>(&clientAddr), &len);
        if (clientFd < 0) break;
        std::thread(&MobileServerHost::HandleClient, this, clientFd).detach();
    }
}

void MobileServerHost::HandleClient(int clientFd) {
    uint8_t buf[512];
    ssize_t n = ::recv(clientFd, buf, sizeof(buf), 0);
    if (n >= static_cast<ssize_t>(sizeof(EP7NetMsgGeneric))) {
        EP7NetMsgGeneric hdr{};
        std::memcpy(&hdr, buf, sizeof(hdr));
        if (hdr.nType == EP7_NET_MSG_HEARTBEAT_CLIENT_ANS) {
            EP7NetMsgGeneric ack{sizeof(EP7NetMsgGeneric), EP7_NET_MSG_HEARTBEAT_CLIENT_ANS};
            ::send(clientFd, &ack, sizeof(ack), 0);
        } else if (hdr.nType == EP7_NET_MSG_LOGIN_2 && n >= static_cast<ssize_t>(sizeof(EP7NetLoginData))) {
            EP7NetLoginData pkt{};
            std::memcpy(&pkt, buf, sizeof(pkt));
            pkt.szUserid[EP7_USR_ID_LENGTH] = '\0';
            pkt.szPassword[EP7_USR_PASS_LENGTH] = '\0';
            bool ok = Authenticate(pkt.szUserid, pkt.szPassword);
            EP7NetLoginFeedback fb;
            fb.nResult = ok ? EP7_LOGIN_FB_SUB_OK : EP7_LOGIN_FB_SUB_INCORRECT;
            fb.nChaRemain = ok ? 4 : 0;
            ::send(clientFd, &fb, sizeof(fb), 0);
        } else if (hdr.nType == EP7_NET_MSG_REQ_CHA_BINFO && n >= static_cast<ssize_t>(sizeof(EP7NetChaBaInfo))) {
            EP7NetChaBbaInfo bba;
            bba.nChaSNum = 0; // No fake characters; populated only from real DB records
            ::send(clientFd, &bba, sizeof(bba), 0);
        }
    }
    ::close(clientFd);
}

} // namespace RANMobile
