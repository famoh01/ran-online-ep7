#pragma once

#include <string>

namespace RANMobile {
    class NetworkClient;
    class MobileRenderer;
}

enum class LoginFocus {
    None,
    Username,
    Password
};

enum class LoginAuthState {
    Disconnected,
    Connecting,
    Connected,
    Authenticating,
    LoginAccepted,
    LoginRejected,
    ConnectionError
};

class LoginScreen {
public:
    void Initialize(RANMobile::NetworkClient* net);

    void SetUsername(const std::string& username) { m_username = username; }
    void SetPassword(const std::string& password) { m_password = password; }
    void SetRememberAccount(bool remember) { m_rememberAccount = remember; }

    const std::string& GetGameTitle() const { return m_gameTitle; }
    const std::string& GetServerDisplayName() const { return m_serverDisplayName; }
    const std::string& GetUsername() const { return m_username; }
    std::string GetMaskedPassword() const { return std::string(m_password.size(), '*'); }
    bool GetRememberAccount() const { return m_rememberAccount; }
    LoginFocus GetFocus() const { return m_focus; }
    LoginAuthState GetAuthState() const { return m_authState; }
    const std::string& GetStatusMessage() const { return m_statusMessage; }
    bool WantsExit() const { return m_exitRequested; }

    void HandleTouchDown(int pointerId, float x, float y);
    void HandleTouchMove(int pointerId, float x, float y);
    void HandleTouchUp(int pointerId, float x, float y);
    void HandleTextInput(const std::string& utf8);
    void HandleBackspace();
    void Update(float deltaTime);

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

    LoginFocus m_focus = LoginFocus::None;
    LoginAuthState m_authState = LoginAuthState::Disconnected;
    std::string m_statusMessage = "DISCONNECTED";
    bool m_exitRequested = false;

    float m_screenW = 1280.0f;
    float m_screenH = 720.0f;
    int m_activePointerId = -1;
};
