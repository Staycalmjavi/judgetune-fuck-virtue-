#pragma once
// JUDGETUNE - YIN fundamental frequency detector. Pure C++17.
// Input is low-passed and decimated to ~12 kHz, analysed every ~5 ms.
#include <vector>
#include <cmath>
#include <algorithm>
#include "DspUtils.h"

class YinDetector
{
public:
    struct Result
    {
        bool  voiced = false;
        float f0     = 0.f;   // Hz
        float conf   = 0.f;   // 0..1
        float rms    = 0.f;   // analysis window RMS
    };

    void prepare (double sampleRate)
    {
        sr   = sampleRate;
        D    = std::max (1, (int) std::lround (sr / 12000.0));
        srD  = sr / (double) D;
        lp1.setLowpass (sr, srD * 0.40);
        lp2.setLowpass (sr, srD * 0.40);

        allocTau = (int) std::ceil (srD / 55.0) + 4;
        W        = 384;
        ringSize = 1;
        while (ringSize < W + allocTau + 16) ringSize <<= 1;
        ring.assign ((size_t) ringSize, 0.f);
        frame.assign ((size_t) (W + allocTau + 16), 0.f);
        d.assign ((size_t) allocTau + 4, 0.f);
        dp.assign ((size_t) allocTau + 4, 0.f);
        hop = 64;
        setRange (80.f, 800.f);
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.f);
        lp1.reset(); lp2.reset();
        wpos = 0; decCount = 0; hopCount = 0;
        res = {};
    }

    void setRange (float fmin, float fmax)
    {
        tauMin = std::max (2, (int) std::floor (srD / (double) fmax));
        tauMax = std::min (allocTau - 3, (int) std::ceil (srD / (double) fmin));
        if (tauMax < tauMin + 4) tauMax = tauMin + 4;
    }

    void setThreshold (float t) { thr = t; }

    // Returns true when a fresh result is available.
    bool push (float x)
    {
        const float y = lp2.process (lp1.process (x));
        if (++decCount < D) return false;
        decCount = 0;
        ring[(size_t) wpos] = y;
        wpos = (wpos + 1) & (ringSize - 1);
        if (++hopCount < hop) return false;
        hopCount = 0;
        analyse();
        return true;
    }

    const Result& result() const { return res; }

private:
    void analyse()
    {
        const int need = W + tauMax + 1;
        for (int i = 0; i < need; ++i)
            frame[(size_t) i] = ring[(size_t) ((wpos - need + i) & (ringSize - 1))];

        const float* a = frame.data() + tauMax + 1;

        float e = 0.f;
        for (int j = 0; j < W; ++j) e += a[j] * a[j];
        res.rms = std::sqrt (e / (float) W);

        if (res.rms < 0.004f) { res.voiced = false; res.conf = 0.f; return; }

        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float s = 0.f;
            const float* b = a - tau;
            for (int j = 0; j < W; ++j) { const float df = a[j] - b[j]; s += df * df; }
            d[(size_t) tau] = s;
        }

        dp[0] = 1.f;
        float running = 0.f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += d[(size_t) tau];
            dp[(size_t) tau] = running > 1e-12f ? d[(size_t) tau] * (float) tau / running : 1.f;
        }

        int est = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (dp[(size_t) tau] < thr)
            {
                while (tau + 1 < tauMax && dp[(size_t) tau + 1] < dp[(size_t) tau]) ++tau;
                est = tau;
                break;
            }
        }
        if (est < 0)
        {
            float best = 1e9f;
            for (int tau = tauMin; tau < tauMax; ++tau)
                if (dp[(size_t) tau] < best) { best = dp[(size_t) tau]; est = tau; }
            if (est < 0 || best > 0.35f) { res.voiced = false; res.conf = 0.f; return; }
        }

        float tauF = (float) est;
        if (est > 1 && est < tauMax)
        {
            const float s0 = dp[(size_t) est - 1], s1 = dp[(size_t) est], s2 = dp[(size_t) est + 1];
            const float den = s0 + s2 - 2.f * s1;
            if (std::abs (den) > 1e-9f) tauF += jt::clampf (0.5f * (s0 - s2) / den, -1.f, 1.f);
        }

        res.f0     = (float) (srD / (double) tauF);
        res.conf   = jt::clampf (1.f - dp[(size_t) est], 0.f, 1.f);
        res.voiced = true;
    }

    double sr = 48000.0, srD = 12000.0;
    int D = 4, allocTau = 220, W = 384, ringSize = 1024, wpos = 0, decCount = 0, hopCount = 0, hop = 64;
    int tauMin = 14, tauMax = 150;
    float thr = 0.15f;
    jt::Biquad lp1, lp2;
    std::vector<float> ring, frame, d, dp;
    Result res;
};
