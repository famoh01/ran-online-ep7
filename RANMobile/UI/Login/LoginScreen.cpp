#include "LoginScreen.h"
#include "../../Client/Login/AuthenticationManager.h"
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
    // Rendered by MobileRenderer UI pass:
    // Header: RAN ONLINE EP7
    // Server: FAM HAWAII
    // Fields: Username, Password, Remember Account, Login, Settings, Exit
}
