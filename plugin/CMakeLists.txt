# G-Major Reborn — JUCE plugin (VST3 + Standalone)
#
# JUCE: pass -DGMR_JUCE_DIR=/path/to/JUCE (7.0.12 recommended), otherwise it is fetched from GitHub.
# ASIO: pass -DGMR_ASIO_SDK_DIR=/path/to/asiosdk (Steinberg ASIO SDK, downloaded separately).

set(GMR_JUCE_DIR "" CACHE PATH "Path to a JUCE checkout (7.0.12 tested)")
set(GMR_ASIO_SDK_DIR "" CACHE PATH "Path to the Steinberg ASIO SDK (enables ASIO in the Standalone)")
option(GMR_BUILD_HARNESS "Build the processor/editor integration harness" OFF)

if (GMR_JUCE_DIR)
    add_subdirectory(${GMR_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 7.0.12 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

juce_add_plugin(GMajorReborn
    PRODUCT_NAME "G-Major Reborn"
    COMPANY_NAME "GMR Project"
    BUNDLE_ID "org.gmrproject.gmajorreborn"
    PLUGIN_MANUFACTURER_CODE Gmrp
    PLUGIN_CODE Gmr1
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay Reverb Modulation
    VST3_AUTO_MANIFEST FALSE
    ICON_BIG "${CMAKE_SOURCE_DIR}/resources/icon.png")

juce_generate_juce_header(GMajorReborn)

target_sources(GMajorReborn PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Controls.cpp
    Source/Sections.cpp
    Source/RackEditor.cpp
    Source/PresetLibrary.cpp)

set(GMR_JUCE_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=1
    JUCE_USE_WINDOWS_MEDIA_FORMAT=0
    JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE=0)
if (GMR_ASIO_SDK_DIR)
    list(APPEND GMR_JUCE_DEFS JUCE_ASIO=1)
endif()
if (GMR_BUILD_HARNESS)
    list(APPEND GMR_JUCE_DEFS JUCE_MODAL_LOOPS_PERMITTED=1)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        list(APPEND GMR_JUCE_DEFS JUCE_ALSA=0 JUCE_JACK=0)
    endif()
endif()
target_compile_definitions(GMajorReborn PUBLIC ${GMR_JUCE_DEFS})

if (GMR_ASIO_SDK_DIR)
    target_include_directories(GMajorReborn PUBLIC "${GMR_ASIO_SDK_DIR}/common")
    message(STATUS "G-Major Reborn: ASIO enabled (${GMR_ASIO_SDK_DIR})")
else()
    message(STATUS "G-Major Reborn: ASIO disabled (set GMR_ASIO_SDK_DIR to enable)")
endif()

target_link_libraries(GMajorReborn
    PRIVATE gmr_core juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- single-block "Rack" VST3s (v0.9.1)
# Same sources, GMR_RACK_BLOCK selects the block (values of gmr::Block).
option(GMR_BUILD_RACKS "Build the individual G-Major <Block> Rack VST3 plugins" ON)
function(gmr_add_rack target label code blockIndex)
    juce_add_plugin(${target}
        PRODUCT_NAME "G-Major ${label} Rack"
        COMPANY_NAME "GMR Project"
        BUNDLE_ID "org.gmrproject.gmajorreborn.${target}"
        PLUGIN_MANUFACTURER_CODE Gmrp
        PLUGIN_CODE ${code}
        FORMATS VST3
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx
        VST3_AUTO_MANIFEST FALSE
        ICON_BIG "${CMAKE_SOURCE_DIR}/resources/icon.png")
    juce_generate_juce_header(${target})
    target_sources(${target} PRIVATE
        Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Controls.cpp
        Source/Sections.cpp Source/RackEditor.cpp Source/PresetLibrary.cpp)
    target_compile_definitions(${target} PUBLIC ${GMR_JUCE_DEFS} GMR_RACK_BLOCK=${blockIndex})
    if (GMR_ASIO_SDK_DIR)
        target_include_directories(${target} PUBLIC "${GMR_ASIO_SDK_DIR}/common")
    endif()
    target_link_libraries(${target}
        PRIVATE gmr_core juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
endfunction()

if (GMR_BUILD_RACKS)
    # gmr::Block: Gate=1 Eq=2 Comp=3 Filter=4 Pitch=5 Mod=6 Delay=7 Reverb=8
    gmr_add_rack(GMajorGateRack       "Noise Gate"    GmrG 1)
    gmr_add_rack(GMajorEqRack         "EQ"            GmrE 2)
    gmr_add_rack(GMajorCompressorRack "Compressor"    GmrC 3)
    gmr_add_rack(GMajorFilterRack     "Filter-Mod"    GmrF 4)
    gmr_add_rack(GMajorPitchRack      "Pitch"         GmrP 5)
    gmr_add_rack(GMajorChorusRack     "Chorus-Flange" GmrM 6)
    gmr_add_rack(GMajorDelayRack      "Delay"         GmrD 7)
    gmr_add_rack(GMajorReverbRack     "Reverb"        GmrR 8)
endif()

# ---------------------------------------------------------------- headless integration harness
if (GMR_BUILD_HARNESS)
    juce_add_console_app(gmr_plugin_harness PRODUCT_NAME "gmr_plugin_harness")
    target_sources(gmr_plugin_harness PRIVATE ${CMAKE_SOURCE_DIR}/tests/PluginHarness.cpp)
    target_compile_definitions(gmr_plugin_harness PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    juce_generate_juce_header(gmr_plugin_harness)
    target_link_libraries(gmr_plugin_harness PRIVATE GMajorReborn gmr_core juce::juce_audio_utils)
endif()
