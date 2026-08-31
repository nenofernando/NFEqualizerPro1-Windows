# NF Resonance DSP Design

## Signal path
Input → optional M/S matrix → STFT analysis → adaptive local baseline → resonance prominence → sensitivity weighting → attack/release gain map → transient protection → spectral gain → inverse STFT/OLA → Delta or wet output → latency-aligned Mix → output gain.

## Detector idea
For each FFT bin, compare its level to a local spectral neighborhood. A peak that rises above the adaptive local mean is a resonance candidate. `Sharpness` changes neighborhood width; `Selectivity` changes the prominence threshold and gain slope; `Depth` limits maximum attenuation. This avoids treating every loud frequency as a resonance.

## Windowing
2048-point sqrt-Hann, 75% overlap (hop 512). Analysis and synthesis windows multiply to Hann; norm-ring overlap normalization protects amplitude. Nominal latency is one FFT size.

## Improvements expected from coding agent
- Stereo-linked detector in Stereo mode.
- Optional larger FFT in High mode and smaller FFT in Eco mode with crossfaded mode transitions.
- Detector psychoacoustic weighting and frequency-dependent time constants.
- Better local baseline: robust percentile/median approximation rather than only mean.
- Sidechain detector bus.
- Vectorization and no data races for analyzer snapshots.
