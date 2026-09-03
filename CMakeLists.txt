cmake_minimum_required(VERSION 3.22)
project(NFEqualizer VERSION 2.0.0)

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

juce_add_plugin(NFEqualizer
    COMPANY_NAME "NF Audio"
    BUNDLE_ID "com.nfaudio.nfequalizer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE NFAu
    PLUGIN_CODE NFE2
    FORMATS VST3 AU Standalone ${NF_AAX_FORMAT}
    PRODUCT_NAME "NF Pro Eq"
    AAX_CATEGORY "EQ"
)

juce_generate_juce_header(NFEqualizer)

target_sources(NFEqualizer PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetManager.cpp
    Source/DSP/NFEqualizerDSP.cpp
    Source/DSP/NFSaturation.cpp
    Source/DSP/NFFilters.cpp
    Source/UI/NFLookAndFeel.cpp
    Source/UI/NFKnob.cpp
    Source/UI/NFEqualizerPanel.cpp
)

target_include_directories(NFEqualizer PRIVATE
    Source
    Source/DSP
    Source/UI
)

target_compile_definitions(NFEqualizer PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(NFEqualizer PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_cryptography
)

if(APPLE)
    set_target_properties(NFEqualizer_VST3 PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    set_target_properties(NFEqualizer_AU PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")

    if(TARGET NFEqualizer_AAX)
        set_target_properties(NFEqualizer_AAX PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    endif()
endif()
