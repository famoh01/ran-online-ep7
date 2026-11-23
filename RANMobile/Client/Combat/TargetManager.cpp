#include "TargetManager.h"
#include <algorithm>

namespace RANMobile {

void TargetManager::SetTarget(const TargetEntity& target) {
    m_currentTarget = target;
    m_hasTarget = true;
}

void TargetManager::ClearTarget() {
    m_hasTarget = false;
    m_currentTarget = TargetEntity{};
}

bool TargetManager::SelectNearest(const std::vector<TargetEntity>& nearbyEntities) {
    if (nearbyEntities.empty()) {
        ClearTarget();
        return false;
    }
    auto it = std::min_element(nearbyEntities.begin(), nearbyEntities.end(),
        [](const TargetEntity& a, const TargetEntity& b) {
            return a.distance < b.distance;
        });
    SetTarget(*it);
    return true;
}

} // namespace RANMobile
