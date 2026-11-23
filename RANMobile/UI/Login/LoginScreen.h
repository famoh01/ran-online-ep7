#pragma once

#include <string>

namespace RANMobile {
    class NetworkClient;
    class MobileRenderer;
}

class LoginScreen {
public:
    void Initialize(RANMobile::NetworkClient* net);

    void SetUsername(const std::string& username) { m_username = username; }
    void SetPassword(const std::string& password) { m_password = password; }
    void SetRememberAccount(bool remember) { m_rememberAccount = remember; }

    const std::string& GetGameTitle() const { return m_gameTitle; }
    const std::string& GetServerDisplayName() const { return m_serverDisplayName; }
    const std::string& GetUsername() const { return m_username; }
    bool GetRememberAccount() const { return m_rememberAccount; }

    bool OnLoginPressed();
    void Draw();
    void Draw(RANMobile::MobileRenderer* renderer);

private:
    RANMobile::NetworkClient* m_networkClient = nullptr;
    std::string m_gameTitle = "RAN ONLINE EP7";
    std::string m_serverDisplayName = "FAM HAWAII";
    std::string m_username;
    std::string m_password;
    bool m_rememberAccount = false;
};
