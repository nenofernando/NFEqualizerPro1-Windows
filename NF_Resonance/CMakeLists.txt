cmake_minimum_required(VERSION 3.22)
project(NFResonance VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point JUCE_DIR to your JUCE checkout or add JUCE as a subfolder next to this project.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
  add_subdirectory(JUCE)
elseif(DEFINED JUCE_DIR)
  add_subdirectory(${JUCE_DIR} JUCE)
else()
  find_package(JUCE CONFIG REQUIRED)
endif()

option(NF_ENABLE_AAX "Build AAX (requires Avid AAX SDK configured in JUCE)" OFF)
set(NF_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND NF_FORMATS AU)
endif()
if(NF_ENABLE_AAX)
  list(APPEND NF_FORMATS AAX)
endif()

juce_add_plugin(NFResonance
  COMPANY_NAME "NF Audio Tools"
  BUNDLE_ID "com.nfaudiotools.nfresonance"
  PLUGIN_MANUFACTURER_CODE Nfat
  PLUGIN_CODE Nfrs
  FORMATS ${NF_FORMATS}
  PRODUCT_NAME "NF Resonance"
  VST3_CATEGORIES Fx EQ
  AU_MAIN_TYPE kAudioUnitType_Effect
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(NFResonance)

target_sources(NFResonance PRIVATE
  Source/PluginProcessor.cpp Source/PluginProcessor.h
  Source/PluginEditor.cpp Source/PluginEditor.h
  Source/DSP/SpectralEngine.cpp Source/DSP/SpectralEngine.h
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h
  Source/DSP/TransientGuard.h
  Source/UI/NFLookAndFeel.cpp Source/UI/NFLookAndFeel.h
  Source/UI/SpectrumComponent.cpp Source/UI/SpectrumComponent.h
  Source/UI/ControlCurveComponent.cpp Source/UI/ControlCurveComponent.h)

target_compile_definitions(NFResonance PUBLIC
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(NFResonance PRIVATE
  juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# Offline smoke test: exercises the real AudioProcessor (Stereo,
# L/R, Mid/Side, Delta, Bypass) with a synthetic signal and checks
# for NaN/Inf, silent/muted output, and blown-up levels. Not part
# of the plugin build itself.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceSmokeTest PRODUCT_NAME "NF Resonance Smoke Test")

juce_generate_juce_header(NFResonanceSmokeTest)

target_sources(NFResonanceSmokeTest PRIVATE
  Tests/SmokeTest.cpp
  Source/PluginProcessor.cpp Source/PluginProcessor.h
  Source/PluginEditor.cpp Source/PluginEditor.h
  Source/DSP/SpectralEngine.cpp Source/DSP/SpectralEngine.h
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h
  Source/DSP/TransientGuard.h
  Source/UI/NFLookAndFeel.cpp Source/UI/NFLookAndFeel.h
  Source/UI/SpectrumComponent.cpp Source/UI/SpectrumComponent.h
  Source/UI/ControlCurveComponent.cpp Source/UI/ControlCurveComponent.h)

target_include_directories(NFResonanceSmokeTest PRIVATE Source)

target_compile_definitions(NFResonanceSmokeTest PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JucePlugin_Build_VST3=0 JucePlugin_Build_AU=0 JucePlugin_Build_AAX=0)

target_link_libraries(NFResonanceSmokeTest PRIVATE
  juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
