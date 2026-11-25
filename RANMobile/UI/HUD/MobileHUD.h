#pragma once

#include "../Joystick/VirtualJoystick.h"
#include "../Buttons/MobileButton.h"
#include <array>

namespace RANMobile {

class MobileHUD {
public:
    void Initialize(int screenWidth, int screenHeight);
    void SetScreenSize(int width, int height) { Initialize(width, height); }
    void Update(float deltaTime);

    bool OnTouchDown(int32_t pointerId, float x, float y);
    void OnTouchMove(int32_t pointerId, float x, float y);
    void OnTouchUp(int32_t pointerId, float x, float y);

    VirtualJoystick& GetJoystick() { return m_joystick; }
    MobileButton& GetAttackButton() { return m_attackBtn; }
    MobileButton& GetTargetButton() { return m_targetBtn; }
    MobileButton& GetPotionButton() { return m_potionBtn; }
    MobileButton& GetJumpButton() { return m_jumpBtn; }
    MobileButton& GetPickupButton() { return m_pickupBtn; }
    MobileButton& GetSkillButton(size_t index) { return m_skillBtns[index]; }

private:
    VirtualJoystick m_joystick;
public:
    void Draw(class MobileRenderer* renderer);
private:
    MobileButton m_attackBtn{MobileButtonType::Attack};
    MobileButton m_targetBtn{MobileButtonType::Target};
    MobileButton m_potionBtn{MobileButtonType::Potion};
    MobileButton m_jumpBtn{MobileButtonType::Jump};
    MobileButton m_pickupBtn{MobileButtonType::Pickup};
    std::array<MobileButton, 9> m_skillBtns;
};

} // namespace RANMobile
