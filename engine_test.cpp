// Offline sanity tests for TunerEngine.  Build:  g++ -O2 -std=c++17 -I../Source engine_test.cpp -o engine_test
#include "TunerEngine.h"
#include <cstdio>
#include <chrono>
#include <random>

static const double SR = 48000.0;

// Vowel-ish test voice: harmonics with a formant-like envelope, optional vibrato, f0 trajectory callback.
template <typename F0Fn>
std::vector<float> makeVoice (double seconds, F0Fn f0fn, float vibHz = 0.f, float vibCents = 0.f)
{
    std::vector<float> x ((size_t) (seconds * SR));
    double ph = 0.0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double t = (double) i / SR;
        double f0 = f0fn (t);
        if (vibHz > 0) f0 *= std::pow (2.0, (vibCents / 1200.0) * std::sin (2 * M_PI * vibHz * t));
        ph += f0 / SR; ph -= std::floor (ph);
        double s = 0;
        for (int h = 1; h <= 30 && h * f0 < 8000; ++h)
        {
            const double fh = h * f0;
            const double env = 1.0 / (1.0 + std::pow ((fh - 700.0) / 400.0, 2)) + 0.5 / (1.0 + std::pow ((fh - 1800.0) / 500.0, 2)) + 0.05;
            s += env * std::sin (2 * M_PI * h * ph) / h * 3.0;
        }
        x[i] = (float) (0.3 * s);
    }
    return x;
}

std::vector<float> run (const std::vector<float>& in, const TuneParams& p, int* latencyOut = nullptr)
{
    TunerEngine eng; eng.prepare (SR);
    if (latencyOut) *latencyOut = eng.getLatency();
    std::vector<float> out (in.size());
    for (size_t i = 0; i < in.size(); ++i) out[i] = eng.process (in[i], p);
    return out;
}

// f0 trace of a signal (Hz, 0 = unvoiced) at ~5 ms resolution
std::vector<std::pair<double,float>> f0Trace (const std::vector<float>& x)
{
    YinDetector y; y.prepare (SR); y.setThreshold (0.15f);
    std::vector<std::pair<double,float>> tr;
    for (size_t i = 0; i < x.size(); ++i)
        if (y.push (x[i])) tr.push_back ({ (double) i / SR, y.result().voiced ? y.result().f0 : 0.f });
    return tr;
}

float medianF0 (const std::vector<float>& x, double t0, double t1)
{
    auto tr = f0Trace (x);
    std::vector<float> v;
    for (auto& [t, f] : tr) if (t >= t0 && t <= t1 && f > 0) v.push_back (f);
    if (v.empty()) return 0.f;
    std::sort (v.begin(), v.end());
    return v[v.size() / 2];
}

double cents (double f, double ref) { return 1200.0 * std::log2 (f / ref); }

int fails = 0;
void check (const char* name, bool ok, const char* detail = "")
{
    std::printf ("[%s] %s %s\n", ok ? "PASS" : "FAIL", name, detail);
    if (! ok) ++fails;
}

int main()
{
    char buf[160];
    int lat = 0;

    // 1. hard tune, chromatic: 230 Hz (A#3 = 233.08 Hz)
    {
        TuneParams p; p.retune = 100; p.tracking = 80; p.correction = 100; p.vibrato = 0;
        auto in  = makeVoice (2.0, [] (double) { return 230.0; });
        auto out = run (in, p, &lat);
        const float f = medianF0 (out, 1.0, 1.9);
        std::snprintf (buf, sizeof buf, "(out %.2f Hz, err %.1f ct, latency %d smp = %.1f ms)", f, cents (f, 233.082), lat, 1000.0 * lat / SR);
        check ("hard tune 230 Hz -> A#3", std::abs (cents (f, 233.082)) < 8.0, buf);
    }
    // 2. major scale C, 275 Hz sits between C#4 and C4 -> should go to C4 (261.63)
    {
        TuneParams p; p.retune = 100; p.tracking = 80; p.mask = 0b101010110101;
        auto in  = makeVoice (2.0, [] (double) { return 275.0; });
        auto out = run (in, p);
        const float f = medianF0 (out, 1.0, 1.9);
        std::snprintf (buf, sizeof buf, "(out %.2f Hz)", f);
        check ("C major scale snaps 275 Hz -> C4", std::abs (cents (f, 261.626)) < 8.0, buf);
    }
    // 3. retune dynamics: step from A3 (220) to a detuned 240 Hz. Slow retune should lag, fast should not.
    {
        auto in = makeVoice (3.0, [] (double) { return 232.0; }); // +~-?  target A#3 233.08 (chromatic) -> small error
        TuneParams slow; slow.retune = 0;   slow.tracking = 80;
        TuneParams fast; fast.retune = 100; fast.tracking = 80;
        auto outS = run (in, slow), outF = run (in, fast);
        const float fS1 = medianF0 (outS, 0.45, 0.6), fS2 = medianF0 (outS, 2.5, 2.9);
        const float fF1 = medianF0 (outF, 0.45, 0.6);
        std::snprintf (buf, sizeof buf, "(slow@0.5s %.1f ct, slow@2.7s %.1f ct, fast@0.5s %.1f ct)", cents (fS1, 233.082), cents (fS2, 233.082), cents (fF1, 233.082));
        check ("retune 0 glides slowly, 100 snaps", std::abs (cents (fF1, 233.082)) < 8 && std::abs (cents (fS1, 233.082)) > std::abs (cents (fF1, 233.082)) && std::abs (cents (fS2, 233.082)) < 10, buf);
    }
    // 4. negative retune must overshoot and ring: input steps from in tune to 40 cents sharp of A#3
    {
        const double sharp = 233.08 * std::pow (2.0, 40.0 / 1200.0);
        auto in = makeVoice (3.0, [=] (double t) { return t < 1.0 ? 233.08 : sharp; });
        TuneParams p; p.retune = -10; p.tracking = 90; p.humanize = 0;
        auto out = run (in, p);
        auto tr = f0Trace (out);
        double minOver = 1e9, maxOver = -1e9; int crossings = 0; int prevSign = 0;
        for (auto& [t, f] : tr)
        {
            if (t < 1.05 || t > 2.3 || f <= 0) continue;
            const double e = cents (f, 233.08);
            minOver = std::min (minOver, e); maxOver = std::max (maxOver, e);
            const int sg = e > 4 ? 1 : (e < -4 ? -1 : 0);
            if (sg != 0) { if (prevSign != 0 && sg != prevSign) ++crossings; prevSign = sg; }
        }
        std::snprintf (buf, sizeof buf, "(min %+.0f ct, max %+.0f ct, sign changes %d)", minOver, maxOver, crossings);
        check ("retune -10 overshoots & rings around target", minOver < -12 && crossings >= 2, buf);
    }
    // 5. same note change at retune +100 should NOT ring
    {
        auto in = makeVoice (3.0, [] (double t) { return t < 1.0 ? 233.08 : 261.63; });
        TuneParams p; p.retune = 100; p.tracking = 90;
        auto out = run (in, p);
        auto tr = f0Trace (out);
        double maxDev = 0;
        for (auto& [t, f] : tr) if (t > 1.3 && t < 2.5 && f > 0) maxDev = std::max (maxDev, std::abs (cents (f, 261.63)));
        std::snprintf (buf, sizeof buf, "(max dev after settle %.1f ct)", maxDev);
        check ("retune 100 is stable after note change", maxDev < 15, buf);
    }
    // 6. pitch shift +12 st on an in-tune note
    {
        TuneParams p; p.retune = 100; p.shiftSemis = 12.f; p.tracking = 80;
        auto in  = makeVoice (2.0, [] (double) { return 220.0; });
        auto out = run (in, p);
        const float f = medianF0 (out, 1.0, 1.9);
        std::snprintf (buf, sizeof buf, "(out %.2f Hz, expect 440)", f);
        check ("pitch shift +12 st", std::abs (cents (f, 440.0)) < 15.0, buf);
    }
    // 7. formant shift must not move the pitch
    {
        TuneParams p; p.retune = 100; p.formantSemis = 5.f; p.preserve = true; p.tracking = 80;
        auto in  = makeVoice (2.0, [] (double) { return 220.0; });
        auto out = run (in, p);
        const float f = medianF0 (out, 1.0, 1.9);
        std::snprintf (buf, sizeof buf, "(out %.2f Hz)", f);
        check ("formant +5 st leaves pitch at 220", std::abs (cents (f, 220.0)) < 15.0, buf);
    }
    // 8. vibrato: 5.5 Hz +-40 cents around an A#3; hard-tune w/ vibrato 0 should flatten it, 100 should restore it
    {
        auto in = makeVoice (3.0, [] (double) { return 233.08; }, 5.5f, 40.f);
        auto devRange = [&] (const std::vector<float>& o)
        {
            auto tr = f0Trace (o); double lo = 1e9, hi = -1e9;
            for (auto& [t, f] : tr) if (t > 1.0 && f > 0) { double e = cents (f, 233.08); lo = std::min (lo, e); hi = std::max (hi, e); }
            return hi - lo;
        };
        TuneParams flat; flat.retune = 100; flat.vibrato = 0; flat.tracking = 90;
        TuneParams keep = flat; keep.vibrato = 100;
        const double rIn = devRange (in), rFlat = devRange (run (in, flat)), rKeep = devRange (run (in, keep));
        std::snprintf (buf, sizeof buf, "(in %.0f ct p-p, vib 0%% -> %.0f, vib 100%% -> %.0f)", rIn, rFlat, rKeep);
        check ("vibrato control flattens / restores", rFlat < 0.5 * rIn && rKeep > rFlat + 15, buf);
    }
    // 9. unvoiced noise passes through, no NaN, level sane
    {
        std::mt19937 rng (1); std::normal_distribution<float> nd (0.f, 0.1f);
        std::vector<float> in (96000); for (auto& v : in) v = nd (rng);
        TuneParams p; auto out = run (in, p);
        double e1 = 0, e2 = 0; bool nan = false;
        for (size_t i = 24000; i < in.size(); ++i) { e1 += in[i - 1000] * in[i - 1000]; e2 += out[i] * out[i]; if (std::isnan (out[i])) nan = true; }
        std::snprintf (buf, sizeof buf, "(out/in energy %.2f)", e2 / e1);
        check ("noise passes through without NaN", ! nan && e2 / e1 > 0.4 && e2 / e1 < 2.5, buf);
    }
    // 10. CPU
    {
        auto in = makeVoice (10.0, [] (double t) { return 200.0 + 40.0 * std::sin (t); });
        TuneParams p; p.retune = 60;
        auto t0 = std::chrono::steady_clock::now();
        auto out = run (in, p);
        auto dt = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::snprintf (buf, sizeof buf, "(10 s of audio in %.3f s = %.1f%% of one core)", dt, 100.0 * dt / 10.0);
        check ("CPU load acceptable", dt < 1.5, buf);
    }
    std::printf (fails ? "\n%d test(s) FAILED\n" : "\nall tests passed\n", fails);
    return fails ? 1 : 0;
}
