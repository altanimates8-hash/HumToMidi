cmake_minimum_required(VERSION 3.22)
project(HumToMidi VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(HUM_BUILD_TESTS "Build the DSP unit tests" ON)

# ---- JUCE ---------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

# ---- Plugin -------------------------------------------------------------
juce_add_plugin(HumToMidi
    COMPANY_NAME                 "HumTools"
    PLUGIN_MANUFACTURER_CODE     Hmtl
    PLUGIN_CODE                  Hum1
    FORMATS                      VST3 Standalone
    PRODUCT_NAME                 "HumToMidi"
    IS_SYNTH                     FALSE
    NEEDS_MIDI_INPUT             FALSE
    NEEDS_MIDI_OUTPUT            TRUE
    IS_MIDI_EFFECT               FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS  FALSE
    COPY_PLUGIN_AFTER_BUILD      FALSE
    VST3_CATEGORIES              Fx Analyzer)

target_sources(HumToMidi PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp)

target_compile_definitions(HumToMidi PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(HumToMidi
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---- Tests --------------------------------------------------------------
if(HUM_BUILD_TESTS)
    enable_testing()
    add_executable(HumDspTests dsp_test.cpp)
    add_test(NAME dsp COMMAND HumDspTests)
endif()

# Optional: runs a synthetic hum through the real plugin processor (off by default, it compiles JUCE a second time)
option(HUM_BUILD_SMOKE_TEST "Build the processor smoke test" OFF)
if(HUM_BUILD_SMOKE_TEST)
    juce_add_console_app(HumProcessorSmoke PRODUCT_NAME "HumProcessorSmoke")
    target_sources(HumProcessorSmoke PRIVATE
        processor_smoke.cpp
        PluginProcessor.cpp
        PluginEditor.cpp)
    target_compile_definitions(HumProcessorSmoke PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(HumProcessorSmoke PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
    enable_testing()
    add_test(NAME processor COMMAND HumProcessorSmoke)
endif()
