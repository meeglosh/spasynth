cmake_minimum_required(VERSION 3.24)

project(Arsenal VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_XCODE_GENERATE_SCHEME OFF)

# Universal binary is required for release; keep it opt-in for day-to-day
# development because it doubles compile time.
option(ARSENAL_UNIVERSAL_BINARY "Build macOS universal binary (arm64 + x86_64)" OFF)

# Copy built plugins into the system plugin folders (handy locally, breaks on
# CI runners without admin rights).
option(ARSENAL_COPY_PLUGIN "Copy plugins to system folders after build" ON)
if(APPLE AND ARSENAL_UNIVERSAL_BINARY)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

add_subdirectory(libs/JUCE)

# AAX is deliberately absent for v1 (see brief). Adding it later is a matter of
# appending AAX to FORMATS and providing the AAX SDK — no rearchitecture needed.
juce_add_plugin(Arsenal
    COMPANY_NAME "Silverplatter Audio"
    COMPANY_WEBSITE "https://www.silverplatteraudio.com"
    BUNDLE_ID "com.silverplatteraudio.arsenal"
    PLUGIN_MANUFACTURER_CODE SpAu
    PLUGIN_CODE Arsn
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Arsenal"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD ${ARSENAL_COPY_PLUGIN}
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

juce_add_binary_data(ArsenalAssets
    NAMESPACE ArsenalAssets
    SOURCES
        assets/branding/SPAudio_logo_black.svg
        assets/branding/SPAudio_logo_white.svg
        assets/branding/SPAudio_logo_banner_black.svg
        assets/branding/SPAudio_logo_banner_white.svg
        assets/branding/SPAudio_logo_square_black.svg
        assets/branding/SPAudio_logo_sqaure_white.svg)

set(ARSENAL_SOURCES
    source/ArsenalProcessor.cpp
    source/params/ParameterRegistry.cpp
    source/params/Randomizer.cpp
    source/dsp/Wavetable.cpp
    source/dsp/WavetableLoader.cpp
    source/dsp/SampleLoader.cpp
    source/dsp/FXChain.cpp
    source/dsp/ArsenalVoice.cpp
    source/ui/ArsenalEditor.cpp)

target_sources(Arsenal PRIVATE ${ARSENAL_SOURCES})

target_include_directories(Arsenal PRIVATE source)

target_compile_definitions(Arsenal PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Arsenal
    PRIVATE
        ArsenalAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Headless test target: compiles the same sources as the plugin (JUCE modules
# are INTERFACE targets, so a console app is the supported way to reuse them).
option(ARSENAL_BUILD_TESTS "Build headless DSP tests" ON)
if(ARSENAL_BUILD_TESTS)
    enable_testing()

    juce_add_console_app(ArsenalTests PRODUCT_NAME "ArsenalTests")
    target_sources(ArsenalTests PRIVATE tests/ArsenalTests.cpp ${ARSENAL_SOURCES})
    target_include_directories(ArsenalTests PRIVATE source)
    # Modal loops stay on for tests: they pump the message loop to wait for
    # background sample/wavetable loads.
    target_compile_definitions(ArsenalTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(ArsenalTests
        PRIVATE
            ArsenalAssets
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    add_test(NAME RenderSmoke COMMAND ArsenalTests)
endif()
