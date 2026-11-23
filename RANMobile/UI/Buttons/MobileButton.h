#pragma once

#include <cstdint>
#include <functional>

enum class MobileButtonType {
    Attack,
    Target,
    Skill1, Skill2, Skill3, Skill4, Skill5, Skill6, Skill7, Skill8, Skill9,
    Potion,
    Jump,
    Pickup,
    Map,
    Inventory,
    Quest,
    Chat,
    Menu
};

class MobileButton {
public:
    MobileButton() = default;
    explicit MobileButton(MobileButtonType type) : m_type(type) {}

    void SetType(MobileButtonType type) { m_type = type; }
    MobileButtonType GetType() const { return m_type; }

    void SetPosition(float x, float y);
    void SetSize(float width, float height);
    void SetOpacity(float opacity);
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    void SetCooldown(float seconds);

    bool OnTouchDown(int32_t pointerId, float x, float y);
    void OnTouchMove(int32_t pointerId, float x, float y);
    void OnTouchUp(int32_t pointerId, float x, float y);

    void OnTouchDown(float x, float y) { OnTouchDown(0, x, y); }
    void OnTouchMove(float x, float y) { OnTouchMove(0, x, y); }
    void OnTouchUp(float x, float y) { OnTouchUp(0, x, y); }

    void Update(float deltaTime);
    void Draw();

    bool IsPressed() const { return m_pressed; }
    bool IsClicked() const { return m_clicked; }
    bool IsEnabled() const { return m_enabled && m_cooldownRemaining <= 0.0f; }
    float GetCooldownRemaining() const { return m_cooldownRemaining; }

    void SetOnClick(std::function<void()> cb) { m_onClick = std::move(cb); }

private:
    bool HitTest(float x, float y) const;

    MobileButtonType m_type = MobileButtonType::Attack;
    float m_x = 0.0f;
    float m_y = 0.0f;
    float m_width = 80.0f;
    float m_height = 80.0f;
    float m_opacity = 1.0f;

    bool m_pressed = false;
    bool m_clicked = false;
    bool m_enabled = true;
    int32_t m_activePointerId = -1;

    float m_cooldownRemaining = 0.0f;
    std::function<void()> m_onClick;
};
