#include "PluginProcessor.h"
#include <iostream>
void irTestCaptureCabinetInput (const float *, int) {}
void irTestConvolutionCall (const void *, int) {}
static void set (VibeAmpProcessor &p, const char *id, float value)
{
    auto *q = p.apvts.getParameter (id);
    q->setValueNotifyingHost (q->convertTo0to1 (value));
}
int main ()
{
    juce::ScopedJuceInitialiser_GUI init;
    int failed = 0;
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getNonexistentChildFile ("125a-stereo-oracle", "", false);
    dir.createDirectory ();
    for (double fs : {44100., 48000., 96000.})
    for (int quality : {2, 0, 1})
    {
        VibeAmpProcessor p (dir);
        auto layout = p.getBusesLayout ();
        layout.inputBuses.set (0, juce::AudioChannelSet::stereo ());
        p.setBusesLayout (layout);
        set (p, "ampMode", 1);
        set (p, "quality", (float) quality);
        set (p, "cabOn", 0);
        set (p, "ampNoise", 0);
        set (p, "inGateOn", 0);
        set (p, "ampGain", 9);
        set (p, "master", 3);
        set (p, "boostOn", 1);
        set (p, "boostDrive", 8);
        p.prepareToPlay (fs, 128);
        vamp::AmpSim ref[2];
        for (auto& amp : ref)
            amp.prepare (fs, 128, vamp::AmpModes::factorFor (fs, quality));
        const int padding = p.getLatencySamples () - ref[0].getLatencySamples ();
        std::vector<float> delay[2];
        for (auto& ring : delay) ring.assign ((size_t) std::max (1, padding), 0.f);
        int position = 0;
        vamp::AmpSim::Settings settings;
        settings.gain = p.apvts.getRawParameterValue ("ampGain")->load () * .1f;
        settings.master = p.apvts.getRawParameterValue ("master")->load () * .1f;
        settings.boostOn = true;
        settings.boostDrive = .8f;
        juce::dsp::Gain<float> in, out;
        for (auto *g : {&in, &out})
        {
            g->prepare ({fs, 128, 2});
            g->setRampDurationSeconds (.05);
            g->setGainDecibels (0);
        }
        juce::AudioBuffer<float> actual (2, 128), expected (2, 128);
        juce::MidiBuffer midi;
        double error = 0, energy[2] = {};
        for (int k = 0; k < 90; ++k)
        {
            actual.clear ();
            for (int i = 0; i < 128; ++i)
                actual.setSample (1, i,
                                  (float)(.15 * std::sin (2 * vamp::kPi * 277 * (k * 128 + i) / fs + .3)));
            // First half is R-only; second half has genuinely distinct L/R.
            // Zero-input nonlinear circuits can still relax from their bias.
            // Compare that history with an independent zero-driven circuit,
            // rather than asserting mathematically exact zero AC energy.
            if (k >= 45)
                for (int i = 0; i < 128; ++i)
                    actual.setSample (0, i, (float)(.12 * std::sin (2 * vamp::kPi * 110 * (k * 128 + i) / fs)));
            expected.makeCopyOf (actual, true);
            p.processBlock (actual, midi);
            auto block = juce::dsp::AudioBlock<float> (expected);
            juce::dsp::ProcessContextReplacing<float> context (block);
            in.process (context);
            for (int c = 0; c < 2; ++c)
                ref[c].process (expected.getWritePointer (c), 128, settings);
            for (int i = 0; i < 128 && padding > 0; ++i)
            {
                for (int c = 0; c < 2; ++c)
                {
                    const float previous = delay[c][(size_t) position];
                    delay[c][(size_t) position] = expected.getSample (c, i);
                    expected.setSample (c, i, previous);
                }
                position = (position + 1) % padding;
            }
            out.process (context);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 128; ++i)
                {
                    error = std::max (error,
                                      (double)std::abs (actual.getSample (c, i) - expected.getSample (c, i)));
                    energy[c] += (double)expected.getSample (c, i) * expected.getSample (c, i);
                }
        }
        bool ok = error < 1e-6 && energy[0] > 1e-8 && energy[1] > 1e-8;
        failed += !ok;
        std::cout << (ok ? "PASS" : "FAIL")
                  << " STEREO R-only then distinct L/R vs independent circuits and integer padding, fs=" << fs
                  << " quality=" << quality
                  << " max_delta=" << error << " reference_L_energy=" << energy[0] << " reference_R_energy=" << energy[1] << '\n';
    }
    dir.deleteRecursively ();
    return failed ? 1 : 0;
}
