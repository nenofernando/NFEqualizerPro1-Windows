NF RESONANCE — COMPLETE HANDOFF PACK

WHAT THIS PACKAGE IS
A JUCE/C++ reference implementation and original UI kit for a dynamic spectral resonance suppressor. It is designed to be developed and calibrated toward the behavior of modern resonance suppressors without copying proprietary source code or branded UI.

IMPORTANT
1. Open this whole folder in Claude Code or Cursor, not isolated files.
2. Tell the coding agent to read CLAUDE_CURSOR_MASTER_PROMPT.txt first.
3. The canonical visual pieces are in Assets/Visual_Parts/SVG. PNG files are 2x raster fallbacks.
4. Do not redraw or replace the supplied visual proportions unless requested.
5. VST3: macOS/Windows. AU: macOS only. AAX: macOS/Windows but requires the Avid AAX SDK and signing/distribution requirements.
6. JUCE itself is not bundled. Point CMake to a legal JUCE installation.

BUILD EXAMPLES
macOS: cmake -B build -DJUCE_DIR=/path/to/JUCE && cmake --build build --config Release
Windows: cmake -B build -DJUCE_DIR=C:/JUCE && cmake --build build --config Release
AAX: add -DNF_ENABLE_AAX=ON after JUCE/AAX SDK is configured.

FIRST LISTENING TARGETS
- Smooth reduction, not static EQ.
- Delta should contain resonant excess rather than the whole source.
- Preserve consonant attack and drum transients.
- Avoid broad dulling at moderate Depth.
- No clicks when automating Mix/Depth.

See Documentation/CALIBRATION_PROTOCOL.md before claiming any sonic match.
