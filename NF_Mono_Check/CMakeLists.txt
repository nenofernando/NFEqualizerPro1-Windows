cmake_minimum_required(VERSION 3.22)
project(NFMonoCheck VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure with:
# cmake -B build -DJUCE_DIR=/path/to/JUCE
if(NOT DEFINED JUCE_DIR)
    message(FATAL_ERROR "JUCE_DIR is not set. Use -DJUCE_DIR=/path/to/JUCE")
endif()

add_subdirectory(${JUCE_DIR} JUCE)

juce_add_plugin(NFMonoCheck
    COMPANY_NAME "NF Audio Tools"
    BUNDLE_ID "com.nfaudiotools.monocheck"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Nfat
    PLUGIN_CODE Mnck
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "NF Mono Check"
)

juce_generate_juce_header(NFMonoCheck)

target_sources(NFMonoCheck PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(NFMonoCheck PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(NFMonoCheck PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
)

if(APPLE)
    set_target_properties(NFMonoCheck_VST3 PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    set_target_properties(NFMonoCheck_AU PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
endif()
