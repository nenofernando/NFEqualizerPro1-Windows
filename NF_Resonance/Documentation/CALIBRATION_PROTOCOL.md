# Sonic calibration protocol

This file is intentionally included so the project is tuned from legal black-box measurements rather than guesses.

1. Use the same DAW, sample rate, buffer size and level into both plugins. Disable auto-gain.
2. Test sine sweeps (-18 dBFS), pink noise, isolated two-tone peaks, vocal sibilance, nasal vocal, acoustic guitar, overheads and full mix.
3. Capture both normal output and Delta/removed output when available.
4. For each control, test minimum, 25%, 50%, 75%, maximum while all other controls remain fixed.
5. Measure: frequency-dependent reduction, maximum attenuation, attack 10→90%, release 90→10%, transient overshoot, stereo linking, low/high range slopes, latency and CPU.
6. Tune only NF constants and algorithms until curves and timing converge. Do not import or decompile competitor binaries.
7. Acceptance target for a “close” preset can be defined as: median reduction-envelope error < 1.5 dB in the active range and attack/release timing within 20% on the supplied test set, while subjective transient preservation is equal or better.
8. Keep a regression folder of WAV tests and CSV measurements for every tuning revision.
