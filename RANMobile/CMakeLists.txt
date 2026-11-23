cmake_minimum_required(VERSION 3.22)
project(RANMobile LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(RANMOBILE_SOURCES
    Engine/Core/Logger.cpp
    Engine/Core/RANMobileEngine.cpp
    Engine/Renderer/MobileRenderer.cpp
    Engine/Resource/ResourceManager.cpp
    Engine/Touch/TouchSystem.cpp
    Engine/Camera/MobileCamera.cpp
    Engine/Animation/AnimationManager.cpp
    Engine/Audio/AudioManager.cpp
    Engine/Network/NetworkClient.cpp
    Data/Validator/DataValidator.cpp
    Data/Importer/EP7AssetImporter.cpp
    Data/Converter/RANMobileDataConverter.cpp
    Data/Loader/GameDataLoader.cpp
    Data/Loader/ThaiPackLoader.cpp
    Client/Login/AuthenticationManager.cpp
    Client/Character/CharacterSelectManager.cpp
    Client/Gameplay/CharacterController.cpp
    Client/Combat/TargetManager.cpp
    Client/Combat/CombatController.cpp
    Client/Inventory/InventoryManager.cpp
    Client/Quest/QuestManager.cpp
    Client/Map/MapManager.cpp
    Client/Chat/ChatManager.cpp
    UI/Login/LoginScreen.cpp
    UI/Joystick/VirtualJoystick.cpp
    UI/Buttons/MobileButton.cpp
    UI/Buttons/MobileSkillButton.cpp
    UI/HUD/MobileHUD.cpp
    UI/Quest/QuestUI.cpp
)

if(ANDROID)
    add_library(ranmobile SHARED
        ${RANMOBILE_SOURCES}
        Android/native_bridge.cpp
    )
    target_link_libraries(ranmobile
        android
        log
        EGL
        GLESv3
    )
else()
    add_library(ranmobile_core STATIC ${RANMOBILE_SOURCES})
    add_executable(ranmobile_tests Tests/test_main.cpp)
    target_link_libraries(ranmobile_tests PRIVATE ranmobile_core pthread)
endif()
