#pragma once
// JUDGETUNE - small DSP building blocks. Pure C++17, no JUCE dependency.
#include <vector>
#include <cmath>
#include <algorithm>

namespace jt
{
constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int   clampi (int v, int lo, int hi)       { return v < lo ? lo : (v > hi ? hi : v); }

//==============================================================================
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void setLowpass (double sr, double fc, double q = 0.70710678)
    {
        const double w0 = 2.0 * 3.14159265358979323846 * fc / sr;
        const double cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (float) (((1.0 - cw) * 0.5) / a0);
        b1 = (float) ((1.0 - cw) / a0);
        b2 = b0;
        a1 = (float) ((-2.0 * cw) / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }

    void setHighpass (double sr, double fc, double q = 0.70710678)
    {
        const double w0 = 2.0 * 3.14159265358979323846 * fc / sr;
        const double cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (float) (((1.0 + cw) * 0.5) / a0);
        b1 = (float) (-(1.0 + cw) / a0);
        b2 = b0;
        a1 = (float) ((-2.0 * cw) / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }

    void reset() { z1 = z2 = 0; }

    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

//==============================================================================
// Circular delay line with linearly interpolated fractional read.
// Call write() first, then read(d): d = 0 returns the sample just written.
struct DelayBuf
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void prepare (int maxSamples)
    {
        int n = 1;
        while (n < maxSamples + 4) n <<= 1;
        buf.assign ((size_t) n, 0.f);
        mask = n - 1;
        w = 0;
    }

    void write (float x) { w = (w + 1) & mask; buf[(size_t) w] = x; }

    float read (float d) const
    {
        const int   di = (int) d;
        const float fr = d - (float) di;
        const float a  = buf[(size_t) ((w - di) & mask)];
        const float b  = buf[(size_t) ((w - di - 1) & mask)];
        return a + (b - a) * fr;
    }
};

//==============================================================================
// Simple two-tap crossfaded delay-line pitch shifter. Used for the small
// detunes / layer shifts in the Doubler (the main tuner uses PSOLA).
struct SimpleShifter
{
    DelayBuf d;
    float grain = 2048.f, ph = 0.f;

    void prepare (double sr)
    {
        grain = (float) (0.045 * sr);
        d.prepare ((int) grain * 2 + 16);
        ph = 0.f;
    }

    float process (float x, float ratio)
    {
        d.write (x);
        ph += (1.0f - ratio) / grain;
        ph -= std::floor (ph);
        float p2 = ph + 0.5f;
        p2 -= std::floor (p2);
        const float w1 = 0.5f - 0.5f * std::cos (kTwoPi * ph);
        const float a = d.read (ph * grain + 1.f);
        const float b = d.read (p2 * grain + 1.f);
        return a * w1 + b * (1.f - w1);
    }
};

//==============================================================================
struct Allpass1
{
    float a = 0.5f, z = 0.f;
    float process (float x)
    {
        const float y = -a * x + z;
        z = x + a * y;
        return y;
    }
};
} // namespace jt
