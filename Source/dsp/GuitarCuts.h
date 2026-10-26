#pragma once

// 125A compact amp: independent stereo high-pass (LOW CUT) and low-pass
// (HIGH CUT).  Two-pole TPT state-variable Butterworth filters (Q = 1/sqrt(2)).
// Coefficients and wet/dry bypass are smoothed once per FRAME so L/R never
// receive different coefficients.  The filter state is always advanced, even
// while bypassed, to avoid stale-state transients on reactivation.
//
// No allocation, locks, I/O, denormal-prone state or trigonometry in process().
// setCutoffs() may be called from the audio callback when parameters change;
// the trigonometry is only computed on an actual cutoff change.

#include <algorithm>
#include <array>
#include <cmath>

namespace vamp
{
class GuitarCuts
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = std::max (8000.0, sampleRate);
        smoothing = -std::expm1 (-1.0 / (0.015 * fs)); // 15 ms time constant
        lowG = lowTarget = toG (lowHz);
        highG = highTarget = toG (highHz);
        lowWet = highWet = 0.0;
        lowOn = highOn = false;
        reset();
    }

    void reset() noexcept
    {
        lowState = {};
        highState = {};
    }

    void setCutoffs (double low, double high) noexcept
    {
        const auto a = clampCutoff (low);
        const auto b = clampCutoff (high);
        if (a != lowHz) { lowHz = a; lowTarget = toG (lowHz); }
        if (b != highHz) { highHz = b; highTarget = toG (highHz); }
    }

    void setEnabled (bool low, bool high) noexcept
    {
        lowOn = low;
        highOn = high;
    }

    void processStereo (float& left, float& right) noexcept
    {
        advanceFrame();
        left  = processChannel (left, 0);
        right = processChannel (right, 1);
    }

    void processMono (float& sample) noexcept
    {
        advanceFrame();
        sample = processChannel (sample, 0);
    }

private:
    struct FilterState { double ic1 = 0.0, ic2 = 0.0; };

    static constexpr double butterworthK = 1.4142135623730950488;
    double fs = 44100.0, smoothing = 0.0;
    double lowHz = 85.0, highHz = 10000.0;
    double lowG = 0.0, highG = 0.0, lowTarget = 0.0, highTarget = 0.0;
    double lowWet = 0.0, highWet = 0.0;
    bool lowOn = false, highOn = false;
    std::array<FilterState, 2> lowState{}, highState{};

    double clampCutoff (double hz) const noexcept
    {
        if (! std::isfinite (hz)) return 1000.0;
        return std::clamp (hz, 10.0, std::min (20000.0, 0.45 * fs));
    }

    double toG (double hz) const noexcept
    {
        constexpr double pi = 3.14159265358979323846;
        return std::tan (pi * clampCutoff (hz) / fs);
    }

    void advanceFrame() noexcept
    {
        lowG  += smoothing * (lowTarget  - lowG);
        highG += smoothing * (highTarget - highG);
        lowWet  += smoothing * ((lowOn  ? 1.0 : 0.0) - lowWet);
        highWet += smoothing * ((highOn ? 1.0 : 0.0) - highWet);
        if (lowWet  < 1e-14 && ! lowOn) lowWet = 0.0;
        if (highWet < 1e-14 && ! highOn) highWet = 0.0;
    }

    static double filter (double input, double g, FilterState& s, bool highPass) noexcept
    {
        // Topology-preserving transform (TPT) of a two-integrator SVF.
        const double a1 = 1.0 / (1.0 + g * (g + butterworthK));
        const double a2 = g * a1;
        const double a3 = g * a2;
        const double v3 = input - s.ic2;
        const double v1 = a1 * s.ic1 + a2 * v3;
        const double v2 = s.ic2 + a2 * s.ic1 + a3 * v3;
        s.ic1 = 2.0 * v1 - s.ic1;
        s.ic2 = 2.0 * v2 - s.ic2;
        // Hard-zero inaudible residue to avoid denormal CPU stalls.
        if (std::abs (s.ic1) < 1e-22) s.ic1 = 0.0;
        if (std::abs (s.ic2) < 1e-22) s.ic2 = 0.0;
        return highPass ? input - butterworthK * v1 - v2 : v2;
    }

    float processChannel (float sample, unsigned channel) noexcept
    {
        const double x = std::isfinite (sample) ? sample : 0.0;
        const double low = filter (x, lowG, lowState[channel], true);
        const double afterLow = x + lowWet * (low - x);
        const double high = filter (afterLow, highG, highState[channel], false);
        const double afterHigh = afterLow + highWet * (high - afterLow);
        return std::isfinite (afterHigh) ? static_cast<float> (afterHigh) : 0.0f;
    }
};
} // namespace vamp
