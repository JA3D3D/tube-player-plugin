cmake_minimum_required(VERSION 3.22)

# Tube Player: YouTube tutorials in a resizable plugin window (VST3, AU, Standalone).
project(TUBE_PLAYER VERSION 1.0.0 LANGUAGES C CXX)
if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE lives next to this file (CI clones it; see .github/workflows/build.yml), or pass -DJUCE_DIR=...
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH "Path to the JUCE source tree")
add_subdirectory("${JUCE_DIR}" JUCE)

if(APPLE)
    set(TP_FORMATS VST3 AU Standalone)
else()
    set(TP_FORMATS VST3)   # Linux builds only exist to compile-check the shared code
endif()

juce_add_plugin(TubePlayer
    COMPANY_NAME "Bass Bible"
    COMPANY_WEBSITE ""
    BUNDLE_ID "com.bassbible.tubeplayer"
    PLUGIN_MANUFACTURER_CODE Bbtl
    PLUGIN_CODE Tube
    FORMATS ${TP_FORMATS}
    PRODUCT_NAME "Tube Player"
    DESCRIPTION "YouTube tutorials in a resizable plugin window"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Tools
    AU_MAIN_TYPE kAudioUnitType_Effect
    HARDENED_RUNTIME_ENABLED FALSE
    MICROPHONE_PERMISSION_ENABLED FALSE)

juce_add_binary_data(TubePlayerResources SOURCES Resources/ui.html)

target_sources(TubePlayer PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PageServer.cpp
    Source/RecentStore.cpp)

if(APPLE)
    target_sources(TubePlayer PRIVATE Source/TubeWebView_mac.mm)
    set_source_files_properties(Source/TubeWebView_mac.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_link_libraries(TubePlayer PRIVATE "-framework WebKit")
else()
    target_sources(TubePlayer PRIVATE Source/TubeWebView_stub.cpp)
endif()

target_compile_definitions(TubePlayer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

if(NOT APPLE)
    # keep the Linux compile check free of optional system libraries
    target_compile_definitions(TubePlayer PUBLIC
        JUCE_USE_XINPUT=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0
        JUCE_USE_XRENDER=0 JUCE_USE_XSHM=0 JUCE_ALSA=0 JUCE_JACK=0)
endif()

target_link_libraries(TubePlayer
    PRIVATE
        TubePlayerResources
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- tests for the plugin side (cmake -DTP_BUILD_TESTS=ON) ----
option(TP_BUILD_TESTS "Build the plugin-side tests" OFF)
if(TP_BUILD_TESTS)
    juce_add_console_app(TubePlayerTests PRODUCT_NAME "TubePlayerTests")
    target_sources(TubePlayerTests PRIVATE
        tests/PluginTests.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/PageServer.cpp
        Source/RecentStore.cpp)
    # the tests use the stand-in web view on every system; the real one is exercised by pluginval in CI
    target_sources(TubePlayerTests PRIVATE Source/TubeWebView_stub.cpp)
    if(NOT APPLE)
        target_compile_definitions(TubePlayerTests PRIVATE
            JUCE_USE_XINPUT=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0
            JUCE_USE_XRENDER=0 JUCE_USE_XSHM=0 JUCE_ALSA=0 JUCE_JACK=0)
    endif()
    target_compile_definitions(TubePlayerTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="Tube Player" JucePlugin_VersionString="1.0.0")
    target_link_libraries(TubePlayerTests PRIVATE
        TubePlayerResources juce::juce_audio_processors juce::juce_audio_utils juce::juce_gui_extra
        juce::juce_recommended_config_flags)
endif()
