#include "LoginScreen.h"
#include "../../Engine/Network/NetworkClient.h"
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

    if (m_showExitConfirm) {
        float dlgW = 420.0f, dlgH = 200.0f;
        float dx = (m_screenW - dlgW) * 0.5f;
        float dy = (m_screenH - dlgH) * 0.5f;
        if (HitRect(x, y, dx + 30.0f, dy + 120.0f, 165.0f, 48.0f)) {
            m_exitRequested = true;
        } else if (HitRect(x, y, dx + 225.0f, dy + 120.0f, 165.0f, 48.0f)) {
            m_showExitConfirm = false;
        }
        return;
    }

    if (m_showSettingsPanel) {
        float spW = 460.0f, spH = 320.0f;
        float sx = (m_screenW - spW) * 0.5f;
        float sy = (m_screenH - spH) * 0.5f;
        if (HitRect(x, y, sx + 30.0f, sy + 75.0f, spW - 60.0f, 44.0f)) {
            m_soundEnabled = !m_soundEnabled;
        } else if (HitRect(x, y, sx + 30.0f, sy + 130.0f, spW - 60.0f, 44.0f)) {
            m_musicEnabled = !m_musicEnabled;
        } else if (HitRect(x, y, sx + 30.0f, sy + 185.0f, spW - 60.0f, 44.0f)) {
            m_graphicsLevel = (m_graphicsLevel + 1) % 4;
        } else if (HitRect(x, y, sx + 130.0f, sy + 250.0f, 200.0f, 46.0f)) {
            m_showSettingsPanel = false;
        }
        return;
    }

    if (m_showRegisterInfo) {
        float dlgW = 460.0f, dlgH = 220.0f;
        float dx = (m_screenW - dlgW) * 0.5f;
        float dy = (m_screenH - dlgH) * 0.5f;
        if (HitRect(x, y, dx + 130.0f, dy + 145.0f, 200.0f, 46.0f)) {
            m_showRegisterInfo = false;
        }
        return;
    }

    if (HitRect(x, y, px + 30.0f, py + 110.0f, panelW - 60.0f, 44.0f)) {
        m_focus = LoginFocus::Username;
    } else if (HitRect(x, y, px + panelW - 80.0f, py + 170.0f, 50.0f, 44.0f)) {
        m_showPassword = !m_showPassword;
    } else if (HitRect(x, y, px + 30.0f, py + 170.0f, panelW - 110.0f, 44.0f)) {
        m_focus = LoginFocus::Password;
    } else if (HitRect(x, y, px + 30.0f, py + 230.0f, panelW - 60.0f, 44.0f)) {
        m_rememberAccount = !m_rememberAccount;
    } else if (HitRect(x, y, px + 30.0f, py + 300.0f, (panelW - 75.0f) * 0.55f, 48.0f)) {
        m_focus = LoginFocus::None;
        OnLoginPressed();
    } else if (HitRect(x, y, px + 45.0f + (panelW - 75.0f) * 0.55f, py + 300.0f, (panelW - 75.0f) * 0.45f, 48.0f)) {
        m_focus = LoginFocus::None;
        m_showRegisterInfo = true;
    } else if (HitRect(x, y, px + 30.0f, py + 365.0f, (panelW - 75.0f) * 0.5f, 42.0f)) {
        m_focus = LoginFocus::None;
        m_showSettingsPanel = true;
    } else if (HitRect(x, y, px + 45.0f + (panelW - 75.0f) * 0.5f, py + 365.0f, (panelW - 75.0f) * 0.5f, 42.0f)) {
        m_focus = LoginFocus::None;
        m_showExitConfirm = true;
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
    if (m_networkClient && !m_networkClient->IsConnected()) {
        m_networkClient->Connect("127.0.0.1", 5001);
    }
    bool sent = RANMobile::AuthenticationManager::Login(m_networkClient, m_username, m_password);
    if (!sent) {
        m_authState = LoginAuthState::ConnectionError;
        m_statusMessage = "SERVER NOT CONNECTED";
        return false;
    }
    m_statusMessage = "LOGIN OK -> ENTERING GAME";
    m_enterGameRequested = true;
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

    renderer->DrawUIQuad(0.0f, 0.0f, m_screenW, m_screenH, 0.05f, 0.12f, 0.28f, 1.0f);
    renderer->DrawUIText(36.0f, 28.0f, m_gameTitle, 1.3f, 1.0f, 0.85f, 0.25f, 1.0f);

    renderer->DrawUIQuad(px, py, panelW, panelH, 0.06f, 0.11f, 0.22f, 0.90f);
    renderer->DrawUIQuad(px + 20.0f, py + 18.0f, panelW - 40.0f, 54.0f, 0.10f, 0.18f, 0.34f, 1.0f);
    renderer->DrawUIText(px + 40.0f, py + 26.0f, m_serverDisplayName + " SERVER", 1.15f, 1.0f, 0.82f, 0.20f, 1.0f);

    renderer->DrawUIText(px + 30.0f, py + 86.0f, "ชื่อผู้ใช้", 0.9f, 0.85f, 0.92f, 1.0f, 1.0f);
    renderer->DrawUIQuad(px + 30.0f, py + 110.0f, panelW - 60.0f, 44.0f,
                         m_focus == LoginFocus::Username ? 0.12f : 0.05f, 0.10f, 0.20f, 1.0f);
    renderer->DrawUIText(px + 40.0f, py + 122.0f, m_username, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);

    renderer->DrawUIText(px + 30.0f, py + 150.0f, "รหัสผ่าน", 0.9f, 0.85f, 0.92f, 1.0f, 1.0f);
    renderer->DrawUIQuad(px + 30.0f, py + 170.0f, panelW - 60.0f, 44.0f,
                         m_focus == LoginFocus::Password ? 0.12f : 0.05f, 0.10f, 0.20f, 1.0f);
    renderer->DrawUIText(px + 40.0f, py + 182.0f, m_showPassword ? m_password : GetMaskedPassword(), 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);

    renderer->DrawUIQuad(px + 30.0f, py + 230.0f, panelW - 60.0f, 44.0f, 0.09f, 0.15f, 0.28f, 1.0f);
    renderer->DrawUIText(px + 45.0f, py + 242.0f, "เลือกเซิร์ฟเวอร์: " + m_serverDisplayName, 0.9f, 1.0f, 1.0f, 1.0f, 1.0f);

    renderer->DrawUIQuad(px + 30.0f, py + 300.0f, (panelW - 75.0f) * 0.55f, 48.0f, 0.92f, 0.65f, 0.12f, 1.0f);
    renderer->DrawUIText(px + 65.0f, py + 314.0f, "เข้าสู่เกม", 1.05f, 0.10f, 0.08f, 0.02f, 1.0f);

    renderer->DrawUIQuad(px + 45.0f + (panelW - 75.0f) * 0.55f, py + 300.0f, (panelW - 75.0f) * 0.45f, 48.0f, 0.15f, 0.45f, 0.78f, 1.0f);
    renderer->DrawUIText(px + 65.0f + (panelW - 75.0f) * 0.55f, py + 314.0f, "สมัครไอดี", 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);

    renderer->DrawUIQuad(px + 30.0f, py + 365.0f, (panelW - 75.0f) * 0.5f, 42.0f, 0.12f, 0.28f, 0.55f, 1.0f);
    renderer->DrawUIText(px + 45.0f, py + 376.0f, "ตั้งค่าเกม", 0.9f, 1.0f, 1.0f, 1.0f, 1.0f);

    renderer->DrawUIQuad(px + 45.0f + (panelW - 75.0f) * 0.5f, py + 365.0f, (panelW - 75.0f) * 0.5f, 42.0f, 0.12f, 0.24f, 0.45f, 1.0f);
    renderer->DrawUIText(px + 65.0f + (panelW - 75.0f) * 0.5f, py + 376.0f, "ออกจากเกม", 0.9f, 1.0f, 1.0f, 1.0f, 1.0f);

    renderer->DrawUIText(px + 30.0f, py + panelH - 30.0f, "Version : 1.0.0 | " + m_statusMessage, 0.8f, 0.8f, 0.9f, 1.0f, 1.0f);

    if (m_showSettingsPanel) {
        float spW = 460.0f, spH = 320.0f;
        float sx = (m_screenW - spW) * 0.5f;
        float sy = (m_screenH - spH) * 0.5f;
        renderer->DrawUIQuad(sx, sy, spW, spH, 0.05f, 0.10f, 0.22f, 0.96f);
        renderer->DrawUIText(sx + 30.0f, sy + 24.0f, "ตั้งค่าเกม", 1.1f, 1.0f, 0.85f, 0.25f, 1.0f);
        renderer->DrawUIQuad(sx + 30.0f, sy + 75.0f, spW - 60.0f, 44.0f, 0.10f, 0.20f, 0.38f, 1.0f);
        renderer->DrawUIText(sx + 45.0f, sy + 87.0f, std::string("เสียงเอฟเฟกต์: ") + (m_soundEnabled ? "เปิด" : "ปิด"), 0.9f, 1.0f, 1.0f, 1.0f, 1.0f);
        renderer->DrawUIQuad(sx + 30.0f, sy + 130.0f, spW - 60.0f, 44.0f, 0.10f, 0.20f, 0.38f, 1.0f);
        renderer->DrawUIText(sx + 45.0f, sy + 142.0f, std::string("เพลงประกอบ: ") + (m_musicEnabled ? "เปิด" : "ปิด"), 0.9f, 1.0f, 1.0f, 1.0f, 1.0f);
        const char* gfxNames[] = {"LOW", "MEDIUM", "HIGH", "ULTRA"};
        renderer->DrawUIQuad(sx + 30.0f, sy + 185.0f, spW - 60.0f, 44.0f, 0.10f, 0.20f, 0.38f, 1.0f);
        renderer->DrawUIText(sx + 45.0f, sy + 197.0f, std::string("กราฟิก: ") + gfxNames[m_graphicsLevel % 4], 0.9f, 1.0f, 1.0f, 1.0f, 1.0f);
        renderer->DrawUIQuad(sx + 30.0f, sy + 240.0f, spW - 60.0f, 44.0f, 0.08f, 0.14f, 0.28f, 1.0f);
        renderer->DrawUIText(sx + 45.0f, sy + 252.0f, "Server: 127.0.0.1 : 5001 | CONNECTED", 0.78f, 0.35f, 1.0f, 0.55f, 1.0f);
#ifndef EP7_GIT_SHA
#define EP7_GIT_SHA "dev"
#endif
        renderer->DrawUIText(sx + 30.0f, sy + 292.0f, std::string("Build SHA: ") + EP7_GIT_SHA, 0.75f, 0.75f, 0.85f, 0.90f, 1.0f);
        renderer->DrawUIQuad(sx + 130.0f, sy + 320.0f, 200.0f, 46.0f, 0.92f, 0.65f, 0.12f, 1.0f);
        renderer->DrawUIText(sx + 205.0f, sy + 333.0f, "ปิด", 1.0f, 0.10f, 0.08f, 0.02f, 1.0f);
    }

    if (m_showExitConfirm) {
        float dlgW = 420.0f, dlgH = 200.0f;
        float dx = (m_screenW - dlgW) * 0.5f;
        float dy = (m_screenH - dlgH) * 0.5f;
        renderer->DrawUIQuad(dx, dy, dlgW, dlgH, 0.05f, 0.10f, 0.22f, 0.96f);
        renderer->DrawUIText(dx + 45.0f, dy + 40.0f, "ต้องการออกจากเกมหรือไม่?", 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
        renderer->DrawUIQuad(dx + 30.0f, dy + 120.0f, 165.0f, 48.0f, 0.75f, 0.20f, 0.20f, 1.0f);
        renderer->DrawUIText(dx + 95.0f, dy + 134.0f, "ใช่", 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
        renderer->DrawUIQuad(dx + 225.0f, dy + 120.0f, 165.0f, 48.0f, 0.15f, 0.32f, 0.60f, 1.0f);
        renderer->DrawUIText(dx + 280.0f, dy + 134.0f, "ไม่ใช่", 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    }

    if (m_showRegisterInfo) {
        float dlgW = 460.0f, dlgH = 220.0f;
        float dx = (m_screenW - dlgW) * 0.5f;
        float dy = (m_screenH - dlgH) * 0.5f;
        renderer->DrawUIQuad(dx, dy, dlgW, dlgH, 0.05f, 0.10f, 0.22f, 0.96f);
        renderer->DrawUIText(dx + 35.0f, dy + 35.0f, "สมัครไอดี FAM HAWAII", 1.05f, 1.0f, 0.85f, 0.25f, 1.0f);
        renderer->DrawUIText(dx + 35.0f, dy + 85.0f, "กรุณาสมัครสมาชิกผ่านหน้าเว็บไซต์เซิร์ฟเวอร์", 0.85f, 1.0f, 1.0f, 1.0f, 1.0f);
        renderer->DrawUIQuad(dx + 130.0f, dy + 145.0f, 200.0f, 46.0f, 0.92f, 0.65f, 0.12f, 1.0f);
        renderer->DrawUIText(dx + 205.0f, dy + 158.0f, "ปิด", 1.0f, 0.10f, 0.08f, 0.02f, 1.0f);
    }
}
