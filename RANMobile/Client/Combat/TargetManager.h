#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace RANMobile {

struct TargetEntity {
    uint32_t globId = 0;
    std::string name;
    uint32_t hp = 0;
    uint32_t maxHp = 0;
    float distance = 0.0f;
};

class TargetManager {
public:
    void SetTarget(const TargetEntity& target);
    void ClearTarget();
    bool HasTarget() const { return m_hasTarget; }
    const TargetEntity& GetTarget() const { return m_currentTarget; }

    bool SelectNearest(const std::vector<TargetEntity>& nearbyEntities);

private:
    bool m_hasTarget = false;
    TargetEntity m_currentTarget;
};

} // namespace RANMobile
