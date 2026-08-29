cmake_minimum_required(VERSION 3.22)

if (APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
        "Build architectures for macOS")
endif()

project(NFWhiteDelay VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure with:
# cmake -B build -DJUCE_DIR=/path/to/JUCE
if(NOT DEFINED JUCE_DIR)
    message(FATAL_ERROR "JUCE_DIR is not set. Use -DJUCE_DIR=/path/to/JUCE")
endif()

add_subdirectory(${JUCE_DIR} JUCE)

# Configure with -DAAX_SDK_PATH=/path/to/aax-sdk to also build the AAX format.
# Without it, AAX is simply left out of FORMATS -- no fake headers/libs.
if(DEFINED AAX_SDK_PATH)
    juce_set_aax_sdk_path(${AAX_SDK_PATH})
    set(NF_AAX_FORMAT AAX)
else()
    set(NF_AAX_FORMAT)
endif()

# AU is macOS-only; JUCE already skips it automatically on Windows, but we
# keep the FORMATS list itself explicit about the split for clarity.
if(APPLE)
    set(NF_AU_FORMAT AU)
else()
    set(NF_AU_FORMAT)
endif()

juce_add_plugin(NFWhiteDelay
    COMPANY_NAME "NF Audio Tools"
    BUNDLE_ID "com.nfaudiotools.nfwhitedelay"
    PLUGIN_MANUFACTURER_CODE NFAT
    PLUGIN_CODE NfWd
    FORMATS VST3 ${NF_AU_FORMAT} Standalone ${NF_AAX_FORMAT}
    PRODUCT_NAME "NF White Delay"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    AAX_CATEGORY "Delay"
)

juce_generate_juce_header(NFWhiteDelay)

# Official premium UI bitmaps (FASE 2). Chassis.png is available for
# reference crops only — do not stretch it as a full-window background.
juce_add_binary_data(NFWhiteDelayAssets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Assets/ui/knob_large.png
        Assets/ui/knob_small.png
        Assets/ui/button_off.png
        Assets/ui/button_on.png
        Assets/ui/display_frame.png
        Assets/ui/chassis.png
        Assets/ui/bypass_off.png
        Assets/ui/bypass_on.png
        Assets/ui/icon_save.png
        Assets/ui/icon_menu.png
)

target_sources(NFWhiteDelay PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/NFWhiteLookAndFeel.cpp
    Source/UI/NFPremiumLookAndFeel.cpp
    Source/DSP/DelayEngine.cpp
    Source/DSP/HostSync.cpp
    Source/DSP/ModulationEngine.cpp
)

target_compile_definitions(NFWhiteDelay PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(NFWhiteDelay
    PRIVATE
        NFWhiteDelayAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_basics
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ----------------------------------------------------------------
# TESTES -- executável de console independente (sem host/GUI), mesmo
# padrão do NF Vocal Tune (Tests/PitchTrackerTests.cpp). Não faz parte
# do build do plugin em si.
#
# -DNF_ENABLE_ASAN=ON compila este alvo com AddressSanitizer +
# UndefinedBehaviorSanitizer -- é o jeito real de checar "sem leitura
# fora do buffer" (um teste de valores não prova isso sozinho).
# ----------------------------------------------------------------
option(NF_ENABLE_ASAN "Build NFWhiteDelayTests with AddressSanitizer + UBSan" OFF)

juce_add_console_app(NFWhiteDelayTests PRODUCT_NAME "NF White Delay Tests")

target_sources(NFWhiteDelayTests PRIVATE
    Tests/main.cpp
    Tests/AllocationGuard.cpp
    Tests/DelayEngineTests.cpp
    Tests/FeedbackTests.cpp
    Tests/PingPongTests.cpp
    Tests/LargeJumpTests.cpp
    Tests/HostSyncTests.cpp
    Tests/SyncIntegrationTests.cpp
    Tests/FilterTests.cpp
    Tests/ModulationTests.cpp
    Tests/ModulationCrossfadeTests.cpp
    Tests/StressTests.cpp
    Tests/DuckingTests.cpp
    Tests/LoFiTests.cpp
    Tests/CharacterTests.cpp
    Tests/Phase5StressTests.cpp
    Tests/OutputStageTests.cpp
    Tests/Phase6IntegrationTests.cpp
    Tests/MixLawAndResponseTests.cpp
    Source/DSP/DelayEngine.cpp
    Source/DSP/HostSync.cpp
    Source/DSP/ModulationEngine.cpp
)

target_include_directories(NFWhiteDelayTests PRIVATE
    Source
)

target_link_libraries(NFWhiteDelayTests PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

if(NF_ENABLE_ASAN)
    target_compile_options(NFWhiteDelayTests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(NFWhiteDelayTests PRIVATE -fsanitize=address,undefined)
endif()

if(APPLE)
    if(TARGET NFWhiteDelay_VST3)
        set_target_properties(NFWhiteDelay_VST3 PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    endif()
    if(TARGET NFWhiteDelay_AU)
        set_target_properties(NFWhiteDelay_AU PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    endif()
    if(TARGET NFWhiteDelay_AAX)
        set_target_properties(NFWhiteDelay_AAX PROPERTIES XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO")
    endif()
endif()

# ----------------------------------------------------------------
# FASE 2 UI snapshot harness — paints the editor once and writes
# prototype_chassis_time_sync_display.png for visual approval.
# ----------------------------------------------------------------
add_executable(NFWhiteDelayUiSnapshot
    Tests/UiSnapshot.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/NFWhiteLookAndFeel.cpp
    Source/UI/NFPremiumLookAndFeel.cpp
    Source/DSP/DelayEngine.cpp
    Source/DSP/HostSync.cpp
    Source/DSP/ModulationEngine.cpp
)

target_include_directories(NFWhiteDelayUiSnapshot PRIVATE
    Source
    ${CMAKE_CURRENT_BINARY_DIR}/NFWhiteDelay_artefacts/JuceLibraryCode
    ${CMAKE_CURRENT_BINARY_DIR}/juce_binarydata_NFWhiteDelayAssets/JuceLibraryCode
)
add_dependencies(NFWhiteDelayUiSnapshot NFWhiteDelay)

target_compile_definitions(NFWhiteDelayUiSnapshot PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STANDALONE_APPLICATION=1
)

target_link_libraries(NFWhiteDelayUiSnapshot PRIVATE
    NFWhiteDelayAssets
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
