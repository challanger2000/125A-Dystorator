#include "../cpu_quality/Reference16.h"
#include "../cpu_quality/baseline/AmpSim.h"
#include "HeapProbe.h"
#include "PluginProcessor.h"
#include <complex>
#include <fstream>
#include <iostream>
#include <juce_audio_formats/juce_audio_formats.h>
#include <numeric>
#include <thread>
void irTestCaptureCabinetInput (const float *, int) {}
void irTestConvolutionCall (const void *, int) {}
static int failures = 0, checks = 0;
static void check (bool ok, const char *msg)
{
    ++checks;
    failures += !ok;
    std::cout << (ok ? "PASS " : "FAIL ") << msg << std::endl;
}
static void set (VibeAmpProcessor &p, const char *id, float value)
{
    auto *x = p.apvts.getParameter (id);
    if (!x)
        throw std::runtime_error (id);
    x->setValueNotifyingHost (x->convertTo0to1 (value));
}
template <class S> static S settings (int channel = 2, int drive = 0)
{
    S s;
    s.channel = channel;
    s.gain = .9f;
    s.master = .3f;
    s.bass = .5f;
    s.mid = .5f;
    s.treble = .6f;
    s.presence = .5f;
    s.resonance = .5f;
    s.boostOn = drive >= 0;
    s.boostModel = std::max (0, drive);
    s.boostDrive = .8f;
    s.boostLevel = .7f;
    s.noise = false;
    return s;
}
static float input (int frame, double fs)
{
    double t = frame / fs;
    double env = .3 + .7 * std::exp (-std::fmod (t, .075) * 35);
    return (float)(env * (.12 * std::sin (2 * vamp::kPi * 110 * t + .3) +
                          .03 * std::sin (2 * vamp::kPi * 2723 * t)));
}
static void pcm ()
{
    for (double fs : {44100., 48000., 96000.})
        for (int ch : {0, 1, 2})
            for (int drive : {-1, 0, 1})
            {
                vamp::AmpSim after;
                vamp_before::AmpSim before;
                after.prepare (fs, 256);
                before.prepare (fs, 256);
                auto a = settings<vamp::AmpSim::Settings> (ch, drive);
                auto b = settings<vamp_before::AmpSim::Settings> (ch, drive);
                a.noise = b.noise = (ch == 2 && drive == 0);
                std::ofstream frozenPCM, optimizedPCM;
                if (fs == 48000 && ch == 2 && drive == 0)
                {
                    frozenPCM.open ("build/quality-before-high.f32", std::ios::binary);
                    optimizedPCM.open ("build/quality-after-high.f32", std::ios::binary);
                }
                double max = 0, error = 0, energy = 0;
                float x[256], y[256];
                for (int k = 0; k < 40; ++k)
                {
                    if (k == 12)
                    {
                        a.gain = b.gain = .35f;
                        a.presence = b.presence = .8f;
                    }
                    if (k == 24)
                    {
                        a.channel = b.channel = (ch + 1) % 3;
                        a.tubeSet = b.tubeSet = 3;
                    }
                    for (int i = 0; i < 256; ++i)
                        x[i] = y[i] = input (k * 256 + i, fs);
                    before.process (x, 256, b);
                    after.process (y, 256, a);
                    for (int i = 0; i < 256; ++i)
                    {
                        max = std::max (max, (double)std::abs (x[i] - y[i]));
                        error += (double)(x[i] - y[i]) * (x[i] - y[i]);
                        energy += (double)x[i] * x[i];
                    }
                    if (frozenPCM.is_open ())
                    {
                        frozenPCM.write (reinterpret_cast<const char *> (x), sizeof (x));
                        optimizedPCM.write (reinterpret_cast<const char *> (y), sizeof (y));
                    }
                }
                double residual = 10 * std::log10 (std::max (error, 1e-300) / std::max (energy, 1e-300));
                std::cout << "PCM fs=" << fs << " channel=" << ch << " drive=" << drive << " max=" << max
                          << " residual_db=" << residual << " energy=" << energy << '\n';
                check (
                    max < 1e-6 && residual < -100 && energy > 1e-8,
                    "HIGH vs pinned 8ca580d PCM, dynamic knobs/channel/tube-set (no threshold relaxation)");
            }
}
static void modes (const juce::File &dir)
{
    for (double fs : {44100., 48000., 96000.})
    {
        VibeAmpProcessor p (dir);
        auto layout = p.getBusesLayout ();
        layout.inputBuses.set (0, juce::AudioChannelSet::stereo ());
        p.setBusesLayout (layout);
        set (p, "ampNoise", 0);
        set (p, "cabOn", 0);
        set (p, "boostOn", 1);
        set (p, "boostDrive", 8);
        set (p, "ampGain", 9);
        p.prepareToPlay (fs, 128);
        int latency = p.getLatencySamples ();
        juce::AudioBuffer<float> a (2, 128);
        juce::MidiBuffer m;
        int frame = 0;
        unsigned alloc = 0, freed = 0;
        bool finite = true;
        double energy = 0;
        auto run = [&] (double seconds)
        {
            for (int k = 0; k < (int)std::ceil (seconds * fs / 128); ++k)
            {
                a.clear ();
                for (int i = 0; i < 128; ++i)
                    a.setSample (1, i, input (frame + i, fs));
                frame += 128;
                heapProbe::allocations = heapProbe::deletions = 0;
                heapProbe::enabled = true;
                p.processBlock (a, m);
                heapProbe::enabled = false;
                alloc += heapProbe::allocations;
                freed += heapProbe::deletions;
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 128; ++i)
                    {
                        finite = finite && std::isfinite (a.getSample (c, i));
                        energy += (double)a.getSample (c, i) * a.getSample (c, i);
                    }
            }
        };
        for (int q : {2, 0, 1})
            for (int mode : {0, 1})
            {
                set (p, "ampMode", (float)mode);
                set (p, "quality", (float)q);
                run (.23);
                check (!p.isAmpTransitioning () && p.getEffectiveAmpLanes () == (mode ? 2 : 1) &&
                           p.getEffectiveOversamplingFactor () == vamp::AmpModes::factorFor (fs, q),
                       "actual processor accepts both AMP MODEs and all effective host-rate quality factors");
                check (p.getLatencySamples () == latency,
                       "quality/mode switches preserve the truthful padded HIGH latency");
                if (mode == 0)
                {
                    double delta = 0, e = 0;
                    for (int i = 0; i < 128; ++i)
                    {
                        delta = std::max (delta, (double)std::abs (a.getSample (0, i) - a.getSample (1, i)));
                        e += (double)a.getSample (0, i) * a.getSample (0, i);
                    }
                    check (delta == 0 && e > 1e-10,
                           "MONO averages R-only into one audible amp and duplicates output before cabinet");
                }

            }
        for (int k = 0; k < 25; ++k)
        {
            set (p, "quality", (float)(k % 3));
            set (p, "ampMode", (float)(k % 2));
            run (.005);
        }
        set (p, "quality", 0);
        set (p, "ampMode", 0);
        run (.24);
        check (!p.isAmpTransitioning () && p.getEffectiveAmpLanes () == 1 &&
                   p.getEffectiveOversamplingFactor () == vamp::AmpModes::factorFor (fs, 0),
               "dense switches finish latest request without resetting active warm/fade");
        check (finite && energy > 1e-8 && alloc == 0 && freed == 0,
               "live switches produce finite non-silent PCM with zero callback C++ new/delete");
        check (p.presets.save ("quality-scene"), "save mode/quality preset");
        juce::MemoryBlock state;
        p.getStateInformation (state);
        set (p, "quality", 2);
        set (p, "ampMode", 1);
        run (.2);
        p.setStateInformation (state.getData (), (int)state.getSize ());
        run (.23);
        check (p.getEffectiveAmpLanes () == 1 &&
                   p.getEffectiveOversamplingFactor () == vamp::AmpModes::factorFor (fs, 0),
               "host state restores actual audible mono/ECO bank");
        set (p, "quality", 2);
        set (p, "ampMode", 1);
        check (p.presets.load (p.presets.indexOf ("quality-scene")),
               "preset restores mode/quality selection");
        run (.23);
        check (p.getEffectiveAmpLanes () == 1 &&
                   p.getEffectiveOversamplingFactor () == vamp::AmpModes::factorFor (fs, 0),
               "preset reload drives actual mono/ECO signal path");
        auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData (), (int)state.getSize ());
        for (auto *child = xml->getFirstChildElement (); child;)
        {
            auto *next = child->getNextElement ();
            auto id = child->getStringAttribute ("id");
            if (id == "ampMode" || id == "quality")
                xml->removeChildElement (child, true);
            child = next;
        }
        juce::MemoryBlock legacy;
        juce::AudioProcessor::copyXmlToBinary (*xml, legacy);
        p.setStateInformation (legacy.getData (), (int)legacy.getSize ());
        run (.23);
        check (p.getEffectiveAmpLanes () == 2 &&
                   p.getEffectiveOversamplingFactor () == vamp::AmpModes::factorFor (fs, 2),
               "legacy host scene migrates to original STEREO/HIGH defaults");
    }
}
// Hann-window spectra: THD and specified third-order IMD; non-harmonic energy
// is an alias/transient/noise proxy, cross-checked against a frozen 16x renderer.
static double power (const std::vector<float> &data, int bin)
{
    std::complex<double> v{};
    int n = (int)data.size ();
    for (int i = 0; i < n; ++i)
    {
        double w = .5 - .5 * std::cos (2 * vamp::kPi * i / (n - 1));
        v += (double)data[i] * w * std::polar (1., -2 * vamp::kPi * bin * i / n);
    }
    return std::norm (v);
}
static double db (double x) { return 10 * std::log10 (std::max (x, 1e-30)); }
template <class Amp, class S> static void metric (Amp &amp, S s, double fs, const char *name)
{
    constexpr int n = 8192;
    int bin = ((int)std::round (3000 * n / fs)) | 1;
    float block[128];
    std::vector<float> y (n);
    for (int kind = 0; kind < 3; ++kind)
    {
        int pos = 0;
        int warm = (int)std::ceil (.3 * fs / 128) * 128;
        int b1 = ((int)std::round (700 * n / fs)) | 1, b2 = ((int)std::round (900 * n / fs)) | 1;
        for (int k = 0; k < warm + n; k += 128)
        {
            for (int i = 0; i < 128; ++i)
            {
                int f = k + i;
                double phase = 2 * vamp::kPi * f / n;
                block[i] =
                    kind == 0 ? (float)(.12 * std::sin (phase * bin))
                    : kind == 1
                        ? (float)(.075 * (std::sin (phase * b1) + std::sin (phase * b2)))
                        : (float)((f < warm + n / 2 ? .025 : .15) * std::sin (2 * vamp::kPi * 110 * f / fs));
            }
            amp.process (block, 128, s);
            if (k >= warm)
            {
                std::copy_n (block, 128, y.data () + pos);
                pos += 128;
            }
        }
        double rms = 0, dc = 0, peak = 0, quiet = 0, loud = 0;
        for (int i = 0; i < n; ++i)
        {
            double v = y[i];
            rms += v * v;
            dc += v;
            peak = std::max (peak, std::abs (v));
            (i < n / 2 ? quiet : loud) += v * v;
        }
        std::cout << "METRIC fs=" << fs << " quality=" << name << " signal=" << kind
                  << " rms=" << std::sqrt (rms / n) << " dc=" << dc / n << " peak=" << peak
                  << " dynamic_db=" << db (loud / std::max (quiet, 1e-30));
        if (kind == 0)
        {
            double fundamental = power (y, bin), harmonics = 0;
            for (int h = 2; h * bin < n / 2 && h * bin * fs / n <= 20000; ++h)
                harmonics += power (y, h * bin);
            // JUCE FFT independent of the circuit solver; retain a guard around expected harmonics.
            juce::dsp::FFT fft (13);
            std::vector<float> spec (2 * n);
            for (int i = 0; i < n; ++i)
                spec[i] = y[i] * (float)(.5 - .5 * std::cos (2 * vamp::kPi * i / (n - 1)));
            fft.performFrequencyOnlyForwardTransform (spec.data ());
            double other = 0, total = 0;
            for (int k = 1; k < n / 2 && k * fs / n <= 20000; ++k)
            {
                double p = (double)spec[k] * spec[k];
                total += p;
                bool tonal = false;
                for (int h = 1; h * bin < n / 2 && h * bin * fs / n <= 20000; ++h)
                    tonal = tonal || std::abs (k - h * bin) <= 4;
                if (!tonal)
                    other += p;
            }
            std::cout << " thd_db=" << db (harmonics / std::max (fundamental, 1e-30))
                      << " inharmonic_db=" << db (other / std::max (total, 1e-30));
        }
        if (kind == 1)
            std::cout << " imd3_db="
                      << db ((power (y, 2 * b1 - b2) + power (y, 2 * b2 - b1)) /
                             std::max (power (y, b1) + power (y, b2), 1e-30));
        std::cout << std::endl;
        check (std::isfinite (rms) && std::isfinite (dc) && rms > 1e-12 && peak > 1e-8,
               "metric render is finite and non-silent (measurement, not subjective quality approval)");
    }
}
static void metrics ()
{
    for (double fs : {44100., 48000., 96000.})
    {
        vamp_before::AmpSim before;
        before.prepare (fs, 128);
        metric (before, settings<vamp_before::AmpSim::Settings> (), fs, "BEFORE_HIGH");
        for (int q : {2, 1, 0})
        {
            vamp::AmpSim amp;
            amp.prepare (fs, 128, vamp::AmpModes::factorFor (fs, q));
            metric (amp, settings<vamp::AmpSim::Settings> (), fs,
                    q == 2   ? "HIGH"
                    : q == 1 ? "STANDARD"
                             : "ECO");
        }
        vamp_before::Reference16 ref;
        ref.prepare (fs, 128);
        metric (ref, settings<vamp_before::Reference16::Settings> (), fs, "REF16");
    }
}

static void stress (const juce::File &dir)
{
    VibeAmpProcessor p (dir);
    auto layout = p.getBusesLayout ();
    layout.inputBuses.set (0, juce::AudioChannelSet::stereo ());
    p.setBusesLayout (layout);
    set (p, "ampNoise", 0);
    p.prepareToPlay (48000, 64);
    const auto ir = dir.getChildFile ("stereo-stress.wav");
    juce::WavAudioFormat format;
    auto out = ir.createOutputStream ();
    std::unique_ptr<juce::AudioFormatWriter> writer (
        format.createWriterFor (out.release (), 48000, 2, 24, {}, 0));
    juce::AudioBuffer<float> impulse (2, 64);
    impulse.clear ();
    impulse.setSample (0, 0, .8f);
    impulse.setSample (1, 0, -.6f);
    if (!writer || !writer->writeFromAudioSampleBuffer (impulse, 0, 64))
        throw std::runtime_error ("stress IR");
    writer.reset ();
    std::atomic<bool> start{false}, finite{true};
    std::atomic<unsigned> alloc{0}, freed{0};
    std::thread audio (
        [&]
        {
            juce::AudioBuffer<float> a (2, 64);
            juce::MidiBuffer m;
            while (!start.load ())
                std::this_thread::yield ();
            for (int k = 0; k < 1200; ++k)
            {
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 64; ++i)
                        a.setSample (c, i, input (k * 64 + i + c * 71, 48000));
                heapProbe::allocations = heapProbe::deletions = 0;
                heapProbe::enabled = true;
                if (k >= 300 && k < 500)
                    p.processBlockBypassed (a, m);
                else
                    p.processBlock (a, m);
                heapProbe::enabled = false;
                alloc += heapProbe::allocations;
                freed += heapProbe::deletions;
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 64; ++i)
                        finite = finite.load () && std::isfinite (a.getSample (c, i));
                juce::Thread::sleep (1);
            }
        });
    start = true;
    bool accepted = true;
    for (int k = 0; k < 35; ++k)
    {
        set (p, "quality", (float)(k % 3));
        set (p, "ampMode", (float)(k % 2));
        accepted = (k % 3 == 0 ? p.clearIR () : p.loadIR (ir)) && accepted;
        accepted = p.presets.save ("concurrent-mode") && accepted;
        accepted = p.presets.load (p.presets.indexOf ("concurrent-mode")) && accepted;
        juce::MemoryBlock state;
        p.getStateInformation (state);
        p.setStateInformation (state.getData (), (int)state.getSize ());
        juce::Thread::sleep (3);
    }
    set (p, "quality", 0);
    set (p, "ampMode", 0);
    audio.join ();
    check (accepted && finite,
           "concurrent stereo IR/preset/host-state/mode/quality and host bypass complete with finite audio");
    check (alloc == 0 && freed == 0, "concurrent mode/IR callback has no C++ new/delete");
    check (!p.isAmpTransitioning () && p.getEffectiveAmpLanes () == 1 &&
               p.getEffectiveOversamplingFactor () == 2,
           "concurrent last mono/ECO selection becomes active");
}

int main (int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getNonexistentChildFile ("125a-modes", "", false);
    dir.createDirectory ();
    std::string mode = argc > 1 ? argv[1] : "all";
    try
    {
        if (mode == "metrics")
            metrics ();
        else if (mode == "stress")
            stress (dir);
        else
        {
            if (mode != "modes")
                pcm ();
            if (mode != "pcm")
                modes (dir);
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what () << '\n';
        ++failures;
    }
    dir.deleteRecursively ();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
