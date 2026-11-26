#include "../Engine/Core/RANMobileEngine.h"
#include "../Engine/Renderer/MobileRenderer.h"
#include "../Engine/Resource/ResourceManager.h"
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
#include "../UI/Inventory/InventoryUI.h"
#include "../UI/Skills/SkillUI.h"
#include "../Data/Loader/ThaiPackLoader.h"
#include "../UI/Quest/QuestUI.h"
#include "../Server/MobileServerHost.h"
#include "../Data/Importer/EP7TextureDecoder.h"
#include "../Data/Importer/EP7WorldCharParser.h"
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
#include "../Data/Importer/EP7AssetPipeline.h"
#include "../Data/Importer/EP7SerialCodec.h"
#include "../Data/Importer/EP7SerialFile.h"
#include "../Data/Importer/EP7SceneData.h"
#include "../Engine/Scene/EP7SceneRenderer.h"

#include <cassert>
#include <iostream>
#include <cmath>
#include <filesystem>
#include <fstream>

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

    // 14. Phase 32-41: Real ThaiPackLoader + InventoryUI + SkillUI
    auto tpStats = RANMobile::ThaiPackLoader::InspectThaiPack("external/ep7_release/thaipack_original");
    assert(tpStats.found);
    assert(tpStats.ntkCount == 309);
    assert(tpStats.qstCount == 730);

    RANMobile::InventoryUI invUI;
    invUI.Initialize(&inv);
    invUI.SelectSlot(0);
    assert(invUI.GetSelectedSlot() == 0);

    RANMobile::SkillUI skillUI;
    skillUI.BindSkillSlot(0, 101);
    assert(skillUI.GetSkillSlot(0)->CanUse());

    // 15. Real LoginScreen Focus, Text Input, Backspace, Password Masking, Render
    auto* engLogin = engine.GetLoginScreen();
    assert(engLogin != nullptr);
    engLogin->HandleTouchDown(0, 640.0f, 220.0f); // Username field
    assert(engLogin->GetFocus() == LoginFocus::Username);
    engLogin->HandleTextInput("famuser");
    engLogin->HandleBackspace();
    assert(engLogin->GetUsername() == "famuse");

    engLogin->HandleTouchDown(0, 640.0f, 280.0f); // Password field
    assert(engLogin->GetFocus() == LoginFocus::Password);
    engLogin->HandleTextInput("secret");
    assert(engLogin->GetMaskedPassword() == "******");
    engine.Render();
    assert(engine.GetRenderer()->GetStats().drawCalls > 0);

    // 16. UTF-8 Thai Font Decoding & Width Measurement
    std::string thaiSample = "สวัสดี FAM HAWAII 123";
    size_t utfIdx = 0;
    assert(RANMobile::MobileRenderer::DecodeUTF8(thaiSample, utfIdx) == 0x0E2A); // ส
    assert(RANMobile::MobileRenderer::DecodeUTF8(thaiSample, utfIdx) == 0x0E27); // ว
    assert(RANMobile::MobileRenderer::DecodeUTF8(thaiSample, utfIdx) == 0x0E31); // ั (combining)
    assert(engine.GetRenderer()->MeasureTextWidth(thaiSample, 1.0f) > 0.0f);

    // 17. Settings & Exit Confirmation Hit Testing
    engLogin->HandleTouchDown(0, 420.0f, 485.0f); // Settings button
    assert(engLogin->IsSettingsOpen());
    engLogin->HandleTouchDown(0, 640.0f, 300.0f); // Sound toggle
    assert(!engLogin->IsSoundEnabled());
    engLogin->HandleTouchDown(0, 640.0f, 470.0f); // Close settings
    assert(!engLogin->IsSettingsOpen());

    engLogin->HandleTouchDown(0, 800.0f, 485.0f); // Exit button
    assert(engLogin->IsExitConfirmOpen());
    engLogin->HandleTouchDown(0, 720.0f, 400.0f); // NO
    assert(!engLogin->IsExitConfirmOpen());
    assert(!engLogin->WantsExit());

    // 18. Android Server Host (Register, Authenticate, Start/Stop)
    RANMobile::MobileServerHost serverHost;
    assert(serverHost.RegisterAccount("hawaii01", "pass1234"));
    assert(serverHost.Authenticate("hawaii01", "pass1234"));
    assert(!serverHost.Authenticate("hawaii01", "wrongpass"));
    assert(serverHost.Start(15001));
    assert(serverHost.IsRunning());
    serverHost.Stop();
    assert(!serverHost.IsRunning());

    // 19. EP7TextureDecoder (.mtf decrypt + .dds DXT1/DXT5 decode)
    {
        // Encrypt a 4-byte payload using MemoryTexture.cpp:138-139 rule
        std::vector<uint8_t> mtf(16, 0);
        int ver = 0x100 /* TEX_VERSION */, sz = 4, ftype = 0 /* TEXTURE_DDS */;
        std::memcpy(mtf.data(), &ver, 4);
        std::memcpy(mtf.data() + 4, &sz, 4);
        std::memcpy(mtf.data() + 8, &ftype, 4);
        const uint8_t plain[4] = {'D', 'D', 'S', ' '};
        for (int i = 0; i < 4; ++i) {
            mtf[12 + i] = static_cast<uint8_t>((plain[i] ^ 0x47) - 0x03);
        }
        std::vector<uint8_t> outRaw;
        int outType = -1;
        assert(RANMobile::EP7TextureDecoder::DecryptMTF(mtf, outRaw, outType));
        assert(outType == 0 && outRaw.size() == 4);
        assert(std::memcmp(outRaw.data(), plain, 4) == 0);

        // Minimal 4x4 DXT1 DDS (128-byte header + 8-byte block)
        std::vector<uint8_t> dds(136, 0);
        std::memcpy(dds.data(), "DDS ", 4);
        uint32_t flags = 0, h = 4, w = 4, pfSize = 32, pfFlags = 4, fourcc = 0x31545844;
        std::memcpy(dds.data() + 8, &flags, 4);
        std::memcpy(dds.data() + 12, &h, 4);
        std::memcpy(dds.data() + 16, &w, 4);
        std::memcpy(dds.data() + 76, &pfSize, 4);
        std::memcpy(dds.data() + 80, &pfFlags, 4);
        std::memcpy(dds.data() + 84, &fourcc, 4);
        // c0 = 0xF800 (red), c1 = 0x001F (blue), idx = 0 (all pixel 0 -> c0)
        dds[128] = 0x00; dds[129] = 0xF8;
        dds[130] = 0x1F; dds[131] = 0x00;
        auto tex = RANMobile::EP7TextureDecoder::DecodeDDS(dds);
        assert(tex.valid && tex.width == 4 && tex.height == 4);
        assert(tex.rgba8[0] == 255 && tex.rgba8[1] == 0 && tex.rgba8[2] == 0 && tex.rgba8[3] == 255);
    }

    // 20. EP7WorldCharParser & BinaryReader Bounds/Error Handling
    {
        std::vector<uint8_t> empty;
        assert(!RANMobile::EP7WorldCharParser::ParseWldHeader(empty).valid);
        assert(!RANMobile::EP7WorldCharParser::ParseChfHeader(empty).valid);
        assert(!RANMobile::EP7WorldCharParser::ParseCpsHeader(empty).valid);

        std::vector<uint8_t> badWld(132, 0); // empty fileType -> invalid header
        assert(!RANMobile::EP7WorldCharParser::ParseWldHeader(badWld).valid);

        std::vector<uint8_t> badVer(132, 0);
        badVer[0] = 'L'; badVer[1] = 'A'; badVer[2] = 'N'; badVer[3] = 'D';
        uint32_t vBad = 0x9999;
        std::memcpy(badVer.data() + 128, &vBad, 4);
        assert(!RANMobile::EP7WorldCharParser::ParseWldHeader(badVer).valid);

        std::vector<uint8_t> badLenBuf = { 0xFF, 0x00, 0x00, 0x00, 'A', 'B' }; // len=255 > remaining=2
        RANMobile::BinaryReader br(badLenBuf);
        std::string s;
        assert(!br.ReadLengthPrefixedString(s));
        assert(!br.Skip(100));
    }

    // 27. Original DDS header metadata, uncompressed decode and alpha rules
    {
        std::vector<uint8_t> dds(132, 0);
        std::memcpy(dds.data(), "DDS ", 4);
        uint32_t flags = 0x00020000, h = 1, w = 1, depth = 1, mips = 2;
        uint32_t pfSize = 32, pfFlags = 1, fourcc = 0;
        uint32_t bpp = 32, rm = 0x00FF0000, gm = 0x0000FF00, bm = 0x000000FF, am = 0xFF000000;
        std::memcpy(dds.data() + 8, &flags, 4);
        std::memcpy(dds.data() + 12, &h, 4);
        std::memcpy(dds.data() + 16, &w, 4);
        std::memcpy(dds.data() + 24, &depth, 4);
        std::memcpy(dds.data() + 28, &mips, 4);
        std::memcpy(dds.data() + 76, &pfSize, 4);
        std::memcpy(dds.data() + 80, &pfFlags, 4);
        std::memcpy(dds.data() + 84, &fourcc, 4);
        std::memcpy(dds.data() + 88, &bpp, 4);
        std::memcpy(dds.data() + 92, &rm, 4);
        std::memcpy(dds.data() + 96, &gm, 4);
        std::memcpy(dds.data() + 100, &bm, 4);
        std::memcpy(dds.data() + 104, &am, 4);
        uint32_t pixel = 0x10203040;
        std::memcpy(dds.data() + 128, &pixel, 4);

        const auto info = RANMobile::EP7TextureDecoder::InspectDDS(dds);
        assert(info.valid && info.width == 1 && info.height == 1);
        assert(info.format == RANMobile::EP7TextureFormat::A8R8G8B8);
        assert(info.mipMapCount == 2 && info.hasAlpha);
        const auto decoded = RANMobile::EP7TextureDecoder::DecodeDDS(dds, "sample_a.dds");
        assert(decoded.valid && decoded.rgba8.size() == 4);
        // With A8R8G8B8 masks, pixel 0x10203040 decodes to R=0x20 G=0x30 B=0x40 A=0x10.
        assert(decoded.rgba8[0] == 0x20 && decoded.rgba8[1] == 0x30 &&
               decoded.rgba8[2] == 0x40 && decoded.rgba8[3] == 0x10);
        assert(decoded.alphaMode == RANMobile::EP7AlphaMode::Hard);
        assert(RANMobile::EP7TextureDecoder::AlphaModeFromName(
                   "cutout_a1.dds", RANMobile::EP7TextureFormat::DXT1, false) ==
               RANMobile::EP7AlphaMode::Hard);
        assert(RANMobile::EP7TextureDecoder::AlphaModeFromName(
                   "blend_a2.dds", RANMobile::EP7TextureFormat::DXT5, false) ==
               RANMobile::EP7AlphaMode::Soft02);
        assert(RANMobile::EP7TextureDecoder::AlphaModeFromName(
                   "opaque.dds", RANMobile::EP7TextureFormat::DXT1, false) ==
               RANMobile::EP7AlphaMode::None);
        assert(RANMobile::EP7TextureDecoder::AlphaModeFromName(
                   "decal_a.dds", RANMobile::EP7TextureFormat::DXT3, false) ==
               RANMobile::EP7AlphaMode::Hard);

        dds[76] = 0; // corrupt DDS_PIXELFORMAT size
        assert(!RANMobile::EP7TextureDecoder::InspectDDS(dds).valid);
    }

    // 28. Original CSerialFile byte coding and real ThaiPack serialized files
    {
        std::vector<uint8_t> original(256);
        for (size_t i = 0; i < original.size(); ++i) original[i] = static_cast<uint8_t>(i);
        std::vector<uint8_t> encoded = original;
        for (size_t i = 0; i < encoded.size(); ++i) {
            encoded[i] = RANMobile::EP7Serial::kEncodeTable[i];
        }
        RANMobile::EP7Serial::DecodeInPlace(encoded.data(), encoded.size());
        assert(encoded == original);

        // Synthetic CSerialFile with the exact header layout used by GLLevelFile.
        std::vector<uint8_t> serial(128 + 4, 0);
        const std::string serialType = "GLLEVEL";
        std::memcpy(serial.data(), serialType.data(), serialType.size());
        const uint32_t serialVersion = 0x0104;
        std::memcpy(serial.data() + 128, &serialVersion, 4);
        const std::string payloadText = "hello";
        const uint32_t payloadLength = static_cast<uint32_t>(payloadText.size() + 1);
        for (int i = 0; i < 4; ++i) {
            serial.push_back(RANMobile::EP7Serial::kEncodeTable[
                static_cast<uint8_t>(payloadLength >> (8 * i))]);
        }
        for (size_t i = 0; i <= payloadText.size(); ++i) {
            const char c = i < payloadText.size() ? payloadText[i] : '\0';
            serial.push_back(RANMobile::EP7Serial::kEncodeTable[static_cast<uint8_t>(c)]);
        }
        const auto view = RANMobile::EP7SerialFile::Read(serial);
        assert(view.valid && view.fileType == serialType);
        assert(view.version == serialVersion && RANMobile::EP7SerialFile::IsEncodedVersion(view.version));
        size_t pos = 0;
        std::string decodedText;
        assert(RANMobile::EP7SerialFile::ReadString(view.payload, pos, decodedText, 32));
        assert(decodedText == payloadText);
    }

    // 29. mapslist.mst parser: field order from GLMapList::LoadMapList
    {
        const std::string text =
            "; comment\n[MAP_LIST_INFO]\nMapKeySize=20\n[MAP_LIST]\n"
            "MAP=1,preview.glmap,1,0,0,1,0,0,0,0,0,0,0,0,0,0,Preview Map,bgm,preview.dds,0\n"
            "MAP=0,unused.glmap,2,0,0,1,0,0,0,0,0,0,0,0,0,0,Unused,bgm,none.dds,0\n";
        const auto report = RANMobile::EP7MapListParser::Parse(text);
        assert(report.valid && report.mapKeySize == 20);
        assert(report.entries.size() == 1);
        assert(report.entries[0].file == "preview.glmap");
        assert(report.entries[0].mapName == "Preview Map");
        assert(report.entries[0].loadingImage == "preview.dds");
        const auto malformed = RANMobile::EP7MapListParser::Parse(
            "[MAP_LIST_INFO]\nMapKeySize=20\n[MAP_LIST]\nMAP=1,short.glmap\n");
        assert(!malformed.valid && !malformed.errors.empty());
    }

    // 30. .glmap SLEVEL_HEAD parser and real scene metadata preview fixture
    {
        namespace fs = std::filesystem;
        const fs::path tmp = fs::temp_directory_path() / "ranmobile_phase3c_scene";
        std::error_code ec;
        fs::remove_all(tmp, ec);
        fs::create_directories(tmp / "map", ec);

        const std::string wldFile = "preview.wld";
        const std::string mapName = "Preview Map";
        std::vector<uint8_t> block;
        const auto appendU32 = [&block](uint32_t v) {
            for (int i = 0; i < 4; ++i) block.push_back(static_cast<uint8_t>(v >> (8 * i)));
        };
        const auto appendString = [&](const std::string& value) {
            appendU32(static_cast<uint32_t>(value.size() + 1));
            block.insert(block.end(), value.begin(), value.end());
            block.push_back(0);
        };
        appendString(wldFile);
        appendString(mapName);
        appendU32(1); // BRIGHT
        appendU32(0); // QUAD_DEFAULT_DIV

        std::vector<uint8_t> payload;
        const auto appendPayloadU32 = [&payload](uint32_t v) {
            for (int i = 0; i < 4; ++i) payload.push_back(static_cast<uint8_t>(v >> (8 * i)));
        };
        appendPayloadU32(0x0101); // SLEVEL_HEAD::VERSION
        appendPayloadU32(static_cast<uint32_t>(block.size()));
        payload.insert(payload.end(), block.begin(), block.end());
        std::vector<uint8_t> glmap(128, 0);
        const std::string type = "GLLEVEL";
        std::memcpy(glmap.data(), type.data(), type.size());
        const uint32_t fileVersion = 0x0104; // GLLevelFile::VERSION
        for (int i = 0; i < 4; ++i) glmap.push_back(static_cast<uint8_t>(fileVersion >> (8 * i)));
        for (uint8_t b : payload) glmap.push_back(RANMobile::EP7Serial::kEncodeTable[b]);

        const auto header = RANMobile::EP7LevelHeaderParser::Parse(glmap);
        assert(header.valid && header.wldFile == wldFile && header.mapName == mapName);
        assert(header.fileVersion == 0x0104 && header.headVersion == 0x0101);

        {
            std::ofstream out(tmp / "mapslist.mst");
            out << "[MAP_LIST_INFO]\nMapKeySize=20\n[MAP_LIST]\n"
                << "MAP=1,preview.glmap,1,0,0,1,0,0,0,0,0,0,0,0,0,0,Preview Map,bgm,preview.dds,0\n";
        }
        {
            std::ofstream out(tmp / "map" / "preview.glmap", std::ios::binary);
            out.write(reinterpret_cast<const char*>(glmap.data()), glmap.size());
        }
        {
            std::vector<uint8_t> dds(136, 0);
            std::memcpy(dds.data(), "DDS ", 4);
            uint32_t flags = 0, h = 4, w = 4, pfSize = 32, pfFlags = 4, fourcc = 0x31545844;
            std::memcpy(dds.data() + 8, &flags, 4);
            std::memcpy(dds.data() + 12, &h, 4);
            std::memcpy(dds.data() + 16, &w, 4);
            std::memcpy(dds.data() + 76, &pfSize, 4);
            std::memcpy(dds.data() + 80, &pfFlags, 4);
            std::memcpy(dds.data() + 84, &fourcc, 4);
            dds[128] = 0; dds[129] = 0xF8; dds[130] = 0x1F; dds[131] = 0;
            std::ofstream out(tmp / "preview.dds", std::ios::binary);
            out.write(reinterpret_cast<const char*>(dds.data()), dds.size());
        }

        RANMobile::EP7SceneRenderer scene;
        auto* renderer = engine.GetRenderer();
        const size_t texturesBefore = renderer->GetAssetTextureCount();
        assert(scene.Load(tmp.string(), renderer, engine.GetResourceManager()));
        assert(scene.IsLoaded() && scene.HasPreviewTexture());
        assert(scene.GetIndex().scenes.size() == 1);
        assert(scene.GetIndex().scenes[0].wldFile == wldFile);
        assert(renderer->GetAssetTextureCount() == texturesBefore + 1);
        engine.SetScreenState(EngineScreenState::ScenePreview);
        const float designW = renderer->GetDesignWidth();
        const float designH = renderer->GetDesignHeight();
        engine.SetSafeAreaInsets(24.0f, 48.0f, 24.0f, 24.0f);
        assert(renderer->GetDesignWidth() == designW && renderer->GetDesignHeight() == designH);
        engine.Render();
        engine.SetDiagnosticsVisible(true);
        engine.Render();
        engine.SetDiagnosticsVisible(false);
        engine.SetScreenState(EngineScreenState::Login);
        scene.Unload(renderer);
        assert(renderer->GetAssetTextureCount() == texturesBefore);
        fs::remove_all(tmp, ec);
    }

    // 21. RenderLayout: aspect-fit, orientation and safe-area conversion
    {
        RANMobile::RenderLayout layout;
        layout.SetPhysicalSize(1280, 720);
        assert(std::fabs(layout.GetScale() - 1.0f) < 1e-4f);
        assert(std::fabs(layout.ToDesignX(640.0f) - 640.0f) < 1e-4f);
        layout.SetPhysicalSize(1920, 1080);
        assert(std::fabs(layout.GetScale() - 1.5f) < 1e-4f);
        assert(std::fabs(layout.ToDesignX(960.0f) - 640.0f) < 1e-4f);
        assert(std::fabs(layout.ToPhysicalY(360.0f) - 540.0f) < 1e-4f);
        layout.SetPhysicalSize(1080, 1920);
        assert(layout.IsPortrait());
        assert(std::fabs(layout.GetDesignWidth() - 720.0f) < 1e-4f);
        assert(std::fabs(layout.GetDesignHeight() - 1280.0f) < 1e-4f);
        assert(std::fabs(layout.GetScale() - 1.5f) < 1e-4f);
        layout.SetSafeAreaInsets(0.0f, 60.0f, 0.0f, 0.0f);
        assert(layout.GetSafeTop() > 0.0f);
        assert(layout.IsInsideDesignCanvas(540.0f, 960.0f));
    }

    // 22. TouchSystem: pointer lifecycle, multi-touch and design conversion
    {
        RANMobile::TouchSystem touches;
        touches.SetViewport(1920, 1080);
        touches.OnTouchDown(1, 960.0f, 540.0f);
        touches.OnTouchDown(2, 480.0f, 270.0f);
        assert(touches.GetActiveTouchCount() == 2);
        assert(std::fabs(touches.GetTouch(1)->designX - 640.0f) < 1e-4f);
        assert(std::fabs(touches.GetTouch(2)->designY - 180.0f) < 1e-4f);
        touches.OnTouchMove(2, 1440.0f, 810.0f);
        assert(std::fabs(touches.GetTouch(2)->designX - 960.0f) < 1e-4f);
        assert(std::fabs(touches.GetTouch(2)->deltaX - 960.0f) < 1e-4f);
        touches.OnTouchMove(99, 10.0f, 10.0f); // unknown pointer ignored
        assert(touches.GetTouch(99) == nullptr);
        touches.OnTouchCancel(1, 960.0f, 540.0f);
        assert(touches.GetActiveTouchCount() == 1);
        assert(touches.GetLastPhase(1) == RANMobile::TouchPhase::Cancel);
        touches.OnTouchUp(2, 1440.0f, 810.0f);
        assert(touches.GetActiveTouchCount() == 0);
        assert(touches.GetLastPhase(2) == RANMobile::TouchPhase::Up);
    }

    // 23. Reusable UI foundation: hit testing and target sizing
    {
        const RANMobile::UI::UIRect rect{10.0f, 20.0f, 48.0f, 48.0f};
        assert(RANMobile::UI::HitTest(rect, 30.0f, 40.0f));
        assert(!RANMobile::UI::HitTest(rect, 70.0f, 40.0f));
        const auto centered = RANMobile::UI::CenteredRect(1280.0f, 720.0f, 420.0f, 200.0f);
        assert(std::fabs(centered.x - 430.0f) < 1e-4f);
        const auto inset = RANMobile::UI::InsetRect(rect, 4.0f);
        assert(std::fabs(inset.width - 40.0f) < 1e-4f);
        RANMobile::UI::MobileUITheme theme;
        assert(theme.minTargetSize >= 48.0f);
    }

    // 24. Login UI: show/hide password, validation, accurate connection state
    {
        auto* ui = engine.GetLoginScreen();
        ui->SetSafeAreaInsets(0.0f, 0.0f, 0.0f, 0.0f);
        const auto toggle = ui->GetPasswordToggleRect();
        assert(toggle.width >= 48.0f || toggle.height >= 44.0f);
        assert(!ui->IsPasswordVisible());
        ui->HandleTouchDown(21, toggle.x + toggle.width * 0.5f, toggle.y + toggle.height * 0.5f);
        assert(ui->IsPasswordVisible());
        ui->HandleTouchUp(21, toggle.x, toggle.y);

        ui->SetUsername("");
        ui->SetPassword("");
        assert(!ui->OnLoginPressed());
        assert(ui->GetAuthState() == LoginAuthState::LoginRejected);
        assert(ui->GetStatusMessage() == "ENTER USERNAME AND PASSWORD");

        ui->SetUsername("famuser");
        ui->SetPassword("secret");
        assert(!ui->OnLoginPressed()); // no NetworkClient::Connect call in this phase
        assert(ui->GetAuthState() == LoginAuthState::ConnectionError);
        assert(ui->GetStatusMessage() == "SERVER NOT CONNECTED");
        assert(ui->GetAuthState() != LoginAuthState::LoginAccepted);

        // Portrait reflow keeps controls inside the design canvas.
        engine.Resize(720, 1280);
        engine.Render();
        assert(ui->IsPortraitLayout());
        const auto portraitLogin = ui->GetLoginButtonRect();
        assert(portraitLogin.x >= 0.0f && portraitLogin.x + portraitLogin.width <= 720.0f);
        assert(portraitLogin.height >= 48.0f);
        engine.Resize(1280, 720);
        engine.Render();
        assert(!ui->IsPortraitLayout());
    }

    // 25. Asset pipeline: source-format validation without fabricated assets
    {
        auto missing = RANMobile::EP7AssetPipeline::ScanDirectory("/nonexistent_ep7_assets");
        assert(!missing.rootExists);
        assert(!missing.ToString().empty());

        namespace fs = std::filesystem;
        const fs::path tmp = fs::temp_directory_path() / "ranmobile_phase3b_assets";
        std::error_code ec;
        fs::remove_all(tmp, ec);
        fs::create_directories(tmp / "texture", ec);
        { std::ofstream(tmp / "bad.dds", std::ios::binary) << "NOPE"; }
        {
            std::ofstream out(tmp / "texture" / "ok.dds", std::ios::binary);
            const char header[4] = {'D', 'D', 'S', ' '};
            out.write(header, 4);
            for (int i = 4; i < 128; ++i) out.put('\0');
        }
        { std::ofstream(tmp / "notes.txt") << "not an EP7 asset"; }

        RANMobile::ResourceManager resources;
        const auto report = RANMobile::EP7AssetPipeline::ScanDirectory(tmp.string(), &resources);
        assert(report.rootExists);
        assert(report.filesScanned == 3);
        assert(report.accepted == 1);
        assert(report.rejected == 2);
        assert(resources.GetResourceCount() == 1);
        assert(report.acceptedByFormat.count(".dds") == 1);
        fs::remove_all(tmp, ec);
    }

    // 26. Renderer lifecycle: surface loss suspends frames, recreation restores them
    {
        auto* renderer = engine.GetRenderer();
        assert(renderer->HasGPUContext());
        renderer->OnSurfaceLost();
        assert(!renderer->HasGPUContext());
        assert(!renderer->BeginFrame());
        renderer->OnSurfaceCreated(1280, 720);
        assert(renderer->HasGPUContext());
        assert(renderer->BeginFrame());
        renderer->EndFrame();
    }

    // 31. Android IME UTF-8 text path: full-field replacement, codepoint-aware
    // deletion, focus transitions and keyboard-responsive layout.
    {
        auto* ui = engine.GetLoginScreen();
        const std::string thai = u8"สวัสดีEP7";
        ui->SetFocus(LoginFocus::Username);
        ui->SetFieldText(LoginFocus::Username, thai);
        assert(ui->GetUsername() == thai);

        ui->HandleTextInput(u8"ก");
        const std::string mixed = thai + u8"ก";
        assert(ui->GetUsername() == mixed);
        ui->HandleBackspace();
        assert(ui->GetUsername() == thai);

        const std::string overLong = std::string(25, 'A');
        ui->SetFieldText(LoginFocus::Username, overLong);
        assert(ui->GetUsername().size() == 20);

        ui->SetFocus(LoginFocus::Password);
        ui->SetFieldText(LoginFocus::Password, u8"密码🔑");
        assert(ui->GetMaskedPassword() == "***");

        ui->SetKeyboardVisible(true);
        assert(ui->IsKeyboardVisible());
        const auto keyboardLayout = ui->GetLoginButtonRect();
        assert(keyboardLayout.y + keyboardLayout.height <= 540.0f);
        ui->SetKeyboardVisible(false);
        assert(!ui->IsKeyboardVisible());

        ui->SetFocus(LoginFocus::None);
        engine.OnIMEActionDone();
        assert(ui->GetAuthState() != LoginAuthState::LoginAccepted);
        ui->SetUsername("");
        ui->SetPassword("");
        ui->SetFocus(LoginFocus::Username);
        engine.OnIMEActionDone();
        assert(ui->GetAuthState() == LoginAuthState::LoginRejected);
        ui->SetFocus(LoginFocus::None);
    }

    engine.Shutdown();
    assert(!engine.IsInitialized());

    std::cout << "ALL TESTS PASSED!\n";
    return 0;
}
