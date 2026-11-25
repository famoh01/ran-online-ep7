#pragma once
#include "../../Engine/Network/EP7Protocol.h"

class VirtualJoystick;

namespace RANMobile {

class MobileCamera;

class CharacterController {
public:
    void Update(float deltaTime, const VirtualJoystick& joystick, const MobileCamera& camera);
    EP7NetPcGoto BuildGotoPacket(float tarX, float tarY, float tarZ) const;
    void ApplyServerGoto(const EP7NetPcGoto& pkt);
    EP7NetPcAttack BuildAttackRequest(uint32_t tarCrow, uint32_t tarId, uint32_t aniSel = 0) const;
    EP7NetChat BuildChatMessage(uint32_t chatType, const char* sender, const char* msg) const;
    EP7NetPcReqSkill BuildSkillRequest(uint16_t skillMain, uint16_t skillSub, float tarX, float tarY, float tarZ) const;
    EP7NetPcReqBuyFromNpc BuildShopPurchaseRequest(uint16_t itemMain, uint16_t itemSub, uint32_t npcId, uint16_t buyNum = 1) const;
    EP7NetPcReqInvenDrug BuildUseDrugRequest(uint16_t posX, uint16_t posY) const;

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
