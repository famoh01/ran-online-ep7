cmake_minimum_required(VERSION 3.22)
project(RANMobile LANGUAGES C CXX)

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
    Data/Importer/EP7TextureDecoder.cpp
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
    Server/MobileServerHost.cpp
)

if(ANDROID)
    if(CMAKE_ANDROID_NDK)
        set(NDK_ROOT "${CMAKE_ANDROID_NDK}")
    elseif(ANDROID_NDK)
        set(NDK_ROOT "${ANDROID_NDK}")
    elseif(DEFINED ENV{ANDROID_NDK_HOME})
        set(NDK_ROOT "$ENV{ANDROID_NDK_HOME}")
    endif()

    set(NATIVE_APP_GLUE_DIR "${NDK_ROOT}/sources/android/native_app_glue")
    if(NOT EXISTS "${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c")
        message(FATAL_ERROR "android_native_app_glue.c not found at: ${NATIVE_APP_GLUE_DIR}")
    endif()

    add_library(ranmobile SHARED
        ${RANMOBILE_SOURCES}
        Android/native_bridge.cpp
        "${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c"
    )
    target_include_directories(ranmobile PRIVATE "${NATIVE_APP_GLUE_DIR}")
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
