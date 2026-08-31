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
  Source/DSP/SpectralProminenceEngine.cpp Source/DSP/SpectralProminenceEngine.h
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
  Source/DSP/SpectralProminenceEngine.cpp Source/DSP/SpectralProminenceEngine.h
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
  Source/DSP/SpectralProminenceEngine.cpp Source/DSP/SpectralProminenceEngine.h
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
  Source/DSP/SpectralProminenceEngine.cpp Source/DSP/SpectralProminenceEngine.h
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
  Source/DSP/SpectralProminenceEngine.cpp Source/DSP/SpectralProminenceEngine.h
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
# Detector benchmark harness: baselines the current V1 detector
# (pink noise + injected resonances at controlled freq/level/Q)
# ahead of the Detector V2 redesign.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceDetectorV2Validation PRODUCT_NAME "NF Resonance Detector V2 Validation")

juce_generate_juce_header(NFResonanceDetectorV2Validation)

target_sources(NFResonanceDetectorV2Validation PRIVATE
  Tests/DetectorV2Validation.cpp
  Source/PluginProcessor.cpp Source/PluginProcessor.h
  Source/PluginEditor.cpp Source/PluginEditor.h
  Source/DSP/SpectralEngine.cpp Source/DSP/SpectralEngine.h
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h
  Source/DSP/SpectralProminenceEngine.cpp Source/DSP/SpectralProminenceEngine.h
  Source/DSP/TransientGuard.h
  Source/UI/NFLookAndFeel.cpp Source/UI/NFLookAndFeel.h
  Source/UI/SpectrumComponent.cpp Source/UI/SpectrumComponent.h
  Source/UI/ControlCurveComponent.cpp Source/UI/ControlCurveComponent.h)

target_include_directories(NFResonanceDetectorV2Validation PRIVATE Source)

target_compile_definitions(NFResonanceDetectorV2Validation PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
  JucePlugin_Build_VST3=0 JucePlugin_Build_AU=0 JucePlugin_Build_AAX=0)

target_link_libraries(NFResonanceDetectorV2Validation PRIVATE
  juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# Detector unit test: exercises ResonanceDetector::compute() directly
# with synthetic spectral frames -- no FFT/windowing/OLA/pink-noise
# randomness -- to isolate detector-envelope timing from STFT effects.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceDetectorUnitTest PRODUCT_NAME "NF Resonance Detector Unit Test")

juce_generate_juce_header(NFResonanceDetectorUnitTest)

target_sources(NFResonanceDetectorUnitTest PRIVATE
  Tests/DetectorUnitTest.cpp
  Source/DSP/ResonanceDetector.cpp Source/DSP/ResonanceDetector.h
  Source/DSP/SpectralProminenceEngine.cpp Source/DSP/SpectralProminenceEngine.h)

target_include_directories(NFResonanceDetectorUnitTest PRIVATE Source)

target_link_libraries(NFResonanceDetectorUnitTest PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# V2-A Spectral Prominence Engine benchmark: synthetic spectral frames
# with exactly-known ground truth (no FFT/windowing/pink-noise).
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceDetectorV2ABenchmark PRODUCT_NAME "NF Resonance Detector V2A Benchmark")

juce_generate_juce_header(NFResonanceDetectorV2ABenchmark)

target_sources(NFResonanceDetectorV2ABenchmark PRIVATE
  Tests/DetectorV2ABenchmark.cpp
  Source/DSP/SpectralProminenceEngine.cpp Source/DSP/SpectralProminenceEngine.h)

target_include_directories(NFResonanceDetectorV2ABenchmark PRIVATE Source)

target_link_libraries(NFResonanceDetectorV2ABenchmark PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# V2-A2 hybrid candidate-based prominence engine validation.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceDetectorV2A2Benchmark PRODUCT_NAME "NF Resonance Detector V2A2 Benchmark")

juce_generate_juce_header(NFResonanceDetectorV2A2Benchmark)

target_sources(NFResonanceDetectorV2A2Benchmark PRIVATE
  Tests/DetectorV2A2Benchmark.cpp
  Source/DSP/SpectralProminenceEngineV2.cpp Source/DSP/SpectralProminenceEngineV2.h)

target_include_directories(NFResonanceDetectorV2A2Benchmark PRIVATE Source)

target_link_libraries(NFResonanceDetectorV2A2Benchmark PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# Diagnose candidate counts under different content types/thresholds.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceCandidateDiagnosis PRODUCT_NAME "NF Resonance Candidate Diagnosis")

juce_generate_juce_header(NFResonanceCandidateDiagnosis)

target_sources(NFResonanceCandidateDiagnosis PRIVATE
  Tests/CandidateDiagnosis.cpp
  Source/DSP/SpectralProminenceEngineV2.cpp Source/DSP/SpectralProminenceEngineV2.h)

target_include_directories(NFResonanceCandidateDiagnosis PRIVATE Source)

target_link_libraries(NFResonanceCandidateDiagnosis PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# Compares cheap-pass signal-smoothing variants: recall, false-candidate
# rate, and CPU.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceCheapPassSmoothing PRODUCT_NAME "NF Resonance Cheap Pass Smoothing")

juce_generate_juce_header(NFResonanceCheapPassSmoothing)

target_sources(NFResonanceCheapPassSmoothing PRIVATE
  Tests/CheapPassSmoothingBenchmark.cpp
  Source/DSP/SpectralProminenceEngineV2.cpp Source/DSP/SpectralProminenceEngineV2.h)

target_include_directories(NFResonanceCheapPassSmoothing PRIVATE Source)

target_link_libraries(NFResonanceCheapPassSmoothing PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# V2-A3 scale-specific prominence engine validation.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceDetectorV2A3Benchmark PRODUCT_NAME "NF Resonance Detector V2A3 Benchmark")

juce_generate_juce_header(NFResonanceDetectorV2A3Benchmark)

target_sources(NFResonanceDetectorV2A3Benchmark PRIVATE
  Tests/DetectorV2A3Benchmark.cpp
  Source/DSP/SpectralProminenceEngineV3.cpp Source/DSP/SpectralProminenceEngineV3.h
  Source/DSP/SpectralProminenceEngineV2.cpp Source/DSP/SpectralProminenceEngineV2.h)

target_include_directories(NFResonanceDetectorV2A3Benchmark PRIVATE Source)

target_link_libraries(NFResonanceDetectorV2A3Benchmark PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# V2-A4 region-based NARROW refinement validation.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceDetectorV2A4Benchmark PRODUCT_NAME "NF Resonance Detector V2A4 Benchmark")

juce_generate_juce_header(NFResonanceDetectorV2A4Benchmark)

target_sources(NFResonanceDetectorV2A4Benchmark PRIVATE
  Tests/DetectorV2A4Benchmark.cpp
  Source/DSP/SpectralProminenceEngineV4.cpp Source/DSP/SpectralProminenceEngineV4.h
  Source/DSP/SpectralProminenceEngineV2.cpp Source/DSP/SpectralProminenceEngineV2.h)

target_include_directories(NFResonanceDetectorV2A4Benchmark PRIVATE Source)

target_link_libraries(NFResonanceDetectorV2A4Benchmark PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# V2-A4 region budget sweep on worst-case content.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceV2A4BudgetSweep PRODUCT_NAME "NF Resonance V2A4 Budget Sweep")

juce_generate_juce_header(NFResonanceV2A4BudgetSweep)

target_sources(NFResonanceV2A4BudgetSweep PRIVATE
  Tests/V2A4BudgetSweep.cpp
  Source/DSP/SpectralProminenceEngineV4.cpp Source/DSP/SpectralProminenceEngineV4.h)

target_include_directories(NFResonanceV2A4BudgetSweep PRIVATE Source)

target_link_libraries(NFResonanceV2A4BudgetSweep PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# Final validation of budget=16 production candidacy for V2-A4.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceV2A4Budget16Validation PRODUCT_NAME "NF Resonance V2A4 Budget16 Validation")

juce_generate_juce_header(NFResonanceV2A4Budget16Validation)

target_sources(NFResonanceV2A4Budget16Validation PRIVATE
  Tests/V2A4Budget16Validation.cpp
  Source/DSP/SpectralProminenceEngineV4.cpp Source/DSP/SpectralProminenceEngineV4.h
  Source/DSP/SpectralProminenceEngineV2.cpp Source/DSP/SpectralProminenceEngineV2.h)

target_include_directories(NFResonanceV2A4Budget16Validation PRIVATE Source)

target_link_libraries(NFResonanceV2A4Budget16Validation PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# V2-A4 tiered scheduler validation (separates candidate detection,
# scheduling/fairness, and refinement accuracy).
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceV2A4TieredValidation PRODUCT_NAME "NF Resonance V2A4 Tiered Validation")

juce_generate_juce_header(NFResonanceV2A4TieredValidation)

target_sources(NFResonanceV2A4TieredValidation PRIVATE
  Tests/V2A4TieredValidation.cpp
  Source/DSP/SpectralProminenceEngineV4.cpp Source/DSP/SpectralProminenceEngineV4.h
  Source/DSP/SpectralProminenceEngineV2.cpp Source/DSP/SpectralProminenceEngineV2.h)

target_include_directories(NFResonanceV2A4TieredValidation PRIVATE Source)

target_link_libraries(NFResonanceV2A4TieredValidation PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# V2-A5 deterministic fast prominence engine validation.
# ----------------------------------------------------------------
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

# ----------------------------------------------------------------
# V2-A5 isolated CPU microbenchmark: SpectralProminenceEngineV5 only,
# same-frame x10000 per scenario, per-stage timing, zero-alloc proof.
# ----------------------------------------------------------------
juce_add_console_app(NFResonanceV2A5CpuIsolationBenchmark PRODUCT_NAME "NF Resonance V2A5 CPU Isolation Benchmark")

juce_generate_juce_header(NFResonanceV2A5CpuIsolationBenchmark)

target_sources(NFResonanceV2A5CpuIsolationBenchmark PRIVATE
  Tests/V2A5CpuIsolationBenchmark.cpp
  Source/DSP/SpectralProminenceEngineV5.cpp Source/DSP/SpectralProminenceEngineV5.h)

target_include_directories(NFResonanceV2A5CpuIsolationBenchmark PRIVATE Source)

target_link_libraries(NFResonanceV2A5CpuIsolationBenchmark PRIVATE
  juce::juce_core juce::juce_dsp
  juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

# ----------------------------------------------------------------
# V2-A5B precision comparison: Method A vs Method C vs P25 oracle,
# corrected synthetic generator, multi-sample-rate, precision-only
# (CPU is measured separately by V2A5CpuIsolationBenchmark).
# ----------------------------------------------------------------
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

# ----------------------------------------------------------------
# V2-A5C: core/context geometry calibration sweep for Method C,
# targeting the leakage bias found in the V2-A5B comparison.
# ----------------------------------------------------------------
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
