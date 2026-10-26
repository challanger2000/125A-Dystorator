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
                   .getNonexistentChildFile ("125a-mono-oracle", "", false);
    dir.createDirectory ();
    for (double fs : {44100., 48000., 96000.})
    {
        VibeAmpProcessor p (dir);
        auto layout = p.getBusesLayout ();
        layout.inputBuses.set (0, juce::AudioChannelSet::stereo ());
        p.setBusesLayout (layout);
        set (p, "ampMode", 0);
        set (p, "quality", 2);
        set (p, "cabOn", 0);
        set (p, "ampNoise", 0);
        set (p, "inGateOn", 0);
        set (p, "ampGain", 9);
        set (p, "master", 3);
        set (p, "boostOn", 1);
        set (p, "boostDrive", 8);
        p.prepareToPlay (fs, 128);
        vamp::AmpSim ref;
        ref.prepare (fs, 128);
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
        double error = 0, energy = 0;
        for (int k = 0; k < 90; ++k)
        {
            actual.clear ();
            for (int i = 0; i < 128; ++i)
                actual.setSample (1, i,
                                  (float)(.15 * std::sin (2 * vamp::kPi * 277 * (k * 128 + i) / fs + .3)));
            expected.makeCopyOf (actual, true);
            p.processBlock (actual, midi);
            auto block = juce::dsp::AudioBlock<float> (expected);
            juce::dsp::ProcessContextReplacing<float> context (block);
            in.process (context);
            for (int i = 0; i < 128; ++i)
                expected.setSample (0, i, .5f * expected.getSample (0, i) + .5f * expected.getSample (1, i));
            ref.process (expected.getWritePointer (0), 128, settings);
            expected.copyFrom (1, 0, expected, 0, 0, 128);
            out.process (context);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 128; ++i)
                {
                    error = std::max (error,
                                      (double)std::abs (actual.getSample (c, i) - expected.getSample (c, i)));
                    energy += (double)expected.getSample (c, i) * expected.getSample (c, i);
                }
        }
        bool ok = error < 1e-6 && energy > 1e-8;
        failed += !ok;
        std::cout << (ok ? "PASS" : "FAIL")
                  << " MONO R-only vs independent average -> original HIGH AmpSim -> duplicate, fs=" << fs
                  << " max_delta=" << error << " reference_energy=" << energy << '\n';
    }
    dir.deleteRecursively ();
    return failed ? 1 : 0;
}
