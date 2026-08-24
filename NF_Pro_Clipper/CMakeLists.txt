cmake_minimum_required(VERSION 3.22)

if (APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
        "Build architectures for macOS")
endif()

project(NFProClipper VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure with:
# cmake -B build -DJUCE_DIR=/path/to/JUCE
if(NOT DEFINED JUCE_DIR)
    message(FATAL_ERROR "JUCE_DIR is not set. Use -DJUCE_DIR=/path/to/JUCE")
endif()

add_subdirectory(${JUCE_DIR} JUCE)

juce_add_plugin(NFProClipper
    COMPANY_NAME "Neno Fernando Audio Tools"
    BUNDLE_ID "com.nenofernando.nfproclipper"
    PLUGIN_MANUFACTURER_CODE NFAT
    PLUGIN_CODE NFPC
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "NF Pro Clipper"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

juce_generate_juce_header(NFProClipper)

target_sources(NFProClipper PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(NFProClipper PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(NFProClipper
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_basics
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(APPLE)
    set_target_properties(NFProClipper_VST3 PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    set_target_properties(NFProClipper_AU PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
endif()
