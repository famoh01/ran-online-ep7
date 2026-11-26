#pragma once

class VirtualJoystick;

namespace RANMobile {

class MobileCamera;

class CharacterController {
public:
    void Update(float deltaTime, const VirtualJoystick& joystick, const MobileCamera& camera);
    void SetPosition(float x, float y, float z) { m_posX = x; m_posY = y; m_posZ = z; }

    float GetPosX() const { return m_posX; }
    float GetPosY() const { return m_posY; }
    float GetPosZ() const { return m_posZ; }
    bool IsMoving() const { return m_isMoving; }
    bool IsRunning() const { return m_isRunning; }

private:
    float m_posX = 0.0f;
    float m_posY = 0.0f;
    float m_posZ = 0.0f;
    float m_walkSpeed = 4.0f;
    float m_runSpeed = 8.0f;
    bool m_isMoving = false;
    bool m_isRunning = false;
};

} // namespace RANMobile
