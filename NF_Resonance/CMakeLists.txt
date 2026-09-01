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
