cmake_minimum_required(VERSION 3.21)
include(cmake/PRG32Version.cmake)
project(PRG32_QT VERSION ${PRG32QT_VERSION_CORE} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(PRG32QT_BUILD_APP "Build Qt application" ON)
option(PRG32QT_BUILD_TESTS "Build portable core tests" ON)
option(PRG32QT_TV_MODE "Build a controller-first fullscreen TV application" OFF)
# The PRG32 firmware's own audio engine (components/prg32_audio of the PRG32 SDK), so the desktop
# renders exactly the console's sound: 8 voices, ADSR envelopes, the SID-like synth, the stereo
# mix. On by default; OFF keeps the older float mixer (no ADSR), which is not an audio reference.
option(PRG32QT_FIRMWARE_AUDIO "Render sound with the PRG32 firmware audio engine" ON)
set(PRG32_SDK_DIR "" CACHE PATH "PRG32 SDK checkout (default: the pinned PRG32_SDK_REVISION, fetched)")
set(PRG32_SDK_REVISION "679dfb58783bd007125de9ca0b75eaa077908739" CACHE STRING "PRG32 SDK commit fetched when PRG32_SDK_DIR is empty")

file(WRITE "${CMAKE_BINARY_DIR}/prg32qt-version.txt" "${PRG32QT_VERSION}\n")

add_library(prg32qt_core
    src/core/Audio.cpp
    src/core/Cartridge.cpp
    src/core/Framebuffer.cpp
    src/core/PerformanceTiming.cpp
    src/core/RiscVDisassembler.cpp
    src/core/Rv32Cpu.cpp
    src/core/Runtime.cpp)
target_include_directories(prg32qt_core PUBLIC src/core)
target_compile_definitions(prg32qt_core PUBLIC PRG32QT_VERSION="${PRG32QT_VERSION}")

if(PRG32QT_FIRMWARE_AUDIO)
  if(NOT PRG32_SDK_DIR)
    # Only the sources are needed: SOURCE_SUBDIR points at a folder without a CMakeLists.txt,
    # so the SDK's own (ESP-IDF) build is never configured here.
    include(FetchContent)
    FetchContent_Declare(prg32_sdk
        GIT_REPOSITORY https://github.com/riscv-prg32/PRG32.git
        GIT_TAG ${PRG32_SDK_REVISION}
        SOURCE_SUBDIR components/prg32_audio/include)
    FetchContent_MakeAvailable(prg32_sdk)
    set(PRG32_SDK_DIR "${prg32_sdk_SOURCE_DIR}")
  endif()
  set(PRG32_AUDIO_DIR "${PRG32_SDK_DIR}/components/prg32_audio")
  if(NOT EXISTS "${PRG32_AUDIO_DIR}/audio_synth.c")
    message(FATAL_ERROR "PRG32QT_FIRMWARE_AUDIO: no PRG32 audio engine in ${PRG32_SDK_DIR} (set PRG32_SDK_DIR, or OFF for the older mixer)")
  endif()
  enable_language(C)
  # The engine is compiled unchanged; firmware_audio/ gives it time in 20 ms steps and a PCM queue
  # in place of the console's audio task and I2S driver (the same port as PRG32-Construction-Kit's).
  add_library(prg32_firmware_audio STATIC
      ${PRG32_AUDIO_DIR}/audio.c ${PRG32_AUDIO_DIR}/audio_assets.c ${PRG32_AUDIO_DIR}/audio_mixer.c
      ${PRG32_AUDIO_DIR}/audio_synth.c ${PRG32_AUDIO_DIR}/audio_tracker.c src/core/firmware_audio/audio_port.c)
  target_include_directories(prg32_firmware_audio
      PRIVATE src/core/firmware_audio/shim ${PRG32_AUDIO_DIR}
      PUBLIC src/core/firmware_audio ${PRG32_AUDIO_DIR}/include)
  target_compile_options(prg32_firmware_audio PRIVATE -O2 -ffp-contract=off)
  target_sources(prg32qt_core PRIVATE src/core/FirmwareAudio.cpp)
  target_link_libraries(prg32qt_core PUBLIC prg32_firmware_audio)
  target_compile_definitions(prg32qt_core PUBLIC PRG32QT_FIRMWARE_AUDIO=1)
endif()

if(PRG32QT_BUILD_TESTS)
  enable_testing()
  add_executable(prg32qt-headless src/core/headless.cpp)
  target_link_libraries(prg32qt-headless PRIVATE prg32qt_core)
  add_executable(prg32qt_tests tests/core_tests.cpp)
  target_include_directories(prg32qt_tests PRIVATE src/qt)
  target_link_libraries(prg32qt_tests PRIVATE prg32qt_core)
  add_test(NAME prg32qt_core COMMAND prg32qt_tests)
  add_test(NAME prg32qt_ios_fixture_asteroids COMMAND prg32qt-headless ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/Asteroids.prg32 300)
  add_test(NAME prg32qt_ios_fixture_bach COMMAND prg32qt-headless ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/Bach.prg32 300)
  add_test(NAME prg32qt_performance_contract COMMAND prg32qt-headless ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/PerformanceTest.prg32 605 --require-performance)
endif()

if(PRG32QT_BUILD_APP)
  find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Network Multimedia WebSockets)
  qt_standard_project_setup(REQUIRES 6.5)
  if(ANDROID)
    include(FetchContent)
    FetchContent_Declare(android_openssl
        URL https://github.com/KDAB/android_openssl/archive/b71f1470962019bd89534a2919f5925f93bc5779.tar.gz
        URL_HASH SHA256=6c1dfff0af367bba2f2c2a27f1c2061019537524dc7892ee8efbdc8e1c6224fa
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(android_openssl)
    include("${android_openssl_SOURCE_DIR}/android_openssl.cmake")
  endif()
  set(PRG32QT_APP_SOURCES
      src/qt/main.cpp src/qt/AppController.cpp src/qt/StoreClient.cpp src/qt/FrameItem.cpp src/qt/WebApiServer.cpp
      src/qt/QtAudioEngine.cpp src/qt/QtMultiplayerService.cpp src/qt/QtMultiplayerService.h
      src/qt/RasterImageItem.cpp src/qt/RasterImageItem.h src/qt/GamepadBackend.h
      src/qt/MobileSafeArea.h src/qt/MdnsAdvertiser.h)
  if(IOS)
    enable_language(OBJCXX)
    list(APPEND PRG32QT_APP_SOURCES src/qt/GamepadBackend_mac.mm src/qt/MobileSafeArea_ios.mm)
  elseif(APPLE)
    enable_language(OBJCXX)
    list(APPEND PRG32QT_APP_SOURCES src/qt/GamepadBackend_mac.mm src/qt/MobileSafeArea_stub.cpp)
  elseif(WIN32)
    list(APPEND PRG32QT_APP_SOURCES src/qt/GamepadBackend_win.cpp src/qt/MobileSafeArea_stub.cpp)
  elseif(UNIX AND NOT ANDROID)
    list(APPEND PRG32QT_APP_SOURCES src/qt/GamepadBackend_linux.cpp src/qt/MobileSafeArea_stub.cpp)
  elseif(ANDROID)
    list(APPEND PRG32QT_APP_SOURCES src/qt/GamepadBackend_stub.cpp src/qt/MobileSafeArea_android.cpp)
  else()
    list(APPEND PRG32QT_APP_SOURCES src/qt/GamepadBackend_stub.cpp src/qt/MobileSafeArea_stub.cpp src/qt/MobileSafeArea.h)
  endif()
  if(APPLE)
    list(APPEND PRG32QT_APP_SOURCES src/qt/MdnsAdvertiser_apple.cpp)
  else()
    list(APPEND PRG32QT_APP_SOURCES src/qt/MdnsAdvertiser_udp.cpp)
  endif()

  if(PRG32QT_BUILD_TESTS AND PRG32QT_FIRMWARE_AUDIO AND NOT CMAKE_CROSSCOMPILING)
    # The firmware engine's sound through the real output device (skipped without one). Desktop
    # builds only: on Android and iOS a second Qt executable becomes a second app package, and
    # the test could not run on the build machine anyway.
    qt_add_executable(prg32qt_audio_device_check tests/audio_device_check.cpp src/qt/QtAudioEngine.cpp)
    target_include_directories(prg32qt_audio_device_check PRIVATE src/core src/qt)
    target_link_libraries(prg32qt_audio_device_check PRIVATE prg32qt_core Qt6::Core Qt6::Multimedia)
    add_test(NAME prg32qt_audio_device COMMAND prg32qt_audio_device_check
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/Bach.prg32 3)
    set_tests_properties(prg32qt_audio_device PROPERTIES SKIP_RETURN_CODE 77)
  endif()

  qt_add_executable(PRG32_QT ${PRG32QT_APP_SOURCES})
  qt_add_qml_module(PRG32_QT URI PRG32Qt VERSION 1.0 QML_FILES qml/Main.qml)
  qt_add_resources(PRG32_QT prg32qt_assets PREFIX "/prg32qt" FILES assets/prg32_logo.png assets/prg32_title.png assets/prg32_appicon_1024.png)
  target_include_directories(PRG32_QT PRIVATE src/core src/qt)
  target_link_libraries(PRG32_QT PRIVATE prg32qt_core Qt6::Core Qt6::Gui Qt6::Quick Qt6::QuickControls2 Qt6::Network Qt6::Multimedia Qt6::WebSockets)
  if(IOS OR CMAKE_SYSTEM_NAME STREQUAL "tvOS")
    qt_import_plugins(PRG32_QT EXCLUDE Qt6::QFFmpegMediaPlugin)
  endif()
  target_compile_definitions(PRG32_QT PRIVATE PRG32QT_DEFAULT_STORE="https://store.prg32.uniparthenope.it/")
  if(PRG32QT_TV_MODE)
    target_compile_definitions(PRG32_QT PRIVATE PRG32QT_TV_MODE=1)
  endif()
  set_target_properties(PRG32_QT PROPERTIES OUTPUT_NAME "PRG32" MACOSX_BUNDLE TRUE MACOSX_BUNDLE_BUNDLE_NAME "PRG32" MACOSX_BUNDLE_GUI_IDENTIFIER "org.riscv-prg32.prg32qt" MACOSX_BUNDLE_ICON_FILE "prg32.icns" WIN32_EXECUTABLE TRUE)
  install(TARGETS PRG32_QT
      BUNDLE DESTINATION .
      RUNTIME DESTINATION .
      LIBRARY DESTINATION .
      ARCHIVE DESTINATION lib)
  if(ANDROID)
    add_android_openssl_libraries(PRG32_QT)
    set_target_properties(PRG32_QT PROPERTIES
      QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/platform/android"
      QT_ANDROID_PACKAGE_NAME "org.riscvprg32.prg32qt"
      QT_ANDROID_VERSION_CODE ${PRG32QT_ANDROID_VERSION_CODE}
      QT_ANDROID_VERSION_NAME "${PRG32QT_VERSION}")
    if(PRG32QT_TV_MODE)
      set_target_properties(PRG32_QT PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/platform/android-tv"
        QT_ANDROID_PACKAGE_NAME "org.riscvprg32.prg32qt.tv")
    endif()
  endif()
  if(APPLE)
    target_sources(PRG32_QT PRIVATE assets/prg32.icns)
    set_source_files_properties(assets/prg32.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_link_libraries(PRG32_QT PRIVATE "-framework GameController")
    if(NOT IOS AND NOT CMAKE_SYSTEM_NAME STREQUAL "tvOS")
      target_link_libraries(PRG32_QT PRIVATE "-framework IOKit")
    endif()
  endif()
  if(WIN32)
    target_link_libraries(PRG32_QT PRIVATE winmm)
  endif()
  if(IOS)
    target_sources(PRG32_QT PRIVATE platform/ios/Assets.xcassets)
    set_source_files_properties(platform/ios/Assets.xcassets PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(PRG32_QT PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/platform/ios/Info.plist" XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.riscv-prg32.prg32qt" XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon" XCODE_ATTRIBUTE_MARKETING_VERSION "${PRG32QT_VERSION_CORE}" XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION "${PRG32QT_ANDROID_VERSION_CODE}")
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "tvOS")
    set_target_properties(PRG32_QT PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/platform/tvos/Info.plist" XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "org.riscv-prg32.prg32qt.tv" XCODE_ATTRIBUTE_MARKETING_VERSION "${PRG32QT_VERSION_CORE}" XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION "${PRG32QT_ANDROID_VERSION_CODE}" XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "3")
  endif()
endif()
