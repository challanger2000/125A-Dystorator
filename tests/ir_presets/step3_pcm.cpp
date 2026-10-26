#include "PluginProcessor.h"
#include "../step3/baseline/AmpSim.h"
#include <iostream>
#include <fstream>
void irTestCaptureCabinetInput(const float*,int) {}
void irTestConvolutionCall(const void*,int) {}
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
                vamp_step3_before::AmpSim before;
                after.prepare (fs, 256);
                before.prepare (fs, 256);
                auto a = settings<vamp::AmpSim::Settings> (ch, drive);
                auto b = settings<vamp_step3_before::AmpSim::Settings> (ch, drive);
                a.noise = b.noise = (ch == 2 && drive == 0);
                std::ofstream frozenPCM, optimizedPCM;
                if (fs == 48000 && ch == 2 && drive == 0)
                {
                    frozenPCM.open ("build/step3-before-high.f32", std::ios::binary);
                    optimizedPCM.open ("build/step3-after-high.f32", std::ios::binary);
                }
                double max = 0, error = 0, energy = 0;
                float x[256], y[256];
                for (int k = 0; k < 40; ++k)
                {
                    if (k == 4) a.boostModel = b.boostModel = 1;
            if (k == 8) a.boostOn = b.boostOn = false;
            if (k == 10) a.boostModel = b.boostModel = 0;
            if (k == 14) a.boostOn = b.boostOn = drive >= 0;
            if (k == 18) a.boostModel = b.boostModel = 1;
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
                    "HIGH vs pinned 398204e PCM, dynamic knobs/channel/tube-set (no threshold relaxation)");
            }
}

int main(){juce::ScopedJuceInitialiser_GUI init;pcm();std::cout<<checks<<" checks, "<<failures<<" failures"<<std::endl;return failures?1:0;}
