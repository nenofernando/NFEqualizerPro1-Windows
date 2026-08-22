cmake_minimum_required(VERSION 3.22)
project(NFTapeMachine VERSION 1.0.0)

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

juce_add_plugin(NFTapeMachine
    COMPANY_NAME "NF Audio"
    BUNDLE_ID "com.nfaudio.nftapemachine"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE NFAu
    PLUGIN_CODE NFTm
    FORMATS VST3 AU Standalone ${NF_AAX_FORMAT}
    PRODUCT_NAME "NF Tape Machine"
    AAX_CATEGORY "Harmonic"
)

juce_generate_juce_header(NFTapeMachine)

target_sources(NFTapeMachine PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DSP/NFTapeEngine.cpp
    Source/UI/NFTapeLookAndFeel.cpp
    Source/UI/NFVUComponent.cpp
    Source/UI/NFTapeReels.cpp
)

target_include_directories(NFTapeMachine PRIVATE
    Source
    Source/DSP
    Source/UI
)

target_compile_definitions(NFTapeMachine PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(NFTapeMachine PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
)

if(APPLE)
    set_target_properties(NFTapeMachine_VST3 PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    set_target_properties(NFTapeMachine_AU PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")

    if(TARGET NFTapeMachine_AAX)
        set_target_properties(NFTapeMachine_AAX PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    endif()
endif()
