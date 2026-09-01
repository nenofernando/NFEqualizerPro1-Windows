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
  Source/DSP/GainMaskEngine.cpp Source/DSP/GainMaskEngine.h
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h
  Source/DSP/TransientProtectionEngine.cpp Source/DSP/TransientProtectionEngine.h
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
juce_add_console_app(NFResonanceCalibrationAudit PRODUCT_NAME "NF Resonance Calibration Audit")
juce_generate_juce_header(NFResonanceCalibrationAudit)
target_sources(NFResonanceCalibrationAudit PRIVATE
  Tests/CalibrationAudit.cpp
  Source/DSP/GainMaskEngine.cpp Source/DSP/GainMaskEngine.h
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h
  Source/DSP/TransientProtectionEngine.cpp Source/DSP/TransientProtectionEngine.h)
target_include_directories(NFResonanceCalibrationAudit PRIVATE Source)
target_link_libraries(NFResonanceCalibrationAudit PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceSensitivityCurveCheck PRODUCT_NAME "NF Resonance Sensitivity Curve Check")
juce_generate_juce_header(NFResonanceSensitivityCurveCheck)
target_sources(NFResonanceSensitivityCurveCheck PRIVATE
  Tests/SensitivityCurveCheck.cpp
  Source/DSP/GainMaskEngine.cpp Source/DSP/GainMaskEngine.h
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h
  Source/DSP/TransientProtectionEngine.cpp Source/DSP/TransientProtectionEngine.h)
target_include_directories(NFResonanceSensitivityCurveCheck PRIVATE Source)
target_link_libraries(NFResonanceSensitivityCurveCheck PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceGainMaskV2Check PRODUCT_NAME "NF Resonance Gain Mask V2 Check")
juce_generate_juce_header(NFResonanceGainMaskV2Check)
target_sources(NFResonanceGainMaskV2Check PRIVATE
  Tests/GainMaskV2Check.cpp
  Source/PluginProcessor.cpp Source/PluginProcessor.h
  Source/PluginEditor.cpp Source/PluginEditor.h
  Source/DSP/SpectralEngine.cpp Source/DSP/SpectralEngine.h
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h
  Source/DSP/TransientGuard.h
  Source/DSP/GainMaskEngine.cpp Source/DSP/GainMaskEngine.h
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h
  Source/DSP/TransientProtectionEngine.cpp Source/DSP/TransientProtectionEngine.h
  Source/UI/NFLookAndFeel.cpp Source/UI/NFLookAndFeel.h
  Source/UI/SpectrumComponent.cpp Source/UI/SpectrumComponent.h
  Source/UI/ControlCurveComponent.cpp Source/UI/ControlCurveComponent.h)
target_include_directories(NFResonanceGainMaskV2Check PRIVATE Source)
target_compile_definitions(NFResonanceGainMaskV2Check PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JucePlugin_Build_VST3=0 JucePlugin_Build_AU=0 JucePlugin_Build_AAX=0)
target_link_libraries(NFResonanceGainMaskV2Check PRIVATE
  juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceSmokeTest PRODUCT_NAME "NF Resonance Smoke Test")

juce_generate_juce_header(NFResonanceSmokeTest)

target_sources(NFResonanceSmokeTest PRIVATE
  Tests/SmokeTest.cpp
  Source/PluginProcessor.cpp Source/PluginProcessor.h
  Source/PluginEditor.cpp Source/PluginEditor.h
  Source/DSP/SpectralEngine.cpp Source/DSP/SpectralEngine.h
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h
  Source/DSP/TransientGuard.h
  Source/DSP/GainMaskEngine.cpp Source/DSP/GainMaskEngine.h
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h
  Source/DSP/TransientProtectionEngine.cpp Source/DSP/TransientProtectionEngine.h
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

# ----------------------------------------------------------------
# Investigation-only: A/B/C comparison of the three startup/warm-up
# strategies (time gate, norm-relative gate, virtual pre-roll).
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceWarmupCompare PRODUCT_NAME "NF Resonance Warmup Compare")

juce_generate_juce_header(NFResonanceWarmupCompare)

target_sources(NFResonanceWarmupCompare PRIVATE
  Tests/WarmupCompare.cpp
  Source/PluginProcessor.cpp Source/PluginProcessor.h
  Source/PluginEditor.cpp Source/PluginEditor.h
  Source/DSP/SpectralEngine.cpp Source/DSP/SpectralEngine.h
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h
  Source/DSP/TransientGuard.h
  Source/DSP/GainMaskEngine.cpp Source/DSP/GainMaskEngine.h
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h
  Source/DSP/TransientProtectionEngine.cpp Source/DSP/TransientProtectionEngine.h
  Source/UI/NFLookAndFeel.cpp Source/UI/NFLookAndFeel.h
  Source/UI/SpectrumComponent.cpp Source/UI/SpectrumComponent.h
  Source/UI/ControlCurveComponent.cpp Source/UI/ControlCurveComponent.h)

target_include_directories(NFResonanceWarmupCompare PRIVATE Source)

target_compile_definitions(NFResonanceWarmupCompare PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JucePlugin_Build_VST3=0 JucePlugin_Build_AU=0 JucePlugin_Build_AAX=0)

target_link_libraries(NFResonanceWarmupCompare PRIVATE
  juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# Investigation-only: deep diagnosis of the C2 (true left-pad STFT)
# startup overshoot at Depth != 0 -- per-frame mask capture, OLA
# decomposition, controlled mask-override experiments.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceOvershootDiagnosis PRODUCT_NAME "NF Resonance Overshoot Diagnosis")

juce_generate_juce_header(NFResonanceOvershootDiagnosis)

target_sources(NFResonanceOvershootDiagnosis PRIVATE
  Tests/OvershootDiagnosis.cpp
  Source/PluginProcessor.cpp Source/PluginProcessor.h
  Source/PluginEditor.cpp Source/PluginEditor.h
  Source/DSP/SpectralEngine.cpp Source/DSP/SpectralEngine.h
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h
  Source/DSP/TransientGuard.h
  Source/DSP/GainMaskEngine.cpp Source/DSP/GainMaskEngine.h
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h
  Source/DSP/TransientProtectionEngine.cpp Source/DSP/TransientProtectionEngine.h
  Source/UI/NFLookAndFeel.cpp Source/UI/NFLookAndFeel.h
  Source/UI/SpectrumComponent.cpp Source/UI/SpectrumComponent.h
  Source/UI/ControlCurveComponent.cpp Source/UI/ControlCurveComponent.h)

target_include_directories(NFResonanceOvershootDiagnosis PRIVATE Source)

target_compile_definitions(NFResonanceOvershootDiagnosis PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JucePlugin_Build_VST3=0 JucePlugin_Build_AU=0 JucePlugin_Build_AAX=0)

target_link_libraries(NFResonanceOvershootDiagnosis PRIVATE
  juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# Final validation of C2 (true left-pad STFT) + algorithmic latency
# validity boundary, ahead of the production checkpoint.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceC2FinalValidation PRODUCT_NAME "NF Resonance C2 Final Validation")

juce_generate_juce_header(NFResonanceC2FinalValidation)

target_sources(NFResonanceC2FinalValidation PRIVATE
  Tests/C2FinalValidation.cpp
  Source/PluginProcessor.cpp Source/PluginProcessor.h
  Source/PluginEditor.cpp Source/PluginEditor.h
  Source/DSP/SpectralEngine.cpp Source/DSP/SpectralEngine.h
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h
  Source/DSP/TransientGuard.h
  Source/DSP/GainMaskEngine.cpp Source/DSP/GainMaskEngine.h
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h
  Source/DSP/TransientProtectionEngine.cpp Source/DSP/TransientProtectionEngine.h
  Source/UI/NFLookAndFeel.cpp Source/UI/NFLookAndFeel.h
  Source/UI/SpectrumComponent.cpp Source/UI/SpectrumComponent.h
  Source/UI/ControlCurveComponent.cpp Source/UI/ControlCurveComponent.h)

target_include_directories(NFResonanceC2FinalValidation PRIVATE Source)

target_compile_definitions(NFResonanceC2FinalValidation PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JucePlugin_Build_VST3=0 JucePlugin_Build_AU=0 JucePlugin_Build_AAX=0)

target_link_libraries(NFResonanceC2FinalValidation PRIVATE
  juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# DAW Preview 0.1b isolated diagnostic: unity-gain/depth-gain-loss/
# Delta-semantics/Delta-content/M-S-matrix battery, read-only.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceDAWPreviewDiagnostics PRODUCT_NAME "NF Resonance DAW Preview Diagnostics")

juce_generate_juce_header(NFResonanceDAWPreviewDiagnostics)

target_sources(NFResonanceDAWPreviewDiagnostics PRIVATE
  Tests/DAWPreviewDiagnostics.cpp
  Source/PluginProcessor.cpp Source/PluginProcessor.h
  Source/PluginEditor.cpp Source/PluginEditor.h
  Source/DSP/SpectralEngine.cpp Source/DSP/SpectralEngine.h
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h
  Source/DSP/TransientGuard.h
  Source/DSP/GainMaskEngine.cpp Source/DSP/GainMaskEngine.h
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h
  Source/DSP/TransientProtectionEngine.cpp Source/DSP/TransientProtectionEngine.h
  Source/UI/NFLookAndFeel.cpp Source/UI/NFLookAndFeel.h
  Source/UI/SpectrumComponent.cpp Source/UI/SpectrumComponent.h
  Source/UI/ControlCurveComponent.cpp Source/UI/ControlCurveComponent.h)

target_include_directories(NFResonanceDAWPreviewDiagnostics PRIVATE Source)

target_compile_definitions(NFResonanceDAWPreviewDiagnostics PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JucePlugin_Build_VST3=0 JucePlugin_Build_AU=0 JucePlugin_Build_AAX=0)

target_link_libraries(NFResonanceDAWPreviewDiagnostics PRIVATE
  juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# Sensitivity Curve real-wiring check: confirms curveAt() actually
# uses the new per-point frequency parameters. Direct ResonanceDetector
# unit test, no FFT/STFT/host involved.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceSensitivityCurveWiringCheck PRODUCT_NAME "NF Resonance Sensitivity Curve Wiring Check")

juce_generate_juce_header(NFResonanceSensitivityCurveWiringCheck)

target_sources(NFResonanceSensitivityCurveWiringCheck PRIVATE
  Tests/SensitivityCurveWiringCheck.cpp
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h)

target_include_directories(NFResonanceSensitivityCurveWiringCheck PRIVATE Source)

target_compile_definitions(NFResonanceSensitivityCurveWiringCheck PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JucePlugin_Build_VST3=0 JucePlugin_Build_AU=0 JucePlugin_Build_AAX=0)

target_link_libraries(NFResonanceSensitivityCurveWiringCheck PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# Global Range Shape (LOW/HIGH) visual geometry check: prints ASCII plots of
# the exact curve math ControlCurveComponent::paint() sums per sample point,
# for the 5 cases requested to confirm the LOW/HIGH-taper correction.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceGlobalRangeShapeVisualCheck PRODUCT_NAME "NF Resonance Global Range Shape Visual Check")

juce_generate_juce_header(NFResonanceGlobalRangeShapeVisualCheck)

target_sources(NFResonanceGlobalRangeShapeVisualCheck PRIVATE
  Tests/GlobalRangeShapeVisualCheck.cpp
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h)

target_include_directories(NFResonanceGlobalRangeShapeVisualCheck PRIVATE Source)

target_compile_definitions(NFResonanceGlobalRangeShapeVisualCheck PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JucePlugin_Build_VST3=0 JucePlugin_Build_AU=0 JucePlugin_Build_AAX=0)

target_link_libraries(NFResonanceGlobalRangeShapeVisualCheck PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# Checkpoint A closure: legacy c500/freq_c500 automation, applied AFTER
# construction (simulating host automation post-state-load), must still
# drive real DSP output -- not just show up as a value in the host.
# ----------------------------------------------------------------
# ----------------------------------------------------------------
# Bypass null test: with Bypass ON, audible output must equal the
# latency-aligned dry input regardless of Output/Depth/Sharpness/
# Selectivity/Attack/Release/Mix/bands/LOW-HIGH. Also checks the ON/OFF
# transition during playback for clicks/spikes, and a sample-rate/buffer-
# size matrix.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceBypassNullCheck PRODUCT_NAME "NF Resonance Bypass Null Check")

juce_generate_juce_header(NFResonanceBypassNullCheck)

target_sources(NFResonanceBypassNullCheck PRIVATE
  Tests/BypassNullCheck.cpp
  Source/PluginProcessor.cpp Source/PluginProcessor.h
  Source/PluginEditor.cpp Source/PluginEditor.h
  Source/DSP/SpectralEngine.cpp Source/DSP/SpectralEngine.h
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h
  Source/DSP/TransientGuard.h
  Source/DSP/GainMaskEngine.cpp Source/DSP/GainMaskEngine.h
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h
  Source/DSP/TransientProtectionEngine.cpp Source/DSP/TransientProtectionEngine.h
  Source/UI/NFLookAndFeel.cpp Source/UI/NFLookAndFeel.h
  Source/UI/SpectrumComponent.cpp Source/UI/SpectrumComponent.h
  Source/UI/ControlCurveComponent.cpp Source/UI/ControlCurveComponent.h)

target_include_directories(NFResonanceBypassNullCheck PRIVATE Source)

target_compile_definitions(NFResonanceBypassNullCheck PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JucePlugin_Build_VST3=0 JucePlugin_Build_AU=0 JucePlugin_Build_AAX=0)

target_link_libraries(NFResonanceBypassNullCheck PRIVATE
  juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceLegacyAutomationCompatCheck PRODUCT_NAME "NF Resonance Legacy Automation Compat Check")

juce_generate_juce_header(NFResonanceLegacyAutomationCompatCheck)

target_sources(NFResonanceLegacyAutomationCompatCheck PRIVATE
  Tests/LegacyAutomationCompatCheck.cpp
  Source/PluginProcessor.cpp Source/PluginProcessor.h
  Source/PluginEditor.cpp Source/PluginEditor.h
  Source/DSP/SpectralEngine.cpp Source/DSP/SpectralEngine.h
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h
  Source/DSP/TransientGuard.h
  Source/DSP/GainMaskEngine.cpp Source/DSP/GainMaskEngine.h
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h
  Source/DSP/TransientProtectionEngine.cpp Source/DSP/TransientProtectionEngine.h
  Source/UI/NFLookAndFeel.cpp Source/UI/NFLookAndFeel.h
  Source/UI/SpectrumComponent.cpp Source/UI/SpectrumComponent.h
  Source/UI/ControlCurveComponent.cpp Source/UI/ControlCurveComponent.h)

target_include_directories(NFResonanceLegacyAutomationCompatCheck PRIVATE Source)

target_compile_definitions(NFResonanceLegacyAutomationCompatCheck PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JucePlugin_Build_VST3=0 JucePlugin_Build_AU=0 JucePlugin_Build_AAX=0)

target_link_libraries(NFResonanceLegacyAutomationCompatCheck PRIVATE
  juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# Factory default neutrality check: LOW/HIGH are the detector's active-
# region boundary, not a source of reductionDb by themselves -- proves this
# end-to-end through the real AudioProcessor (Depth=0 -> exactly 0dB
# everywhere; no synthetic LOW-boundary valley; a real inserted resonance
# still localises correctly).
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceFactoryDefaultNeutralityCheck PRODUCT_NAME "NF Resonance Factory Default Neutrality Check")

juce_generate_juce_header(NFResonanceFactoryDefaultNeutralityCheck)

target_sources(NFResonanceFactoryDefaultNeutralityCheck PRIVATE
  Tests/FactoryDefaultNeutralityCheck.cpp
  Source/PluginProcessor.cpp Source/PluginProcessor.h
  Source/PluginEditor.cpp Source/PluginEditor.h
  Source/DSP/SpectralEngine.cpp Source/DSP/SpectralEngine.h
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h
  Source/DSP/TransientGuard.h
  Source/DSP/GainMaskEngine.cpp Source/DSP/GainMaskEngine.h
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h
  Source/DSP/TransientProtectionEngine.cpp Source/DSP/TransientProtectionEngine.h
  Source/UI/NFLookAndFeel.cpp Source/UI/NFLookAndFeel.h
  Source/UI/SpectrumComponent.cpp Source/UI/SpectrumComponent.h
  Source/UI/ControlCurveComponent.cpp Source/UI/ControlCurveComponent.h)

target_include_directories(NFResonanceFactoryDefaultNeutralityCheck PRIVATE Source)

target_compile_definitions(NFResonanceFactoryDefaultNeutralityCheck PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JucePlugin_Build_VST3=0 JucePlugin_Build_AU=0 JucePlugin_Build_AAX=0)

target_link_libraries(NFResonanceFactoryDefaultNeutralityCheck PRIVATE
  juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# LOW/HIGH ON/OFF semantics check: disabling opens the detector's range at
# that extreme WITHOUT touching the saved lowHz/highHz value; re-enabling
# restores the exact prior boundary.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceLowHighEnabledCheck PRODUCT_NAME "NF Resonance Low High Enabled Check")

juce_generate_juce_header(NFResonanceLowHighEnabledCheck)

target_sources(NFResonanceLowHighEnabledCheck PRIVATE
  Tests/LowHighEnabledCheck.cpp
  Source/PluginProcessor.cpp Source/PluginProcessor.h
  Source/PluginEditor.cpp Source/PluginEditor.h
  Source/DSP/SpectralEngine.cpp Source/DSP/SpectralEngine.h
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h
  Source/DSP/TransientGuard.h
  Source/DSP/GainMaskEngine.cpp Source/DSP/GainMaskEngine.h
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h
  Source/DSP/TransientProtectionEngine.cpp Source/DSP/TransientProtectionEngine.h
  Source/UI/NFLookAndFeel.cpp Source/UI/NFLookAndFeel.h
  Source/UI/SpectrumComponent.cpp Source/UI/SpectrumComponent.h
  Source/UI/ControlCurveComponent.cpp Source/UI/ControlCurveComponent.h)

target_include_directories(NFResonanceLowHighEnabledCheck PRIVATE Source)

target_compile_definitions(NFResonanceLowHighEnabledCheck PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JucePlugin_Build_VST3=0 JucePlugin_Build_AU=0 JucePlugin_Build_AAX=0)

target_link_libraries(NFResonanceLowHighEnabledCheck PRIVATE
  juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# Throwaway defaults dump: prints the real compiled-in default parameter
# values straight from a freshly constructed processor (no state loaded),
# to sanity-check against Standalone-app persisted-state confusion.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceDefaultsDumpCheck PRODUCT_NAME "NF Resonance Defaults Dump Check")

juce_generate_juce_header(NFResonanceDefaultsDumpCheck)

target_sources(NFResonanceDefaultsDumpCheck PRIVATE
  Tests/DefaultsDumpCheck.cpp
  Source/PluginProcessor.cpp Source/PluginProcessor.h
  Source/PluginEditor.cpp Source/PluginEditor.h
  Source/DSP/SpectralEngine.cpp Source/DSP/SpectralEngine.h
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h
  Source/DSP/TransientGuard.h
  Source/DSP/GainMaskEngine.cpp Source/DSP/GainMaskEngine.h
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h
  Source/DSP/TransientProtectionEngine.cpp Source/DSP/TransientProtectionEngine.h
  Source/UI/NFLookAndFeel.cpp Source/UI/NFLookAndFeel.h
  Source/UI/SpectrumComponent.cpp Source/UI/SpectrumComponent.h
  Source/UI/ControlCurveComponent.cpp Source/UI/ControlCurveComponent.h)

target_include_directories(NFResonanceDefaultsDumpCheck PRIVATE Source)

target_compile_definitions(NFResonanceDefaultsDumpCheck PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JucePlugin_Build_VST3=0 JucePlugin_Build_AU=0 JucePlugin_Build_AAX=0)

target_link_libraries(NFResonanceDefaultsDumpCheck PRIVATE
  juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# Checkpoint A closure: CPU cost of ResonanceDetector::compute() at
# 0/1/7/16/32 active bands.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceMultibandCpuBenchmark PRODUCT_NAME "NF Resonance Multiband CPU Benchmark")

juce_generate_juce_header(NFResonanceMultibandCpuBenchmark)

target_sources(NFResonanceMultibandCpuBenchmark PRIVATE
  Tests/MultibandCpuBenchmark.cpp
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h)

target_include_directories(NFResonanceMultibandCpuBenchmark PRIVATE Source)

target_compile_definitions(NFResonanceMultibandCpuBenchmark PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JucePlugin_Build_VST3=0 JucePlugin_Build_AU=0 JucePlugin_Build_AAX=0)

target_link_libraries(NFResonanceMultibandCpuBenchmark PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# Sonic Alpha V2: SpectralProminenceEngineV5 (A5C geometry + Method C)
# ported in from main's validated V2 prominence research. Prominence-only
# (no reduction stage yet) -- see PHYSICAL B audit.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceNarrowMethodDefaultCheck PRODUCT_NAME "NF Resonance Narrow Method Default Check")
juce_generate_juce_header(NFResonanceNarrowMethodDefaultCheck)
target_sources(NFResonanceNarrowMethodDefaultCheck PRIVATE
  Tests/NarrowMethodDefaultCheck.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h)
target_include_directories(NFResonanceNarrowMethodDefaultCheck PRIVATE Source)
target_link_libraries(NFResonanceNarrowMethodDefaultCheck PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceDetectorV2A5Benchmark PRODUCT_NAME "NF Resonance Detector V2A5 Benchmark")
juce_generate_juce_header(NFResonanceDetectorV2A5Benchmark)
target_sources(NFResonanceDetectorV2A5Benchmark PRIVATE
  Tests/DetectorV2A5Benchmark.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/SpectralProminenceEngineV2.cpp Source/DSP/SpectralProminenceEngineV2.h)
target_include_directories(NFResonanceDetectorV2A5Benchmark PRIVATE Source)
target_link_libraries(NFResonanceDetectorV2A5Benchmark PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceV2A5CpuIsolationBenchmark PRODUCT_NAME "NF Resonance V2A5 CPU Isolation Benchmark")
juce_generate_juce_header(NFResonanceV2A5CpuIsolationBenchmark)
target_sources(NFResonanceV2A5CpuIsolationBenchmark PRIVATE
  Tests/V2A5CpuIsolationBenchmark.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h)
target_include_directories(NFResonanceV2A5CpuIsolationBenchmark PRIVATE Source)
target_link_libraries(NFResonanceV2A5CpuIsolationBenchmark PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceV2A5BPrecisionComparison PRODUCT_NAME "NF Resonance V2A5B Precision Comparison")
juce_generate_juce_header(NFResonanceV2A5BPrecisionComparison)
target_sources(NFResonanceV2A5BPrecisionComparison PRIVATE
  Tests/V2A5B_PrecisionComparison.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/SpectralProminenceEngineV2.cpp Source/DSP/SpectralProminenceEngineV2.h)
target_include_directories(NFResonanceV2A5BPrecisionComparison PRIVATE Source)
target_link_libraries(NFResonanceV2A5BPrecisionComparison PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceV2A5CGeometrySweep PRODUCT_NAME "NF Resonance V2A5C Geometry Sweep")
juce_generate_juce_header(NFResonanceV2A5CGeometrySweep)
target_sources(NFResonanceV2A5CGeometrySweep PRIVATE
  Tests/V2A5C_GeometrySweep.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/SpectralProminenceEngineV2.cpp Source/DSP/SpectralProminenceEngineV2.h)
target_include_directories(NFResonanceV2A5CGeometrySweep PRIVATE Source)
target_link_libraries(NFResonanceV2A5CGeometrySweep PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceV1BaselineVsV2A5CProminence PRODUCT_NAME "NF Resonance V1 Baseline vs V2A5C Prominence")
juce_generate_juce_header(NFResonanceV1BaselineVsV2A5CProminence)
target_sources(NFResonanceV1BaselineVsV2A5CProminence PRIVATE
  Tests/V1BaselineVsV2A5CProminence.cpp
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h)
target_include_directories(NFResonanceV1BaselineVsV2A5CProminence PRIVATE Source)
target_link_libraries(NFResonanceV1BaselineVsV2A5CProminence PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceMusicalMaterialV1vsV2 PRODUCT_NAME "NF Resonance Musical Material V1 vs V2")
juce_generate_juce_header(NFResonanceMusicalMaterialV1vsV2)
target_sources(NFResonanceMusicalMaterialV1vsV2 PRIVATE
  Tests/MusicalMaterialV1vsV2.cpp
  Source/PluginProcessor.cpp Source/PluginProcessor.h
  Source/PluginEditor.cpp Source/PluginEditor.h
  Source/DSP/SpectralEngine.cpp Source/DSP/SpectralEngine.h
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h
  Source/DSP/TransientGuard.h
  Source/DSP/GainMaskEngine.cpp Source/DSP/GainMaskEngine.h
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h
  Source/DSP/TransientProtectionEngine.cpp Source/DSP/TransientProtectionEngine.h
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/UI/NFLookAndFeel.cpp Source/UI/NFLookAndFeel.h
  Source/UI/SpectrumComponent.cpp Source/UI/SpectrumComponent.h
  Source/UI/ControlCurveComponent.cpp Source/UI/ControlCurveComponent.h)
target_include_directories(NFResonanceMusicalMaterialV1vsV2 PRIVATE Source)
target_compile_definitions(NFResonanceMusicalMaterialV1vsV2 PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JucePlugin_Build_VST3=0 JucePlugin_Build_AU=0 JucePlugin_Build_AAX=0)
target_link_libraries(NFResonanceMusicalMaterialV1vsV2 PRIVATE
  juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceMethodCCpuBenchmark PRODUCT_NAME "NF Resonance Method C CPU Benchmark")
juce_generate_juce_header(NFResonanceMethodCCpuBenchmark)
target_sources(NFResonanceMethodCCpuBenchmark PRIVATE
  Tests/MethodCCpuBenchmark.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h)
target_include_directories(NFResonanceMethodCCpuBenchmark PRIVATE Source)
target_link_libraries(NFResonanceMethodCCpuBenchmark PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# Same benchmark, compiled with NF_PROMINENCE_PROFILING=1 -- the internal
# per-stage std::chrono calls inside computeProminence() are compiled BACK
# IN, so B2/C2 measure whether that instrumentation itself adds overhead
# versus B1/C1 (the default, profiling-OFF, commercial-build configuration).
juce_add_console_app(NFResonanceMethodCCpuBenchmarkProfiled PRODUCT_NAME "NF Resonance Method C CPU Benchmark Profiled")
juce_generate_juce_header(NFResonanceMethodCCpuBenchmarkProfiled)
target_sources(NFResonanceMethodCCpuBenchmarkProfiled PRIVATE
  Tests/MethodCCpuBenchmark.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h)
target_include_directories(NFResonanceMethodCCpuBenchmarkProfiled PRIVATE Source)
target_compile_definitions(NFResonanceMethodCCpuBenchmarkProfiled PRIVATE NF_PROMINENCE_PROFILING=1)
target_link_libraries(NFResonanceMethodCCpuBenchmarkProfiled PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceSequentialSampleRateReprepareCheck PRODUCT_NAME "NF Resonance Sequential Sample Rate Reprepare Check")
juce_generate_juce_header(NFResonanceSequentialSampleRateReprepareCheck)
target_sources(NFResonanceSequentialSampleRateReprepareCheck PRIVATE
  Tests/SequentialSampleRateReprepareCheck.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h)
target_include_directories(NFResonanceSequentialSampleRateReprepareCheck PRIVATE Source)
target_link_libraries(NFResonanceSequentialSampleRateReprepareCheck PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ASAN/UBSAN variant for root-cause diagnosis -- separate target so the
# normal Release regression target above stays clean/fast.
juce_add_console_app(NFResonanceSequentialSampleRateReprepareCheckASAN PRODUCT_NAME "NF Resonance Sequential Sample Rate Reprepare Check ASAN")
juce_generate_juce_header(NFResonanceSequentialSampleRateReprepareCheckASAN)
target_sources(NFResonanceSequentialSampleRateReprepareCheckASAN PRIVATE
  Tests/SequentialSampleRateReprepareCheck.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h)
target_include_directories(NFResonanceSequentialSampleRateReprepareCheckASAN PRIVATE Source)
target_compile_options(NFResonanceSequentialSampleRateReprepareCheckASAN PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -g)
target_link_options(NFResonanceSequentialSampleRateReprepareCheckASAN PRIVATE -fsanitize=address,undefined)
target_link_libraries(NFResonanceSequentialSampleRateReprepareCheckASAN PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# PHYSICAL C Checkpoint C1: ConfidenceEngine (prominence -> evidence ->
# confidence), diagnostic only -- no gain reduction anywhere in this target.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceConfidenceLayerCheck PRODUCT_NAME "NF Resonance Confidence Layer Check")
juce_generate_juce_header(NFResonanceConfidenceLayerCheck)
target_sources(NFResonanceConfidenceLayerCheck PRIVATE
  Tests/ConfidenceLayerCheck.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonanceConfidenceLayerCheck PRIVATE Source)
target_link_libraries(NFResonanceConfidenceLayerCheck PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# PHYSICAL C Blocker 2: LowFrequencyHarmonicAnalyzer -- auxiliary,
# audio-path-independent low-frequency harmonic context (decimated to
# ~44.1-48kHz regardless of host SR), tested standalone first.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceLowFreqHarmonicAnalyzerCheck PRODUCT_NAME "NF Resonance Low Freq Harmonic Analyzer Check")
juce_generate_juce_header(NFResonanceLowFreqHarmonicAnalyzerCheck)
target_sources(NFResonanceLowFreqHarmonicAnalyzerCheck PRIVATE
  Tests/LowFreqHarmonicAnalyzerCheck.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonanceLowFreqHarmonicAnalyzerCheck PRIVATE Source)
target_link_libraries(NFResonanceLowFreqHarmonicAnalyzerCheck PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceLowFreqHarmonicAnalyzerCheckASAN PRODUCT_NAME "NF Resonance Low Freq Harmonic Analyzer Check ASAN")
juce_generate_juce_header(NFResonanceLowFreqHarmonicAnalyzerCheckASAN)
target_sources(NFResonanceLowFreqHarmonicAnalyzerCheckASAN PRIVATE
  Tests/LowFreqHarmonicAnalyzerCheck.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonanceLowFreqHarmonicAnalyzerCheckASAN PRIVATE Source)
target_compile_options(NFResonanceLowFreqHarmonicAnalyzerCheckASAN PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -g)
target_link_options(NFResonanceLowFreqHarmonicAnalyzerCheckASAN PRIVATE -fsanitize=address,undefined)
target_link_libraries(NFResonanceLowFreqHarmonicAnalyzerCheckASAN PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# PHYSICAL C Blocker 2 -- F0 bias root-cause diagnosis (stage-by-stage:
# pre-decimation / post-decimation magnitude / post-prominence).
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceLowFreqF0BiasDiagnosis PRODUCT_NAME "NF Resonance Low Freq F0 Bias Diagnosis")
juce_generate_juce_header(NFResonanceLowFreqF0BiasDiagnosis)
target_sources(NFResonanceLowFreqF0BiasDiagnosis PRIVATE
  Tests/LowFreqF0BiasDiagnosis.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h)
target_include_directories(NFResonanceLowFreqF0BiasDiagnosis PRIVATE Source)
target_link_libraries(NFResonanceLowFreqF0BiasDiagnosis PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# PHYSICAL C Blocker 2 -- short characterization sweep confirming the
# f0Reliability >= 2-matches gate doesn't break legitimate cases.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceLowFreqF0ReliabilitySweep PRODUCT_NAME "NF Resonance Low Freq F0 Reliability Sweep")
juce_generate_juce_header(NFResonanceLowFreqF0ReliabilitySweep)
target_sources(NFResonanceLowFreqF0ReliabilitySweep PRIVATE
  Tests/LowFreqF0ReliabilitySweep.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h)
target_include_directories(NFResonanceLowFreqF0ReliabilitySweep PRIVATE Source)
target_link_libraries(NFResonanceLowFreqF0ReliabilitySweep PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# PHYSICAL C2: LowFrequencyHarmonicAnalyzer integrated into
# ConfidenceEngine as the single source of harmonic truth below the
# aux/host crossover. Diagnostic only, no gain reduction.
# ----------------------------------------------------------------
juce_add_console_app(NFResonancePhysicalC2IntegrationCheck PRODUCT_NAME "NF Resonance Physical C2 Integration Check")
juce_generate_juce_header(NFResonancePhysicalC2IntegrationCheck)
target_sources(NFResonancePhysicalC2IntegrationCheck PRIVATE
  Tests/PhysicalC2IntegrationCheck.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonancePhysicalC2IntegrationCheck PRIVATE Source)
target_link_libraries(NFResonancePhysicalC2IntegrationCheck PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceMainRelDiag PRODUCT_NAME "NF Resonance Main Rel Diag")
juce_generate_juce_header(NFResonanceMainRelDiag)
target_sources(NFResonanceMainRelDiag PRIVATE
  Tests/MainRelDiag.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonanceMainRelDiag PRIVATE Source)
target_link_libraries(NFResonanceMainRelDiag PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# PHYSICAL C2: dual-uncertainty (aux+main both unreliable) characterization
# sweep, comparing Policy A (current)/B (damping)/C (conservative hold)
# offline. Does not change production ConfidenceEngine behavior.
# ----------------------------------------------------------------
juce_add_console_app(NFResonancePhysicalC2DualUncertaintySweep PRODUCT_NAME "NF Resonance Physical C2 Dual Uncertainty Sweep")
juce_generate_juce_header(NFResonancePhysicalC2DualUncertaintySweep)
target_sources(NFResonancePhysicalC2DualUncertaintySweep PRIVATE
  Tests/PhysicalC2DualUncertaintySweep.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonancePhysicalC2DualUncertaintySweep PRIVATE Source)
target_link_libraries(NFResonancePhysicalC2DualUncertaintySweep PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# PHYSICAL C2.1: candidate admission / peak floor investigation.
# Diagnostic only, does not touch ConfidenceEngine.cpp production logic.
# ----------------------------------------------------------------
juce_add_console_app(NFResonancePhysicalC2CandidateAdmissionSweep PRODUCT_NAME "NF Resonance Physical C2 Candidate Admission Sweep")
juce_generate_juce_header(NFResonancePhysicalC2CandidateAdmissionSweep)
target_sources(NFResonancePhysicalC2CandidateAdmissionSweep PRIVATE
  Tests/PhysicalC2CandidateAdmissionSweep.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonancePhysicalC2CandidateAdmissionSweep PRIVATE Source)
target_link_libraries(NFResonancePhysicalC2CandidateAdmissionSweep PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceC2_120HzRootCause PRODUCT_NAME "NF Resonance C2 120Hz Root Cause")
juce_generate_juce_header(NFResonanceC2_120HzRootCause)
target_sources(NFResonanceC2_120HzRootCause PRIVATE
  Tests/C2_120HzRootCause.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h)
target_include_directories(NFResonanceC2_120HzRootCause PRIVATE Source)
target_link_libraries(NFResonanceC2_120HzRootCause PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceMagCheck PRODUCT_NAME "NF Resonance Mag Check")
juce_generate_juce_header(NFResonanceMagCheck)
target_sources(NFResonanceMagCheck PRIVATE
  Tests/MagCheck.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h)
target_include_directories(NFResonanceMagCheck PRIVATE Source)
target_link_libraries(NFResonanceMagCheck PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonancePhysicalC22PoolPriorityCheck PRODUCT_NAME "NF Resonance Physical C22 Pool Priority Check")
juce_generate_juce_header(NFResonancePhysicalC22PoolPriorityCheck)
target_sources(NFResonancePhysicalC22PoolPriorityCheck PRIVATE
  Tests/PhysicalC22PoolPriorityCheck.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h)
target_include_directories(NFResonancePhysicalC22PoolPriorityCheck PRIVATE Source)
target_link_libraries(NFResonancePhysicalC22PoolPriorityCheck PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceAdmCheck2 PRODUCT_NAME "NF Resonance Adm Check 2")
juce_generate_juce_header(NFResonanceAdmCheck2)
target_sources(NFResonanceAdmCheck2 PRIVATE
  Tests/AdmCheck2.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonanceAdmCheck2 PRIVATE Source)
target_link_libraries(NFResonanceAdmCheck2 PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceAllocCheck PRODUCT_NAME "NF Resonance Alloc Check")
juce_generate_juce_header(NFResonanceAllocCheck)
target_sources(NFResonanceAllocCheck PRIVATE
  Tests/AllocCheck.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonanceAllocCheck PRIVATE Source)
target_link_libraries(NFResonanceAllocCheck PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceC23ProminenceCompare PRODUCT_NAME "NF Resonance C23 Prominence Compare")
juce_generate_juce_header(NFResonanceC23ProminenceCompare)
target_sources(NFResonanceC23ProminenceCompare PRIVATE
  Tests/C23_ProminenceCompare.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h)
target_include_directories(NFResonanceC23ProminenceCompare PRIVATE Source)
target_link_libraries(NFResonanceC23ProminenceCompare PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceC23BlendCompare PRODUCT_NAME "NF Resonance C23 Blend Compare")
juce_generate_juce_header(NFResonanceC23BlendCompare)
target_sources(NFResonanceC23BlendCompare PRIVATE
  Tests/C23_BlendCompare.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h)
target_include_directories(NFResonanceC23BlendCompare PRIVATE Source)
target_link_libraries(NFResonanceC23BlendCompare PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceC23ApplesToApples PRODUCT_NAME "NF Resonance C23 Apples To Apples")
juce_generate_juce_header(NFResonanceC23ApplesToApples)
target_sources(NFResonanceC23ApplesToApples PRIVATE
  Tests/C23_ApplesToApples.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonanceC23ApplesToApples PRIVATE Source)
target_link_libraries(NFResonanceC23ApplesToApples PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceC23aRefinedBlend PRODUCT_NAME "NF Resonance C23a Refined Blend")
juce_generate_juce_header(NFResonanceC23aRefinedBlend)
target_sources(NFResonanceC23aRefinedBlend PRIVATE
  Tests/C23a_RefinedBlend.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonanceC23aRefinedBlend PRIVATE Source)
target_link_libraries(NFResonanceC23aRefinedBlend PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceC23cCrossSourceDiag PRODUCT_NAME "NF Resonance C23c Cross Source Diag")
juce_generate_juce_header(NFResonanceC23cCrossSourceDiag)
target_sources(NFResonanceC23cCrossSourceDiag PRIVATE
  Tests/C23c_CrossSourceDiag.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h)
target_include_directories(NFResonanceC23cCrossSourceDiag PRIVATE Source)
target_link_libraries(NFResonanceC23cCrossSourceDiag PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceC23dNativeVsDecimated PRODUCT_NAME "NF Resonance C23d Native Vs Decimated")
juce_generate_juce_header(NFResonanceC23dNativeVsDecimated)
target_sources(NFResonanceC23dNativeVsDecimated PRIVATE
  Tests/C23d_NativeVsDecimated.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h)
target_include_directories(NFResonanceC23dNativeVsDecimated PRIVATE Source)
target_link_libraries(NFResonanceC23dNativeVsDecimated PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# PHYSICAL C -- FINAL FULL PIPELINE CHECK. Uses the real, integrated
# production ConfidenceEngine (C2.2 soft admission + C2.3 prominence
# assist + Policy A harmonic blend). Still diagnostic only, no gain.
# ----------------------------------------------------------------
juce_add_console_app(NFResonancePhysicalCFinalPipelineCheck PRODUCT_NAME "NF Resonance Physical C Final Pipeline Check")
juce_generate_juce_header(NFResonancePhysicalCFinalPipelineCheck)
target_sources(NFResonancePhysicalCFinalPipelineCheck PRIVATE
  Tests/PhysicalC_FinalPipelineCheck.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonancePhysicalCFinalPipelineCheck PRIVATE Source)
target_link_libraries(NFResonancePhysicalCFinalPipelineCheck PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceIsolateMagDb PRODUCT_NAME "NF Resonance Isolate MagDb")
juce_generate_juce_header(NFResonanceIsolateMagDb)
target_sources(NFResonanceIsolateMagDb PRIVATE
  Tests/IsolateMagDb.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonanceIsolateMagDb PRIVATE Source)
target_link_libraries(NFResonanceIsolateMagDb PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceTopologyProof PRODUCT_NAME "NF Resonance Topology Proof")
juce_generate_juce_header(NFResonanceTopologyProof)
target_sources(NFResonanceTopologyProof PRIVATE
  Tests/TopologyProof.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h)
target_include_directories(NFResonanceTopologyProof PRIVATE Source)
target_link_libraries(NFResonanceTopologyProof PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceC23f96k120vs170 PRODUCT_NAME "NF Resonance C23f 96k 120 vs 170")
juce_generate_juce_header(NFResonanceC23f96k120vs170)
target_sources(NFResonanceC23f96k120vs170 PRIVATE
  Tests/C23f_96k120vs170.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonanceC23f96k120vs170 PRIVATE Source)
target_link_libraries(NFResonanceC23f96k120vs170 PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonancePhysicalDTransientCheck PRODUCT_NAME "NF Resonance Physical D Transient Check")
juce_generate_juce_header(NFResonancePhysicalDTransientCheck)
target_sources(NFResonancePhysicalDTransientCheck PRIVATE
  Tests/PhysicalD_TransientProtectionCheck.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h
  Source/DSP/TransientProtectionEngine.cpp Source/DSP/TransientProtectionEngine.h)
target_include_directories(NFResonancePhysicalDTransientCheck PRIVATE Source)
target_link_libraries(NFResonancePhysicalDTransientCheck PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceC23kActionAuthority PRODUCT_NAME "NF Resonance C23k Action Authority")
juce_generate_juce_header(NFResonanceC23kActionAuthority)
target_sources(NFResonanceC23kActionAuthority PRIVATE
  Tests/C23k_ActionAuthorityValidation.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonanceC23kActionAuthority PRIVATE Source)
target_link_libraries(NFResonanceC23kActionAuthority PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceC23iOverconfidenceAudit PRODUCT_NAME "NF Resonance C23i Overconfidence Audit")
juce_generate_juce_header(NFResonanceC23iOverconfidenceAudit)
target_sources(NFResonanceC23iOverconfidenceAudit PRIVATE
  Tests/C23i_FundamentalOverconfidenceAudit.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonanceC23iOverconfidenceAudit PRIVATE Source)
target_link_libraries(NFResonanceC23iOverconfidenceAudit PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceC23hCascadeDiag PRODUCT_NAME "NF Resonance C23h Cascade Diag")
juce_generate_juce_header(NFResonanceC23hCascadeDiag)
target_sources(NFResonanceC23hCascadeDiag PRIVATE
  Tests/C23h_BridgeCascadeDiag.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonanceC23hCascadeDiag PRIVATE Source)
target_link_libraries(NFResonanceC23hCascadeDiag PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceC23hDebug113 PRODUCT_NAME "NF Resonance C23h Debug 113")
juce_generate_juce_header(NFResonanceC23hDebug113)
target_sources(NFResonanceC23hDebug113 PRIVATE
  Tests/C23h_Debug113.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonanceC23hDebug113 PRIVATE Source)
target_link_libraries(NFResonanceC23hDebug113 PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonanceC23hNegativeBridge PRODUCT_NAME "NF Resonance C23h Negative Bridge")
juce_generate_juce_header(NFResonanceC23hNegativeBridge)
target_sources(NFResonanceC23hNegativeBridge PRIVATE
  Tests/C23h_NegativeBridgeCheck.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonanceC23hNegativeBridge PRIVATE Source)
target_link_libraries(NFResonanceC23hNegativeBridge PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonance48k135GroundTruth PRODUCT_NAME "NF Resonance 48k 135 Ground Truth")
juce_generate_juce_header(NFResonance48k135GroundTruth)
target_sources(NFResonance48k135GroundTruth PRIVATE
  Tests/C23g_48k135GroundTruth.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonance48k135GroundTruth PRIVATE Source)
target_link_libraries(NFResonance48k135GroundTruth PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonance48k135vs80 PRODUCT_NAME "NF Resonance 48k 135 vs 80")
juce_generate_juce_header(NFResonance48k135vs80)
target_sources(NFResonance48k135vs80 PRIVATE
  Tests/C_48k135vs80.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonance48k135vs80 PRIVATE Source)
target_link_libraries(NFResonance48k135vs80 PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

juce_add_console_app(NFResonancePoolRescueCheck PRODUCT_NAME "NF Resonance Pool Rescue Check")
juce_generate_juce_header(NFResonancePoolRescueCheck)
target_sources(NFResonancePoolRescueCheck PRIVATE
  Tests/PoolRescueCheck.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h
  Source/DSP/LowFrequencyHarmonicAnalyzer.cpp Source/DSP/LowFrequencyHarmonicAnalyzer.h
  Source/DSP/ConfidenceEngine.cpp Source/DSP/ConfidenceEngine.h)
target_include_directories(NFResonancePoolRescueCheck PRIVATE Source)
target_link_libraries(NFResonancePoolRescueCheck PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
