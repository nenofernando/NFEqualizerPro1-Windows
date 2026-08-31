# Visual implementation handoff

Every reusable piece is separated in `Assets/Visual_Parts/SVG`; 2x PNG fallbacks are in the adjacent PNG folder. `visual_manifest.json` lists each file and intrinsic size. `LAYOUT_SPEC.json` defines the reference 1536×864 geometry.

Recommended implementation: draw scalable vectors in JUCE or embed SVG resources at build time. Do not load files from disk at runtime. Use BinaryData or convert the art to JUCE draw calls. Keep the SVG files in the source package as the canonical reference.

State assets: toggle_off/on and button_off/on. Knob SVGs are visual baselines; the included NFLookAndFeel provides procedural rotation so no sprite-sheet dependency is required.

The concept PNG is a reference composition only. The code UI should preserve dimensions, hierarchy, dark graphite panels and purple accent while remaining an original NF design.
