#include "LoginScreen.h"
#include "../../Client/Login/AuthenticationManager.h"
#include "../../Engine/Renderer/MobileRenderer.h"
#include "../../Engine/Core/Logger.h"

void LoginScreen::Initialize(RANMobile::NetworkClient* net) {
    m_networkClient = net;
    m_serverDisplayName = RANMobile::AuthenticationManager::GetServerDisplayName();
    m_authState = LoginAuthState::Disconnected;
    m_statusMessage = "READY - " + m_serverDisplayName;
    RANMobile::Logger::Info("LoginScreen", "Initialized LoginScreen: " + m_gameTitle + " | Server: " + m_serverDisplayName);
}

static bool HitRect(float x, float y, float rx, float ry, float rw, float rh) {
    return x >= rx && x <= (rx + rw) && y >= ry && y <= (ry + rh);
}

void LoginScreen::HandleTouchDown(int pointerId, float x, float y) {
    m_activePointerId = pointerId;
    float panelW = m_screenW * 0.46f;
    float panelH = m_screenH * 0.72f;
    float px = (m_screenW - panelW) * 0.5f;
    float py = (m_screenH - panelH) * 0.5f;

    if (HitRect(x, y, px + 30.0f, py + 110.0f, panelW - 60.0f, 44.0f)) {
        m_focus = LoginFocus::Username;
    } else if (HitRect(x, y, px + 30.0f, py + 170.0f, panelW - 60.0f, 44.0f)) {
        m_focus = LoginFocus::Password;
    } else if (HitRect(x, y, px + 30.0f, py + 230.0f, panelW - 60.0f, 44.0f)) {
        m_rememberAccount = !m_rememberAccount;
    } else if (HitRect(x, y, px + 30.0f, py + 300.0f, panelW - 60.0f, 48.0f)) {
        m_focus = LoginFocus::None;
        OnLoginPressed();
    } else if (HitRect(x, y, px + 45.0f + (panelW - 75.0f) * 0.5f, py + 365.0f, (panelW - 75.0f) * 0.5f, 42.0f)) {
        m_exitRequested = true;
    } else {
        m_focus = LoginFocus::None;
    }
}

void LoginScreen::HandleTouchMove(int pointerId, float x, float y) {
    (void)pointerId; (void)x; (void)y;
}

void LoginScreen::HandleTouchUp(int pointerId, float x, float y) {
    (void)x; (void)y;
    if (pointerId == m_activePointerId) {
        m_activePointerId = -1;
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
    (void)deltaTime;
}

bool LoginScreen::OnLoginPressed() {
    if (m_username.empty() || m_password.empty()) {
        m_authState = LoginAuthState::LoginRejected;
        m_statusMessage = "ENTER USERNAME AND PASSWORD";
        return false;
    }
    m_authState = LoginAuthState::Authenticating;
    bool sent = RANMobile::AuthenticationManager::Login(m_networkClient, m_username, m_password);
    if (!sent) {
        m_authState = LoginAuthState::ConnectionError;
        m_statusMessage = "SERVER NOT CONNECTED";
        return false;
    }
    m_statusMessage = "AUTHENTICATING...";
    return true;
}

void LoginScreen::Draw() {
    Draw(nullptr);
}

void LoginScreen::Draw(RANMobile::MobileRenderer* renderer) {
    if (!renderer) return;
    m_screenW = static_cast<float>(renderer->GetWidth());
    m_screenH = static_cast<float>(renderer->GetHeight());
    float panelW = m_screenW * 0.46f;
    float panelH = m_screenH * 0.72f;
    float px = (m_screenW - panelW) * 0.5f;
    float py = (m_screenH - panelH) * 0.5f;

    renderer->DrawUIQuad(px, py, panelW, panelH, 0.08f, 0.11f, 0.18f, 0.92f);
    renderer->DrawUIQuad(px + 20.0f, py + 20.0f, panelW - 40.0f, 56.0f, 0.14f, 0.22f, 0.36f, 1.0f);
    renderer->DrawUIText(px + 40.0f, py + 32.0f, m_gameTitle, 1.2f, 1.0f, 0.9f, 0.4f, 1.0f);
    renderer->DrawUIText(px + 40.0f, py + 56.0f, m_serverDisplayName, 1.0f, 0.4f, 0.9f, 1.0f, 1.0f);

    renderer->DrawUIText(px + 30.0f, py + 90.0f, "USERNAME", 0.9f, 0.9f, 0.9f, 0.9f, 1.0f);
    renderer->DrawUIQuad(px + 30.0f, py + 110.0f, panelW - 60.0f, 44.0f,
                         m_focus == LoginFocus::Username ? 0.12f : 0.05f, 0.09f, 0.16f, 1.0f);
    renderer->DrawUIText(px + 40.0f, py + 124.0f, m_username, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);

    renderer->DrawUIText(px + 30.0f, py + 154.0f, "PASSWORD", 0.9f, 0.9f, 0.9f, 0.9f, 1.0f);
    renderer->DrawUIQuad(px + 30.0f, py + 170.0f, panelW - 60.0f, 44.0f,
                         m_focus == LoginFocus::Password ? 0.12f : 0.05f, 0.09f, 0.16f, 1.0f);
    renderer->DrawUIText(px + 40.0f, py + 184.0f, GetMaskedPassword(), 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);

    renderer->DrawUIQuad(px + 30.0f, py + 230.0f, panelW - 60.0f, 44.0f, 0.10f, 0.15f, 0.25f, 1.0f);
    renderer->DrawUIText(px + 45.0f, py + 244.0f, "SERVER: " + m_serverDisplayName, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f);

    renderer->DrawUIQuad(px + 30.0f, py + 300.0f, panelW - 60.0f, 48.0f, 0.18f, 0.45f, 0.75f, 1.0f);
    renderer->DrawUIText(px + panelW * 0.42f, py + 316.0f, "LOGIN", 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);

    renderer->DrawUIQuad(px + 30.0f, py + 365.0f, (panelW - 75.0f) * 0.5f, 42.0f, 0.20f, 0.24f, 0.32f, 1.0f);
    renderer->DrawUIText(px + 45.0f, py + 378.0f, "SETTINGS", 0.9f, 1.0f, 1.0f, 1.0f, 1.0f);

    renderer->DrawUIQuad(px + 45.0f + (panelW - 75.0f) * 0.5f, py + 365.0f, (panelW - 75.0f) * 0.5f, 42.0f, 0.45f, 0.18f, 0.18f, 1.0f);
    renderer->DrawUIText(px + 65.0f + (panelW - 75.0f) * 0.5f, py + 378.0f, "EXIT", 0.9f, 1.0f, 1.0f, 1.0f, 1.0f);

    renderer->DrawUIText(px + 30.0f, py + panelH - 30.0f, m_statusMessage, 0.8f, 0.8f, 0.9f, 1.0f, 1.0f);
}
