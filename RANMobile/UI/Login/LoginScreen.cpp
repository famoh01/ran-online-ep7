#include "LoginScreen.h"
#include "../../Client/Login/AuthenticationManager.h"
#include "../../Engine/Renderer/MobileRenderer.h"
#include "../../Engine/Core/Logger.h"

void LoginScreen::Initialize(RANMobile::NetworkClient* net) {
    m_networkClient = net;
    m_serverDisplayName = RANMobile::AuthenticationManager::GetServerDisplayName();
    RANMobile::Logger::Info("LoginScreen", "Initialized LoginScreen: " + m_gameTitle + " | Server: " + m_serverDisplayName);
}

bool LoginScreen::OnLoginPressed() {
    if (m_username.empty() || m_password.empty()) {
        return false;
    }
    return RANMobile::AuthenticationManager::Login(m_networkClient, m_username, m_password);
}

void LoginScreen::Draw() {
    Draw(nullptr);
}

void LoginScreen::Draw(RANMobile::MobileRenderer* renderer) {
    if (!renderer) return;
    float w = static_cast<float>(renderer->GetWidth());
    float h = static_cast<float>(renderer->GetHeight());
    float panelW = w * 0.46f;
    float panelH = h * 0.72f;
    float px = (w - panelW) * 0.5f;
    float py = (h - panelH) * 0.5f;

    // Login panel background (RAN ONLINE EP7 - FAM HAWAII)
    renderer->DrawUIQuad(px, py, panelW, panelH, 0.08f, 0.11f, 0.18f, 0.92f);
    // Title bar (RAN ONLINE EP7 / FAM HAWAII)
    renderer->DrawUIQuad(px + 20.0f, py + 20.0f, panelW - 40.0f, 56.0f, 0.14f, 0.22f, 0.36f, 1.0f);
    // Username & Password input fields
    renderer->DrawUIQuad(px + 30.0f, py + 110.0f, panelW - 60.0f, 44.0f, 0.05f, 0.07f, 0.12f, 1.0f);
    renderer->DrawUIQuad(px + 30.0f, py + 170.0f, panelW - 60.0f, 44.0f, 0.05f, 0.07f, 0.12f, 1.0f);
    // Server selector: FAM HAWAII
    renderer->DrawUIQuad(px + 30.0f, py + 230.0f, panelW - 60.0f, 44.0f, 0.10f, 0.15f, 0.25f, 1.0f);
    // Login, Settings, Exit buttons
    renderer->DrawUIQuad(px + 30.0f, py + 300.0f, panelW - 60.0f, 48.0f, 0.18f, 0.45f, 0.75f, 1.0f);
    renderer->DrawUIQuad(px + 30.0f, py + 365.0f, (panelW - 75.0f) * 0.5f, 42.0f, 0.20f, 0.24f, 0.32f, 1.0f);
    renderer->DrawUIQuad(px + 45.0f + (panelW - 75.0f) * 0.5f, py + 365.0f, (panelW - 75.0f) * 0.5f, 42.0f, 0.45f, 0.18f, 0.18f, 1.0f);
}
