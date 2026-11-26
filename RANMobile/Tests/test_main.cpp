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
#include "../Data/Importer/EP7HashVerify.h"
#include "../Data/Importer/EP7SerialCodec.h"
#include "../Data/Importer/EP7SerialFile.h"
#include "../Data/Importer/EP7SceneData.h"
#include "../Data/Importer/EP7WorldParser.h"
#include "../Engine/Scene/EP7SceneRenderer.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
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

        // StrictAuthentic default must reject legacy GLLEVEL 0x0104 / SLEVEL_HEAD 0x0101.
        const auto strictRejected = RANMobile::EP7LevelHeaderParser::Parse(glmap);
        assert(!strictRejected.valid);
        assert(strictRejected.fileVersion == 0x0104);

        const auto header = RANMobile::EP7LevelHeaderParser::Parse(
            glmap, RANMobile::EP7ValidationMode::LegacySourceCompat);
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
        // StrictAuthentic default rejects plain-text INI mapslist.mst without fallback.
        assert(!scene.Load(tmp.string(), renderer, engine.GetResourceManager()));
        assert(!scene.IsLoaded());
        assert(scene.Load(tmp.string(), renderer, engine.GetResourceManager(),
                          RANMobile::EP7ValidationMode::LegacySourceCompat));
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

    // 32. Phase 3H: ByteCrypt bijections (Old, MapsList, LevelNew) & SHA-256 verification
    {
        assert(RANMobile::EP7Serial::IsValidBijection(RANMobile::EP7Serial::kEncodeTable));
        assert(RANMobile::EP7Serial::IsValidBijection(RANMobile::EP7Serial::kMapsListEncodeTable));
        assert(RANMobile::EP7Serial::IsValidBijection(RANMobile::EP7Serial::kLevelNewEncodeTable));

        const std::vector<RANMobile::EP7Serial::EP7ByteCryptMode> modes = {
            RANMobile::EP7Serial::EP7ByteCryptMode::None,
            RANMobile::EP7Serial::EP7ByteCryptMode::Old,
            RANMobile::EP7Serial::EP7ByteCryptMode::MapsList,
            RANMobile::EP7Serial::EP7ByteCryptMode::LevelNew,
        };
        for (auto m : modes) {
            std::vector<uint8_t> buf(256);
            for (size_t i = 0; i < 256; ++i) buf[i] = static_cast<uint8_t>(i);
            const auto& enc = RANMobile::EP7Serial::EncodeTable(m);
            std::vector<uint8_t> cipher(256);
            for (size_t i = 0; i < 256; ++i) cipher[i] = enc[buf[i]];
            RANMobile::EP7Serial::DecodeInPlace(cipher.data(), cipher.size(), m);
            assert(cipher == buf);
        }

        // FIPS 180-4 SHA-256 vectors
        assert(RANMobile::EP7HashVerify::Sha256Hex(std::vector<uint8_t>{}) ==
               "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        const std::vector<uint8_t> abc = {'a', 'b', 'c'};
        assert(RANMobile::EP7HashVerify::Sha256Hex(abc) ==
               "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        const auto okHash = RANMobile::EP7HashVerify::VerifyBytesAgainstExpected(
            abc, "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD", 3);
        assert(okHash.valid);
        assert(!RANMobile::EP7HashVerify::VerifyBytesAgainstExpected(
            abc, "0000000000000000000000000000000000000000000000000000000000000000", 3).valid);
        assert(!RANMobile::EP7HashVerify::VerifyBytesAgainstExpected(
            abc, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", 4).valid);
    }

    // 33. Phase 3H: Source-layout binary GLMAPS_LIST 0x0200 + glmap 0x0200 / SLEVEL_HEAD 0x0102
    //     round-trip and case-insensitive StrictAuthentic scene index resolution (synthetic fixture)
    {
        namespace fs = std::filesystem;
        const auto pushU16 = [](std::vector<uint8_t>& out, uint16_t v) {
            out.push_back(static_cast<uint8_t>(v & 0xFFu));
            out.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
        };
        const auto pushU32 = [](std::vector<uint8_t>& out, uint32_t v) {
            for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFFu));
        };
        const auto pushStr = [&](std::vector<uint8_t>& out, const std::string& s) {
            pushU32(out, static_cast<uint32_t>(s.size() + 1));
            out.insert(out.end(), s.begin(), s.end());
            out.push_back(0);
        };
        const auto buildMapsList = [&](uint32_t fileVer, const std::string& fileHead,
                                       const std::vector<uint8_t>& plainPayload,
                                       RANMobile::EP7Serial::EP7ByteCryptMode cryptMode) {
            std::vector<uint8_t> out(128, 0);
            std::memcpy(out.data(), fileHead.data(), std::min(fileHead.size(), size_t(127)));
            pushU32(out, fileVer);
            const auto& enc = RANMobile::EP7Serial::EncodeTable(cryptMode);
            for (uint8_t b : plainPayload) out.push_back(enc[b]);
            return out;
        };
        const auto buildSMapNode0203 = [&](bool used, const std::string& file,
                                           uint16_t mainId, uint16_t subId,
                                           const std::string& mapName,
                                           const std::string& loadingImg) {
            std::vector<uint8_t> rec;
            pushU32(rec, 0x0203u);
            rec.push_back(used ? 1u : 0u);
            pushStr(rec, file);
            pushU16(rec, mainId);
            pushU16(rec, subId);
            pushU32(rec, 0u); // dwFieldSID
            for (int i = 0; i < 11; ++i) rec.push_back(0u);
            pushStr(rec, mapName);
            pushStr(rec, "bgm.ogg");
            pushStr(rec, loadingImg);
            for (int i = 0; i < 16; ++i) rec.push_back(0u);
            return rec;
        };
        const auto buildGlmap0200 = [&](const std::string& mapName, const std::string& wldFile,
                                        uint32_t division, uint32_t bright,
                                        uint32_t headVer = 0x0102u,
                                        const std::string& fileType = "glmap",
                                        uint32_t fileVer = 0x0200u,
                                        RANMobile::EP7Serial::EP7ByteCryptMode cryptMode =
                                            RANMobile::EP7Serial::EP7ByteCryptMode::LevelNew) {
            std::vector<uint8_t> block;
            pushStr(block, mapName);
            pushStr(block, wldFile);
            pushU32(block, division);
            pushU32(block, bright);

            std::vector<uint8_t> payload;
            pushU32(payload, headVer);
            pushU32(payload, static_cast<uint32_t>(block.size()));
            payload.insert(payload.end(), block.begin(), block.end());

            std::vector<uint8_t> out(128, 0);
            std::memcpy(out.data(), fileType.data(), std::min(fileType.size(), size_t(127)));
            pushU32(out, fileVer);
            const auto& enc = RANMobile::EP7Serial::EncodeTable(cryptMode);
            for (uint8_t b : payload) out.push_back(enc[b]);
            return out;
        };

        std::vector<uint8_t> mapPayload;
        pushU32(mapPayload, 2u); // 2 records: 1 used, 1 unused
        const auto rec0 = buildSMapNode0203(true, "zone_a.lev", 0, 0, "ZoneA", "load_a.dds");
        const auto rec1 = buildSMapNode0203(false, "zone_b.LEV", 1, 0, "ZoneB", "");
        mapPayload.insert(mapPayload.end(), rec0.begin(), rec0.end());
        mapPayload.insert(mapPayload.end(), rec1.begin(), rec1.end());
        const auto binMapList = buildMapsList(0x0200u, "GLMAPS_LIST", mapPayload,
                                              RANMobile::EP7Serial::EP7ByteCryptMode::MapsList);

        const auto mapReport = RANMobile::EP7BinaryMapListParser::Parse(binMapList);
        assert(mapReport.valid);
        assert(mapReport.format == "GLMAPS_LIST");
        assert(mapReport.fileVersion == 0x0200u);
        assert(mapReport.recordVersion == 0x0203u);
        assert(mapReport.totalRecords == 2u);
        assert(mapReport.usedRecords == 1u);
        assert(mapReport.bytesConsumed == binMapList.size());
        assert(mapReport.entries[0].file == "zone_a.lev");
        assert(mapReport.entries[0].mapName == "ZoneA");

        const auto binLev = buildGlmap0200("", "zone_a.wld", 16u, 2u);
        const auto levHdr = RANMobile::EP7LevelHeaderParser::Parse(binLev);
        assert(levHdr.valid);
        assert(levHdr.fileType == "glmap");
        assert(levHdr.fileVersion == 0x0200u);
        assert(levHdr.headVersion == 0x0102u);
        assert(levHdr.wldFile == "zone_a.wld");
        assert(levHdr.mapName.empty());
        assert(levHdr.division == 16u && levHdr.bright == 2u);
        assert(levHdr.bytesConsumed == binLev.size());

        // Case-insensitive lookup ("zone_a.lev" -> "Zone_A.Lev") & manifest verification
        const fs::path tmp = fs::temp_directory_path() / "ranmobile_phase3h_synth";
        std::error_code ec;
        fs::remove_all(tmp, ec);
        fs::create_directories(tmp / "glogic" / "level", ec);
        fs::create_directories(tmp / "textures", ec);
        {
            std::ofstream o(tmp / "glogic" / "mapslist.mst", std::ios::binary);
            o.write(reinterpret_cast<const char*>(binMapList.data()), binMapList.size());
        }
        {
            std::ofstream o(tmp / "glogic" / "level" / "Zone_A.Lev", std::ios::binary);
            o.write(reinterpret_cast<const char*>(binLev.data()), binLev.size());
        }
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
        {
            std::ofstream o(tmp / "textures" / "LOAD_A.DDS", std::ios::binary);
            o.write(reinterpret_cast<const char*>(dds.data()), dds.size());
        }

        const fs::path manifestFile = tmp / "manifest.tsv";
        {
            std::ofstream m(manifestFile);
            const std::string mSha = RANMobile::EP7HashVerify::Sha256Hex(binMapList);
            const std::string lSha = RANMobile::EP7HashVerify::Sha256Hex(binLev);
            const std::string dSha = RANMobile::EP7HashVerify::Sha256Hex(dds);
            m << "path\toid\tsha256\tsize\n";
            m << "glogic/mapslist.mst\t" << mSha << "\t" << mSha << "\t" << binMapList.size() << "\n";
            m << "glogic/level/Zone_A.Lev\t" << lSha << "\t" << lSha << "\t" << binLev.size() << "\n";
            m << "textures/LOAD_A.DDS\t" << dSha << "\t" << dSha << "\t" << dds.size() << "\n";
        }

        assert(engine.LoadSceneWithManifest(tmp.string(), manifestFile.string()));
        const auto* scenePtr = engine.GetSceneRenderer();
        assert(scenePtr->IsLoaded() && scenePtr->HasPreviewTexture());
        assert(scenePtr->GetValidationMode() == RANMobile::EP7ValidationMode::StrictAuthentic);
        assert(scenePtr->GetResolvedLevelCount() == 1u);
        assert(scenePtr->GetIndex().manifestVerified);
        assert(scenePtr->GetIndex().scenes[0].mapName == "ZoneA");
        assert(scenePtr->GetIndex().scenes[0].wldFile == "zone_a.wld");
        assert(!scenePtr->GetIndex().scenes[0].wldFound);

        // requireReferencedWlds must fail when zone_a.wld is absent
        RANMobile::EP7SceneBuildOptions strictWldOpts;
        strictWldOpts.requireReferencedWlds = true;
        strictWldOpts.manifestPath = manifestFile.string();
        assert(!RANMobile::EP7SceneIndexBuilder::Build(tmp.string(), strictWldOpts).valid);

        // Corrupting 1 byte in mapslist.mst must fail manifest verification strictly
        std::vector<uint8_t> corruptMapList = binMapList;
        corruptMapList.back() ^= 0x01u;
        {
            std::ofstream o(tmp / "glogic" / "mapslist.mst", std::ios::binary);
            o.write(reinterpret_cast<const char*>(corruptMapList.data()), corruptMapList.size());
        }
        assert(!engine.LoadSceneWithManifest(tmp.string(), manifestFile.string()));

        // Restore valid mapslist.mst, add a corrupt .Lev for unused/second entry or corrupt Zone_A.Lev
        {
            std::ofstream o(tmp / "glogic" / "mapslist.mst", std::ios::binary);
            o.write(reinterpret_cast<const char*>(binMapList.data()), binMapList.size());
        }
        std::vector<uint8_t> corruptLev = binLev;
        corruptLev[135] ^= 0xFFu;
        {
            std::ofstream o(tmp / "glogic" / "level" / "Zone_A.Lev", std::ios::binary);
            o.write(reinterpret_cast<const char*>(corruptLev.data()), corruptLev.size());
        }
        assert(!engine.LoadSceneWithManifest(tmp.string(), manifestFile.string()));
        assert(!engine.LoadScene(tmp.string()));

        engine.GetSceneRenderer()->Unload(engine.GetRenderer());
        fs::remove_all(tmp, ec);
    }

    // 34. Phase 3H: Strict no-fallback negative & adversarial tests (mapslist.mst & .Lev)
    {
        const auto pushU16 = [](std::vector<uint8_t>& out, uint16_t v) {
            out.push_back(static_cast<uint8_t>(v & 0xFFu));
            out.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
        };
        const auto pushU32 = [](std::vector<uint8_t>& out, uint32_t v) {
            for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFFu));
        };
        const auto pushStr = [&](std::vector<uint8_t>& out, const std::string& s) {
            pushU32(out, static_cast<uint32_t>(s.size() + 1));
            out.insert(out.end(), s.begin(), s.end());
            out.push_back(0);
        };
        const auto wrapSerial = [&](const std::string& head, uint32_t ver,
                                    const std::vector<uint8_t>& plainPayload,
                                    RANMobile::EP7Serial::EP7ByteCryptMode mode) {
            std::vector<uint8_t> out(128, 0);
            std::memcpy(out.data(), head.data(), std::min(head.size(), size_t(127)));
            pushU32(out, ver);
            const auto& enc = RANMobile::EP7Serial::EncodeTable(mode);
            for (uint8_t b : plainPayload) out.push_back(enc[b]);
            return out;
        };

        // Empty / truncated / unterminated headers
        assert(!RANMobile::EP7BinaryMapListParser::Parse({}).valid);
        assert(!RANMobile::EP7BinaryMapListParser::Parse(std::vector<uint8_t>(131, 0)).valid);
        assert(!RANMobile::EP7LevelHeaderParser::Parse({}).valid);
        assert(!RANMobile::EP7LevelHeaderParser::Parse(std::vector<uint8_t>(131, 0)).valid);
        assert(!RANMobile::EP7SerialFile::ReadHeaderOnly(std::vector<uint8_t>(132, 'A')).valid); // no NUL
        {
            std::vector<uint8_t> nonAsciiHdr(132, 0);
            nonAsciiHdr[0] = 0x80u;
            assert(!RANMobile::EP7SerialFile::ReadHeaderOnly(nonAsciiHdr).valid);
        }

        // Unsupported formats (plain INI / JSON / wrong fileType) rejected with zero fallback
        {
            const std::string ini = "[MAP_LIST_INFO]\nMapKeySize=20\n[MAP_LIST]\nMAP=1,a.lev,0,0,0\n";
            const std::vector<uint8_t> iniBytes(ini.begin(), ini.end());
            assert(!RANMobile::EP7BinaryMapListParser::Parse(iniBytes).valid);

            const std::string json = "{\"maps\":[{\"file\":\"innerzone_01.lev\"}]}";
            assert(!RANMobile::EP7BinaryMapListParser::Parse(
                std::vector<uint8_t>(json.begin(), json.end())).valid);

            const auto wrongHead = wrapSerial("LAND.MAN", 0x0200u, {1, 0, 0, 0},
                                              RANMobile::EP7Serial::EP7ByteCryptMode::MapsList);
            assert(!RANMobile::EP7BinaryMapListParser::Parse(wrongHead).valid);
            assert(!RANMobile::EP7LevelHeaderParser::Parse(wrongHead).valid);
        }

        // Invalid versions in StrictAuthentic
        {
            const auto oldVerMapList = wrapSerial("GLMAPS_LIST", 0x0100u, {1, 0, 0, 0},
                                                  RANMobile::EP7Serial::EP7ByteCryptMode::Old);
            assert(!RANMobile::EP7BinaryMapListParser::Parse(oldVerMapList).valid);

            const auto futureVerMapList = wrapSerial("GLMAPS_LIST", 0x0300u, {1, 0, 0, 0},
                                                     RANMobile::EP7Serial::EP7ByteCryptMode::MapsList);
            assert(!RANMobile::EP7BinaryMapListParser::Parse(futureVerMapList).valid);
        }

        // Helper for single-record GLMAPS_LIST 0x0200 testing
        const auto makeOneMapList = [&](uint32_t recVer, bool used, uint8_t usedByte,
                                        const std::string& file, uint16_t mid, uint16_t sid,
                                        const std::string& name, uint8_t extraFlagByte,
                                        bool appendTrailingByte) {
            (void)used;
            std::vector<uint8_t> p;
            pushU32(p, 1u);
            pushU32(p, recVer);
            p.push_back(usedByte);
            pushStr(p, file);
            pushU16(p, mid);
            pushU16(p, sid);
            pushU32(p, 0u);
            for (int i = 0; i < 11; ++i) p.push_back(extraFlagByte);
            pushStr(p, name);
            pushStr(p, "bgm.ogg");
            pushStr(p, "loading.dds");
            for (int i = 0; i < 16; ++i) p.push_back(0u);
            if (appendTrailingByte) p.push_back(0xAAu);
            return wrapSerial("GLMAPS_LIST", 0x0200u, p,
                              RANMobile::EP7Serial::EP7ByteCryptMode::MapsList);
        };

        // Valid baseline check
        assert(RANMobile::EP7BinaryMapListParser::Parse(
            makeOneMapList(0x0203u, true, 1u, "a.lev", 0, 0, "OK", 0u, false)).valid);
        // Wrong record version (0x0202) in StrictAuthentic
        assert(!RANMobile::EP7BinaryMapListParser::Parse(
            makeOneMapList(0x0202u, true, 1u, "a.lev", 0, 0, "OK", 0u, false)).valid);
        // Invalid bool bytes (>1)
        assert(!RANMobile::EP7BinaryMapListParser::Parse(
            makeOneMapList(0x0203u, true, 2u, "a.lev", 0, 0, "OK", 0u, false)).valid);
        assert(!RANMobile::EP7BinaryMapListParser::Parse(
            makeOneMapList(0x0203u, true, 1u, "a.lev", 0, 0, "OK", 0x42u, false)).valid);
        // Out-of-bounds MainID (>= 256), SubID (>= 256), MapName (> 16)
        assert(!RANMobile::EP7BinaryMapListParser::Parse(
            makeOneMapList(0x0203u, true, 1u, "a.lev", 256, 0, "OK", 0u, false)).valid);
        assert(!RANMobile::EP7BinaryMapListParser::Parse(
            makeOneMapList(0x0203u, true, 1u, "a.lev", 0, 256, "OK", 0u, false)).valid);
        assert(!RANMobile::EP7BinaryMapListParser::Parse(
            makeOneMapList(0x0203u, true, 1u, "a.lev", 0, 0, "12345678901234567", 0u, false)).valid);
        // Path traversal & wrong extension
        assert(!RANMobile::EP7BinaryMapListParser::Parse(
            makeOneMapList(0x0203u, true, 1u, "../evil.lev", 0, 0, "OK", 0u, false)).valid);
        assert(!RANMobile::EP7BinaryMapListParser::Parse(
            makeOneMapList(0x0203u, true, 1u, "dir/evil.lev", 0, 0, "OK", 0u, false)).valid);
        assert(!RANMobile::EP7BinaryMapListParser::Parse(
            makeOneMapList(0x0203u, true, 1u, "evil.txt", 0, 0, "OK", 0u, false)).valid);
        // Trailing unparsed byte & zero used records
        assert(!RANMobile::EP7BinaryMapListParser::Parse(
            makeOneMapList(0x0203u, true, 1u, "a.lev", 0, 0, "OK", 0u, true)).valid);
        assert(!RANMobile::EP7BinaryMapListParser::Parse(
            makeOneMapList(0x0203u, false, 0u, "a.lev", 0, 0, "OK", 0u, false)).valid);

        // Duplicate used ID and duplicate case-insensitive used filename
        {
            std::vector<uint8_t> dupId;
            pushU32(dupId, 2u);
            const auto addRec = [&](uint16_t mid, uint16_t sid, const std::string& f) {
                pushU32(dupId, 0x0203u);
                dupId.push_back(1u);
                pushStr(dupId, f);
                pushU16(dupId, mid);
                pushU16(dupId, sid);
                pushU32(dupId, 0u);
                for (int i = 0; i < 11; ++i) dupId.push_back(0u);
                pushStr(dupId, "Map");
                pushStr(dupId, "bgm.ogg");
                pushStr(dupId, "l.dds");
                for (int i = 0; i < 16; ++i) dupId.push_back(0u);
            };
            addRec(1, 0, "a.lev");
            addRec(1, 0, "b.lev");
            assert(!RANMobile::EP7BinaryMapListParser::Parse(
                wrapSerial("GLMAPS_LIST", 0x0200u, dupId,
                           RANMobile::EP7Serial::EP7ByteCryptMode::MapsList)).valid);

            std::vector<uint8_t> dupFile;
            pushU32(dupFile, 2u);
            auto addRec2 = [&](uint16_t mid, uint16_t sid, const std::string& f) {
                pushU32(dupFile, 0x0203u);
                dupFile.push_back(1u);
                pushStr(dupFile, f);
                pushU16(dupFile, mid);
                pushU16(dupFile, sid);
                pushU32(dupFile, 0u);
                for (int i = 0; i < 11; ++i) dupFile.push_back(0u);
                pushStr(dupFile, "Map");
                pushStr(dupFile, "bgm.ogg");
                pushStr(dupFile, "l.dds");
                for (int i = 0; i < 16; ++i) dupFile.push_back(0u);
            };
            addRec2(1, 0, "Same.Lev");
            addRec2(2, 0, "same.lev");
            assert(!RANMobile::EP7BinaryMapListParser::Parse(
                wrapSerial("GLMAPS_LIST", 0x0200u, dupFile,
                           RANMobile::EP7Serial::EP7ByteCryptMode::MapsList)).valid);
        }

        // Scene index negative checks: missing root, empty dir, mapslist with 0 resolved levels
        {
            assert(!RANMobile::EP7SceneIndexBuilder::Build("/nonexistent_phase3h_dir").valid);
            namespace fs = std::filesystem;
            const fs::path emptyTmp = fs::temp_directory_path() / "ranmobile_phase3h_empty";
            std::error_code ec;
            fs::remove_all(emptyTmp, ec);
            fs::create_directories(emptyTmp, ec);
            assert(!RANMobile::EP7SceneIndexBuilder::Build(emptyTmp.string()).valid);

            const auto onlyMapList = makeOneMapList(0x0203u, true, 1u, "missing.lev", 0, 0, "OK", 0u, false);
            {
                std::ofstream o(emptyTmp / "mapslist.mst", std::ios::binary);
                o.write(reinterpret_cast<const char*>(onlyMapList.data()), onlyMapList.size());
            }
            const auto noLevels = RANMobile::EP7SceneIndexBuilder::Build(emptyTmp.string());
            assert(!noLevels.valid);
            assert(noLevels.resolvedLevelHeaders == 0u);
            assert(noLevels.missingLevelFiles == 1u);
            fs::remove_all(emptyTmp, ec);
        }

        // Truncated payload & oversized dwNum (> MAPLIST_MAX 1000)
        {
            const auto validBytes = makeOneMapList(0x0203u, true, 1u, "a.lev", 0, 0, "OK", 0u, false);
            std::vector<uint8_t> truncated(validBytes.begin(), validBytes.end() - 5);
            assert(!RANMobile::EP7BinaryMapListParser::Parse(truncated).valid);

            std::vector<uint8_t> tooMany;
            pushU32(tooMany, 1001u);
            assert(!RANMobile::EP7BinaryMapListParser::Parse(
                wrapSerial("GLMAPS_LIST", 0x0200u, tooMany,
                           RANMobile::EP7Serial::EP7ByteCryptMode::MapsList)).valid);
        }

        // Malformed CSerialFile strings (0 length, missing NUL, embedded NUL, length > payload)
        {
            std::string strErr;
            size_t off = 0;
            std::string s;
            std::vector<uint8_t> zeroLen = {0, 0, 0, 0};
            assert(!RANMobile::EP7SerialFile::ReadString(zeroLen, off, s, 64, &strErr));

            off = 0;
            std::vector<uint8_t> noNul = {3, 0, 0, 0, 'A', 'B', 'C'};
            assert(!RANMobile::EP7SerialFile::ReadString(noNul, off, s, 64, &strErr));

            off = 0;
            std::vector<uint8_t> embeddedNul = {4, 0, 0, 0, 'A', 0, 'B', 0};
            assert(!RANMobile::EP7SerialFile::ReadString(embeddedNul, off, s, 64, &strErr));

            off = 0;
            std::vector<uint8_t> overLen = {10, 0, 0, 0, 'A', 0};
            assert(!RANMobile::EP7SerialFile::ReadString(overLen, off, s, 64, &strErr));
        }

        // Malformed SLEVEL_HEAD 0x0102: wrong block size, bad division, bad bright, bad wld ext
        const auto makeLevRaw = [&](uint32_t headVer, uint32_t declaredBlockSize,
                                    const std::string& mapName, const std::string& wld,
                                    uint32_t div, uint32_t bright) {
            std::vector<uint8_t> blk;
            pushStr(blk, mapName);
            pushStr(blk, wld);
            pushU32(blk, div);
            pushU32(blk, bright);
            std::vector<uint8_t> p;
            pushU32(p, headVer);
            pushU32(p, declaredBlockSize);
            p.insert(p.end(), blk.begin(), blk.end());
            return wrapSerial("glmap", 0x0200u, p, RANMobile::EP7Serial::EP7ByteCryptMode::LevelNew);
        };
        assert(RANMobile::EP7LevelHeaderParser::Parse(
            makeLevRaw(0x0102u, 23u, "", "a.wld", 16u, 2u)).valid);
        assert(!RANMobile::EP7LevelHeaderParser::Parse(
            makeLevRaw(0x0102u, 24u, "", "a.wld", 16u, 2u)).valid); // block size mismatch
        assert(!RANMobile::EP7LevelHeaderParser::Parse(
            makeLevRaw(0x0102u, 23u, "", "a.wld", 3u, 2u)).valid);  // invalid division
        assert(!RANMobile::EP7LevelHeaderParser::Parse(
            makeLevRaw(0x0102u, 23u, "", "a.wld", 16u, 3u)).valid); // invalid bright
        assert(!RANMobile::EP7LevelHeaderParser::Parse(
            makeLevRaw(0x0102u, 23u, "", "a.txt", 16u, 2u)).valid); // bad .wld ext
        assert(!RANMobile::EP7LevelHeaderParser::Parse(
            makeLevRaw(0x0102u, 25u, "", "../a.wld", 16u, 2u)).valid); // traversal
        assert(!RANMobile::EP7LevelHeaderParser::Parse(
            makeLevRaw(0x0101u, 23u, "", "a.wld", 16u, 2u)).valid); // 0x0101 rejected in strict
    }

    // 35. Phase 3H: Authentic Phase 3G binary verification (executed locally whenever
    //     external/recovery/phase3g/retrieval_manifest.tsv exists, or forced via env var)
    {
        namespace fs = std::filesystem;
        const fs::path manifestPath = "external/recovery/phase3g/retrieval_manifest.tsv";
        const char* forceAuth = std::getenv("RANMOBILE_REQUIRE_AUTHENTIC_PHASE3G");
        const bool requireAuthentic = (forceAuth != nullptr && std::string(forceAuth) == "1");
        const bool manifestExists = fs::exists(manifestPath);
        if (requireAuthentic) {
            assert(manifestExists);
        }

        if (manifestExists) {
            const auto readBin = [](const fs::path& p) {
                std::ifstream in(p, std::ios::binary | std::ios::ate);
                assert(in.is_open());
                const std::streamsize sz = in.tellg();
                assert(sz >= 0);
                in.seekg(0, std::ios::beg);
                std::vector<uint8_t> out(static_cast<size_t>(sz));
                if (sz > 0) assert(in.read(reinterpret_cast<char*>(out.data()), sz));
                return out;
            };

            // Verify all 24 Phase 3G recovered objects against retrieval_manifest.tsv
            const RANMobile::EP7Manifest manifest = RANMobile::EP7HashVerify::LoadManifest(manifestPath.string());
            assert(manifest.loaded);
            assert(manifest.entries.size() == 24u);
            for (const auto& entry : manifest.entries) {
                assert(!entry.localPath.empty());
                assert(fs::exists(entry.localPath));
                const auto bytes = readBin(entry.localPath);
                const auto hv = RANMobile::EP7HashVerify::VerifyBytesAgainstManifest(
                    manifest, entry.repoPath, bytes);
                assert(hv.valid);
            }

            // Authentic mapslist.mst: GLMAPS_LIST 0x0200, SMAPNODE_DATA 0x0203, 13,714 B, 120/104
            const auto mapsBytes = readBin("external/recovery/phase3g/authentic/mapslist.mst");
            const auto mapsReport = RANMobile::EP7BinaryMapListParser::Parse(mapsBytes);
            assert(mapsReport.valid);
            assert(mapsReport.validationMode == RANMobile::EP7ValidationMode::StrictAuthentic);
            assert(mapsReport.format == "GLMAPS_LIST");
            assert(mapsReport.fileVersion == 0x0200u);
            assert(mapsReport.recordVersion == 0x0203u);
            assert(mapsReport.totalRecords == 120u);
            assert(mapsReport.usedRecords == 104u);
            assert(mapsReport.fileBytes == 13714u);
            assert(mapsReport.bytesConsumed == 13714u);
            assert(mapsReport.sha256 ==
                   "cc52d4d0483a7dcd91ced28ac6de783eca0859736ea13bebe5fa7a49cd23488f");
            assert(mapsReport.entries[0].used);
            assert(mapsReport.entries[0].file == "innerzone_01.lev");
            assert(mapsReport.entries[0].mainID == 0u && mapsReport.entries[0].subID == 0u);
            assert(mapsReport.entries[0].mapName == "SG_Campus1F");
            assert(mapsReport.entries[0].bgm == "ran07.ogg");
            assert(mapsReport.entries[0].loadingImage == "loading_032.dds");
            assert(!mapsReport.entries[1].used);
            assert(mapsReport.entries[1].file == "Sea_Death.LEV");
            assert(mapsReport.entries[2].used);
            assert(mapsReport.entries[2].file == "w_school_01.Lev");
            assert(mapsReport.entries[2].mapName == "SG_Campus");
            assert(mapsReport.entries[2].loadingImage == "loading_054.dds");
            for (const auto& rec : mapsReport.entries) {
                assert(rec.recordVersion == 0x0203u);
                assert(rec.mainID < 256u && rec.subID < 256u);
                assert(rec.mapName.size() <= 16u);
            }

            // Authentic innerzone_01.Lev: glmap 0x0200, SLEVEL_HEAD 0x0102, 34-byte block
            const auto levBytes = readBin("external/recovery/phase3g/authentic/innerzone_01.Lev");
            const auto levHdr = RANMobile::EP7LevelHeaderParser::Parse(levBytes);
            assert(levHdr.valid);
            assert(levHdr.fileType == "glmap");
            assert(levHdr.fileVersion == 0x0200u);
            assert(levHdr.headVersion == 0x0102u);
            assert(levHdr.headBlockSize == 34u);
            assert(levHdr.headBlockBytesConsumed == 34u);
            assert(levHdr.bytesConsumed == 174u);
            assert(levHdr.fileBytes == 12654u);
            assert(levHdr.remainingPayloadBytes == 12480u);
            assert(levHdr.mapName.empty());
            assert(levHdr.wldFile == "innerzone_01.wld");
            assert(levHdr.division == 16u);
            assert(levHdr.bright == 2u);
            assert(levHdr.sha256 ==
                   "f1738a67987929d0c2e62248f2987b647fc22516f7cc32835645a1fd24a5235c");

            // Authentic login.wld header + SHA-256 verification
            const auto loginWld = readBin("external/recovery/phase3g/authentic/login.wld");
            const auto wldRes = RANMobile::EP7WorldCharParser::ParseWldHeader(loginWld);
            assert(wldRes.valid && wldRes.fileType == "LAND.MAN" && wldRes.version == 0x0108u);

            // End-to-end StrictAuthentic scene-index & preview load using ONLY verified authentic files
            const fs::path stage = fs::temp_directory_path() / "ranmobile_phase3h_authentic_stage";
            std::error_code ec;
            fs::remove_all(stage, ec);
            fs::create_directories(stage / "data" / "glogic" / "level", ec);
            fs::create_directories(stage / "textures" / "gui", ec);
            fs::copy_file("external/recovery/phase3g/authentic/mapslist.mst",
                          stage / "data" / "glogic" / "mapslist.mst", ec);
            fs::copy_file("external/recovery/phase3g/authentic/innerzone_01.Lev",
                          stage / "data" / "glogic" / "level" / "innerzone_01.Lev", ec);
            fs::copy_file("external/recovery/phase3g/authentic/loading_032.dds",
                          stage / "textures" / "gui" / "loading_032.dds", ec);

            assert(engine.LoadSceneWithManifest(stage.string(), manifestPath.string()));
            const auto* sr = engine.GetSceneRenderer();
            assert(sr->IsLoaded());
            assert(sr->HasPreviewTexture());
            assert(sr->GetIndex().manifestVerified);
            assert(sr->GetIndex().totalMapRecords == 120u);
            assert(sr->GetIndex().usedMapRecords == 104u);
            assert(sr->GetIndex().mapListBytesConsumed == 13714u);
            assert(sr->GetIndex().resolvedLevelHeaders == 1u);
            assert(sr->GetIndex().missingLevelFiles == 103u);
            assert(sr->GetIndex().invalidLevelHeaders == 0u);
            assert(sr->GetIndex().scenes[0].glmapFile == "innerzone_01.lev");
            assert(sr->GetIndex().scenes[0].mapName == "SG_Campus1F");
            assert(sr->GetIndex().scenes[0].wldFile == "innerzone_01.wld");
            assert(sr->GetIndex().scenes[0].division == 16u);
            assert(sr->GetIndex().scenes[0].bright == 2u);
            assert(sr->GetIndex().scenes[0].loadingImage == "loading_032.dds");
            assert(!sr->GetIndex().scenes[0].wldFound);
            assert(sr->GetStatus().find("loading_032.dds (1024x512)") != std::string::npos);

            sr->Draw(engine.GetRenderer());
            engine.GetSceneRenderer()->Unload(engine.GetRenderer());

            // Default StrictAuthentic LoadScene (without manifestPath) also succeeds
            assert(engine.LoadScene(stage.string()));
            engine.GetSceneRenderer()->Unload(engine.GetRenderer());

            // requireAllReferencedLevels = true must reject partial 1-of-104 asset drop
            RANMobile::EP7SceneBuildOptions allLevelsOpts;
            allLevelsOpts.requireAllReferencedLevels = true;
            assert(!RANMobile::EP7SceneIndexBuilder::Build(stage.string(), allLevelsOpts).valid);

            // Corrupt 1 byte of authentic loading_032.dds copy -> strict manifest check must reject
            {
                std::fstream d(stage / "textures" / "gui" / "loading_032.dds",
                               std::ios::binary | std::ios::in | std::ios::out);
                assert(d.is_open());
                d.seekg(200);
                char byte = 0;
                d.read(&byte, 1);
                byte = static_cast<char>(byte ^ 0x33);
                d.seekp(200);
                d.write(&byte, 1);
            }
            assert(!engine.LoadSceneWithManifest(stage.string(), manifestPath.string()));
            fs::copy_file("external/recovery/phase3g/authentic/loading_032.dds",
                          stage / "textures" / "gui" / "loading_032.dds",
                          fs::copy_options::overwrite_existing, ec);

            // Corrupt 1 byte of authentic .Lev copy -> strict manifest check must reject
            {
                std::fstream c(stage / "data" / "glogic" / "level" / "innerzone_01.Lev",
                               std::ios::binary | std::ios::in | std::ios::out);
                assert(c.is_open());
                c.seekg(140);
                char byte = 0;
                c.read(&byte, 1);
                byte = static_cast<char>(byte ^ 0x55);
                c.seekp(140);
                c.write(&byte, 1);
            }
            assert(!engine.LoadSceneWithManifest(stage.string(), manifestPath.string()));

            fs::remove_all(stage, ec);
            std::cout << "[PHASE 3H] Authentic Phase 3G verification PASSED (24/24 manifest objects, 13,714 B mapslist.mst, 12,654 B innerzone_01.Lev, 1024x512 loading_032.dds)\n";
        } else {
            std::cout << "[PHASE 3H] Authentic Phase 3G local files absent; manifest checks skipped (CI mode)\n";
        }
    }

    // 36. Phase 3I: TGA (16/24/32-bpp, RLE, top-down/bottom-up, 16-bit alpha) & baseline JPEG SOF0 + DecodeTextureFile
    {
        // Uncompressed 2x1 32-bpp BGRA top-down TGA
        std::vector<uint8_t> tga32(18 + 8, 0);
        tga32[2] = 2; // uncompressed true-color
        tga32[12] = 2; tga32[14] = 1; tga32[16] = 32; tga32[17] = 0x28; // top-down + 8 alpha bits
        // pixel 0: B=10, G=20, R=30, A=40; pixel 1: B=200, G=150, R=100, A=255
        tga32[18] = 10; tga32[19] = 20; tga32[20] = 30; tga32[21] = 40;
        tga32[22] = 200; tga32[23] = 150; tga32[24] = 100; tga32[25] = 255;
        const auto d32 = RANMobile::EP7TextureDecoder::DecodeTextureFile(tga32, "fx_alpha.tga");
        assert(d32.valid && d32.width == 2 && d32.height == 1);
        assert(d32.rgba8[0] == 30 && d32.rgba8[1] == 20 && d32.rgba8[2] == 10 && d32.rgba8[3] == 40);
        assert(d32.rgba8[4] == 100 && d32.rgba8[5] == 150 && d32.rgba8[6] == 200 && d32.rgba8[7] == 255);
        assert(d32.alphaMode == RANMobile::EP7AlphaMode::Soft);

        // RLE 2x2 24-bpp bottom-up TGA: row0(bottom)=red(2), row1(top)=green(2)
        std::vector<uint8_t> tgaRle(18, 0);
        tgaRle[2] = 10; // RLE true-color
        tgaRle[12] = 2; tgaRle[14] = 2; tgaRle[16] = 24; tgaRle[17] = 0x00; // bottom-up
        tgaRle.push_back(0x81); tgaRle.push_back(0); tgaRle.push_back(0); tgaRle.push_back(255); // run=2 red (BGR)
        tgaRle.push_back(0x81); tgaRle.push_back(0); tgaRle.push_back(255); tgaRle.push_back(0); // run=2 green (BGR)
        const auto dRle = RANMobile::EP7TextureDecoder::DecodeTGA(tgaRle, "rle_24.tga");
        assert(dRle.valid && dRle.width == 2 && dRle.height == 2);
        // top row should be green, bottom row should be red
        assert(dRle.rgba8[0] == 0 && dRle.rgba8[1] == 255 && dRle.rgba8[2] == 0 && dRle.rgba8[3] == 255);
        assert(dRle.rgba8[8] == 255 && dRle.rgba8[9] == 0 && dRle.rgba8[10] == 0 && dRle.rgba8[11] == 255);

        // 16-bpp TGA with 1-bit alpha attribute (descriptor & 0x0F == 1)
        std::vector<uint8_t> tga16(18 + 4, 0);
        tga16[2] = 2; tga16[12] = 2; tga16[14] = 1; tga16[16] = 16; tga16[17] = 0x21; // top-down + 1 alpha bit
        // pixel 0: alpha=0 (0x0000); pixel 1: alpha=1 + white (0xFFFF)
        tga16[18] = 0x00; tga16[19] = 0x00;
        tga16[20] = 0xFF; tga16[21] = 0xFF;
        const auto d16 = RANMobile::EP7TextureDecoder::DecodeTGA(tga16, "cutout16.tga");
        assert(d16.valid && d16.header.hasAlpha);
        assert(d16.rgba8[3] == 0 && d16.rgba8[7] == 255);

        // Minimal 8x8 grayscale baseline SOF0 JPEG (all-black DC=0, EOB=0, DQT=1)
        std::vector<uint8_t> jpg;
        auto putBE16 = [&](uint16_t v) { jpg.push_back(uint8_t(v >> 8)); jpg.push_back(uint8_t(v & 0xFF)); };
        jpg.push_back(0xFF); jpg.push_back(0xD8); // SOI
        jpg.push_back(0xFF); jpg.push_back(0xDB); putBE16(67); jpg.push_back(0);
        for (int i = 0; i < 64; ++i) jpg.push_back(1);
        jpg.push_back(0xFF); jpg.push_back(0xC0); putBE16(11); jpg.push_back(8); putBE16(8); putBE16(8);
        jpg.push_back(1); jpg.push_back(1); jpg.push_back(0x11); jpg.push_back(0);
        // DHT DC id=0: 1 symbol (0x00 -> code 0 len 1)
        jpg.push_back(0xFF); jpg.push_back(0xC4); putBE16(2 + 17 + 1); jpg.push_back(0x00);
        jpg.push_back(1); for (int i = 1; i < 16; ++i) jpg.push_back(0); jpg.push_back(0x00);
        // DHT AC id=0: 1 symbol (0x00 EOB -> code 0 len 1)
        jpg.push_back(0xFF); jpg.push_back(0xC4); putBE16(2 + 17 + 1); jpg.push_back(0x10);
        jpg.push_back(1); for (int i = 1; i < 16; ++i) jpg.push_back(0); jpg.push_back(0x00);
        // SOS: 1 comp, DC/AC 0, spectral 0..63, bit stream: 0 (DC len 0) + 0 (EOB) -> 0b00000000 = 0x00
        jpg.push_back(0xFF); jpg.push_back(0xDA); putBE16(8);
        jpg.push_back(1); jpg.push_back(1); jpg.push_back(0x00); jpg.push_back(0); jpg.push_back(63); jpg.push_back(0);
        jpg.push_back(0x00);
        jpg.push_back(0xFF); jpg.push_back(0xD9); // EOI
        const auto dJpg = RANMobile::EP7TextureDecoder::DecodeTextureFile(jpg, "mini.jpg");
        assert(dJpg.valid && dJpg.width == 8 && dJpg.height == 8);
        assert(dJpg.header.format == RANMobile::EP7TextureFormat::JPEG);
        assert(dJpg.rgba8[0] == 128 && dJpg.rgba8[3] == 255); // Y=128 midpoint gray

        // Negative checks: empty, truncated TGA, progressive SOF2 JPEG
        assert(!RANMobile::EP7TextureDecoder::DecodeTextureFile({}, "empty.tga").valid);
        assert(!RANMobile::EP7TextureDecoder::DecodeTGA(std::vector<uint8_t>(10, 0), "bad.tga").valid);
        std::vector<uint8_t> progJpg = {0xFF, 0xD8, 0xFF, 0xC2, 0x00, 0x04, 0x08, 0x00};
        assert(!RANMobile::EP7TextureDecoder::DecodeJPEG(progJpg).valid);
    }

    // 37. Phase 3I: MobileCamera LH View/Proj + 6-Plane Frustum Culling + MobileRenderer 3D Mesh Registry & EGL Reset
    {
        RANMobile::MobileCamera cam3d;
        cam3d.SetDistanceBounds(2.0f, 500.0f);
        cam3d.SetProjectionParams(60.0f, 16.0f / 9.0f, 1.0f, 1000.0f);
        cam3d.Reset(0.0f, 0.0f, 20.0f, 0.0f, 0.0f, 10.0f);

        float ex = 0.0f, ey = 0.0f, ez = 0.0f;
        cam3d.GetEyePosition(ex, ey, ez);
        assert(std::fabs(ex) < 1e-4f && std::fabs(ey) < 1e-4f && std::fabs(ez - (-10.0f)) < 1e-4f);

        float vp[16]{};
        cam3d.ComputeViewProjectionMatrix(vp);
        // Point (0, 0, -10 + 1) = (0, 0, -9) is right at nearZ=1 -> z_ndc == -1
        const float nearZ = -9.0f;
        const float zClipNear = vp[2] * 0 + vp[6] * 0 + vp[10] * nearZ + vp[14];
        const float wClipNear = vp[3] * 0 + vp[7] * 0 + vp[11] * nearZ + vp[15];
        assert(std::fabs((zClipNear / wClipNear) - (-1.0f)) < 1e-4f);

        // Point (0, 0, -10 + 1000) = (0, 0, 990) is right at farZ=1000 -> z_ndc == +1
        const float farPt = 990.0f;
        const float zClipFar = vp[2] * 0 + vp[6] * 0 + vp[10] * farPt + vp[14];
        const float wClipFar = vp[3] * 0 + vp[7] * 0 + vp[11] * farPt + vp[15];
        assert(std::fabs((zClipFar / wClipFar) - 1.0f) < 1e-4f);

        auto* renderer = engine.GetRenderer();
        renderer->SetCameraMatrices(cam3d);
        assert(renderer->IsBoxInFrustum(-2.0f, -2.0f, 8.0f, 2.0f, 2.0f, 12.0f));     // at target -> IN
        assert(!renderer->IsBoxInFrustum(-2.0f, -2.0f, -40.0f, 2.0f, 2.0f, -25.0f)); // behind camera -> CULLED
        assert(!renderer->IsBoxInFrustum(400.0f, -2.0f, 8.0f, 410.0f, 2.0f, 12.0f)); // outside right plane -> CULLED

        // Register 3D mesh & world texture, survive OnSurfaceLost/OnSurfaceCreated, render 3D pass
        const std::vector<RANMobile::EP7WorldVertex> triVerts = {
            {0.0f, 1.0f, 10.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0xFFFFFFFFu},
            {1.0f, -1.0f, 10.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f, 0xFFFFFFFFu},
            {-1.0f, -1.0f, 10.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0xFFFFFFFFu}
        };
        const std::vector<uint16_t> triIdx = {0, 1, 2};
        const size_t meshBefore = renderer->GetMesh3DCount();
        const uint32_t mHandle = renderer->RegisterMesh3D(triVerts, triIdx);
        assert(mHandle != 0 && renderer->GetMesh3DCount() == meshBefore + 1);
        const std::vector<uint8_t> whiteTex(4 * 4 * 4, 255);
        assert(renderer->LoadWorldTexture("test3i_white", 4, 4, whiteTex));
        const uint32_t texId = renderer->GetAssetTexture("test3i_white");
        assert(texId != 0);

        // Simulate EGL context loss & recreation -> mesh & texture both remain intact
        renderer->OnSurfaceLost();
        assert(!renderer->HasGPUContext());
        renderer->OnSurfaceCreated(1280, 720);
        assert(renderer->HasGPUContext());
        assert(renderer->GetMesh3DCount() == meshBefore + 1);
        assert(renderer->GetAssetTexture("test3i_white") != 0);

        assert(renderer->BeginFrame());
        RANMobile::WorldLightingState lightSt;
        assert(renderer->BeginWorldPass(cam3d, lightSt));
        RANMobile::EP7AABB inBox{{-1.0f, -1.0f, 9.5f}, {1.0f, 1.0f, 10.5f}, true};
        RANMobile::EP7AABB behindBox{{-1.0f, -1.0f, -35.0f}, {1.0f, 1.0f, -25.0f}, true};
        renderer->DrawMesh3D(mHandle, renderer->GetAssetTexture("test3i_white"),
                             RANMobile::EP7Mat4::Identity(), inBox);
        renderer->DrawMesh3D(mHandle, 0, RANMobile::EP7Mat4::Identity(), behindBox);
        renderer->EndWorldPass();
        renderer->EndFrame();
        assert(renderer->GetStats().drawCalls == 1u);
        assert(renderer->GetStats().triangles == 1u);
        assert(renderer->GetStats().culledObjects == 1u);

        renderer->UnregisterMesh3D(mHandle);
        renderer->UnloadAssetTexture("test3i_white");
        renderer->ClearCamera3D();
    }

    // 38. Phase 3I: EP7WorldParser negative guards + Multi-Touch ScenePreview/InGame camera & HUD DPI routing
    {
        assert(!RANMobile::EP7WorldParser::ParseWld({}).valid);
        assert(!RANMobile::EP7WorldParser::ParseFullLevel({}).valid);
        RANMobile::EP7PieceAsset badPiece;
        std::string pieceErr;
        assert(!RANMobile::EP7WorldParser::ParsePieceFile("bad.pie", {}, badPiece, &pieceErr));
        assert(!pieceErr.empty());

        // Multi-touch camera orbit & two-finger pinch in ScenePreview
        engine.SetScreenState(EngineScreenState::ScenePreview);
        engine.Resize(1920, 1080);
        auto* cam = engine.GetCamera();
        cam->SetDistanceBounds(5.0f, 200.0f);
        cam->Reset(10.0f, 20.0f, 50.0f, 0.0f, 0.0f, 0.0f);
        const float startYaw = cam->GetYaw();
        engine.OnTouchDown(10, 800.0f, 400.0f);
        engine.OnTouchMove(10, 920.0f, 440.0f);
        engine.Update(0.5f);
        assert(cam->GetYaw() > startYaw);
        engine.OnTouchDown(11, 1020.0f, 440.0f);
        engine.OnTouchMove(11, 1220.0f, 440.0f); // spread fingers -> zoom in
        assert(cam->GetDistance() < 50.0f);
        engine.OnTouchUp(10, 920.0f, 440.0f);
        engine.OnTouchUp(11, 1220.0f, 440.0f);

        // InGame HUD + simultaneous camera touch + design-space draw calls
        engine.SetScreenState(EngineScreenState::InGame);
        engine.Render();
        assert(engine.GetRenderer()->GetStats().drawCalls > 0);
        engine.Resize(1280, 720);
        engine.SetScreenState(EngineScreenState::Login);
    }

    // 39. Phase 3I: Authentic EP7 3D world, pieces, level, textures & end-to-end 3D render verification
    {
        namespace fs = std::filesystem;
        const fs::path p3iManifest = "external/recovery/phase3i/retrieval_manifest.tsv";
        const char* forceP3I = std::getenv("RANMOBILE_REQUIRE_AUTHENTIC_PHASE3I");
        const bool requireP3I = (forceP3I != nullptr && std::string(forceP3I) == "1");
        const bool hasP3I = fs::exists(p3iManifest);
        if (requireP3I) {
            assert(hasP3I);
        }

        if (hasP3I) {
            const auto readBin = [](const fs::path& p) {
                std::ifstream in(p, std::ios::binary | std::ios::ate);
                assert(in.is_open());
                const std::streamsize sz = in.tellg();
                assert(sz >= 0);
                in.seekg(0, std::ios::beg);
                std::vector<uint8_t> out(static_cast<size_t>(sz));
                if (sz > 0) assert(in.read(reinterpret_cast<char*>(out.data()), sz));
                return out;
            };

            // Verify all 63 Phase 3I manifest entries against SHA-256 + exact byte length
            const auto manifest = RANMobile::EP7HashVerify::LoadManifest(p3iManifest.string());
            assert(manifest.loaded && manifest.entries.size() == 63u);
            for (const auto& entry : manifest.entries) {
                assert(!entry.localPath.empty() && fs::exists(entry.localPath));
                const auto bytes = readBin(entry.localPath);
                const auto hv = RANMobile::EP7HashVerify::VerifyBytesAgainstManifest(
                    manifest, entry.repoPath, bytes);
                assert(hv.valid);
            }

            // Authentic login.wld (0x0108, 3,098,590 B exact EOF)
            if (fs::exists("external/recovery/phase3g/authentic/login.wld")) {
                const auto lwBytes = readBin("external/recovery/phase3g/authentic/login.wld");
                const auto lw = RANMobile::EP7WorldParser::ParseWld(lwBytes);
                assert(lw.valid);
                assert(lw.version == 0x0108u);
                assert(lw.wldBytesConsumed == 3098590u && lw.wldBytesConsumed == lw.wldFileBytes);
                assert(lw.terrainBatches.size() == 191u);
                assert(lw.TotalTerrainVertices() == 15570u);
                assert(lw.TotalTerrainTriangles() == 10817u);
                assert(lw.lights.size() == 1u);
                assert(lw.fog.enabled && std::fabs(lw.fog.fogStart - 1800.0f) < 0.1f &&
                       std::fabs(lw.fog.fogEnd - 2000.0f) < 0.1f);
                assert(lw.worldBounds.valid && std::fabs(lw.worldBounds.max.x) < 1e5f);
            }

            // Authentic innerzone_01.wld + innerzone_01.wld0 (0x0113, exact EOF on both)
            const auto izWld = readBin("external/recovery/phase3i/authentic/innerzone_01.wld");
            const auto izWld0 = readBin("external/recovery/phase3i/authentic/innerzone_01.wld0");
            auto iz = RANMobile::EP7WorldParser::ParseWld(izWld, izWld0);
            assert(iz.valid && iz.version == 0x0113u);
            assert(iz.wldBytesConsumed == 1190775u && iz.wld0BytesConsumed == 5808480u);
            assert(iz.terrainBatches.size() == 140u);
            assert(iz.TotalTerrainVertices() == 77960u);
            assert(iz.TotalTerrainTriangles() == 50243u);
            assert(iz.lights.size() == 49u);
            assert(iz.navVertexCount == 11364u && iz.navCellCount == 4205u);
            assert(iz.pieceInstances.size() == 579u);
            assert(iz.fog.enabled && std::fabs(iz.fog.fogStart - 720.0f) < 0.1f &&
                   std::fabs(iz.fog.fogEnd - 790.0f) < 0.1f);

            RANMobile::EP7WorldParser::ResolveAndAttachPieces(
                iz, "external/recovery/phase3i/authentic/piece");
            assert(iz.pieceAssets.size() == 12u);
            assert(iz.missingPieceFiles.empty());
            for (const auto& pa : iz.pieceAssets) {
                assert(pa.valid && !pa.batches.empty());
            }

            // Authentic innerzone_01.Lev (0x0200, 12,654 B exact EOF)
            if (fs::exists("external/recovery/phase3g/authentic/innerzone_01.Lev")) {
                const auto levBytes = readBin("external/recovery/phase3g/authentic/innerzone_01.Lev");
                const auto lev = RANMobile::EP7WorldParser::ParseFullLevel(levBytes);
                assert(lev.valid && lev.bytesConsumed == 12654u);
                assert(lev.mobSpawns.size() == 44u && lev.gates.size() == 10u);
            }

            // All 12 authentic .pie / .pis pieces + all 12 Phase 3I textures
            size_t pieceFilesVerified = 0;
            for (const auto& entry : fs::directory_iterator("external/recovery/phase3i/authentic/piece")) {
                if (!entry.is_regular_file()) continue;
                const auto bytes = readBin(entry.path());
                RANMobile::EP7PieceAsset piece;
                assert(RANMobile::EP7WorldParser::ParsePieceFile(
                    entry.path().filename().string(), bytes, piece));
                assert(piece.valid && piece.bytesConsumed == bytes.size());
                ++pieceFilesVerified;
            }
            assert(pieceFilesVerified == 12u);

            size_t texturesDecoded = 0;
            for (const auto& entry : fs::directory_iterator("external/recovery/phase3i/authentic/textures")) {
                if (!entry.is_regular_file()) continue;
                const auto bytes = readBin(entry.path());
                const auto dec = RANMobile::EP7TextureDecoder::DecodeTextureFile(
                    bytes, entry.path().filename().string());
                assert(dec.valid && dec.width > 0 && dec.height > 0);
                ++texturesDecoded;
            }
            assert(texturesDecoded == 12u);

            // End-to-end 3D scene load, multi-pass render, frustum culling, and EGL surface recovery
            if (fs::exists("external/recovery/phase3g/authentic/mapslist.mst") &&
                fs::exists("external/recovery/phase3g/authentic/innerzone_01.Lev") &&
                fs::exists("external/recovery/phase3g/authentic/loading_032.dds")) {
                const fs::path stage3d = fs::temp_directory_path() / "ranmobile_phase3i_3d_stage";
                std::error_code ec;
                fs::remove_all(stage3d, ec);
                fs::create_directories(stage3d / "data" / "glogic" / "level", ec);
                fs::create_directories(stage3d / "data" / "map", ec);
                fs::create_directories(stage3d / "data" / "piece", ec);
                fs::create_directories(stage3d / "textures" / "gui", ec);
                fs::create_directories(stage3d / "textures" / "map", ec);
                fs::copy_file("external/recovery/phase3g/authentic/mapslist.mst",
                              stage3d / "data" / "glogic" / "mapslist.mst", ec);
                fs::copy_file("external/recovery/phase3g/authentic/innerzone_01.Lev",
                              stage3d / "data" / "glogic" / "level" / "innerzone_01.Lev", ec);
                fs::copy_file("external/recovery/phase3g/authentic/loading_032.dds",
                              stage3d / "textures" / "gui" / "loading_032.dds", ec);
                fs::copy_file("external/recovery/phase3i/authentic/innerzone_01.wld",
                              stage3d / "data" / "map" / "innerzone_01.wld", ec);
                fs::copy_file("external/recovery/phase3i/authentic/innerzone_01.wld0",
                              stage3d / "data" / "map" / "innerzone_01.wld0", ec);
                for (const auto& entry : fs::directory_iterator("external/recovery/phase3i/authentic/piece")) {
                    if (entry.is_regular_file()) {
                        fs::copy_file(entry.path(), stage3d / "data" / "piece" / entry.path().filename(), ec);
                    }
                }
                for (const auto& entry : fs::directory_iterator("external/recovery/phase3i/authentic/textures")) {
                    if (entry.is_regular_file()) {
                        fs::copy_file(entry.path(), stage3d / "textures" / "map" / entry.path().filename(), ec);
                    }
                }

                assert(engine.LoadScene(stage3d.string()));
                const auto* sr = engine.GetSceneRenderer();
                assert(sr->IsLoaded() && sr->HasWorld3D() && sr->HasPreviewTexture());
                assert(sr->GetCurrentWorldFile() == "innerzone_01.wld");
                assert(sr->GetWorldData().pieceAssets.size() == 12u);
                assert(sr->GetWorldData().missingPieceFiles.empty());
                assert(sr->GetLoadedTextureCount() >= 5u);
                assert(sr->GetRegisteredMeshCount() > 140u);

                engine.SetScreenState(EngineScreenState::ScenePreview);
                engine.SetDiagnosticsVisible(true);
                engine.Render();
                const auto stats1 = engine.GetRenderer()->GetStats();
                assert(stats1.drawCalls > 100u && stats1.triangles > 10000u && stats1.culledObjects > 0u);

                // Rotate camera + move into InGame with HUD and test EGL surface recreation
                engine.OnTouchDown(1, 500.0f, 360.0f);
                engine.OnTouchMove(1, 680.0f, 320.0f);
                engine.OnTouchUp(1, 680.0f, 320.0f);
                engine.Update(1.0f / 30.0f);
                engine.SetScreenState(EngineScreenState::InGame);
                engine.OnSurfaceLost();
                engine.OnSurfaceCreated(1080, 1920);
                engine.Render();
                assert(engine.GetRenderer()->GetStats().drawCalls > 100u);

                engine.SetDiagnosticsVisible(false);
                engine.SetScreenState(EngineScreenState::Login);
                engine.Resize(1280, 720);
                engine.GetSceneRenderer()->Unload(engine.GetRenderer());

                if (fs::exists("external/recovery/phase3g/authentic/login.wld")) {
                    fs::copy_file("external/recovery/phase3g/authentic/login.wld",
                                  stage3d / "data" / "map" / "login.wld", ec);
                    assert(engine.LoadStandaloneWorld(stage3d.string(), "login.wld"));
                    assert(sr->HasWorld3D() && sr->GetCurrentWorldFile() == "login.wld");
                    engine.SetScreenState(EngineScreenState::ScenePreview);
                    engine.Render();
                    assert(engine.GetRenderer()->GetStats().drawCalls > 50u);
                    engine.SetScreenState(EngineScreenState::Login);
                    engine.GetSceneRenderer()->Unload(engine.GetRenderer());
                }

                fs::remove_all(stage3d, ec);
            }

            std::cout << "[PHASE 3I] Authentic Phase 3I verification PASSED (63/63 manifest objects, login.wld + innerzone_01.{wld,wld0,Lev} exact EOF, 12/12 .pie/.pis, 12/12 textures, 3D multi-pass render)\n";
        } else {
            std::cout << "[PHASE 3I] Authentic Phase 3I local files absent; manifest checks skipped (CI mode)\n";
        }
    }

    engine.Shutdown();
    assert(!engine.IsInitialized());

    std::cout << "ALL TESTS PASSED!\n";
    return 0;
}
