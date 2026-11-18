#pragma once
// JUDGETUNE - TunerEngine
// Mono pitch corrector: YIN detection -> scale targeting -> retune dynamics ->
// time-domain PSOLA resynthesis with independent pitch and formant scaling.
// Pure C++17 (no JUCE) so it can be unit tested offline.
#include <vector>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include "DspUtils.h"
#include "PitchDetector.h"

struct TuneParams
{
    float retune      = 70.f;   // -10..100  (100 = instant / hard tune, 0 = slow, <0 = overshoot / wobble)
    float humanize    = 0.f;    // 0..100
    float flex        = 0.f;    // 0..100
    float tracking    = 60.f;   // 0..100
    float correction  = 100.f;  // 0..100
    float natural     = 0.f;    // 0..100
    float vibrato     = 0.f;    // 0..200 (%)
    float transition  = 0.f;    // ms
    float shiftSemis  = 0.f;    // pitch + fine, semitones
    float formantSemis = 0.f;
    bool  preserve    = true;
    float refHz       = 440.f;
    int   mask        = 0xFFF;  // allowed pitch classes (bit 0 = C)
    int   heldMask    = 0;      // pitch classes of held MIDI notes (0 = none)
    float fmin        = 80.f, fmax = 800.f;
};

class TunerEngine
{
public:
    // GUI / metering outputs (written on the audio thread)
    std::atomic<float> visDet { -1.f }, visTgt { -1.f }, visOut { -1.f }, visVoiced { 0.f };
    std::array<std::atomic<float>, 12> pcHist;

    TunerEngine() { for (auto& h : pcHist) h.store (0.f); }

    void prepare (double sampleRate)
    {
        sr       = sampleRate;
        Hmax     = (int) std::lround (0.016 * sr);
        latency  = Hmax + Hmax / 2;
        inSize   = 32768; inMask = inSize - 1;
        accSize  = 1; while (accSize < Hmax * 6) accSize <<= 1;
        accMask  = accSize - 1;
        inRing.assign ((size_t) inSize, 0.f);
        acc.assign ((size_t) accSize, 0.f);
        wsum.assign ((size_t) accSize, 0.f);
        yin.prepare (sr);
        ctrlBlock = 64;
        reset();
    }

    void reset()
    {
        std::fill (inRing.begin(), inRing.end(), 0.f);
        std::fill (acc.begin(), acc.end(), 0.f);
        std::fill (wsum.begin(), wsum.end(), 0.f);
        yin.reset();
        n = 0; nextMark = (double) Hmax; lastC = 0; haveLastC = false;
        ctrlCount = 0;
        T0 = (float) (sr / 200.0); curTs = T0; fScale = 1.f;
        pIn = 60.f; havePitch = false; haveMeasured = false; latestVoiced = false;
        measured = 60.f; unvoicedTime = 1.f; offTime = 1.f; jumpCount = 0;
        curTarget = -1; tgtSm = 60.f; holdTime = 0.f; slowDev = 0.f;
        c = cv = 0.f; lp9 = lp35 = 0.f; semisSm = 0.f; vg = 0.f; voicedTarget = 0.f;
        lastFmin = -1.f; lastFmax = -1.f;
    }

    int getLatency() const { return latency; }

    // One mono sample in, one latency-aligned mono sample out.
    float process (float x, const TuneParams& p)
    {
        inRing[(size_t) (n & inMask)] = x;

        if (p.fmin != lastFmin || p.fmax != lastFmax)
        {
            yin.setRange (p.fmin, p.fmax);
            lastFmin = p.fmin; lastFmax = p.fmax;
        }
        yin.setThreshold (0.10f + 0.15f * p.tracking * 0.01f);
        if (yin.push (x)) onDetect (p);

        if (++ctrlCount >= ctrlBlock)
        {
            ctrlCount = 0;
            control (p, (float) ctrlBlock / (float) sr);
        }

        const double limit = (double) (n + Hmax);
        while (nextMark <= limit)
        {
            addGrain (nextMark);
            nextMark += (double) curTs;
        }

        const size_t idx = (size_t) (n & accMask);
        const float  ws  = wsum[idx];
        const float  y   = acc[idx] / std::max (ws, 0.3f);
        acc[idx] = 0.f; wsum[idx] = 0.f;

        const float dry = inRing[(size_t) ((n - latency) & inMask)];
        vg += (voicedTarget - vg) * 0.004f;
        ++n;
        return y * vg + dry * (1.f - vg);
    }

private:
    //--------------------------------------------------------------------------
    void onDetect (const TuneParams& p)
    {
        const auto& r = yin.result();
        if (r.voiced && r.f0 > 20.f)
        {
            const float m = 69.f + 12.f * std::log2 (r.f0 / p.refHz);
            bool accept = true;
            if (havePitch && unvoicedTime < 0.15f && std::abs (m - pIn) > 3.5f)
            {
                if (++jumpCount < 3) accept = false;   // reject isolated octave jumps
            }
            if (accept)
            {
                jumpCount = 0;
                measured = m; haveMeasured = true;
                const int pc = ((int) std::lround (m) % 12 + 12) % 12;
                const float wgt = r.conf * std::min (1.f, r.rms * 8.f);
                for (auto& h : pcHist) h.store (h.load() * 0.9997f);
                pcHist[(size_t) pc].store (pcHist[(size_t) pc].load() + wgt);
            }
            latestVoiced = true;
        }
        else
            latestVoiced = false;
    }

    //--------------------------------------------------------------------------
    static int pcOf (int note) { return ((note % 12) + 12) % 12; }

    void control (const TuneParams& p, float dt)
    {
        if (latestVoiced) unvoicedTime = 0.f; else unvoicedTime += dt;
        const bool voicedNow = haveMeasured && unvoicedTime < 0.06f;

        if (voicedNow)
        {
            if (offTime > 0.12f || ! havePitch)
            {
                pIn = measured; havePitch = true;
                c = cv = 0.f; slowDev = 0.f; curTarget = -1; holdTime = 0.f;
                lp9 = lp35 = pIn;
            }
            offTime = 0.f;
        }
        else
            offTime += dt;

        voicedTarget = voicedNow ? 1.f : 0.f;

        float semisTarget = p.shiftSemis;

        if (havePitch && voicedNow)
        {
            // --- tracking: smoothing of the detected pitch --------------------
            const float tauTrack = 0.06f + (0.004f - 0.06f) * (p.tracking * 0.01f);
            pIn += (1.f - std::exp (-dt / tauTrack)) * (measured - pIn);

            // --- target note selection (scale / MIDI) -------------------------
            int mask = p.heldMask != 0 ? p.heldMask : p.mask;
            if (mask == 0) mask = 0xFFF;

            int bestN = -1; float bestD = 1e9f;
            const int r = (int) std::lround (pIn);
            for (int nn = r - 7; nn <= r + 7; ++nn)
            {
                if (! ((mask >> pcOf (nn)) & 1)) continue;
                const float dd = std::abs ((float) nn - pIn);
                if (dd < bestD) { bestD = dd; bestN = nn; }
            }
            if (bestN < 0) bestN = r;

            const float hyst = 0.12f + 0.15f * p.humanize * 0.01f;
            if (curTarget >= 0 && ((mask >> pcOf (curTarget)) & 1)
                && std::abs ((float) curTarget - pIn) <= bestD + hyst)
                bestN = curTarget;

            if (bestN != curTarget)
            {
                if (curTarget < 0) tgtSm = (float) bestN;
                curTarget = bestN; holdTime = 0.f;
            }
            else
                holdTime += dt;

            // --- note transition (glide between target notes) -----------------
            if (p.transition > 0.5f)
                tgtSm += (1.f - std::exp (-dt / (p.transition * 0.001f))) * ((float) curTarget - tgtSm);
            else
                tgtSm = (float) curTarget;

            // --- naturalness: keep part of the singer's slow intonation -------
            slowDev += (1.f - std::exp (-dt / 0.3f)) * ((pIn - tgtSm) - slowDev);
            slowDev  = jt::clampf (slowDev, -0.6f, 0.6f);
            const float tgtEff = tgtSm + 0.8f * (p.natural * 0.01f) * slowDev;

            // --- flex-tune: soft dead-zone around the note --------------------
            const float e = tgtEff - pIn;
            const float w = p.flex * 0.01f * 0.5f;
            float cDes = e;
            if (w > 0.001f)
            {
                const float a = std::min (1.f, std::abs (e) / w);
                cDes = e * (a * a * (3.f - 2.f * a));
            }

            // --- retune dynamics ----------------------------------------------
            const float amt  = p.correction * 0.01f;
            const float mult = 1.f + p.humanize * 0.01f * 4.f * std::min (1.f, holdTime / 0.4f);
            float tauEff;
            if (p.retune >= 0.f)
            {
                // 100 = ~instant, 0 = 400 ms first-order glide
                const float x = 1.f - p.retune * 0.01f;
                tauEff = (0.0004f + 0.4f * x * x) * mult;
                c += (1.f - std::exp (-dt / tauEff)) * (cDes - c);
                cv = 0.f;
            }
            else
            {
                // -10..0 : under-damped spring. Overshoots the note and rings.
                // -10 = fastest, least damped (about 6.5 Hz wobble).
                const float u    = jt::clampf (-p.retune * 0.1f, 0.f, 1.f);
                const float zeta = 1.f - 0.88f * u;
                const float wn   = (5.f + u * (jt::kTwoPi * 6.5f - 5.f)) / std::sqrt (mult);
                const float acc2 = wn * wn * (cDes - c) - 2.f * zeta * wn * cv;
                cv += acc2 * dt;  c += cv * dt;
                c  = jt::clampf (c, -12.f, 12.f);
                cv = jt::clampf (cv, -60.f, 60.f);
                tauEff = 1.f / wn;
            }

            // --- vibrato control (band 3.5..9 Hz of the *input* pitch) -------
            lp9  += (1.f - std::exp (-dt * jt::kTwoPi * 9.0f)) * (pIn - lp9);
            lp35 += (1.f - std::exp (-dt * jt::kTwoPi * 3.5f)) * (pIn - lp35);
            const float bpIn = 0.8f * std::tanh ((lp9 - lp35) / 0.8f);
            const float x6   = jt::kTwoPi * 6.f * tauEff;
            const float pass0 = x6 / std::sqrt (1.f + x6 * x6);       // vibrato passed by the retune filter
            const float pass  = 1.f - amt * (1.f - pass0);
            const float inject = (p.vibrato * 0.01f - pass) * bpIn;

            semisTarget = c * amt + inject + p.shiftSemis;

            visDet.store (pIn);
            visTgt.store (tgtSm);
            visVoiced.store (1.f);
        }
        else
        {
            visDet.store (-1.f);
            visVoiced.store (0.f);
        }

        semisTarget = jt::clampf (semisTarget, -36.f, 36.f);
        semisSm += (1.f - std::exp (-dt / 0.002f)) * (semisTarget - semisSm);
        const float ratio = jt::clampf (std::exp2 (semisSm / 12.f), 0.25f, 4.f);

        if (havePitch)
        {
            const float f0 = p.refHz * std::exp2 ((pIn - 69.f) / 12.f);
            T0 = jt::clampf ((float) sr / f0, (float) sr / 1200.f, (float) sr / 60.f);
        }
        curTs  = std::max (20.f, T0 / ratio);
        const float fR = std::exp2 (p.formantSemis / 12.f);
        fScale = jt::clampf (fR * (p.preserve ? 1.f : ratio), 0.5f, 2.5f);

        if (voicedNow) visOut.store (pIn + semisSm);
        else           visOut.store (-1.f);
    }

    //--------------------------------------------------------------------------
    void addGrain (double ts)
    {
        const double T0d = (double) T0;
        const int    H   = jt::clampi ((int) std::lround (curTs), 16, Hmax);
        const double target = ts - (double) latency;

        double cc;
        if (! haveLastC || std::abs (target - lastC) > 3.0 * T0d)
            cc = target;
        else
            cc = lastC + T0d * std::round ((target - lastC) / T0d);

        // lock grain centres onto the local waveform peak (glottal-pulse proxy)
        {
            const long long ci = (long long) std::llround (cc);
            const int range = std::max (2, (int) (T0d * 0.25));
            float best = -1e30f; long long bestPos = ci;
            for (int j = -range; j <= range; ++j)
            {
                const long long pos = ci + j;
                if (pos > n - 1 || pos < n - inSize + 64) continue;
                const float v = inRing[(size_t) (pos & inMask)];
                if (v > best) { best = v; bestPos = pos; }
            }
            cc = (double) bestPos;
        }

        const double s   = (double) fScale;
        const int    len = 2 * H;
        const long long startOut = (long long) std::floor (ts) - H;
        for (int i = 0; i < len; ++i)
        {
            const long long outPos = startOut + i;
            if (outPos < n) continue;
            double sp = cc + ((double) i - (double) H + 0.5) * s;
            if (sp > (double) (n - 1)) sp = (double) (n - 1);
            if (sp < (double) (n - inSize + 64)) continue;
            const long long i0 = (long long) std::floor (sp);
            const float fr = (float) (sp - (double) i0);
            const float a = inRing[(size_t) (i0 & inMask)];
            const float b = inRing[(size_t) ((i0 + 1) & inMask)];
            const float w = 0.5f - 0.5f * std::cos (jt::kTwoPi * ((float) i + 0.5f) / (float) len);
            acc [(size_t) (outPos & accMask)] += w * (a + (b - a) * fr);
            wsum[(size_t) (outPos & accMask)] += w;
        }
        lastC = cc; haveLastC = true;
    }

    //--------------------------------------------------------------------------
    double sr = 48000.0;
    int Hmax = 768, latency = 1152, inSize = 32768, inMask = 32767, accSize = 8192, accMask = 8191;
    int ctrlBlock = 64, ctrlCount = 0;
    long long n = 0;
    double nextMark = 0.0, lastC = 0.0; bool haveLastC = false;
    std::vector<float> inRing, acc, wsum;
    YinDetector yin;
    float lastFmin = -1.f, lastFmax = -1.f;

    // detection / control state
    float measured = 60.f, pIn = 60.f, unvoicedTime = 1.f, offTime = 1.f;
    bool  havePitch = false, haveMeasured = false, latestVoiced = false;
    int   jumpCount = 0, curTarget = -1;
    float tgtSm = 60.f, holdTime = 0.f, slowDev = 0.f;
    float c = 0.f, cv = 0.f, lp9 = 0.f, lp35 = 0.f, semisSm = 0.f;
    float T0 = 240.f, curTs = 240.f, fScale = 1.f;
    float vg = 0.f, voicedTarget = 0.f;
};
