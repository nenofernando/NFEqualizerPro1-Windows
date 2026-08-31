#pragma once
#include <array>
#include <atomic>

// ============================================================================
// Resonance Map Snapshot -- architecture prep only (NOT wired to any real
// data yet). This is the intended data contract for the automatic realtime
// resonance-detection pipeline:
//
//     audio -> FFT -> prominence -> persistence/confidence -> resonance map -> reduction
//
// Today (V2-A5), only "prominence" exists (SpectralProminenceEngineV5 / the
// A/C estimators under evaluation). "persistence/confidence" -- turning a
// per-frame prominence score into a stable, non-flickering judgement that a
// frequency region is a genuine, persistent resonance rather than a
// transient peak -- is explicitly NOT implemented yet; that is V2-B/V2-C's
// job. Nothing in this file invents confidence data. A weak (+2dB) resonance
// is expected to need several frames of accumulated confidence before it
// would ever be marked `active`; a strong resonance could become `active`
// quickly. None of that logic exists yet -- this struct just reserves the
// field for when it does.
//
// UI/DSP separation: the audio thread computes regions and calls publish()
// only -- it must never touch anything UI-related. The message/UI thread
// calls read() on a timer (see SpectrumComponent) and only ever looks at the
// most recently published snapshot. Neither side blocks the other:
//
//   - Fixed-size storage (maxRegions), no heap allocation in publish()/read().
//   - Double-buffered: the audio thread always writes into the buffer that
//     is NOT currently published, then atomically flips which index is
//     "published" -- a classic single-writer/single-reader lock-free
//     snapshot. No mutex, no spin-wait, no blocking on either side.
//   - The UI thread may read a snapshot that is one audio block "stale" (it
//     started reading just before a new publish() completed) -- that is
//     expected and harmless for a visual overlay; it is never a torn/partial
//     read of a single region, since read() only ever looks at a fully
//     published buffer.
// ============================================================================

struct ResonanceRegion
{
    float frequencyHz = 0.0f;
    float prominenceDb = 0.0f;   // raw detector output for this region (exists today, V2-A5)
    float confidence = 0.0f;     // 0..1, smooth -- NOT a binary threshold. NOT implemented yet (V2-B/V2-C).
    float reductionDb = 0.0f;    // actual applied reduction -- distinct from prominence/confidence by design;
                                  // a region can be detected as resonant before it receives meaningful reduction.
    bool active = false;         // persistent-resonance classification (vs a transient blip). NOT implemented yet.
};

class ResonanceMapSnapshot
{
public:
    static constexpr int maxRegions = 32; // fixed-size -- no heap allocation, ever

    // AUDIO THREAD ONLY. Never allocates, never blocks. `count` is clamped
    // to maxRegions. Currently uncalled anywhere in the codebase -- there is
    // no real confidence/persistence data yet, so nothing should call this
    // with fabricated values.
    void publish(const std::array<ResonanceRegion, maxRegions>& regions, int count) noexcept
    {
        int w = writeIndex;
        buffers[(size_t) w] = regions;
        counts[(size_t) w] = count < 0 ? 0 : (count > maxRegions ? maxRegions : count);
        publishedIndex.store(w, std::memory_order_release);
        writeIndex = 1 - w; // next publish() writes into the other buffer
    }

    // UI/MESSAGE THREAD ONLY. Never allocates, never blocks. Returns the
    // region count of whatever was most recently published (0 if publish()
    // has never been called, which is the current, expected state).
    int read(std::array<ResonanceRegion, maxRegions>& outRegions) const noexcept
    {
        int r = publishedIndex.load(std::memory_order_acquire);
        outRegions = buffers[(size_t) r];
        return counts[(size_t) r];
    }

private:
    std::array<std::array<ResonanceRegion, maxRegions>, 2> buffers{};
    std::array<int, 2> counts{ 0, 0 };
    std::atomic<int> publishedIndex{ 0 };
    int writeIndex = 1;
};
