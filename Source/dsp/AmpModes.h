#pragma once
#include "AmpSim.h"
#include "MultibandGate.h"
#include <array>
#include <stdexcept>

namespace vamp
{
// Preallocated nonlinear paths. Quality refers to a 44.1/48-kHz host:
// ECO 2x, STANDARD 4x, HIGH original adaptive 8x/8x/4x at 44.1/48/96k.
// Lower modes target 88.2/176.4 kHz internally, so at 96k they use 1x/2x.
// Every wet path is padded to HIGH's reported integer latency. No live latency
// change, allocation, preparation, thread handoff or host notification is needed.
class AmpModes
{
public:
    static int factorFor (double fs, int quality) noexcept
    {
        quality = std::clamp (quality, 0, 2);
        int stages = quality == 2 ? 1 : 0;
        const double target = quality == 2 ? 352800.0 : quality == 1 ? 176400.0 : 88200.0;
        const int cap = quality + 1;
        while (stages < cap && fs * (1 << stages) < target) ++stages;
        return 1 << stages;
    }

    void prepare (double fs, int maxBlock, int inputs, int mode, int quality)
    {
        for (int q = 0; q < 3; ++q)
            for (int lane = 0; lane < 3; ++lane)
            {
                amps[q][lane].prepare (fs, maxBlock, factorFor (fs, q));
                gates[q][lane].prepare (fs);
            }
        latency = amps[2][0].getLatencySamples();
        for (int q = 0; q < 3; ++q)
            for (int lane = 0; lane < 3; ++lane)
            {
                const int amount = latency - amps[q][lane].getLatencySamples();
                if (amount < 0) throw std::runtime_error ("Quality latency exceeds HIGH");
                delays[q][lane].prepare (amount);
            }
        oldOutput.setSize (2, maxBlock);
        newOutput.setSize (2, maxBlock);
        warmFrames = fadeFrames = std::max (1, (int) (0.05 * fs));
        active = config (inputs, mode, quality);
        incoming = -1;
        warmLeft = fadePosition = 0;
        gateEnabled = false;
    }

    int getLatencySamples() const noexcept { return latency; }
    int getActiveQuality() const noexcept { return active / 2; }
    int getActiveLanes() const noexcept { return (active & 1) ? 2 : 1; }
    int getActiveFactor() const noexcept { return amps[active / 2][0].getOversamplingFactor(); }
    bool transitioning() const noexcept { return incoming >= 0; }

    void process (juce::dsp::AudioBlock<float> block, const AmpSim::Settings& settings,
                  int inputs, int mode, int quality, bool gateOn, float threshold, float release) noexcept
    {
        if (gateOn && ! gateEnabled)
            for (auto& group : gates) for (auto& gate : group) gate.reset();
        gateEnabled = gateOn;
        const int wanted = config (inputs, mode, quality); // latest request; don't restart a running fade
        if (incoming < 0 && wanted != active)
        {
            incoming = wanted;
            warmLeft = warmFrames;
            fadePosition = 0;
        }
        const int n = (int) block.getNumSamples();
        render (active, block, oldOutput, settings, inputs, threshold, release);
        if (incoming >= 0) render (incoming, block, newOutput, settings, inputs, threshold, release);
        for (int i = 0; i < n; ++i)
        {
            float mix = 0;
            if (incoming >= 0)
            {
                if (warmLeft > 0) --warmLeft;
                else mix = std::min (1.f, (float) ++fadePosition / (float) fadeFrames);
            }
            for (int channel = 0; channel < 2; ++channel)
            {
                const float old = oldOutput.getSample (channel, i);
                block.getChannelPointer ((size_t) channel)[i] = mix == 0 ? old :
                    mix == 1 ? newOutput.getSample (channel, i) : old + mix * (newOutput.getSample (channel, i) - old);
            }
        }
        if (incoming >= 0 && fadePosition >= fadeFrames)
        {
            active = incoming;
            incoming = -1;
        }
    }

private:
    struct Delay
    {
        std::vector<float> ring;
        int position = 0, length = 0;
        void prepare (int samples) { length = samples; ring.assign ((size_t) samples + 1, 0.f); position = 0; }
        void process (float* data, int count) noexcept
        {
            if (length == 0) return;
            for (int i = 0; i < count; ++i)
            {
                ring[(size_t) position] = data[i];
                position = (position + 1) % (length + 1);
                data[i] = ring[(size_t) position];
            }
        }
    };
    static int config (int inputs, int mode, int quality) noexcept
    { return 2 * std::clamp (quality, 0, 2) + (inputs >= 2 && mode != 0 ? 1 : 0); }

    void render (int selected, juce::dsp::AudioBlock<float> input, juce::AudioBuffer<float>& output,
                 const AmpSim::Settings& settings, int inputs, float threshold, float release) noexcept
    {
        const int q = selected / 2, count = (int) input.getNumSamples();
        if ((selected & 1) == 0)
        {
            float* mono = output.getWritePointer (0);
            for (int i = 0; i < count; ++i)
                mono[i] = inputs == 1 ? input.getChannelPointer (0)[i] :
                    .5f * input.getChannelPointer (0)[i] + .5f * input.getChannelPointer (1)[i];
            if (gateEnabled) gates[q][2].process (mono, count, threshold, release);
            amps[q][2].process (mono, count, settings);
            delays[q][2].process (mono, count);
            output.copyFrom (1, 0, output, 0, 0, count);
        }
        else
            for (int lane = 0; lane < 2; ++lane)
            {
                output.copyFrom (lane, 0, input.getChannelPointer ((size_t) lane), count);
                float* data = output.getWritePointer (lane);
                if (gateEnabled) gates[q][lane].process (data, count, threshold, release);
                amps[q][lane].process (data, count, settings);
                delays[q][lane].process (data, count);
            }
    }
    std::array<std::array<AmpSim, 3>, 3> amps;
    std::array<std::array<InputGate, 3>, 3> gates;
    std::array<std::array<Delay, 3>, 3> delays;
    juce::AudioBuffer<float> oldOutput, newOutput;
    int active = 5, incoming = -1, latency = 0, warmFrames = 1, fadeFrames = 1, warmLeft = 0, fadePosition = 0;
    bool gateEnabled = false;
};
}
