#include "CombatController.h"
#include "../../Engine/Network/NetworkClient.h"
#include "../../Engine/Core/Logger.h"

namespace RANMobile {

void CombatController::Initialize(NetworkClient* net, TargetManager* targetMgr) {
    m_networkClient = net;
    m_targetManager = targetMgr;
}

bool CombatController::RequestBasicAttack(float attackRange) {
    if (!m_targetManager || !m_targetManager->HasTarget()) {
        return false;
    }
    const auto& target = m_targetManager->GetTarget();
    if (target.distance > attackRange) {
        return false;
    }
    if (!m_networkClient || !m_networkClient->IsConnected()) {
        Logger::Warn("CombatController", "Cannot send EP7 attack packet: not connected");
        return false;
    }
    return true;
}

bool CombatController::RequestSkillAttack(int32_t skillId, float skillRange) {
    if (skillId < 0 || !m_targetManager || !m_targetManager->HasTarget()) {
        return false;
    }
    if (m_targetManager->GetTarget().distance > skillRange) {
        return false;
    }
    if (!m_networkClient || !m_networkClient->IsConnected()) {
        Logger::Warn("CombatController", "Cannot send EP7 skill packet: not connected");
        return false;
    }
    return true;
}

} // namespace RANMobile
