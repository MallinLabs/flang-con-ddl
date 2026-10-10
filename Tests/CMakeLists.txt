include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.9.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

juce_add_console_app(FlangTests PRODUCT_NAME "FlangTests")

target_sources(FlangTests PRIVATE
    test_tone_stack.cpp
    test_preamp.cpp
    test_drive.cpp
    test_mod.cpp
)

target_include_directories(FlangTests PRIVATE ${CMAKE_SOURCE_DIR}/Source)

target_link_libraries(FlangTests
    PRIVATE
        Catch2::Catch2WithMain
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

add_test(NAME dsp COMMAND FlangTests)
