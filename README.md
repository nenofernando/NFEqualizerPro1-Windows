# NF Equalizer V2 — JUCE

## Visual identity
- Fluorescent green main panel
- Purple knobs
- Black outer and inner knob contours
- White knob position indicator
- White scale ticks around each knob
- Black section panels
- Purple control-strip accents

## DSP
- 3-band musical EQ
- LOW shelf
- MID peak
- HIGH shelf
- Input / Output
- Drive
- Character
- Mix
- 4x oversampling in saturation
- APVTS parameter state
- VST3 + AU

## Build

Requirements:
- JUCE 7.x or newer
- CMake 3.22+
- C++17
- macOS for AU

Configure:

cmake -B build -DJUCE_DIR=/absolute/path/to/JUCE

Build:

cmake --build build --config Release

The generated plugin targets are VST3 and AU.

## Notes
This V2 focuses on the requested visual identity. The EQ/DSP is an original implementation using standard JUCE DSP building blocks and is not a 1:1 copy of any commercial hardware/software circuit.

Before commercial release, test at 44.1/48/88.2/96 kHz, mono/stereo, DAW automation, preset recall, plugin scanning and CPU usage.
