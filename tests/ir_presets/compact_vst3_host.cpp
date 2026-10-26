#include "HostedCPUBenchmark.h"
#include <cmath>
#include <iostream>
#include <juce_audio_utils/juce_audio_utils.h>

int main (int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc != 2 && argc != 3)
    {
        std::cerr << "Usage: compact_vst3_host /absolute/plugin.vst3 [cpu]\n";
        return 2;
    }
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile (descriptions, juce::String (argv[1]));
    if (descriptions.size () != 1)
    {
        std::cerr << "FAIL expected one discovered VST3 effect\n";
        return 1;
    }
    if (argc == 3)
    {
        if (juce::String (argv[2]) != "cpu")
            return 2;
        return hostedCPU (format, *descriptions[0]);
    }
    juce::String error;
    auto make = [&] { return format.createInstanceFromDescription (*descriptions[0], 48000, 64, error); };
    auto active = make (), silent = make (), restored = make ();
    if (!active || !silent || !restored)
    {
        std::cerr << "FAIL VST3 instantiate: " << error << '\n';
        return 1;
    }
    for (auto *p : {active.get (), silent.get (), restored.get ()})
    {
        auto layout = p->getBusesLayout ();
        layout.inputBuses.set (0, juce::AudioChannelSet::stereo ());
        layout.outputBuses.set (0, juce::AudioChannelSet::stereo ());
        if (!p->setBusesLayout (layout))
        {
            std::cerr << "FAIL VST3 stereo bus negotiation\n";
            return 1;
        }
    }
    // A default-to-default recall could pass with a broken setState. Use
    // non-default audible controls through the actual hosted parameter API.
    for (auto *p : {active.get (), silent.get ()})
    {
        for (const auto &setting : std::vector<std::pair<const char *, float>>{
                 {"Gain", .3f}, {"Master", .2f}, {"Low Cut On", 1}, {"High Cut On", 1}})
        {
            bool found = false;
            for (auto *parameter : p->getParameters ())
                if (parameter->getName (128) == setting.first)
                {
                    parameter->setValueNotifyingHost (setting.second);
                    found = true;
                    break;
                }
            if (!found)
            {
                std::cerr << "FAIL non-default state setup\n";
                return 1;
            }
        }
    }
    // Restore the wrapper's own component/controller state before prepare.
    juce::MemoryBlock state;
    active->getStateInformation (state);
    restored->setStateInformation (state.getData (), (int)state.getSize ());
    for (auto *p : {active.get (), silent.get (), restored.get ()})
        p->prepareToPlay (48000, 64);
    for (auto *p : {active.get (), restored.get ()})
    {
        const auto scene = hostedComponentState (*p);
        bool gain = false, master = false, low = false, high = false;
        if (scene)
            for (auto *child : scene->getChildWithTagNameIterator ("PARAM"))
            {
                const auto id = child->getStringAttribute ("id");
                const double v = child->getDoubleAttribute ("value");
                if (id == "ampGain")
                    gain = std::abs (v - 3.) < 1e-5;
                if (id == "master")
                    master = std::abs (v - 2.) < 1e-5;
                if (id == "lowCutOn")
                    low = v == 1.;
                if (id == "highCutOn")
                    high = v == 1.;
            }
        if (!(gain && master && low && high))
        {
            std::cerr << "FAIL actual component non-default state after prepare\n";
            return 1;
        }
    }
    std::cout << "PASS actual VST3 non-default component state survives save/restore before prepare\n";
    double audible = 0, bleed = 0, recall = 0;
    bool finite = true;
    juce::AudioBuffer<float> a (2, 64), b (2, 64), c (2, 64);
    juce::MidiBuffer midi;
    for (int k = 0; k < 90; ++k)
    {
        a.clear ();
        b.clear ();
        for (int i = 0; i < 64; ++i)
            a.setSample (1, i, .1f * std::sin ((k * 64 + i) * .031f));
        c.makeCopyOf (a, true);
        active->processBlock (a, midi);
        silent->processBlock (b, midi);
        restored->processBlock (c, midi);
        for (int i = 0; i < 64; ++i)
        {
            const double r = a.getSample (1, i) - b.getSample (1, i),
                         l = a.getSample (0, i) - b.getSample (0, i);
            audible += r * r;
            bleed += l * l;
            for (int channel = 0; channel < 2; ++channel)
            {
                finite = finite && std::isfinite (a.getSample (channel, i));
                recall =
                    std::max (recall, (double)std::abs (a.getSample (channel, i) - c.getSample (channel, i)));
            }
        }
    }
    bool pass =
        finite && audible > 1e-8 && bleed < 1e-12 && recall < 1e-6 && active->getLatencySamples () > 0;
    std::cout << (pass ? "PASS" : "FAIL")
              << " Linux packaged VST3 discover/instantiate/stereo R-only/no-crosstalk/state/audio"
              << " audible=" << audible << " bleed=" << bleed << " recall_max=" << recall
              << " latency=" << active->getLatencySamples ()
              << " parameters=" << active->getParameters ().size () << '\n';
    auto *bypass = active->getBypassParameter ();
    double dryError = 0, warmError = 0;
    if (bypass)
    {
        bypass->setValueNotifyingHost (1.f);
        for (int k = 90; k < 130; ++k)
        {
            a.clear ();
            for (int i = 0; i < 64; ++i)
                a.setSample (1, i, .1f * std::sin ((k * 64 + i) * .031f));
            c.makeCopyOf (a, true);
            active->processBlock (a, midi);
            restored->processBlock (c, midi);
            if (k >= 100 && k < 120)
                for (int i = 0; i < 64; ++i)
                {
                    const float dry = .1f * std::sin ((k * 64 + i - active->getLatencySamples ()) * .031f);
                    dryError = std::max (dryError, (double)std::abs (a.getSample (1, i) - dry));
                    dryError = std::max (dryError, (double)std::abs (a.getSample (0, i)));
                }
            if (k == 119)
                bypass->setValueNotifyingHost (0.f);
            if (k >= 127)
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 64; ++i)
                        warmError = std::max (warmError,
                                              (double)std::abs (a.getSample (ch, i) - c.getSample (ch, i)));
        }
    }
    const bool bypassPass = bypass && dryError < 1e-6 && warmError < 1e-6;
    std::cout << (bypassPass ? "PASS" : "FAIL")
              << " VST3 host bypass parameter -> actual delayed dry/warm wet"
              << " dry_max=" << dryError << " warm_max=" << warmError << '\n';
    pass = pass && bypassPass;
    for (auto *p : {active.get (), silent.get (), restored.get ()})
        p->releaseResources ();
    return pass ? 0 : 1;
}
