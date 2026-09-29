#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>

namespace vc
{
// Optional, bounded audio-to-UI telemetry. No allocation, FFT or locks on the
// audio thread. Atomic cells permit a reader to abandon an overwritten frame.
class VisualTap
{
public:
    static constexpr int size = 1024;
    struct Frame { std::array<std::array<float, size>, 4> wave {}; double sampleRate = 44100; unsigned serial = 0; };
    std::atomic<float> bpm { 120 };
    std::atomic<int> readers { 0 };
    void prepare (double sr) { sampleRate.store ((float) sr); }
    void begin (const juce::AudioBuffer<float>& b)
    {
        writing = readers.load (std::memory_order_relaxed) > 0;
        if (!writing) return;
        sequence.fetch_add (1, std::memory_order_acq_rel);
        copy (b, 0);
    }
    void end (const juce::AudioBuffer<float>& b)
    {
        if (!writing) return;
        copy (b, 2);
        position = (position + (unsigned) b.getNumSamples()) % size;
        head.store (position, std::memory_order_relaxed);
        sequence.fetch_add (1, std::memory_order_release);
    }
    bool read (Frame& f) const
    {
        const auto before = sequence.load (std::memory_order_acquire);
        if ((before & 1u) || before == f.serial) return false;
        const auto start = head.load (std::memory_order_relaxed);
        for (int c = 0; c < 4; ++c)
            for (int i = 0; i < size; ++i)
                f.wave[(size_t)c][(size_t)i] = data[(size_t)c][(start + (unsigned)i) % size].load (std::memory_order_relaxed);
        f.sampleRate = sampleRate.load();
        std::atomic_thread_fence (std::memory_order_acquire);
        if (sequence.load (std::memory_order_acquire) != before) return false;
        f.serial = before;
        return true;
    }
private:
    void copy (const juce::AudioBuffer<float>& b, int first)
    {
        if (b.getNumChannels() == 0) return;
        // Only the latest window is useful when hosts deliver very large blocks.
        for (int c = 0; c < 2; ++c)
        {
            const auto* src = b.getReadPointer (juce::jmin (c, b.getNumChannels() - 1));
            for (int i = juce::jmax (0, b.getNumSamples() - size); i < b.getNumSamples(); ++i)
                data[(size_t)(first+c)][(position + (unsigned)i) % size].store (
                    std::isfinite (src[i]) ? src[i] : 0.0f, std::memory_order_relaxed);
        }
    }
    static_assert (std::atomic<float>::is_always_lock_free);
    std::array<std::array<std::atomic<float>, size>, 4> data {};
    std::atomic<unsigned> sequence { 0 }, head { 0 };
    std::atomic<float> sampleRate { 44100 };
    unsigned position = 0;
    bool writing = false;
};
}
