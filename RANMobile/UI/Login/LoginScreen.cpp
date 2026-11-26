#include "LoginScreen.h"
#include "../../Client/Login/AuthenticationManager.h"
#include "../../Engine/Renderer/MobileRenderer.h"
#include "../../Engine/Core/Logger.h"

#include <algorithm>
#include <cmath>

namespace UI = RANMobile::UI;

void LoginScreen::Initialize(RANMobile::NetworkClient* net) {
    m_networkClient = net;
    m_serverDisplayName = RANMobile::AuthenticationManager::GetServerDisplayName();
    SetAuthState(LoginAuthState::Disconnected, "DISCONNECTED - " + m_serverDisplayName);
    RANMobile::Logger::Info("LoginScreen", "Initialized login UI: " + m_gameTitle + " | Server: " + m_serverDisplayName);
}

void LoginScreen::SetAuthState(LoginAuthState state, const std::string& message) {
    m_authState = state;
    m_statusMessage = message;
}

void LoginScreen::SetSafeAreaInsets(float left, float top, float right, float bottom) {
    m_safeLeft = std::max(0.0f, left);
    m_safeTop = std::max(0.0f, top);
    m_safeRight = std::max(0.0f, right);
    m_safeBottom = std::max(0.0f, bottom);
}

LoginScreen::LoginLayout LoginScreen::ComputeLayout() const {
    LoginLayout layout;
    layout.canvasW = m_screenW;
    layout.canvasH = m_screenH;
    layout.portrait = m_screenH > m_screenW;

    const float cw = std::max(320.0f, m_screenW - m_safeLeft - m_safeRight);
    const float ch = std::max(480.0f, m_screenH - m_safeTop - m_safeBottom);
    const float left = m_safeLeft;
    const float top = m_safeTop;

    if (!layout.portrait) {
        // Landscape keeps the original EP7 control anchors while applying the
        // shared visual system and a 48-unit minimum where space allows.
        const float panelW = cw * 0.46f;
        const float panelH = ch * 0.72f;
        const float px = left + (cw - panelW) * 0.5f;
        const float py = top + (ch - panelH) * 0.5f;
        layout.panel = {px, py, panelW, panelH};
        layout.username = {px + 30.0f, py + 110.0f, panelW - 60.0f, 44.0f};
        layout.password = {px + 30.0f, py + 170.0f, panelW - 60.0f, 44.0f};
        layout.passwordToggle = {px + panelW - 80.0f, py + 170.0f, 50.0f, 44.0f};
        layout.remember = {px + 30.0f, py + 230.0f, panelW - 60.0f, 44.0f};
        layout.login = {px + 30.0f, py + 300.0f, (panelW - 75.0f) * 0.55f, 48.0f};
        layout.reg = {px + 45.0f + (panelW - 75.0f) * 0.55f, py + 300.0f,
                      (panelW - 75.0f) * 0.45f, 48.0f};
        layout.settings = {px + 30.0f, py + 365.0f, (panelW - 75.0f) * 0.5f, 48.0f};
        layout.exit = {px + 45.0f + (panelW - 75.0f) * 0.5f, py + 365.0f,
                       (panelW - 75.0f) * 0.5f, 48.0f};
        layout.status = {px + 30.0f, py + panelH - 58.0f, panelW - 60.0f, 38.0f};
    } else {
        // Portrait reflow: full-width stacked controls with larger targets.
        const float panelW = cw - 48.0f;
        const float panelH = std::min(920.0f, ch - 120.0f);
        const float px = left + 24.0f;
        const float py = top + std::max(28.0f, (ch - panelH) * 0.5f);
        layout.panel = {px, py, panelW, panelH};
        layout.username = {px + 24.0f, py + 140.0f, panelW - 48.0f, 56.0f};
        layout.password = {px + 24.0f, py + 244.0f, panelW - 48.0f, 56.0f};
        layout.passwordToggle = {px + panelW - 96.0f, py + 244.0f, 72.0f, 56.0f};
        layout.remember = {px + 24.0f, py + 324.0f, panelW - 48.0f, 56.0f};
        layout.login = {px + 24.0f, py + 410.0f, panelW - 48.0f, 64.0f};
        layout.reg = {px + 24.0f, py + 490.0f, panelW - 48.0f, 64.0f};
        layout.settings = {px + 24.0f, py + 580.0f, panelW - 48.0f, 56.0f};
        layout.exit = {px + 24.0f, py + 652.0f, panelW - 48.0f, 56.0f};
        layout.status = {px + 24.0f, py + 830.0f, panelW - 48.0f, 48.0f};
    }
    return layout;
}

UI::UIRect LoginScreen::GetPasswordToggleRect() const {
    return ComputeLayout().passwordToggle;
}

UI::UIRect LoginScreen::GetLoginButtonRect() const {
    return ComputeLayout().login;
}

LoginScreen::LoginControl LoginScreen::HitControl(float x, float y, const LoginLayout& l) const {
    if (m_showExitConfirm) {
        const float w = l.portrait ? std::min(l.panel.width - 40.0f, 420.0f) : 420.0f;
        const float h = 200.0f;
        const float dx = (l.canvasW - w) * 0.5f;
        const float dy = (l.canvasH - h) * 0.5f;
        if (UI::HitTest({dx + 30.0f, dy + 120.0f, (w - 90.0f) * 0.5f, 48.0f}, x, y)) return LoginControl::ConfirmExit;
        if (UI::HitTest({dx + 60.0f + (w - 90.0f) * 0.5f, dy + 120.0f, (w - 90.0f) * 0.5f, 48.0f}, x, y)) return LoginControl::CancelExit;
        return LoginControl::None;
    }

    if (m_showSettingsPanel) {
        const float w = l.portrait ? std::min(l.panel.width - 40.0f, 460.0f) : 460.0f;
        const float h = l.portrait ? 420.0f : 320.0f;
        const float sx = (l.canvasW - w) * 0.5f;
        const float sy = (l.canvasH - h) * 0.5f;
        const float rowW = w - 60.0f;
        if (UI::HitTest({sx + 30.0f, sy + 75.0f, rowW, 44.0f}, x, y)) return LoginControl::Sound;
        if (UI::HitTest({sx + 30.0f, sy + 130.0f, rowW, 44.0f}, x, y)) return LoginControl::Music;
        if (UI::HitTest({sx + 30.0f, sy + 185.0f, rowW, 44.0f}, x, y)) return LoginControl::Graphics;
        if (UI::HitTest({sx + 130.0f, sy + (l.portrait ? 340.0f : 250.0f), 200.0f, 48.0f}, x, y)) return LoginControl::CloseDialog;
        return LoginControl::None;
    }

    if (m_showRegisterInfo) {
        const float w = l.portrait ? std::min(l.panel.width - 40.0f, 460.0f) : 460.0f;
        const float h = 220.0f;
        const float dx = (l.canvasW - w) * 0.5f;
        const float dy = (l.canvasH - h) * 0.5f;
        if (UI::HitTest({dx + 130.0f, dy + 145.0f, 200.0f, 48.0f}, x, y)) return LoginControl::CloseDialog;
        return LoginControl::None;
    }

    if (UI::HitTest(l.username, x, y)) return LoginControl::Username;
    if (UI::HitTest(l.passwordToggle, x, y)) return LoginControl::PasswordToggle;
    if (UI::HitTest(l.password, x, y)) return LoginControl::Password;
    if (UI::HitTest(l.remember, x, y)) return LoginControl::Remember;
    if (UI::HitTest(l.login, x, y)) return LoginControl::Login;
    if (UI::HitTest(l.reg, x, y)) return LoginControl::Register;
    if (UI::HitTest(l.settings, x, y)) return LoginControl::Settings;
    if (UI::HitTest(l.exit, x, y)) return LoginControl::Exit;
    return LoginControl::None;
}

void LoginScreen::ApplyControl(LoginControl control) {
    switch (control) {
        case LoginControl::Username: m_focus = LoginFocus::Username; break;
        case LoginControl::Password: m_focus = LoginFocus::Password; break;
        case LoginControl::PasswordToggle: m_showPassword = !m_showPassword; break;
        case LoginControl::Remember: m_rememberAccount = !m_rememberAccount; break;
        case LoginControl::Login: m_focus = LoginFocus::None; OnLoginPressed(); break;
        case LoginControl::Register: m_focus = LoginFocus::None; m_showRegisterInfo = true; break;
        case LoginControl::Settings: m_focus = LoginFocus::None; m_showSettingsPanel = true; break;
        case LoginControl::Exit: m_focus = LoginFocus::None; m_showExitConfirm = true; break;
        case LoginControl::Sound: m_soundEnabled = !m_soundEnabled; break;
        case LoginControl::Music: m_musicEnabled = !m_musicEnabled; break;
        case LoginControl::Graphics: m_graphicsLevel = (m_graphicsLevel + 1) % 4; break;
        case LoginControl::CloseDialog: m_showSettingsPanel = false; m_showRegisterInfo = false; break;
        case LoginControl::ConfirmExit: m_exitRequested = true; break;
        case LoginControl::CancelExit: m_showExitConfirm = false; break;
        case LoginControl::None: m_focus = LoginFocus::None; break;
    }
}

void LoginScreen::HandleTouchDown(int pointerId, float x, float y) {
    m_activePointerId = pointerId;
    const LoginLayout layout = ComputeLayout();
    m_pressedControl = HitControl(x, y, layout);
    ApplyControl(m_pressedControl);
}

void LoginScreen::HandleTouchMove(int pointerId, float x, float y) {
    (void)pointerId;
    (void)x;
    (void)y;
    // Buttons use press-and-release on the same control; dragging off the
    // original control cancels the visual pressed state.
    if (m_pressedControl != LoginControl::None) {
        const LoginLayout layout = ComputeLayout();
        if (HitControl(x, y, layout) != m_pressedControl) m_pressedControl = LoginControl::None;
    }
}

void LoginScreen::HandleTouchUp(int pointerId, float x, float y) {
    (void)x;
    (void)y;
    if (pointerId == m_activePointerId) {
        m_activePointerId = -1;
        m_pressedControl = LoginControl::None;
    }
}

void LoginScreen::HandleTouchCancel(int pointerId) {
    if (pointerId == m_activePointerId) {
        m_activePointerId = -1;
        m_pressedControl = LoginControl::None;
    }
}

void LoginScreen::HandleTextInput(const std::string& utf8) {
    if (utf8.empty()) return;
    if (m_focus == LoginFocus::Username && m_username.size() < 20) {
        m_username += utf8;
    } else if (m_focus == LoginFocus::Password && m_password.size() < 20) {
        m_password += utf8;
    }
}

void LoginScreen::HandleBackspace() {
    if (m_focus == LoginFocus::Username && !m_username.empty()) {
        m_username.pop_back();
    } else if (m_focus == LoginFocus::Password && !m_password.empty()) {
        m_password.pop_back();
    }
}

void LoginScreen::Update(float deltaTime) {
    m_animationTime += std::max(0.0f, deltaTime);
}

bool LoginScreen::OnLoginPressed() {
    if (m_username.empty() || m_password.empty()) {
        SetAuthState(LoginAuthState::LoginRejected, "ENTER USERNAME AND PASSWORD");
        return false;
    }
    if (m_authState == LoginAuthState::Authenticating ||
        m_authState == LoginAuthState::AwaitingServerResponse) {
        SetAuthState(m_authState, "LOGIN REQUEST ALREADY PENDING");
        return false;
    }
    SetAuthState(LoginAuthState::Authenticating, "SENDING LOGIN REQUEST...");
    const bool sent = RANMobile::AuthenticationManager::Login(m_networkClient, m_username, m_password);
    if (!sent) {
        SetAuthState(LoginAuthState::ConnectionError, "SERVER NOT CONNECTED");
        return false;
    }
    // A successful socket write is not authentication. No EP7 response handler
    // exists in this phase, so never present this as LOGIN ACCEPTED.
    SetAuthState(LoginAuthState::AwaitingServerResponse,
                 "REQUEST SENT - SERVER RESPONSE NOT IMPLEMENTED");
    return true;
}

void LoginScreen::Draw() {
    Draw(nullptr);
}

void LoginScreen::Draw(RANMobile::MobileRenderer* renderer) {
    if (!renderer) return;
    const LoginLayout layout = ComputeLayout();
    m_screenW = renderer->GetDesignWidth();
    m_screenH = renderer->GetDesignHeight();
    const float w = m_screenW;
    const float h = m_screenH;
    const UI::MobileUITheme theme;

    // Layered background bands approximate depth without inventing game art.
    const float bands = 8.0f;
    for (int i = 0; i < static_cast<int>(bands); ++i) {
        const float t = static_cast<float>(i) / bands;
        renderer->DrawUIQuad(0.0f, h * t, w, h / bands + 1.0f,
                             0.025f + t * 0.03f, 0.05f + t * 0.05f,
                             0.13f + t * 0.08f, 1.0f);
    }
    renderer->DrawUIQuad(0.0f, 0.0f, w, 6.0f, theme.accentR, theme.accentG, theme.accentB, 0.85f);

    const float titleScale = layout.portrait ? 1.15f : 1.3f;
    renderer->DrawUIText(layout.portrait ? (w - renderer->MeasureTextWidth(m_gameTitle, titleScale)) * 0.5f : 36.0f,
                         layout.portrait ? 42.0f : 28.0f, m_gameTitle, titleScale,
                         theme.accentR, theme.accentG, theme.accentB, 1.0f);
    renderer->DrawUIText(layout.portrait ? 36.0f : 38.0f,
                         layout.portrait ? 92.0f : 70.0f,
                         std::string("EP7  |  ") + m_serverDisplayName, 0.78f,
                         0.72f, 0.84f, 1.0f, 1.0f);

    // Panel: shadow, border, body, header strip.
    UI::DrawPanel(renderer, {layout.panel.x + 6.0f, layout.panel.y + 8.0f,
                             layout.panel.width, layout.panel.height}, 0.0f, 0.0f, 0.0f, 0.35f);
    UI::DrawPanel(renderer, {layout.panel.x - 2.0f, layout.panel.y - 2.0f,
                             layout.panel.width + 4.0f, layout.panel.height + 4.0f},
                  theme.accentR, theme.accentG, theme.accentB, 0.35f);
    UI::DrawPanel(renderer, layout.panel, theme.panelR, theme.panelG, theme.panelB, 0.94f);
    renderer->DrawUIQuad(layout.panel.x, layout.panel.y, layout.panel.width, 62.0f,
                         0.08f, 0.16f, 0.31f, 1.0f);
    renderer->DrawUIText(layout.panel.x + 28.0f, layout.panel.y + 18.0f,
                         m_serverDisplayName + "  |  ACCOUNT ACCESS", 1.02f,
                         theme.accentR, theme.accentG, theme.accentB, 1.0f);

    const float labelScale = layout.portrait ? 0.92f : 0.86f;
    renderer->DrawUIText(layout.username.x + 2.0f, layout.username.y - 26.0f,
                         layout.portrait ? "ชื่อผู้ใช้ / USERNAME" : "ชื่อผู้ใช้",
                         labelScale, 0.78f, 0.88f, 1.0f, 1.0f);
    renderer->DrawUIText(layout.password.x + 2.0f, layout.password.y - 26.0f,
                         layout.portrait ? "รหัสผ่าน / PASSWORD" : "รหัสผ่าน",
                         labelScale, 0.78f, 0.88f, 1.0f, 1.0f);

    const auto fieldState = [this](LoginFocus focus) {
        if (m_focus != focus) return UI::ButtonVisualState::Normal;
        return m_pressedControl == LoginControl::None ? UI::ButtonVisualState::Focused
                                                      : UI::ButtonVisualState::Pressed;
    };
    UI::DrawButton(renderer, layout.username, m_username.empty() ? "" : m_username,
                   fieldState(LoginFocus::Username), 0.95f, theme);
    UI::DrawButton(renderer, layout.password, m_password.empty() ? "" : (m_showPassword ? m_password : GetMaskedPassword()),
                   fieldState(LoginFocus::Password), 0.95f, theme);
    UI::DrawButton(renderer, layout.passwordToggle, m_showPassword ? "HIDE" : "SHOW",
                   m_pressedControl == LoginControl::PasswordToggle ? UI::ButtonVisualState::Pressed
                                                                   : UI::ButtonVisualState::Normal,
                   0.72f, theme);

    renderer->DrawUIQuad(layout.remember.x + 2.0f, layout.remember.y + 8.0f, 26.0f, 26.0f,
                         m_rememberAccount ? theme.accentR : 0.10f,
                         m_rememberAccount ? theme.accentG : 0.16f,
                         m_rememberAccount ? theme.accentB : 0.28f, 1.0f);
    if (m_rememberAccount) {
        renderer->DrawUIText(layout.remember.x + 9.0f, layout.remember.y + 8.0f, "X", 0.9f, 0.08f, 0.07f, 0.02f, 1.0f);
    }
    renderer->DrawUIText(layout.remember.x + 38.0f, layout.remember.y + 11.0f,
                         "remember account", 0.78f, 0.82f, 0.88f, 1.0f, 1.0f);

    const bool pending = m_authState == LoginAuthState::Authenticating ||
                         m_authState == LoginAuthState::AwaitingServerResponse;
    UI::DrawButton(renderer, layout.login, "LOGIN / เข้าสู่เกม",
                   pending ? UI::ButtonVisualState::Loading
                           : (m_pressedControl == LoginControl::Login ? UI::ButtonVisualState::Pressed
                                                                     : UI::ButtonVisualState::Normal),
                   0.95f, theme);
    UI::DrawButton(renderer, layout.reg, "REGISTER / สมัคร",
                   m_pressedControl == LoginControl::Register ? UI::ButtonVisualState::Pressed
                                                              : UI::ButtonVisualState::Normal,
                   0.86f, theme);
    UI::DrawButton(renderer, layout.settings, "SETTINGS / ตั้งค่า",
                   m_pressedControl == LoginControl::Settings ? UI::ButtonVisualState::Pressed
                                                             : UI::ButtonVisualState::Normal,
                   0.78f, theme);
    UI::DrawButton(renderer, layout.exit, "EXIT / ออก",
                   m_pressedControl == LoginControl::Exit ? UI::ButtonVisualState::Pressed
                                                          : UI::ButtonVisualState::Normal,
                   0.78f, theme);

    float sr = 0.15f, sg = 0.45f, sb = 0.65f;
    if (m_authState == LoginAuthState::LoginRejected || m_authState == LoginAuthState::ConnectionError) {
        sr = 0.72f; sg = 0.20f; sb = 0.18f;
    } else if (pending) {
        sr = 0.82f; sg = 0.62f; sb = 0.18f;
    } else if (m_authState == LoginAuthState::LoginAccepted) {
        sr = 0.18f; sg = 0.62f; sb = 0.30f;
    }
    UI::DrawStatusPill(renderer, layout.status, m_statusMessage, sr, sg, sb, theme);
    if (m_authState == LoginAuthState::Authenticating) {
        const float phase = std::fmod(m_animationTime * 3.0f, 3.0f);
        for (int i = 0; i < 3; ++i) {
            const float on = (std::fabs(phase - static_cast<float>(i)) < 0.5f) ? 1.0f : 0.35f;
            renderer->DrawUIQuad(layout.status.x + layout.status.width - 70.0f + i * 16.0f,
                                 layout.status.y + 14.0f, 10.0f, 10.0f,
                                 theme.accentR, theme.accentG, theme.accentB, on);
        }
    }
    renderer->DrawUIText(layout.panel.x + 24.0f, layout.panel.y + layout.panel.height - 22.0f,
                         "v1.0.0-ep7  |  PROTOCOL STATUS: UNVERIFIED", 0.66f,
                         0.58f, 0.68f, 0.80f, 1.0f);

    DrawDialogs(renderer, layout);
}

void LoginScreen::DrawDialogs(RANMobile::MobileRenderer* renderer, const LoginLayout& l) {
    const UI::MobileUITheme theme;
    if (m_showSettingsPanel) {
        const float w = l.portrait ? std::min(l.panel.width - 40.0f, 460.0f) : 460.0f;
        const float h = l.portrait ? 420.0f : 320.0f;
        const UI::UIRect dlg = UI::CenteredRect(l.canvasW, l.canvasH, w, h);
        UI::DrawPanel(renderer, {dlg.x - 4.0f, dlg.y - 4.0f, dlg.width + 8.0f, dlg.height + 8.0f},
                      theme.accentR, theme.accentG, theme.accentB, 0.30f);
        UI::DrawPanel(renderer, dlg, 0.05f, 0.11f, 0.23f, 0.98f);
        renderer->DrawUIText(dlg.x + 26.0f, dlg.y + 22.0f, "SETTINGS / ตั้งค่าเกม", 1.0f,
                             theme.accentR, theme.accentG, theme.accentB, 1.0f);
        UI::DrawButton(renderer, {dlg.x + 30.0f, dlg.y + 75.0f, dlg.width - 60.0f, 44.0f},
                       std::string("SOUND FX: ") + (m_soundEnabled ? "ON" : "OFF"),
                       m_pressedControl == LoginControl::Sound ? UI::ButtonVisualState::Pressed : UI::ButtonVisualState::Normal,
                       0.82f, theme);
        UI::DrawButton(renderer, {dlg.x + 30.0f, dlg.y + 130.0f, dlg.width - 60.0f, 44.0f},
                       std::string("MUSIC: ") + (m_musicEnabled ? "ON" : "OFF"),
                       m_pressedControl == LoginControl::Music ? UI::ButtonVisualState::Pressed : UI::ButtonVisualState::Normal,
                       0.82f, theme);
        const char* gfx[] = {"LOW", "MEDIUM", "HIGH", "ULTRA"};
        UI::DrawButton(renderer, {dlg.x + 30.0f, dlg.y + 185.0f, dlg.width - 60.0f, 44.0f},
                       std::string("GRAPHICS: ") + gfx[m_graphicsLevel % 4],
                       m_pressedControl == LoginControl::Graphics ? UI::ButtonVisualState::Pressed : UI::ButtonVisualState::Normal,
                       0.82f, theme);
        UI::DrawButton(renderer, {dlg.x + 130.0f, dlg.y + (l.portrait ? 340.0f : 250.0f), 200.0f, 48.0f},
                       "CLOSE", m_pressedControl == LoginControl::CloseDialog ? UI::ButtonVisualState::Pressed : UI::ButtonVisualState::Normal,
                       0.9f, theme);
    }

    if (m_showExitConfirm) {
        const float w = l.portrait ? std::min(l.panel.width - 40.0f, 420.0f) : 420.0f;
        const UI::UIRect dlg = UI::CenteredRect(l.canvasW, l.canvasH, w, 200.0f);
        UI::DrawPanel(renderer, dlg, 0.05f, 0.10f, 0.22f, 0.98f);
        renderer->DrawUIText(dlg.x + 28.0f, dlg.y + 32.0f, "EXIT RAN ONLINE EP7?", 0.92f,
                             theme.textR, theme.textG, theme.textB, 1.0f);
        const float bw = (w - 90.0f) * 0.5f;
        UI::DrawButton(renderer, {dlg.x + 30.0f, dlg.y + 120.0f, bw, 48.0f}, "YES",
                       m_pressedControl == LoginControl::ConfirmExit ? UI::ButtonVisualState::Pressed : UI::ButtonVisualState::Normal,
                       0.9f, theme);
        UI::DrawButton(renderer, {dlg.x + 60.0f + bw, dlg.y + 120.0f, bw, 48.0f}, "NO",
                       m_pressedControl == LoginControl::CancelExit ? UI::ButtonVisualState::Pressed : UI::ButtonVisualState::Normal,
                       0.9f, theme);
    }

    if (m_showRegisterInfo) {
        const float w = l.portrait ? std::min(l.panel.width - 40.0f, 460.0f) : 460.0f;
        const UI::UIRect dlg = UI::CenteredRect(l.canvasW, l.canvasH, w, 220.0f);
        UI::DrawPanel(renderer, dlg, 0.05f, 0.10f, 0.22f, 0.98f);
        renderer->DrawUIText(dlg.x + 28.0f, dlg.y + 26.0f, "REGISTER / สมัครสมาชิก", 0.94f,
                             theme.accentR, theme.accentG, theme.accentB, 1.0f);
        renderer->DrawUIText(dlg.x + 28.0f, dlg.y + 82.0f,
                             "Register on the FAM HAWAII server website.", 0.76f,
                             theme.textR, theme.textG, theme.textB, 1.0f);
        UI::DrawButton(renderer, {dlg.x + 130.0f, dlg.y + 145.0f, 200.0f, 48.0f}, "CLOSE",
                       m_pressedControl == LoginControl::CloseDialog ? UI::ButtonVisualState::Pressed : UI::ButtonVisualState::Normal,
                       0.9f, theme);
    }
}
