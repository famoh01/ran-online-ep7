#include "../Engine/Core/RANMobileEngine.h"
#include "../Engine/Renderer/MobileRenderer.h"
#include "../Engine/Camera/MobileCamera.h"
#include "../Engine/Touch/TouchSystem.h"
#include "../UI/Login/LoginScreen.h"
#include "../UI/HUD/MobileHUD.h"
#include "../UI/Buttons/MobileSkillButton.h"
#include "../Client/Gameplay/CharacterController.h"
#include "../Client/Character/CharacterSelectManager.h"
#include "../Client/Combat/TargetManager.h"
#include "../Client/Combat/CombatController.h"
#include "../Client/Inventory/InventoryManager.h"
#include "../Client/Quest/QuestManager.h"
#include "../Client/Chat/ChatManager.h"
#include "../UI/Settings/SettingsUI.h"
#include "../UI/Quest/QuestUI.h"
#include "../UI/Map/MapUI.h"
#include "../UI/Chat/ChatUI.h"
#include "../Client/Map/MapManager.h"
#include "../Client/Monster/MonsterAdapter.h"
#include "../Client/NPC/NPCAdapter.h"
#include "../Data/Importer/EP7AssetImporter.h"
#include "../Engine/Resource/ObjectPool.h"
#include "../Engine/Audio/AudioManager.h"
#include "../Engine/Animation/AnimationManager.h"
#include "../Engine/Network/EP7Protocol.h"
#include "../Data/Validator/DataValidator.h"
#include "../Data/Converter/RANMobileDataConverter.h"

#include <cassert>
#include <iostream>
#include <cmath>

int main() {
    std::cout << "=== RAN ONLINE EP7 — FAM HAWAII MOBILE: UNIT & INTEGRATION TESTS ===\n";

    // 1. Engine Lifecycle
    RANMobileEngine engine;
    assert(engine.Initialize());
    assert(engine.IsInitialized());
    engine.Start();
    assert(engine.IsRunning());
    engine.Update(1.0f / 60.0f);
    engine.Render();
    assert(engine.GetFrameCount() == 1);

    // 2. Login Screen (FAM HAWAII)
    LoginScreen login;
    login.Initialize(engine.GetNetworkClient());
    assert(login.GetGameTitle() == "RAN ONLINE EP7");
    assert(login.GetServerDisplayName() == "FAM HAWAII");
    login.SetUsername("testuser");
    login.SetPassword("testpass");
    assert(login.GetUsername() == "testuser");

    // 3. Virtual Joystick
    VirtualJoystick joy;
    joy.SetCenter(180.0f, 540.0f);
    joy.SetRadius(100.0f);
    assert(joy.TouchDown(0, 180.0f, 540.0f));
    joy.TouchMove(0, 280.0f, 540.0f);
    assert(std::abs(joy.GetAxisX() - 1.0f) < 1e-3f);
    assert(joy.IsRunning());
    joy.TouchUp(0);
    assert(!joy.IsActive());

    // 4. Mobile Camera
    RANMobile::MobileCamera cam;
    cam.OnDrag(40.0f, -10.0f);
    cam.OnPinch(1.0f);
    cam.Update(0.1f);
    assert(cam.GetDistance() < 12.0f);

    // 5. Mobile HUD & Multi-touch
    RANMobile::MobileHUD hud;
    hud.Initialize(1280, 720);
    assert(hud.OnTouchDown(1, 1280.0f - 100.0f, 720.0f - 100.0f)); // Attack button
    assert(hud.GetAttackButton().IsPressed());
    hud.OnTouchUp(1, 1280.0f - 100.0f, 720.0f - 100.0f);
    assert(!hud.GetAttackButton().IsPressed());

    // 6. Data Validator (verifies NO mock data is fabricated when EP7 data dir is absent)
    auto report = RANMobile::DataValidator::ValidateDirectory("/nonexistent_ep7_data");
    assert(!report.mapFound);
    assert(!report.monsterFound);
    assert(!report.errors.empty());

    // 7. CharacterController + MobileCamera + VirtualJoystick integration
    RANMobile::CharacterController charCtrl;
    joy.TouchDown(0, 180.0f, 540.0f);
    joy.TouchMove(0, 280.0f, 540.0f);
    charCtrl.Update(0.1f, joy, cam);
    assert(charCtrl.IsMoving());
    assert(charCtrl.IsRunning());
    assert(charCtrl.GetPosX() != 0.0f || charCtrl.GetPosZ() != 0.0f);
    joy.TouchUp(0);

    // 8. MobileSkillButton
    MobileSkillButton skillBtn;
    skillBtn.SetSkillID(101);
    assert(skillBtn.CanUse());
    skillBtn.SetCooldown(2.0f);
    assert(!skillBtn.CanUse());
    skillBtn.Update(2.5f);
    assert(skillBtn.CanUse());

    // 9. EP7Protocol verified constants
    static_assert(RANMobile::EP7_NET_MSG_HEARTBEAT_CLIENT_ANS == 161, "Verified EP7 Heartbeat ID");
    static_assert(RANMobile::EP7_NET_MSG_LOGIN_2 == 1120, "Verified EP7 Login ID");

    // 10. RANMobileDataConverter (reports DATA NOT FOUND IN SOURCE when missing)
    auto convRes = RANMobile::RANMobileDataConverter::ConvertAsset("/nonexistent/crow.mnsf", "/tmp/out");
    assert(!convRes.success);
    assert(convRes.status == "DATA NOT FOUND IN SOURCE");

    // 11. AudioManager & AnimationManager
    RANMobile::AudioManager audio;
    assert(audio.Initialize());
    audio.SetMasterVolume(0.75f);
    assert(std::abs(audio.GetMasterVolume() - 0.75f) < 1e-3f);
    RANMobile::AnimationManager anim;
    anim.PlayAnimation(1, "walk");
    anim.Update(0.016f);
    assert(anim.GetCurrentAnimation() == "walk");

    // 12. CharacterSelectManager, TargetManager, CombatController, Inventory, Quest, Chat, Settings
    RANMobile::CharacterSelectManager charSel;
    assert(charSel.GetSlots().empty());
    RANMobile::TargetManager targetMgr;
    assert(!targetMgr.HasTarget());
    RANMobile::CombatController combat;
    combat.Initialize(engine.GetNetworkClient(), &targetMgr);
    assert(!combat.RequestBasicAttack(2.5f)); // No target
    RANMobile::InventoryManager inv;
    assert(inv.GetItemCount() == 0);
    RANMobile::QuestManager questMgr;
    assert(questMgr.GetActiveQuests().empty());
    RANMobile::ChatManager chat;
    chat.AddMessage(RANMobile::ChatChannel::System, "SYSTEM", "สวัสดี FAM HAWAII");
    assert(chat.GetMessages().size() == 1);
    RANMobile::SettingsUI settings;
    settings.SetGraphicsPreset(RANMobile::GraphicsQuality::Ultra);
    assert(settings.GetGraphicsSettings().fpsLimit == 90);

    // 13. Phase 12-21: QuestUI, MapUI, ChatUI, MonsterAdapter, NPCAdapter, AssetImporter, ObjectPool
    RANMobile::QuestUI questUI;
    questUI.Initialize(&questMgr);
    questUI.SelectQuest(100);
    assert(questUI.GetSelectedQuestID() == 100);

    RANMobile::MapManager mapMgr;
    RANMobile::MapUI mapUI;
    mapUI.Initialize(&mapMgr);
    mapUI.SetZoom(2.0f);
    assert(std::abs(mapUI.GetZoom() - 2.0f) < 1e-3f);

    RANMobile::ChatUI chatUI;
    chatUI.Initialize(&chat);
    chatUI.SetActiveChannel(RANMobile::ChatChannel::Party);
    assert(chatUI.GetActiveChannel() == RANMobile::ChatChannel::Party);

    RANMobile::MonsterAdapter mobAdapter;
    assert(mobAdapter.GetState().globId == 0);
    RANMobile::NPCAdapter npcAdapter;
    assert(npcAdapter.GetState().globId == 0);

    auto assetInfo = RANMobile::EP7AssetImporter::InspectAsset("crow", "/nonexistent/crow.mnsf");
    assert(!assetInfo.found);
    assert(assetInfo.format == ".mnsf");

    RANMobile::ObjectPool<RANMobile::MonsterClientState> pool(4);
    assert(pool.AvailableCount() == 4);
    auto pooledObj = pool.Acquire();
    assert(pool.AvailableCount() == 3);
    pool.Release(std::move(pooledObj));
    assert(pool.AvailableCount() == 4);

    engine.Shutdown();
    assert(!engine.IsInitialized());

    std::cout << "ALL TESTS PASSED!\n";
    return 0;
}
