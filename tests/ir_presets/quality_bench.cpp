#include "PluginProcessor.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <numeric>
#include <thread>
void irTestCaptureCabinetInput (const float *, int) {}
void irTestConvolutionCall (const void *, int) {}
static void set (VibeAmpProcessor &p, const char *id, float value)
{
    if (auto *x = p.apvts.getParameter (id))
        x->setValueNotifyingHost (x->convertTo0to1 (value));
}
static float signal (int frame, int channel, double fs)
{
    double t = frame / fs;
    return (float)(.12 * std::sin (2 * 3.141592653589793 * (channel ? 277 : 110) * t + .3) +
                   .025 * std::sin (2 * 3.141592653589793 * (channel ? 1723 : 3731) * t));
}
int main (int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory)
                      .getNonexistentChildFile ("125a-quality-cpu", "", false);
    folder.createDirectory ();
    int failed = 0, cases = 0;
    const bool pacedOnly = argc > 1 && std::string (argv[1]) == "paced";
    for (double fs : {44100., 48000., 96000.})
        for (int lanes : {1, 2})
        {
            VibeAmpProcessor p (folder);
            const bool modes = p.apvts.getParameter ("quality") != nullptr;
            auto layout = p.getBusesLayout ();
            layout.inputBuses.set (0, !modes && lanes == 1 ? juce::AudioChannelSet::mono ()
                                                           : juce::AudioChannelSet::stereo ());
            layout.outputBuses.set (0, juce::AudioChannelSet::stereo ());
            if (!p.setBusesLayout (layout))
                return 2;
            set (p, "channel", 2);
            set (p, "ampGain", 9);
            set (p, "master", 3);
            set (p, "ampNoise", 0);
            set (p, "inGateOn", 0);
            set (p, "boostOn", 1);
            set (p, "boostDrive", 8);
            set (p, "boostLevel", 7);
            set (p, "cabOn", 1);
            set (p, "lowCutOn", 1);
            set (p, "highCutOn", 1);
            set (p, "ampMode", lanes == 1 ? 0 : 1);
            p.prepareToPlay (fs, 512);
            for (int quality : {2, 1, 0})
            {
                if (!modes && quality != 2)
                    continue;
                set (p, "quality", (float)quality);
                for (int n : {64, 128, 256, 512})
                    for (bool paced : {false, true})
                    {
                        if (pacedOnly && !paced)
                            continue;
                        juce::AudioBuffer<float> audio (2, n);
                        juce::MidiBuffer midi;
                        std::vector<double> times;
                        times.reserve (160);
                        const int warm = (int)std::ceil (.2 * fs / n);
                        const double deadline = 1e6 * n / fs;
                        int over = 0, late = 0;
                        double energy = 0;
                        auto next = std::chrono::steady_clock::now ();
                        for (int k = 0; k < warm + 160; ++k)
                        {
                            for (int i = 0; i < n; ++i)
                            {
                                float l = signal (k * n + i, 0, fs), r = signal (k * n + i, 1, fs);
                                if (lanes == 1)
                                    l = r = .5f * (l + r);
                                audio.setSample (0, i, l);
                                audio.setSample (1, i, r);
                            }
                            if (paced && k >= warm)
                                std::this_thread::sleep_until (next);
                            auto start = std::chrono::steady_clock::now ();
                            p.processBlock (audio, midi);
                            auto end = std::chrono::steady_clock::now ();
                            if (k >= warm)
                            {
                                double us = std::chrono::duration<double, std::micro> (end - start).count ();
                                times.push_back (us);
                                over += us > deadline;
                                if (paced)
                                    late += end > next + std::chrono::duration<double> (n / fs);
                                for (int c = 0; c < 2; ++c)
                                    for (int i = 0; i < n; ++i)
                                    {
                                        float v = audio.getSample (c, i);
                                        if (!std::isfinite (v))
                                            return 3;
                                        energy += (double)v * v;
                                    }
                            }
                            if (k < warm)
                                next = end;
                            else
                                next += std::chrono::duration_cast<std::chrono::steady_clock::duration> (
                                    std::chrono::duration<double> (n / fs));
                        }
                        double mean = std::accumulate (times.begin (), times.end (), 0.) / times.size ();
                        std::sort (times.begin (), times.end ());
                        bool pass = over == 0 && (!paced || late == 0) && energy > 1e-8;
                        failed += !pass;
                        ++cases;
                        std::cout << "QUALITY_CPU version=" << (modes ? "after" : "before") << " fs=" << fs
                                  << " lanes=" << lanes << " quality=" << quality << " frames=" << n
                                  << " mode=" << (paced ? "paced" : "offline") << " mean_us=" << mean
                                  << " p95_us=" << times[151] << " p99_us=" << times[158]
                                  << " max_us=" << times.back () << " deadline_us=" << deadline
                                  << " overruns=" << over << " schedule_misses=" << late
                                  << " energy=" << energy << " status=" << (pass ? "PASS" : "FAIL")
                                  << std::endl;
                    }
            }
        }
    folder.deleteRecursively ();
    std::cout << cases << " CPU cases, " << failed << " deadline failures\n";
    return failed ? 1 : 0;
}
