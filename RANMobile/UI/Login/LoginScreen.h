#pragma once

#include <string>
#include "../Foundation/MobileUIKit.h"

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
    ConnectionError,
    // Request was written to the socket, but no EP7 response handler exists.
    // This is deliberately NOT a successful login.
    AwaitingServerResponse
};

class LoginScreen {
public:
    void Initialize(RANMobile::NetworkClient* net);

    void SetUsername(const std::string& username) { m_username = username; }
    void SetPassword(const std::string& password) { m_password = password; }
    void SetRememberAccount(bool remember) { m_rememberAccount = remember; }
    void ToggleShowPassword() { m_showPassword = !m_showPassword; }
    bool IsPasswordVisible() const { return m_showPassword; }

    const std::string& GetGameTitle() const { return m_gameTitle; }
    const std::string& GetServerDisplayName() const { return m_serverDisplayName; }
    const std::string& GetUsername() const { return m_username; }
    std::string GetMaskedPassword() const { return std::string(m_password.size(), '*'); }
    bool GetRememberAccount() const { return m_rememberAccount; }
    LoginFocus GetFocus() const { return m_focus; }
    LoginAuthState GetAuthState() const { return m_authState; }
    const std::string& GetStatusMessage() const { return m_statusMessage; }
    bool WantsExit() const { return m_exitRequested; }
    bool IsSettingsOpen() const { return m_showSettingsPanel; }
    bool IsExitConfirmOpen() const { return m_showExitConfirm; }
    bool IsSoundEnabled() const { return m_soundEnabled; }
    bool IsMusicEnabled() const { return m_musicEnabled; }
    int GetGraphicsLevel() const { return m_graphicsLevel; }
    bool IsPortraitLayout() const { return m_screenH > m_screenW; }

    // Presentation-only state; never marks a login as accepted.
    void SetAuthState(LoginAuthState state, const std::string& message);
    void SetSafeAreaInsets(float left, float top, float right, float bottom);

    // Exposed for focused input tests; coordinates are virtual design units.
    RANMobile::UI::UIRect GetPasswordToggleRect() const;
    RANMobile::UI::UIRect GetLoginButtonRect() const;

    void HandleTouchDown(int pointerId, float x, float y);
    void HandleTouchMove(int pointerId, float x, float y);
    void HandleTouchUp(int pointerId, float x, float y);
    void HandleTouchCancel(int pointerId);
    void CancelInteraction() { m_activePointerId = -1; m_pressedControl = LoginControl::None; }
    void HandleTextInput(const std::string& utf8);
    void HandleBackspace();
    void Update(float deltaTime);

    bool OnLoginPressed();
    void Draw();
    void Draw(RANMobile::MobileRenderer* renderer);

private:
    enum class LoginControl {
        None, Username, Password, PasswordToggle, Remember, Login, Register,
        Settings, Exit, Sound, Music, Graphics, CloseDialog, ConfirmExit, CancelExit
    };

    struct LoginLayout {
        float canvasW = 1280.0f;
        float canvasH = 720.0f;
        bool portrait = false;
        RANMobile::UI::UIRect panel;
        RANMobile::UI::UIRect username;
        RANMobile::UI::UIRect password;
        RANMobile::UI::UIRect passwordToggle;
        RANMobile::UI::UIRect remember;
        RANMobile::UI::UIRect login;
        RANMobile::UI::UIRect reg;
        RANMobile::UI::UIRect settings;
        RANMobile::UI::UIRect exit;
        RANMobile::UI::UIRect status;
    };

    LoginLayout ComputeLayout() const;
    LoginControl HitControl(float x, float y, const LoginLayout& layout) const;
    void ApplyControl(LoginControl control);
    void DrawDialogs(RANMobile::MobileRenderer* renderer, const LoginLayout& layout);

    RANMobile::NetworkClient* m_networkClient = nullptr;
    std::string m_gameTitle = "RAN ONLINE EP7";
    std::string m_serverDisplayName = "FAM HAWAII";
    std::string m_username;
    std::string m_password;
    bool m_rememberAccount = false;
    bool m_showPassword = false;
    bool m_showSettingsPanel = false;
    bool m_showExitConfirm = false;
    bool m_showRegisterInfo = false;
    bool m_soundEnabled = true;
    bool m_musicEnabled = true;
    int m_graphicsLevel = 1; // 0=LOW, 1=MED, 2=HIGH, 3=ULTRA

    LoginFocus m_focus = LoginFocus::None;
    LoginAuthState m_authState = LoginAuthState::Disconnected;
    std::string m_statusMessage = "DISCONNECTED";
    bool m_exitRequested = false;

    float m_screenW = 1280.0f;
    float m_screenH = 720.0f;
    float m_animationTime = 0.0f;
    float m_safeLeft = 0.0f;
    float m_safeTop = 0.0f;
    float m_safeRight = 0.0f;
    float m_safeBottom = 0.0f;
    int m_activePointerId = -1;
    LoginControl m_pressedControl = LoginControl::None;
};
