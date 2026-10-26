#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <cstring>

// Control side: decode/prepare/publish and reclaim. Audio side: bounded pointer
// exchanges and process only. No Convolution instance is loaded after publication.
class CabinetIR : private juce::Thread
{
public:
    static constexpr juce::int64 maxFileBytes = 64 * 1024 * 1024;
    static constexpr int maxSamples = 4 * 1024 * 1024;
    static constexpr double budgetSampleRate = 768000.0;
    static_assert (std::atomic<void*>::is_always_lock_free, "IR handoff requires lock-free pointers");

    CabinetIR() : juce::Thread ("Cabinet IR reclamation")
    {
        if (! startThread()) throw std::runtime_error ("Cannot start IR reclamation thread");
    }

    ~CabinetIR() // host must have stopped the audio callback
    {
        stopThread (-1);
        delete pending.exchange (nullptr);
        delete retired.exchange (nullptr);
        delete active;
        delete previous;
    }

    // Host lifecycle only: must not overlap process(). Control requests are serialised.
    void prepare (const juce::dsp::ProcessSpec& newSpec)
    {
        std::lock_guard<std::mutex> lock (control);
        auto ready = makeEngine (source.get(), newSpec); // build before disturbing the old state
        spec = newSpec;
        prepared = true;
        transition.setSize ((int) spec.numChannels, (int) spec.maximumBlockSize);
        delete pending.exchange (nullptr);
        delete retired.exchange (nullptr);
        delete active;
        delete previous;
        active = ready.release();
        previous = nullptr;
        fadePosition = fadeLength = 0;
    }

    bool load (const juce::File& file)
    {
        std::lock_guard<std::mutex> lock (control);
        try
        {
            if (! file.existsAsFile() || file.getSize() <= 0 || file.getSize() > maxFileBytes)
                return fail ("IR file is missing, empty or exceeds 64 MiB");
            // Read a bounded immutable snapshot: a rename/replacement after validation
            // cannot make JUCE asynchronously open different or missing file contents.
            juce::MemoryBlock bytes;
            juce::FileInputStream input (file);
            if (! input.openedOk() || input.readIntoMemoryBlock (bytes, (size_t) maxFileBytes + 1) <= 0
                || bytes.getSize() > (size_t) maxFileBytes)
                return fail ("Cannot read IR file");
            // Some readers pad a truncated container with zeros; reject a
            // WAV/AIFF whose declared container extends beyond our snapshot.
            if (bytes.getSize() >= 12)
            {
                const auto* raw = static_cast<const unsigned char*> (bytes.getData());
                const bool riff = std::memcmp (raw, "RIFF", 4) == 0;
                const bool form = std::memcmp (raw, "FORM", 4) == 0;
                if ((riff || form) && (juce::uint64) (riff ? juce::ByteOrder::littleEndianInt (raw + 4)
                                                                          : juce::ByteOrder::bigEndianInt (raw + 4)) + 8 > bytes.getSize())
                    return fail ("Truncated IR container");
            }
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (
                std::make_unique<juce::MemoryInputStream> (bytes, false)));
            if (! reader || (reader->numChannels == 0 || reader->numChannels > 2) || reader->lengthInSamples <= 0
                || reader->lengthInSamples > maxSamples || ! std::isfinite (reader->sampleRate)
                || reader->sampleRate < 8000.0 || reader->sampleRate > 768000.0)
                return fail ("Unsupported or invalid IR audio data");
            // Pre-activation restore does not yet know the host rate. Check the
            // complete supported rate budget now, rather than accepting an IR
            // that would throw during a later ordinary sample-rate change.
            if ((double) reader->lengthInSamples * budgetSampleRate / reader->sampleRate > maxSamples)
                return fail ("IR exceeds the cross-sample-rate resource budget");
            auto candidate = std::make_unique<Source>();
            candidate->data.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
            candidate->rate = reader->sampleRate;
            candidate->path = file.getFullPathName();
            // Preserve separate L/R channels of stereo IRs; no downmix.
            if (! reader->read (candidate->data.getArrayOfWritePointers(), candidate->data.getNumChannels(), 0, candidate->data.getNumSamples()))
                return fail ("Incomplete IR audio data");
            for (int channel = 0; channel < candidate->data.getNumChannels(); ++channel)
                for (int i = 0; i < candidate->data.getNumSamples(); ++i)
                    if (! std::isfinite (candidate->data.getSample (channel, i)))
                        return fail ("IR contains non-finite samples");
            auto ready = prepared ? makeEngine (candidate.get(), spec) : nullptr;
            // All operations that can reject the candidate precede this commit.
            source = std::move (candidate);
            if (prepared) publish (std::move (ready));
            error.clear();
            return true;
        }
        catch (const std::exception&)
        {
            return fail ("Cannot prepare IR (previous cabinet retained)");
        }
    }

    bool clear()
    {
        std::lock_guard<std::mutex> lock (control);
        try
        {
            if (! source) { error.clear(); return true; }
            auto builtin = prepared ? makeEngine (nullptr, spec) : nullptr;
            source.reset();
            if (prepared) publish (std::move (builtin));
            error.clear();
            return true;
        }
        catch (const std::exception&)
        {
            return fail ("Cannot select built-in cabinet (previous cabinet retained)");
        }
    }

    juce::String getPath() const
    {
        std::lock_guard<std::mutex> lock (control);
        return source ? source->path : juce::String();
    }
    juce::String getError() const
    {
        std::lock_guard<std::mutex> lock (control);
        return error;
    }

    template <typename Builtin>
    void process (juce::dsp::AudioBlock<float> block, Builtin&& builtin) noexcept
    {
        // At most one outstanding retired instance. If the control side has not
        // reclaimed it yet, keep rendering; never wait, spin, allocate or delete.
        if (previous == nullptr && retired.load() == nullptr)
            if (auto* next = pending.exchange (nullptr))
            {
                previous = active;
                active = next;
                fadePosition = 0;
                fadeLength = std::max (1, (int) (0.05 * spec.sampleRate));
            }
        if (previous && ! previous->convolution && ! active->convolution)
        {
            retired.store (previous);
            previous = nullptr;
        }
        const int n = (int) block.getNumSamples();
        if (previous)
            transition.copyFrom (0, 0, block.getChannelPointer (0), n);
        render (active, block, builtin);
        if (previous)
        {
            auto old = juce::dsp::AudioBlock<float> (transition).getSubBlock (0, (size_t) n);
            render (previous, old, builtin);
            float* out = block.getChannelPointer (0);
            const float* from = transition.getReadPointer (0);
            for (int i = 0; i < n; ++i)
            {
                const float mix = std::min (1.0f, (float) ++fadePosition / (float) fadeLength);
                out[i] = from[i] + mix * (out[i] - from[i]);
            }
            if (fadePosition >= fadeLength)
            {
                retired.store (previous); // no access to previous after publication
                previous = nullptr;
            }
        }
    }


    // Two-channel atomic scene swap: both channels use the same accepted IR
    // revision and one shared fade position. No audio-thread allocations/locks.
    template <typename Builtin>
    void processStereo (juce::dsp::AudioBlock<float> block, Builtin&& builtin) noexcept
    {
        jassert (block.getNumChannels() == 2 && spec.numChannels == 2);
        if (previous == nullptr && retired.load() == nullptr)
            if (auto* next = pending.exchange (nullptr))
            {
                previous = active;
                active = next;
                fadePosition = 0;
                fadeLength = std::max (1, (int) (0.05 * spec.sampleRate));
            }
        if (previous && ! previous->convolution && ! active->convolution)
        {
            retired.store (previous);
            previous = nullptr;
        }
        const int n = (int) block.getNumSamples();
        if (previous)
            for (int channel = 0; channel < 2; ++channel)
                transition.copyFrom (channel, 0, block.getChannelPointer ((size_t) channel), n);
        renderStereo (active, block, builtin);
        if (previous)
        {
            auto old = juce::dsp::AudioBlock<float> (transition).getSubBlock (0, (size_t) n);
            renderStereo (previous, old, builtin);
            float* l = block.getChannelPointer (0);
            float* r = block.getChannelPointer (1);
            const float* oldL = transition.getReadPointer (0);
            const float* oldR = transition.getReadPointer (1);
            for (int i = 0; i < n; ++i)
            {
                const float mix = std::min (1.0f, (float) ++fadePosition / (float) fadeLength);
                l[i] = oldL[i] + mix * (l[i] - oldL[i]);
                r[i] = oldR[i] + mix * (r[i] - oldR[i]);
            }
            if (fadePosition >= fadeLength)
            {
                retired.store (previous);
                previous = nullptr;
            }
        }
    }

private:
    void run() override
    {
        while (! threadShouldExit())
        {
            delete retired.exchange (nullptr);
            wait (10); // control-side reclamation, never wakes or waits from audio
        }
    }
    struct Source { juce::AudioBuffer<float> data; double rate = 44100; juce::String path; };
    struct Engine { std::unique_ptr<juce::dsp::Convolution> convolution; };
    static std::unique_ptr<Engine> makeEngine (const Source* src, const juce::dsp::ProcessSpec& s)
    {
        auto result = std::make_unique<Engine>();
        if (src)
        {
            if ((double) src->data.getNumSamples() * s.sampleRate / src->rate > maxSamples)
                throw std::runtime_error ("Resampled IR exceeds sample budget");
            auto& c = result->convolution;
            c = std::make_unique<juce::dsp::Convolution>();
            juce::AudioBuffer<float> copy (src->data);
            c->loadImpulseResponse (std::move (copy), src->rate,
                                    src->data.getNumChannels() == 2 && s.numChannels == 2
                                        ? juce::dsp::Convolution::Stereo::yes
                                        : juce::dsp::Convolution::Stereo::no,
                                    juce::dsp::Convolution::Trim::yes, juce::dsp::Convolution::Normalise::yes);
            // JUCE 8.0.4 documents prepare() as synchronous completion of the
            // latest IR load. load/prepare here never overlap this instance's process.
            c->prepare (s);
        }
        return result;
    }
    template <typename Builtin>
    static void render (Engine* e, juce::dsp::AudioBlock<float> block, Builtin& builtin) noexcept
    {
        if (e && e->convolution)
        {
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            e->convolution->process (ctx);
        }
        else
            builtin (block.getChannelPointer (0), (int) block.getNumSamples());
    }

    template <typename Builtin>
    static void renderStereo (Engine* e, juce::dsp::AudioBlock<float> block, Builtin& builtin) noexcept
    {
        if (e && e->convolution)
        {
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            e->convolution->process (ctx);
        }
        else
            for (int channel = 0; channel < (int) block.getNumChannels(); ++channel)
                builtin (block.getChannelPointer ((size_t) channel), (int) block.getNumSamples(), channel);
    }

    bool fail (const juce::String& text) { error = text; return false; }
    void publish (std::unique_ptr<Engine> next)
    {
        delete retired.exchange (nullptr);
        delete pending.exchange (next.release()); // obsolete, never-rendered candidate
    }
    mutable std::mutex control;
    std::unique_ptr<Source> source;
    juce::String error;
    juce::dsp::ProcessSpec spec { 44100.0, 512, 1 };
    bool prepared = false;
    std::atomic<Engine*> pending { nullptr }, retired { nullptr };
    Engine* active = nullptr;   // audio-owned after prepare
    Engine* previous = nullptr; // retained for 50-ms transition
    juce::AudioBuffer<float> transition;
    int fadePosition = 0, fadeLength = 0;
};
