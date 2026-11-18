#pragma once
// JUDGETUNE - Doubler & Widener stereo vocal effects.
#include <JuceHeader.h>
#include "DspUtils.h"

//==============================================================================
class Doubler
{
public:
    struct Params
    {
        float detuneCents = 10.f, delayMs = 15.f, separation = 0.7f, intensity = 0.f;
        float gainL = 0.7f, gainR = 0.7f, pitchL = 0.f, pitchR = 0.f;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        shL.prepare (sr); shR.prepare (sr);
        dlL.prepare ((int) (0.25 * sr)); dlR.prepare ((int) (0.25 * sr));
        for (auto* s : { &smDelL, &smDelR, &smRatL, &smRatR, &smGL, &smGR, &smInt, &smSep })
            s->reset (sr, 0.04);
        smDelL.setCurrentAndTargetValue (0.01f * (float) sr);
        smDelR.setCurrentAndTargetValue (0.01f * (float) sr);
        smRatL.setCurrentAndTargetValue (1.f); smRatR.setCurrentAndTargetValue (1.f);
        smGL.setCurrentAndTargetValue (0.7f);  smGR.setCurrentAndTargetValue (0.7f);
        smInt.setCurrentAndTargetValue (0.f);  smSep.setCurrentAndTargetValue (0.7f);
        lfo = 0.f;
    }

    void setParams (const Params& p)
    {
        smDelL.setTargetValue (p.delayMs * 0.001f * (float) sr);
        smDelR.setTargetValue ((p.delayMs * 1.35f + 0.7f) * 0.001f * (float) sr);
        smRatL.setTargetValue (std::exp2 ((p.pitchL - p.detuneCents * 0.01f) / 12.f));
        smRatR.setTargetValue (std::exp2 ((p.pitchR + p.detuneCents * 0.01f) / 12.f));
        smGL.setTargetValue (p.gainL); smGR.setTargetValue (p.gainR);
        smInt.setTargetValue (p.intensity); smSep.setTargetValue (p.separation);
    }

    // in: tuned mono voice.  out: stereo (centre voice + two panned layers).
    void process (float in, float& outL, float& outR)
    {
        const float intensity = smInt.getNextValue();
        const float sep = smSep.getNextValue();
        const float gl = smGL.getNextValue(), gr = smGR.getNextValue();
        const float dl = smDelL.getNextValue(), dr = smDelR.getNextValue();
        const float rl = smRatL.getNextValue(), rr = smRatR.getNextValue();

        if (intensity <= 0.0001f && ! smInt.isSmoothing())
        {
            // keep the delay lines running so there is no click when the effect is engaged
            dlL.write (shL.process (in, rl)); dlR.write (shR.process (in, rr));
            outL = outR = in;
            return;
        }

        const float a = shL.process (in, rl), b = shR.process (in, rr);
        dlL.write (a); dlR.write (b);

        lfo += 1.f / (float) sr;  if (lfo > 1000.f) lfo = 0.f;
        const float modL = 0.00025f * (float) sr * std::sin (jt::kTwoPi * 0.31f * lfo);
        const float modR = 0.00025f * (float) sr * std::sin (jt::kTwoPi * 0.43f * lfo + 1.3f);

        const float la = dlL.read (std::max (1.f, dl + modL)) * gl;
        const float lb = dlR.read (std::max (1.f, dr + modR)) * gr;

        // constant-power pan: left layer to -sep, right layer to +sep
        const float angL = (1.f - sep) * jt::kPi * 0.25f;   // pan = -sep
        const float angR = (1.f + sep) * jt::kPi * 0.25f;   // pan = +sep
        const float k = 0.8f * intensity;
        outL = in + k * (la * std::cos (angL) + lb * std::cos (angR));
        outR = in + k * (la * std::sin (angL) + lb * std::sin (angR));
    }

private:
    double sr = 48000.0;
    jt::SimpleShifter shL, shR;
    jt::DelayBuf dlL, dlR;
    juce::SmoothedValue<float> smDelL, smDelR, smRatL, smRatR, smGL, smGR, smInt, smSep;
    float lfo = 0.f;
};

//==============================================================================
class Widener
{
public:
    struct Params
    {
        float width = 1.f, balance = 0.f, microMs = 0.f, spread = 0.f, intensity = 0.f, mono = 0.5f;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        micro.prepare ((int) (0.05 * sr));
        for (auto* s : { &smWidth, &smBal, &smMicro, &smSpread, &smInt, &smMono })
            s->reset (sr, 0.03);
        smWidth.setCurrentAndTargetValue (1.f);
        const float apL[3] = { 0.55f, 0.42f, 0.63f }, apR[3] = { -0.50f, -0.38f, -0.60f };
        for (int i = 0; i < 3; ++i) { ap1[i].a = apL[i]; ap2[i].a = apR[i]; ap1[i].z = ap2[i].z = 0.f; }
        sideHp.reset();
        sideHp.setHighpass (sr, 150.0);
    }

    void setParams (const Params& p)
    {
        smWidth.setTargetValue (p.width);
        smBal.setTargetValue (p.balance);
        smMicro.setTargetValue (p.microMs * 0.001f * (float) sr);
        smSpread.setTargetValue (p.spread);
        smInt.setTargetValue (p.intensity);
        smMono.setTargetValue (p.mono);
        const double fc = 80.0 + 320.0 * p.mono;      // more mono-compat => bass stays mono up to higher freq
        if (std::abs ((float) fc - lastFc) > 1.f) { sideHp.setHighpass (sr, fc); lastFc = (float) fc; }
    }

    void process (float& l, float& r)
    {
        const float width = smWidth.getNextValue(), bal = smBal.getNextValue(), mono = smMono.getNextValue();
        const float spread = smSpread.getNextValue(), intensity = smInt.getNextValue();
        const float microD = smMicro.getNextValue();

        l *= bal > 0.f ? 1.f - bal : 1.f;
        r *= bal < 0.f ? 1.f + bal : 1.f;
        const float inL = l, inR = r;

        const float m = 0.5f * (l + r);
        float s = 0.5f * (l - r);
        const float sh = sideHp.process (s);
        s = s * (1.f - mono) + sh * mono;
        s *= width * (1.f - 0.25f * mono);
        float l2 = m + s, r2 = m - s;

        const float sp = spread * (1.f - 0.5f * mono);
        float la = l2, ra = r2;
        for (int i = 0; i < 3; ++i) { la = ap1[i].process (la); ra = ap2[i].process (ra); }
        l2 += sp * (la - l2);
        r2 += sp * (ra - r2);

        micro.write (r2);
        const float rd = micro.read (microD);
        const float k = (microD > 0.5f) ? 1.f - 0.6f * mono : 0.f;
        r2 = r2 * (1.f - k) + rd * k;

        l = inL * (1.f - intensity) + l2 * intensity;
        r = inR * (1.f - intensity) + r2 * intensity;
    }

private:
    double sr = 48000.0;
    jt::DelayBuf micro;
    jt::Allpass1 ap1[3], ap2[3];
    jt::Biquad sideHp;
    float lastFc = -1.f;
    juce::SmoothedValue<float> smWidth, smBal, smMicro, smSpread, smInt, smMono;
};
