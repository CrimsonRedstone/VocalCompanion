cmake_minimum_required(VERSION 3.22)

# Must be set before project() so JUCE, AU, and VST3 inherit it.
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

project(VocalCompanion VERSION 1.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    add_compile_options(/W4 /permissive- /MP /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wshadow -Wno-unused-parameter)
endif()

if(APPLE)
    # Unsigned local builds so double-click install works without a Developer ID.
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "")
    set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_INJECT_BASE_ENTITLEMENTS "NO")
endif()

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# Git for Windows treats GIT_DIR as "the .git folder of a repo". A parent
# script that prepends Git\cmd to PATH must never leave that variable set,
# or FetchContent dies with: fatal: not a git repository: '...Git\cmd\'
unset(ENV{GIT_DIR})
unset(ENV{GIT_WORK_TREE})
unset(ENV{GIT_COMMON_DIR})
unset(ENV{GIT_OBJECT_DIRECTORY})

# ---------------------------------------------------------------------------
# JUCE 9.0.1
# ---------------------------------------------------------------------------
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# CLAP (official JUCE 9 CLAP is still rolling out; clap-juce-extensions is
# the production path used by Surge, ChowDSP, etc.)
# ---------------------------------------------------------------------------
option(VC_ENABLE_CLAP "Build a CLAP wrapper via clap-juce-extensions" ON)

if(VC_ENABLE_CLAP)
    FetchContent_Declare(clap_juce_extensions
        GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(clap_juce_extensions)
endif()

# ---------------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------------
if(APPLE)
    set(VC_FORMATS AU VST3 Standalone)
else()
    set(VC_FORMATS VST3 Standalone)
endif()

juce_add_plugin(VocalCompanion
    VERSION                       1.2.0
    COMPANY_NAME                  "Crimson Redstone"
    COMPANY_WEBSITE               "https://crimsonredstone.bandcamp.com"
    COMPANY_EMAIL                 "crimsonredstone@bandcamp.com"
    BUNDLE_ID                     com.crimsonredstone.vocalcompanion
    PLUGIN_MANUFACTURER_CODE      Crst
    PLUGIN_CODE                   Vcmp
    FORMATS                       ${VC_FORMATS}
    ICON_BIG                      Assets/Branding/VocalCompanion-256.png
    ICON_SMALL                    Assets/Branding/VocalCompanion-32.png
    PRODUCT_NAME                  "Vocal Companion"
    PLUGIN_NAME                   "Vocal Companion"
    DESCRIPTION                   "Modular vocal channel strip and dynamic host for acoustic DI and vocal-synth stems."
    IS_SYNTH                      FALSE
    NEEDS_MIDI_INPUT              TRUE
    NEEDS_MIDI_OUTPUT             FALSE
    IS_MIDI_EFFECT                FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS   TRUE
    COPY_PLUGIN_AFTER_BUILD       TRUE
    HARDENED_RUNTIME_ENABLED      FALSE
    APP_SANDBOX_ENABLED           FALSE
    VST3_CATEGORIES               Fx Dynamics EQ
    AU_MAIN_TYPE                  kAudioUnitType_Effect
    MICROPHONE_PERMISSION_ENABLED FALSE
    NEEDS_WEB_BROWSER             FALSE
    NEEDS_CURL                    FALSE)

juce_add_binary_data(AeternaAssets HEADER_NAME AeternaAssets.h NAMESPACE AeternaAssets SOURCES
    Assets/Aeterna/aeterna_singing_phase_0.png
    Assets/Aeterna/aeterna_singing_phase_1.png
    Assets/Aeterna/aeterna_singing_phase_2.png
    Assets/Aeterna/aeterna_singing_phase_3.png)
juce_add_binary_data(BrandingAssets HEADER_NAME BrandingAssets.h NAMESPACE BrandingAssets SOURCES
    Assets/Branding/VocalCompanion-256.png)
juce_generate_juce_header(VocalCompanion)

target_sources(VocalCompanion
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/VcLookAndFeel.cpp
        Source/Chain.cpp
        Source/DSP/Aeterna.cpp
        Source/DSP/SpectralPitch.cpp
        Source/UI/AeternaView.cpp
        Source/Presets.cpp
        Source/UI/TopBar.cpp
        Source/UI/RackComponent.cpp
        Source/UI/ModuleCard.cpp
        Source/UI/PaletteDock.cpp
        Source/UI/Controls.cpp
        Source/Host/ExternalHost.cpp)

target_include_directories(VocalCompanion PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")

target_compile_definitions(VocalCompanion
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_AU=1)

target_link_libraries(VocalCompanion
    PRIVATE
        AeternaAssets
        BrandingAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

option(VC_ENABLE_LTO "Enable link-time optimization for Release builds" ON)
if(VC_ENABLE_LTO)
    target_link_libraries(VocalCompanion PUBLIC juce::juce_recommended_lto_flags)
endif()

if(VC_ENABLE_CLAP)
    clap_juce_extensions_plugin(
        TARGET VocalCompanion
        CLAP_ID "com.crimsonredstone.vocalcompanion"
        CLAP_FEATURES audio-effect dynamics equalizer "vocal")
endif()

include(cmake/Branding.cmake)

# Opt-in native UI regression checks. No DAW or audio device is required.
option(VC_BUILD_UI_TESTS "Build native UI regression checks" OFF)
if(VC_BUILD_UI_TESTS)
    juce_add_console_app(VocalCompanionUiTests PRODUCT_NAME "Vocal Companion UI Tests")
    target_sources(VocalCompanionUiTests PRIVATE tests/UiRegression.cpp tests/AeternaRegression.cpp tests/AudioRepairRegression.cpp tests/Revision17Regression.cpp tests/Revision18Regression.cpp tests/Revision19Regression.cpp tests/Revision21Regression.cpp
        tests/Revision22Regression.cpp)
    target_include_directories(VocalCompanionUiTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
        "${CMAKE_CURRENT_BINARY_DIR}/VocalCompanion_artefacts/JuceLibraryCode")
    target_link_libraries(VocalCompanionUiTests PRIVATE VocalCompanion
        juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra)
endif()
