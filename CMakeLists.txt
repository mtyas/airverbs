cmake_minimum_required (VERSION 3.22)

set (CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
project (Airverbs VERSION 0.77.0 LANGUAGES C CXX)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set_property (GLOBAL PROPERTY USE_FOLDERS ON)

option (AIRVERBS_BUILD_TESTS "Build the offline test harness" ON)

add_subdirectory (third_party/JUCE)
add_subdirectory (third_party/clap-juce-extensions EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------------------------
# The Airwindows reverbs, compiled unmodified from their LinuxVST sources against a tiny shim.
# ---------------------------------------------------------------------------------------------
file (GLOB AIRVERBS_ENGINE_SOURCES CONFIGURE_DEPENDS source/airwindows/engines/aw_*.cpp)
add_library (AirverbsEngines STATIC ${AIRVERBS_ENGINE_SOURCES} source/airwindows/EngineRegistry.cpp)
target_include_directories (AirverbsEngines
    PUBLIC  source/airwindows/shim source/airwindows
    PRIVATE third_party/airwindows/plugins/LinuxVST/src)
set_target_properties (AirverbsEngines PROPERTIES POSITION_INDEPENDENT_CODE ON)
if (MSVC)
    target_compile_options (AirverbsEngines PRIVATE /w /O2 /bigobj /Zc:preprocessor)
else()
    target_compile_options (AirverbsEngines PRIVATE -w -O2)
endif()

# ---------------------------------------------------------------------------------------------
# Assets rendered in Blender (see assets/blender) + fonts
# ---------------------------------------------------------------------------------------------
file (GLOB AIRVERBS_ASSETS CONFIGURE_DEPENDS assets/rendered/*.png)
juce_add_binary_data (AirverbsAssets SOURCES ${AIRVERBS_ASSETS} assets/layout.json)

# ---------------------------------------------------------------------------------------------
# The plugin
# ---------------------------------------------------------------------------------------------
set (AIRVERBS_FORMATS VST3 Standalone)
if (APPLE)
    list (APPEND AIRVERBS_FORMATS AU)
elseif (UNIX)
    list (APPEND AIRVERBS_FORMATS LV2)
endif()

juce_add_plugin (Airverbs
    COMPANY_NAME "mtyas"
    COMPANY_COPYRIGHT "Released into the public domain (The Unlicense) by mtyas"
    BUNDLE_ID "com.mtyas.airverbs"
    PLUGIN_MANUFACTURER_CODE Mtys
    PLUGIN_CODE Avb1
    FORMATS ${AIRVERBS_FORMATS}
    PRODUCT_NAME "Airverbs"
    DESCRIPTION "A constellation of Airwindows reverbs"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Fx Reverb
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    LV2URI "urn:mtyas:airverbs"
    COPY_PLUGIN_AFTER_BUILD FALSE)

clap_juce_extensions_plugin (TARGET Airverbs
    CLAP_ID "com.mtyas.airverbs"
    CLAP_FEATURES audio-effect reverb stereo)

target_sources (Airverbs PRIVATE
    source/PluginProcessor.cpp
    source/dsp/EngineSlot.cpp
    source/dsp/Constellation.cpp
    source/state/Parameters.cpp
    source/state/MidiLearn.cpp
    source/state/History.cpp
    source/presets/PresetManager.cpp
    source/presets/FactoryPresets.cpp
    source/gui/PluginEditor.cpp
    source/gui/AirverbsLookAndFeel.cpp
    source/gui/ConstellationView.cpp
    source/gui/StarPanel.cpp
    source/gui/PresetBrowser.cpp
    source/gui/Widgets.cpp)

target_compile_definitions (Airverbs PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries (Airverbs
    PRIVATE
        AirverbsEngines
        AirverbsAssets
        juce::juce_audio_utils
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if (AIRVERBS_BUILD_TESTS)
    add_subdirectory (tests)
endif()
