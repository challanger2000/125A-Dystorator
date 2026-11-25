#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Filters.h"
#include "Amp5150.h"
#include "Preamp5150.h"

namespace vamp
{
// Signal levels inside the amp are real volts.
constexpr double kInputVolts = 1.0;          // 0 dBFS input = 1 V peak at the guitar jack
constexpr double kSpeakerToDigital = 1.0 / 100.0; // speaker volts -> digital (max ~56 V -> ~0.56)

// Per-channel calibration of the post-gain pot (the real amp has separate Clean / Lead Post pots).
// Measured with tests/dsp_test ("chain"): CLEAN needs far more post gain than BLUE/RED. 54 since the factory rhythm
// network (R21 1M ahead of the R23 / C63 shunt), 6.7 dB more attenuation than before: CLEAN as loud as in v0.5.0-beta.
constexpr double kChannelPost[3] = { 54.0, 1.0, 1.0 };

class AmpSim
{
public:
    struct Settings
    {
        int channel = 2;
        bool bright = true;    // BRIGHT pushbutton (rhythm: CLEAN and BLUE)
        bool crunch = true;    // CRUNCH pushbutton (BLUE)
        bool noise = false;    // circuit noise of the preamp (resistors, tubes)
        float gain = 0.6f, bass = 0.5f, mid = 0.5f, treble = 0.6f;
        float presence = 0.5f, resonance = 0.5f, master = 0.3f; // all 0..1
        float volumeDb = 0.0f; // post-power-amp level (like an attenuator), does not change the tone
    };

    void prepare (double sampleRate, int maxBlock, int requestedFactor = 0)
    {
        // Oversampling around the whole non-linear amp (preamp + power amp), to at least kMinOsRate:
        // 8x at 44.1 / 48 kHz, 4x at 88.2 / 96 kHz, 2x at 176.4 / 192 kHz. Aliasing (inharmonic energy for
        // heavily clipped 1-5 kHz sines, tests/amp_path_test): 4x -50..-55 dB, 8x -61..-70 dB, ~1.7x the CPU.
        int stages = 1;
        while (sampleRate * (1 << stages) < kMinOsRate && stages < 3)
            ++stages;
        if (requestedFactor != 0)
        {
            jassert (requestedFactor == 1 || requestedFactor == 2 || requestedFactor == 4 || requestedFactor == 8);
            stages = requestedFactor == 8 ? 3 : requestedFactor == 4 ? 2 : requestedFactor == 2 ? 1 : 0;
        }
        oversamplingFactor = 1 << stages;
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            1, (size_t) stages, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        oversampler->initProcessing ((size_t) maxBlock);
        osRate = sampleRate * (1 << stages);

        preamp.setChannelTrims (kChannelPost[0], kChannelPost[1], kChannelPost[2]); // applied inside the switch crossfade
        // STOCK only: settle with nominal tubes
        const auto tubes = TubeSet::make (0);
        preamp.setTubeSet (tubes);
        power.setTubeSet (tubes);
        preamp.prepare (osRate);
        power.prepare (osRate);
        // settle preamp and power amp together on the shared supply, so the rails start at the loaded DC point
        // (as PowerAmp5150::prepare: rails held, load currents averaged once each update's transient has died out)
        auto settle = [this] (double seconds) { for (int i = 0; i < (int) (osRate * seconds); ++i) processAmp5150 (preamp, power, 0.0, 1.0); };
        power.beginSupplySettle();
        for (int pass = 0; pass < 3; ++pass)
        {
            settle (0.1);
            power.resetSupplyAverage();
            settle (0.1);
            power.applySupplyDC();
        }
        settle (0.15);
        power.endSupplySettle();
        volume.reset (osRate, 0.03);
        dcBlock.reset();
        oversampler->reset();
        knobsSet = false;
        firstBlock = true;
        knobCoef = 1.0 - std::exp (-kControlInterval / (kKnobSmoothMs * 1e-3 * osRate));
    }

    // Knob smoothing time constant (ms); 0 = no one-pole, each knob only ramps linearly across the host block (tests)
    void setKnobSmoothing (double ms)
    {
        knobCoef = ms <= 0.0 ? 1.0 : 1.0 - std::exp (-kControlInterval / (ms * 1e-3 * osRate));
    }

    int getLatencySamples() const
    {
        return oversampler ? juce::roundToInt (oversampler->getLatencyInSamples()) : 0;
    }

    int getOversamplingFactor() const noexcept { return oversamplingFactor; }

    void process (float* data, int numSamples, const Settings& s)
    {
        const int channel = juce::jlimit (0, 2, s.channel);
        preamp.setCrunch (s.crunch); // before the channel, so the first block starts on the right circuit
        if (firstBlock) preamp.setModeNow (channel); // nothing audible yet: no switch crossfade
        else preamp.setMode (channel);
        preamp.setBright (s.bright); // no-op unless it changed
        preamp.setNoise (s.noise);   // likewise
        firstBlock = false;
        const double target[kKnobs] = { s.gain, s.bass, s.mid, s.treble, s.master, s.presence, s.resonance };
        std::copy (knobTarget, knobTarget + kKnobs, knobFrom); // the last block's values: the target ramps from there
        std::copy (target, target + kKnobs, knobTarget);
        if (! knobsSet)
        {
            std::copy (target, target + kKnobs, knob);
            std::copy (target, target + kKnobs, knobFrom);
            knobsSet = true;
            applyKnobs (true);
        }
        dcBlock.set (10.0, osRate);


        volume.setTargetValue (std::pow (10.0, s.volumeDb / 20.0) * kSpeakerToDigital);

        float* chans[] = { data };
        juce::dsp::AudioBlock<float> block (chans, 1, (size_t) numSamples);
        auto up = oversampler->processSamplesUp (block);
        float* x = up.getChannelPointer (0);
        const int n = (int) up.getNumSamples();

        for (int i = 0; i < n; ++i)
        {
            if (++controlPhase >= kControlInterval)
            {
                controlPhase = 0;
                stepKnobs ((double) (i + 1) / (double) n);
            }
            double v = (double) x[i] * kInputVolts;

            v = processAmp5150 (preamp, power, v, 1.0); // volts across the speaker (preamp on the power amp's rails; channel trim in the preamp)
            x[i] = dcBlock.process ((float) (v * volume.getNextValue()));
        }

        oversampler->processSamplesDown (block);
    }

private:
    // Knob smoothing. The knobs reach process() once per host block; applied as one jump, every block boundary
    // is a step in the circuit (zipper noise / clicks while a knob moves, coarser with larger buffers). Each knob
    // follows its target with a one-pole (kKnobSmoothMs) instead, and the circuit is updated every
    // kControlInterval oversampled samples (~0.1 ms) while one is still moving. The target itself ramps linearly
    // across the block from the last block's value: a one-pole on the raw per-block staircase still moves in
    // block-rate jerks (Resonance swept with 512-sample blocks: -47 dB of sidebands vs -59 dB with a smooth target).
    // Gain and Presence / Resonance sit inside DK circuits (lead preamp, power amp): their matrices are rebuilt then
    // (DKCircuit: allocation-free), so they follow every kHeavyEvery-th step only (~0.36 ms: still ~55 steps per
    // 20 ms ramp, a quarter of the CPU).
    static constexpr int kKnobs = 7, kControlInterval = 32, kHeavyEvery = 4;
    static constexpr double kKnobSmoothMs = 20.0;
    static constexpr int kHeavy[3] = { 0, 5, 6 }; // Gain, Presence, Resonance

    // frac: position in the current host block (0..1], where the target is between knobFrom and knobTarget
    void stepKnobs (double frac)
    {
        bool moving = false;
        for (int k = 0; k < kKnobs; ++k)
        {
            const double t = knobFrom[k] + frac * (knobTarget[k] - knobFrom[k]), d = t - knob[k];
            if (d == 0.0) continue;
            knob[k] = std::abs (d) < 1e-5 ? t : knob[k] + knobCoef * d;
            moving = true;
        }
        const bool heavyStep = ++heavyPhase >= kHeavyEvery;
        if (heavyStep) heavyPhase = 0;
        bool heavyPending = false;
        for (int k : kHeavy) heavyPending = heavyPending || heavyApplied[k == 0 ? 0 : k - 4] != knob[k];
        if (moving || (heavyStep && heavyPending))
            applyKnobs (heavyStep);
    }

    void applyKnobs (bool heavy = true)
    {
        if (heavy)
            for (int k : kHeavy) heavyApplied[k == 0 ? 0 : k - 4] = knob[k];
        preamp.setControls (heavyApplied[0], knob[1], knob[2], knob[3], knob[4]);
        power.setControls (heavyApplied[1], heavyApplied[2]);
    }

    double knob[kKnobs] {}, knobTarget[kKnobs] {}, knobFrom[kKnobs] {}, knobCoef = 1.0;
    double heavyApplied[3] {}; // Gain, Presence, Resonance as last applied
    int heavyPhase = 0;
    bool knobsSet = false, firstBlock = true;
    int controlPhase = 0;

    static constexpr double kMinOsRate = 352800.0; // 8 x 44.1 kHz
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    double osRate = 176400.0;
    int oversamplingFactor = 1;

    Preamp5150 preamp;
    PowerAmp5150 power;
    OnePoleHP dcBlock;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> volume { kSpeakerToDigital };
};

} // namespace vamp
