#include "../step3/WindowsCrashDiagnostics.h"
#include "HostedCPUBenchmark.h"
#include "Step3HostedQA.h"
#include <cmath>
#include <iostream>
#include <juce_audio_utils/juce_audio_utils.h>

static int runHost (int argc, char **argv)
{
    std::cout << "HOST_STAGE enter argc=" << argc << std::endl;
    const bool heapOnly = argc == 2 && juce::String (argv[1]) == "step3-heap-selftest";
    const bool step3 = argc == 3 && juce::String (argv[2]) == "step3";
    std::cout << "HOST_STAGE install heap=" << (step3 || heapOnly) << std::endl;
    step3Heap::Installation allocationHooks (step3 || heapOnly);
#if defined(_WIN32) && defined(STEP3_DETOURS_AVAILABLE)
    if (heapOnly)
    {
        const bool ok = step3Heap::Installation::selfTest ();
        std::cout << (ok ? "PASS" : "FAIL") << " standalone process-heap positive self-test" << std::endl;
        return ok ? 0 : 1;
    }
#else
    if (heapOnly)
    {
        std::cout << "NOT RUN Windows process-heap self-test" << std::endl;
        return 2;
    }
#endif
    std::cout << "HOST_STAGE initialize JUCE" << std::endl;
    juce::ScopedJuceInitialiser_GUI init;
    if (argc != 2 && argc != 3 && argc != 4)
    {
        std::cerr << "Usage: compact_vst3_host /absolute/plugin.vst3 [cpu]\n";
        return 2;
    }
    juce::VST3PluginFormat format;
    std::cout << "HOST_STAGE discover" << std::endl;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile (descriptions, juce::String (argv[1]));
    std::cout << "HOST_STAGE discovered=" << descriptions.size () << std::endl;
    if (descriptions.size () != 1)
    {
        std::cerr << "FAIL expected one discovered VST3 effect\n";
        return 1;
    }
    if (argc == 4)
    {
        if (juce::String (argv[2]) != "step3-cpu")
            return 2;
        return step3Hosted::cpu (format, *descriptions[0], argv[3]);
    }
    if (argc == 3 && juce::String (argv[2]) == "step3-cpu-setup")
        return step3Hosted::cpuSetup (format, *descriptions[0]);
    if (argc == 3 && !step3)
    {
        if (juce::String (argv[2]) != "cpu")
            return 2;
        return hostedCPU (format, *descriptions[0]);
    }
    juce::String error;
    auto make = [&] { return format.createInstanceFromDescription (*descriptions[0], 48000, 64, error); };
    std::cout << "HOST_STAGE instantiate legacy contract" << std::endl;
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
    // Isolate amp duplication: the preceding stereo run leaves different L/R
    // cabinet/cut histories (float IIR rounding can persist). MONO duplicates
    // BEFORE cabinet/cuts; it does not promise equal post-stereo-cab output.
    // Keep the preceding enabled-cab/cut wrapper/state/bypass checks intact.
    for (auto *parameter : active->getParameters ())
        if (parameter->getName (128) == "Cab On" || parameter->getName (128) == "Low Cut On" ||
            parameter->getName (128) == "High Cut On")
            parameter->setValueNotifyingHost (0.f);
    // Exercise new controls through the real VST3 controller, after prepare.
    const int fixedLatency = active->getLatencySamples ();
    for (int quality = 0; quality < 3; ++quality)
    {
        int found = 0;
        for (auto *parameter : active->getParameters ())
        {
            if (parameter->getName (128) == "AMP MODE")
            {
                parameter->setValueNotifyingHost (0.f);
                ++found;
            }
            if (parameter->getName (128) == "QUALITY")
            {
                parameter->setValueNotifyingHost (quality * .5f);
                ++found;
            }
        }
        double difference = 0, energy = 0;
        for (int k = 0; k < 210; ++k)
        {
            a.clear ();
            for (int i = 0; i < 64; ++i)
                a.setSample (1, i, .1f * std::sin ((k * 64 + i) * .031f));
            active->processBlock (a, midi);
            if (k > 180)
                for (int i = 0; i < 64; ++i)
                {
                    difference =
                        std::max (difference, (double)std::abs (a.getSample (0, i) - a.getSample (1, i)));
                    energy += (double)a.getSample (0, i) * a.getSample (0, i);
                }
        }
        bool modeState = false, qualityState = false;
        if (auto scene = hostedComponentState (*active))
            for (auto *child : scene->getChildWithTagNameIterator ("PARAM"))
            {
                if (child->getStringAttribute ("id") == "ampMode")
                    modeState = child->getDoubleAttribute ("value") == 0.;
                if (child->getStringAttribute ("id") == "quality")
                    qualityState = child->getDoubleAttribute ("value") == quality;
            }
        const bool modePass = found == 2 && modeState && qualityState && difference < 1e-6 && energy > 1e-8 &&
                              active->getLatencySamples () == fixedLatency;
        std::cout << (modePass ? "PASS" : "FAIL")
                  << " VST3 live MONO/R-only/quality/state/fixed latency quality=" << quality
                  << " stereo_difference=" << difference << " energy=" << energy << '\n';
        pass = pass && modePass;
    }
    for (auto *p : {active.get (), silent.get (), restored.get ()})
        p->releaseResources ();
    if (step3)
        pass = step3Hosted::run (format, *descriptions[0]) && pass;
    return pass ? 0 : 1;
}

int main (int argc, char **argv)
{
    step3Crash::install ();
    try
    {
        return runHost (argc, argv);
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL test host exception: " << e.what () << std::endl;
    }
    catch (...)
    {
        std::cerr << "FAIL unknown test host exception" << std::endl;
    }
    return 2;
}
