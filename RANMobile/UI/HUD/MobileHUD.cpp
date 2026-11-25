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
    float w = static_cast<float>(renderer->GetWidth());
    float h = static_cast<float>(renderer->GetHeight());
    renderer->DrawUIQuad(20.0f, 16.0f, 280.0f, 76.0f, 0.05f, 0.10f, 0.22f, 0.85f);
    renderer->DrawUIQuad(60.0f, h - 180.0f, 120.0f, 120.0f, 0.10f, 0.20f, 0.35f, 0.55f);
    renderer->DrawUIQuad(w - 140.0f, h - 140.0f, 96.0f, 96.0f, 0.85f, 0.58f, 0.12f, 0.85f);
    renderer->DrawUIText(w - 122.0f, h - 102.0f, "ATK", 0.9f, 1.0f, 1.0f, 1.0f, 1.0f);
    renderer->DrawUIQuad(w - 230.0f, h - 130.0f, 70.0f, 70.0f, 0.15f, 0.45f, 0.85f, 0.9f);
    renderer->DrawUIText(w - 216.0f, h - 102.0f, "SKL", 0.85f, 1.0f, 1.0f, 1.0f, 1.0f);
    renderer->DrawUIQuad(w - 315.0f, h - 130.0f, 70.0f, 70.0f, 0.25f, 0.60f, 0.35f, 0.9f);
    renderer->DrawUIText(w - 301.0f, h - 102.0f, "INV", 0.85f, 1.0f, 1.0f, 1.0f, 1.0f);
    renderer->DrawUIQuad(w - 400.0f, h - 130.0f, 70.0f, 70.0f, 0.65f, 0.40f, 0.15f, 0.9f);
    renderer->DrawUIText(w - 390.0f, h - 102.0f, "SHOP", 0.78f, 1.0f, 1.0f, 1.0f, 1.0f);
    renderer->DrawUIQuad(w - 485.0f, h - 130.0f, 70.0f, 70.0f, 0.35f, 0.35f, 0.65f, 0.9f);
    renderer->DrawUIText(w - 475.0f, h - 102.0f, "CHAT", 0.78f, 1.0f, 1.0f, 1.0f, 1.0f);
    // Virtual Joystick Knob (follows touch drag)
    float knobX = 110.0f - 22.0f;
    float knobY = (h - 110.0f) - 22.0f;
    renderer->DrawUIQuad(knobX, knobY, 44.0f, 44.0f, 0.85f, 0.90f, 0.98f, 0.90f);

    // S1-S4 skill buttons + BACK
    renderer->DrawUIQuad(w - 230.0f, h - 195.0f, 58.0f, 52.0f, 0.25f, 0.45f, 0.85f, 0.85f);
    renderer->DrawUIText(w - 214.0f, h - 176.0f, "S1", 0.85f, 1.0f, 1.0f, 1.0f, 1.0f);
    renderer->DrawUIQuad(w - 165.0f, h - 195.0f, 58.0f, 52.0f, 0.25f, 0.45f, 0.85f, 0.85f);
    renderer->DrawUIText(w - 149.0f, h - 176.0f, "S2", 0.85f, 1.0f, 1.0f, 1.0f, 1.0f);
    renderer->DrawUIQuad(w - 300.0f, h - 195.0f, 58.0f, 52.0f, 0.25f, 0.45f, 0.85f, 0.85f);
    renderer->DrawUIText(w - 284.0f, h - 176.0f, "S3", 0.85f, 1.0f, 1.0f, 1.0f, 1.0f);
    renderer->DrawUIQuad(w - 365.0f, h - 195.0f, 58.0f, 52.0f, 0.25f, 0.45f, 0.85f, 0.85f);
    renderer->DrawUIText(w - 349.0f, h - 176.0f, "S4", 0.85f, 1.0f, 1.0f, 1.0f, 1.0f);

    renderer->DrawUIQuad(w - 570.0f, h - 130.0f, 70.0f, 70.0f, 0.55f, 0.20f, 0.20f, 0.9f);
    renderer->DrawUIText(w - 558.0f, h - 102.0f, "BACK", 0.78f, 1.0f, 1.0f, 1.0f, 1.0f);

    renderer->DrawUIText(30.0f, 26.0f, "CHAR: NO DATA (SERVER NOT VERIFIED)", 0.80f, 1.0f, 0.85f, 0.45f, 1.0f);
    renderer->DrawUIText(30.0f, 52.0f, "MAP: login.wld | POS: NO DATA", 0.72f, 0.75f, 0.90f, 1.0f, 1.0f);
}
}
