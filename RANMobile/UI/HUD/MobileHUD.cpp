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

#include "../../Engine/Renderer/MobileRenderer.h"
namespace RANMobile {
void MobileHUD::Draw(MobileRenderer* renderer) {
    if (!renderer) return;
    const float w = renderer->GetDesignWidth();
    const float h = renderer->GetDesignHeight();
    renderer->DrawUIQuad(20.0f, 16.0f, 280.0f, 76.0f, 0.05f, 0.10f, 0.22f, 0.85f);
    renderer->DrawUIText(32.0f, 34.0f, "FAM HAWAII", 0.85f, 0.96f, 0.84f, 0.42f, 1.0f);

    const float jx = m_joystick.GetCenterX();
    const float jy = m_joystick.GetCenterY();
    const float jr = m_joystick.GetRadius() > 1.0f ? m_joystick.GetRadius() : 60.0f;
    renderer->DrawUIQuad(jx - jr, jy - jr, jr * 2.0f, jr * 2.0f, 0.10f, 0.20f, 0.35f, 0.48f);
    const float knobX = jx + m_joystick.GetAxisX() * jr * 0.65f - 22.0f;
    const float knobY = jy + m_joystick.GetAxisY() * jr * 0.65f - 22.0f;
    renderer->DrawUIQuad(knobX, knobY, 44.0f, 44.0f, 0.40f, 0.72f, 0.98f, 0.85f);

    const float atkGlow = m_attackBtn.IsPressed() ? 1.0f : 0.85f;
    renderer->DrawUIQuad(w - 150.0f, h - 150.0f, 110.0f, 110.0f,
                         0.85f * atkGlow, 0.52f * atkGlow, 0.12f, 0.88f);
    renderer->DrawUIText(w - 118.0f, h - 106.0f, "ATK", 0.9f, 1.0f, 1.0f, 1.0f, 1.0f);
    renderer->DrawUIQuad(w - 270.0f, h - 120.0f, 80.0f, 80.0f, 0.14f, 0.38f, 0.68f, 0.82f);
    renderer->DrawUIText(w - 252.0f, h - 88.0f, "TGT", 0.72f, 1.0f, 1.0f, 1.0f, 1.0f);
    renderer->DrawUIQuad(w - 130.0f, h - 270.0f, 75.0f, 75.0f, 0.65f, 0.16f, 0.22f, 0.82f);
    renderer->DrawUIText(w - 112.0f, h - 242.0f, "HP", 0.72f, 1.0f, 1.0f, 1.0f, 1.0f);
}
}
