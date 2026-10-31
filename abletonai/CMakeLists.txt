cmake_minimum_required(VERSION 3.22)

# AbletonAi: an AI producer in a resizable plugin window (VST3, AU, Standalone),
# working in your Live set through the AbletonAi Live helper (live-helper/).
project(ABLETON_AI VERSION 1.0.0 LANGUAGES C CXX)
if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE" CACHE PATH "Path to the JUCE source tree")
add_subdirectory("${JUCE_DIR}" JUCE)

# The page is one file: put agent.js inside ui.html
set(UI_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Resources/ui.html")
set(AGENT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/Resources/agent.js")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${UI_SRC}" "${AGENT_SRC}")
file(READ "${UI_SRC}" UI_TEXT)
file(READ "${AGENT_SRC}" AGENT_TEXT)
string(FIND "${UI_TEXT}" "<!--AGENT-->" AGENT_START)
string(FIND "${UI_TEXT}" "<!--/AGENT-->" AGENT_END)
if(AGENT_START LESS 0 OR AGENT_END LESS 0)
    message(FATAL_ERROR "ui.html is missing the <!--AGENT--> markers")
endif()
math(EXPR AGENT_END "${AGENT_END} + 13")
string(SUBSTRING "${UI_TEXT}" 0 ${AGENT_START} UI_HEAD)
string(SUBSTRING "${UI_TEXT}" ${AGENT_END} -1 UI_TAIL)
set(UI_BUILT "${CMAKE_CURRENT_BINARY_DIR}/ui-built.html")
file(WRITE "${UI_BUILT}" "${UI_HEAD}<script>\n${AGENT_TEXT}\n</script>${UI_TAIL}")

if(APPLE)
    set(AI_FORMATS VST3 AU Standalone)
else()
    set(AI_FORMATS VST3)
endif()

juce_add_plugin(AbletonAi
    COMPANY_NAME "Bass Bible"
    BUNDLE_ID "com.bassbible.abletonai"
    PLUGIN_MANUFACTURER_CODE Bbtl
    PLUGIN_CODE Abai
    FORMATS ${AI_FORMATS}
    PRODUCT_NAME "AbletonAi"
    DESCRIPTION "An AI producer that builds in your Live set"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/brand/icon-macos-1024.png"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Tools
    AU_MAIN_TYPE kAudioUnitType_Effect
    HARDENED_RUNTIME_ENABLED FALSE)

juce_add_binary_data(AbletonAiResources SOURCES "${UI_BUILT}")

set(AI_SOURCES Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/PageServer.cpp Source/HelperClient.cpp Source/Settings.cpp)
target_sources(AbletonAi PRIVATE ${AI_SOURCES})
if(APPLE)
    target_sources(AbletonAi PRIVATE Source/AiWebView_mac.mm)
    set_source_files_properties(Source/AiWebView_mac.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_link_libraries(AbletonAi PRIVATE "-framework WebKit")
else()
    target_sources(AbletonAi PRIVATE Source/AiWebView_stub.cpp)
endif()

set(AI_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
set(LINUX_DEFS JUCE_USE_XINPUT=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0
               JUCE_USE_XRENDER=0 JUCE_USE_XSHM=0 JUCE_ALSA=0 JUCE_JACK=0)
target_compile_definitions(AbletonAi PUBLIC ${AI_DEFS})
if(NOT APPLE)
    target_compile_definitions(AbletonAi PUBLIC ${LINUX_DEFS})
endif()
target_link_libraries(AbletonAi
    PRIVATE AbletonAiResources juce::juce_audio_utils juce::juce_gui_extra
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---- tests for the plugin side (cmake -DAI_BUILD_TESTS=ON) ----
option(AI_BUILD_TESTS "Build the plugin-side tests" OFF)
if(AI_BUILD_TESTS)
    juce_add_console_app(AbletonAiTests PRODUCT_NAME "AbletonAiTests")
    target_sources(AbletonAiTests PRIVATE tests/PluginTests.cpp ${AI_SOURCES} Source/AiWebView_stub.cpp)
    target_compile_definitions(AbletonAiTests PRIVATE ${AI_DEFS} JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="AbletonAi" JucePlugin_VersionString="1.0.0"
        AI_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT APPLE)
        target_compile_definitions(AbletonAiTests PRIVATE ${LINUX_DEFS})
    endif()
    target_link_libraries(AbletonAiTests PRIVATE AbletonAiResources juce::juce_audio_processors juce::juce_audio_utils
        juce::juce_gui_extra juce::juce_recommended_config_flags)
endif()
