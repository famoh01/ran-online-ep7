cmake_minimum_required(VERSION 3.22)
project(RANMobile LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(RANMOBILE_SOURCES
    Engine/Core/Logger.cpp
    Engine/Core/RANMobileEngine.cpp
    Engine/Renderer/MobileRenderer.cpp
    Engine/Renderer/GpuSelfTest.cpp
    Engine/Resource/ResourceManager.cpp
    Engine/Touch/TouchSystem.cpp
    Engine/Camera/MobileCamera.cpp
    Engine/Scene/EP7CharacterScene.cpp
    Engine/Animation/AnimationManager.cpp
    Engine/Animation/EP7SkinAniControl.cpp
    Engine/Audio/AudioManager.cpp
    Engine/Network/NetworkClient.cpp
    Data/Validator/DataValidator.cpp
    Data/Importer/EP7AssetImporter.cpp
    Data/Importer/EP7TextureDecoder.cpp
    Data/Importer/EP7WorldCharParser.cpp
    Data/Importer/EP7Animation.cpp
    Data/Importer/EP7CharacterFile.cpp
    Data/Importer/EP7RccArchive.cpp
    Data/Importer/EP7Skeleton.cpp
    Data/Importer/EP7SkinPiece.cpp
    Data/Importer/EP7SkinnedMesh.cpp
    Data/Importer/EP7XFile.cpp
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
    UI/Character/CharacterPreviewOverlay.cpp
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

    # Phase 3J: character / skeleton / animation parser + skinning tests.
    # These use suite-authored ABI fixtures only; authentic-asset verification is the
    # separate verify_ep7_character_assets tool below (needs an extracted EP7 client).
    add_executable(ranmobile_phase3j_tests Tests/test_phase3j.cpp)
    target_link_libraries(ranmobile_phase3j_tests PRIVATE ranmobile_core pthread)

    add_executable(verify_ep7_character_assets Tools/verify_ep7_character_assets.cpp)
    target_link_libraries(verify_ep7_character_assets PRIVATE ranmobile_core pthread)
endif()
