#pragma once
#include <chrono>
#include <cmath>
#include <iostream>
#include <juce_audio_utils/juce_audio_utils.h>
#include <numeric>
#include <thread>

// Same fixture, timing window and deadline as compactCPU, but through the
// actual built VST3's host/processor boundary (including its shipping LTO flags).
static std::unique_ptr<juce::XmlElement> hostedComponentState (juce::AudioProcessor &p)
{
    juce::MemoryBlock host;
    p.getStateInformation (host);
    auto xml = juce::AudioProcessor::getXmlFromBinary (host.getData (), (int)host.getSize ());
    auto *component = xml ? xml->getChildByName ("IComponent") : nullptr;
    juce::MemoryBlock bytes;
    if (!component || !bytes.fromBase64Encoding (component->getAllSubText ()))
        return {};
    return juce::AudioProcessor::getXmlFromBinary (bytes.getData (), (int)bytes.getSize ());
}

static int hostedCPU (juce::VST3PluginFormat &format, const juce::PluginDescription &description)
{
    int failedCases = 0;
    for (double fs : {44100., 48000.})
        for (int inputs : {1, 2})
        {
            juce::String error;
            auto p = format.createInstanceFromDescription (description, fs, 512, error);
            if (!p)
            {
                std::cerr << "FAIL instantiate: " << error << '\n';
                return 2;
            }
            auto layout = p->getBusesLayout ();
            layout.inputBuses.set (0, inputs == 1 ? juce::AudioChannelSet::mono ()
                                                  : juce::AudioChannelSet::stereo ());
            layout.outputBuses.set (0, juce::AudioChannelSet::stereo ());
            if (!p->setBusesLayout (layout))
            {
                std::cerr << "FAIL CPU bus layout\n";
                return 2;
            }
            // Normalized values for the unchanged parameter definitions. Every
            // named parameter must exist; no silently defaulted benchmark setup.
            const std::vector<std::pair<const char *, float>> settings = {
                {"Amp Noise", 0},   {"Input Gate On", 0}, {"Cab On", 1},       {"Low Cut On", 1},
                {"High Cut On", 1}, {"Boost On", 0},      {"Channel", 1},      {"Gain", .5f},
                {"Bass", .5f},      {"Mid", .5f},         {"Treble", .6f},     {"Master", .3f},
                {"Presence", .5f},  {"Resonance", .5f},   {"Amp Volume", .5f}, {"Input", .5f},
                {"Output", .5f}};
            p->prepareToPlay (fs, 512);
            for (const auto &setting : settings)
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
                    std::cerr << "FAIL missing CPU parameter " << setting.first << '\n';
                    return 2;
                }
            }
            for (int frames : {32, 64, 128, 256, 512})
                for (bool paced : {false, true})
                {
                    juce::AudioBuffer<float> audio (2, frames);
                    juce::MidiBuffer midi;
                    std::vector<double> times;
                    times.reserve (160);
                    int overruns = 0, late = 0;
                    double energy = 0;
                    const double deadline = 1e6 * frames / fs;
                    auto next = std::chrono::steady_clock::now ();
                    for (int k = 0; k < 184; ++k)
                    {
                        for (int c = 0; c < 2; ++c)
                            for (int i = 0; i < frames; ++i)
                            {
                                const int channel = c < inputs ? c : 0;
                                const double t = (k * frames + i) / fs;
                                const float input =
                                    (float)(.12 * std::sin (2 * juce::MathConstants<double>::pi *
                                                                (channel ? 277 : 110) * t +
                                                            .3) +
                                            .025 * std::sin (2 * juce::MathConstants<double>::pi *
                                                             (channel ? 1723 : 3731) * t));
                                audio.setSample (c, i, input);
                            }
                        if (paced && k >= 24)
                            std::this_thread::sleep_until (next);
                        const auto begin = std::chrono::steady_clock::now ();
                        p->processBlock (audio, midi);
                        const auto end = std::chrono::steady_clock::now ();
                        if (k >= 24)
                        {
                            const double elapsed =
                                std::chrono::duration<double, std::micro> (end - begin).count ();
                            times.push_back (elapsed);
                            overruns += elapsed > deadline;
                            if (paced)
                                late += end > next + std::chrono::duration<double> (frames / fs);
                            for (int c = 0; c < 2; ++c)
                                for (int i = 0; i < frames; ++i)
                                    energy += (double)audio.getSample (c, i) * audio.getSample (c, i);
                        }
                        if (k < 24)
                            next = end;
                        else
                            next += std::chrono::duration_cast<std::chrono::steady_clock::duration> (
                                std::chrono::duration<double> (frames / fs));
                        if (frames == 32 && !paced && k == 23)
                        {
                            auto state = hostedComponentState (*p);
                            if (!state || !state->hasTagName ("PARAMS"))
                            {
                                std::cerr << "FAIL cannot inspect actual component settings\n";
                                return 2;
                            }
                            const std::vector<std::pair<const char *, double>> actualExpected = {
                                {"ampGain", 5},   {"master", 3},   {"channel", 2},
                                {"cabOn", 1},     {"lowCutOn", 1}, {"highCutOn", 1},
                                {"ampVolume", 0}, {"inGain", 0},   {"outGain", 0}};
                            for (const auto &value : actualExpected)
                            {
                                bool matched = false;
                                for (auto *child : state->getChildWithTagNameIterator ("PARAM"))
                                    if (child->getStringAttribute ("id") == value.first)
                                    {
                                        const double actual = child->getDoubleAttribute ("value");
                                        std::cout << "SETUP " << value.first << '=' << actual
                                                  << " expected=" << value.second << std::endl;
                                        matched = std::abs (actual - value.second) < 1e-4;
                                    }
                                if (!matched)
                                {
                                    std::cerr << "FAIL actual VST3 component benchmark setup " << value.first
                                              << '\n';
                                    return 2;
                                }
                            }
                            // State inspection is outside the measured window.
                            next = std::chrono::steady_clock::now ();
                        }
                    }
                    const double mean = std::accumulate (times.begin (), times.end (), 0.0) / times.size ();
                    std::sort (times.begin (), times.end ());
                    const bool pass =
                        overruns == 0 && (!paced || late == 0) && std::isfinite (energy) && energy > 1e-8;
                    failedCases += !pass;
                    std::cout << "VST3_CPU fs=" << fs << " lanes=" << inputs << " frames=" << frames
                              << " mode=" << (paced ? "paced" : "offline") << " mean_us=" << mean
                              << " p95_us=" << times[151] << " p99_us=" << times[158]
                              << " max_us=" << times.back () << " deadline_us=" << deadline
                              << " utilization_percent=" << 100 * mean / deadline << " overruns=" << overruns
                              << " schedule_misses=" << late << " energy=" << energy
                              << " status=" << (pass ? "PASS" : "FAIL") << std::endl;
                }
            p->releaseResources ();
        }
    std::cout << "VST3 CPU: 40 cases, " << failedCases << " deadline/audio failures\n";
    return failedCases ? 1 : 0;
}
