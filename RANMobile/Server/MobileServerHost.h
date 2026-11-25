#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace RANMobile {

class MobileServerHost {
public:
    MobileServerHost() = default;
    ~MobileServerHost();

    bool Start(uint16_t port = 5001);
    void Stop();
    bool IsRunning() const { return m_running.load(); }
    uint16_t GetPort() const { return m_port; }

    bool RegisterAccount(const std::string& username, const std::string& password);
    bool Authenticate(const std::string& username, const std::string& password);

private:
    void AcceptLoop();
    void HandleClient(int clientFd);
    static uint64_t HashPassword(const std::string& password);

    std::atomic<bool> m_running{false};
    int m_listenFd = -1;
    uint16_t m_port = 5001;
    std::thread m_thread;
    std::mutex m_mutex;
    std::unordered_map<std::string, uint64_t> m_accounts;
};

} // namespace RANMobile
