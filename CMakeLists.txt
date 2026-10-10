cmake_minimum_required(VERSION 3.22)

project(FlangConDulceDeLeche VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compilar arm64 nativo. Live 11.1+ (Universal) solo lista VST3 compilados
# nativamente para Apple Silicon; si sale x86_64 no aparece (sin error).
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "" FORCE)
endif()

# ----------------------------------------------------------------------------
# JUCE (FetchContent, pineado a una version estable)
# ----------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ----------------------------------------------------------------------------
# Plugin
# ----------------------------------------------------------------------------
juce_add_plugin(Flang
    COMPANY_NAME               "Mallin"
    PLUGIN_MANUFACTURER_CODE    Mall
    PLUGIN_CODE                 Flng
    PRODUCT_NAME               "Flang con Dulce de Leche"
    FORMATS                    VST3 Standalone
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    IS_MIDI_EFFECT             FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD    TRUE
    VST3_CATEGORIES            Fx
)

target_sources(Flang PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(Flang PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Flang
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# ----------------------------------------------------------------------------
# Tests offline del DSP (no dependen del host)
# ----------------------------------------------------------------------------
option(FLANG_BUILD_TESTS "Build offline DSP unit tests" ON)

if(FLANG_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
