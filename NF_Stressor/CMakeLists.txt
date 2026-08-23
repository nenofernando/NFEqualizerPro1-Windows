cmake_minimum_required(VERSION 3.22)
project(NFStressor VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure with:
# cmake -B build -DJUCE_DIR=/path/to/JUCE
if(NOT DEFINED JUCE_DIR)
    message(FATAL_ERROR "JUCE_DIR is not set. Use -DJUCE_DIR=/path/to/JUCE")
endif()

add_subdirectory(${JUCE_DIR} JUCE)

# Configure with -DAAX_SDK_PATH=/path/to/aax-sdk to also build the AAX format.
if(DEFINED AAX_SDK_PATH)
    juce_set_aax_sdk_path(${AAX_SDK_PATH})
    set(NF_AAX_FORMAT AAX)
else()
    set(NF_AAX_FORMAT)
endif()

juce_add_plugin(NFStressor
    COMPANY_NAME "NF Audio Tools"
    BUNDLE_ID "com.nfaudiotools.stressor"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE NFAu
    PLUGIN_CODE NFSr
    FORMATS VST3 AU Standalone ${NF_AAX_FORMAT}
    PRODUCT_NAME "NF - Stressor"
    AAX_CATEGORY "Dynamics"
)

juce_generate_juce_header(NFStressor)

target_sources(NFStressor PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/StressorEngine.cpp
    Source/UI/NFStressorLookAndFeel.cpp
    Source/UI/GRLadderMeter.cpp
)

target_include_directories(NFStressor PRIVATE
    Source
    Source/DSP
    Source/UI
)

target_compile_definitions(NFStressor PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(NFStressor PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
)

if(APPLE)
    set_target_properties(NFStressor_VST3 PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    set_target_properties(NFStressor_AU PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")

    if(TARGET NFStressor_AAX)
        set_target_properties(NFStressor_AAX PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    endif()
endif()
