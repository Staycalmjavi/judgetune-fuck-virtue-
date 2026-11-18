cmake_minimum_required(VERSION 3.22)
project(JUDGETUNE VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
option(JT_COPY_AFTER_BUILD "Copy the plugin into the system plugin folder after building" ON)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(JUDGETUNE
    COMPANY_NAME                "Judge Audio"
    PLUGIN_MANUFACTURER_CODE    Jdga
    PLUGIN_CODE                 Jtun
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "JUDGETUNE"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    VST3_CATEGORIES             Fx "Pitch Shift"
    COPY_PLUGIN_AFTER_BUILD     ${JT_COPY_AFTER_BUILD})

juce_generate_juce_header(JUDGETUNE)

target_sources(JUDGETUNE PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(JUDGETUNE PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0

target_link_libraries(JUDGETUNE
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
