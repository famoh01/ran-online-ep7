#include "CharacterController.h"
#include "../../UI/Joystick/VirtualJoystick.h"
#include "../../Engine/Camera/MobileCamera.h"
#include <cmath>

namespace RANMobile {

void CharacterController::Update(float deltaTime, const VirtualJoystick& joystick, const MobileCamera& camera) {
    float ax = joystick.GetAxisX();
    float ay = joystick.GetAxisY();
    float mag = joystick.GetMagnitude();

    if (mag < 0.05f) {
        m_isMoving = false;
        m_isRunning = false;
        return;
    }

    m_isMoving = true;
    m_isRunning = joystick.IsRunning();
    float speed = m_isRunning ? m_runSpeed : m_walkSpeed;

    // Convert joystick 2D input relative to camera yaw
    float yawRad = camera.GetYaw() * 3.14159265f / 180.0f;
    float cosYaw = std::cos(yawRad);
    float sinYaw = std::sin(yawRad);

    float worldX = ax * cosYaw - ay * sinYaw;
    float worldZ = ax * sinYaw + ay * cosYaw;

    m_posX += worldX * speed * deltaTime;
    m_posZ += worldZ * speed * deltaTime;
}

EP7NetPcGoto CharacterController::BuildGotoPacket(float tarX, float tarY, float tarZ) const {
    EP7NetPcGoto pkt;
    pkt.dwActState = m_isRunning ? 1u : 0u;
    pkt.vCurPos = { m_posX, m_posY, m_posZ };
    pkt.vTarPos = { tarX, tarY, tarZ };
    return pkt;
}

void CharacterController::ApplyServerGoto(const EP7NetPcGoto& pkt) {
    m_posX = pkt.vCurPos.x;
    m_posY = pkt.vCurPos.y;
    m_posZ = pkt.vCurPos.z;
}

} // namespace RANMobile
