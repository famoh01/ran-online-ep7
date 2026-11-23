#pragma once

#include "TargetManager.h"
#include <cstdint>

namespace RANMobile {

class NetworkClient;

class CombatController {
public:
    void Initialize(NetworkClient* net, TargetManager* targetMgr);

    bool RequestBasicAttack(float attackRange);
    bool RequestSkillAttack(int32_t skillId, float skillRange);

private:
    NetworkClient* m_networkClient = nullptr;
    TargetManager* m_targetManager = nullptr;
};

} // namespace RANMobile
