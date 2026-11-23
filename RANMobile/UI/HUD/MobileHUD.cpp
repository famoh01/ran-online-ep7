#include "MobileHUD.h"

namespace RANMobile {

void MobileHUD::Initialize(int screenWidth, int screenHeight) {
    float w = static_cast<float>(screenWidth);
    float h = static_cast<float>(screenHeight);

    m_joystick.SetCenter(w * 0.14f, h * 0.72f);
    m_joystick.SetRadius(h * 0.14f);

    m_attackBtn.SetPosition(w - 150.0f, h - 150.0f);
    m_attackBtn.SetSize(110.0f, 110.0f);

    m_targetBtn.SetPosition(w - 270.0f, h - 120.0f);
    m_targetBtn.SetSize(80.0f, 80.0f);

    m_potionBtn.SetPosition(w - 130.0f, h - 270.0f);
    m_potionBtn.SetSize(75.0f, 75.0f);

    m_jumpBtn.SetPosition(w - 220.0f, h - 270.0f);
    m_jumpBtn.SetSize(70.0f, 70.0f);

    m_pickupBtn.SetPosition(w - 300.0f, h - 270.0f);
    m_pickupBtn.SetSize(70.0f, 70.0f);

    for (size_t i = 0; i < m_skillBtns.size(); ++i) {
        m_skillBtns[i].SetType(static_cast<MobileButtonType>(static_cast<int>(MobileButtonType::Skill1) + i));
        m_skillBtns[i].SetPosition(w - 360.0f + static_cast<float>(i % 3) * 85.0f,
                                   h - 240.0f - static_cast<float>(i / 3) * 85.0f);
        m_skillBtns[i].SetSize(75.0f, 75.0f);
    }
}

void MobileHUD::Update(float deltaTime) {
    m_attackBtn.Update(deltaTime);
    m_targetBtn.Update(deltaTime);
    m_potionBtn.Update(deltaTime);
    m_jumpBtn.Update(deltaTime);
    m_pickupBtn.Update(deltaTime);
    for (auto& btn : m_skillBtns) {
        btn.Update(deltaTime);
    }
}

bool MobileHUD::OnTouchDown(int32_t pointerId, float x, float y) {
    if (m_joystick.TouchDown(pointerId, x, y)) return true;
    if (m_attackBtn.OnTouchDown(pointerId, x, y)) return true;
    if (m_targetBtn.OnTouchDown(pointerId, x, y)) return true;
    if (m_potionBtn.OnTouchDown(pointerId, x, y)) return true;
    if (m_jumpBtn.OnTouchDown(pointerId, x, y)) return true;
    if (m_pickupBtn.OnTouchDown(pointerId, x, y)) return true;
    for (auto& btn : m_skillBtns) {
        if (btn.OnTouchDown(pointerId, x, y)) return true;
    }
    return false;
}

void MobileHUD::OnTouchMove(int32_t pointerId, float x, float y) {
    m_joystick.TouchMove(pointerId, x, y);
    m_attackBtn.OnTouchMove(pointerId, x, y);
    m_targetBtn.OnTouchMove(pointerId, x, y);
    m_potionBtn.OnTouchMove(pointerId, x, y);
    m_jumpBtn.OnTouchMove(pointerId, x, y);
    m_pickupBtn.OnTouchMove(pointerId, x, y);
    for (auto& btn : m_skillBtns) {
        btn.OnTouchMove(pointerId, x, y);
    }
}

void MobileHUD::OnTouchUp(int32_t pointerId, float x, float y) {
    m_joystick.TouchUp(pointerId);
    m_attackBtn.OnTouchUp(pointerId, x, y);
    m_targetBtn.OnTouchUp(pointerId, x, y);
    m_potionBtn.OnTouchUp(pointerId, x, y);
    m_jumpBtn.OnTouchUp(pointerId, x, y);
    m_pickupBtn.OnTouchUp(pointerId, x, y);
    for (auto& btn : m_skillBtns) {
        btn.OnTouchUp(pointerId, x, y);
    }
}

} // namespace RANMobile
