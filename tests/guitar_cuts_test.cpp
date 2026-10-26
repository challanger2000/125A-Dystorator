#include "dsp/GuitarCuts.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

constexpr double pi = 3.14159265358979323846;
int fails = 0;
void check (bool yes, const char* message) {
    if (! yes) { std::printf("FAIL: %s\n", message); ++fails; }
}

double response (double fs, double frequency, bool low, bool high)
{
    vamp::GuitarCuts c;
    c.prepare (fs);
    c.setCutoffs (100.0, 5000.0);
    c.setEnabled (low, high);
    double in = 0, out = 0;
    const int length = static_cast<int> (0.6 * fs);
    const int begin = static_cast<int> (0.25 * fs);
    for (int i = 0; i < length; ++i) {
        float x = static_cast<float> (0.5 * std::sin (2.0 * pi * frequency * i / fs));
        float y = x;
        c.processMono (y);
        if (i >= begin) { in += x * x; out += y * y; }
    }
    return 10.0 * std::log10 (out / in);
}

int main()
{
    for (double fs : { 44100.0, 48000.0, 96000.0 }) {
        check (response (fs, 30.0, true, false) < -19.0, "low cut attenuates sub-bass");
        check (std::abs (response (fs, 1000.0, true, false)) < 0.1, "low cut preserves 1 kHz");
        check (response (fs, 12000.0, false, true) < -15.0, "high cut attenuates treble");
        check (std::abs (response (fs, 1000.0, false, true)) < 0.1, "high cut preserves 1 kHz");
        vamp::GuitarCuts c; c.prepare (fs);
        c.setCutoffs (90.0, 10000.0); c.setEnabled (true, true);
        for (int i = 0; i < 20000; ++i) {
            float l = (i == 0 ? 1.0f : 0.0f), r = 0.0f;
            c.processStereo (l, r);
            check (r == 0.0f, "left impulse never leaks to right");
            check (std::isfinite (l), "left impulse stays finite");
        }
        c.prepare (fs); c.setEnabled (true, true);
        for (int i = 0; i < 20000; ++i) {
            float l = 0.0f, r = (i == 0 ? 1.0f : 0.0f);
            c.processStereo (l, r);
            check (l == 0.0f, "right impulse never leaks to left");
        }
        c.prepare (fs); c.setEnabled (true, true);
        for (int i = 0; i < 5000; ++i) {
            float l = static_cast<float> (std::sin (0.031 * i)), r = -l;
            c.processStereo (l, r);
            check (std::abs (l+r) <= 1e-7f, "antiphase stereo symmetry");
        }
        c.prepare (fs);
        for (int i = 0; i < 20000; ++i) {
            float x = static_cast<float> (std::sin (0.019 * i));
            float l = x, r = -x;
            c.processStereo (l, r);
            check (l == x && r == -x, "fully bypassed = transparent");
        }
        c.setEnabled (true, true);
        for (int i = 0; i < 8000; ++i) {
            if (i % 40 == 0) c.setCutoffs (40.0 + i % 800, 3000.0 + i % 16000);
            float l = static_cast<float> (std::sin (0.019 * i)), r = -l;
            c.processStereo (l, r);
            check (std::isfinite (l) && std::isfinite (r), "fast frequency automation finite");
            check (std::abs (l+r) <= 1e-7f, "automation stereo symmetry");
        }
    }
    std::printf("guitar_cuts_test: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? EXIT_FAILURE : EXIT_SUCCESS;
}
