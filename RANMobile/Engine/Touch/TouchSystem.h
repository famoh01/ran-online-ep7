#pragma once

#include <cstdint>
#include <unordered_map>
#include <functional>

namespace RANMobile {

struct TouchPoint {
    int32_t pointerId = -1;
    float x = 0.0f;
    float y = 0.0f;
    float startX = 0.0f;
    float startY = 0.0f;
    bool active = false;
};

class TouchSystem {
public:
    void OnTouchDown(int32_t pointerId, float x, float y);
    void OnTouchMove(int32_t pointerId, float x, float y);
    void OnTouchUp(int32_t pointerId, float x, float y);

    size_t GetActiveTouchCount() const { return m_touches.size(); }
    const TouchPoint* GetTouch(int32_t pointerId) const;
    void Clear();

private:
    std::unordered_map<int32_t, TouchPoint> m_touches;
};

} // namespace RANMobile
