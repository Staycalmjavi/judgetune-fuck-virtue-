#pragma once
#include <JuceHeader.h>

//==============================================================================
namespace PID
{
inline const juce::String retune = "retune", humanize = "humanize", flex = "flex", tracking = "tracking",
    correction = "correction", natural = "natural", vibrato = "vibrato", transition = "transition",
    key = "key", scale = "scale", midiTarget = "midiTarget", refPitch = "refPitch", range = "range",
    pitch = "pitch", fine = "fine", formant = "formant", preserve = "preserve",
    dblDetune = "dblDetune", dblDelay = "dblDelay", dblSep = "dblSep", dblInt = "dblInt",
    dblVolL = "dblVolL", dblVolR = "dblVolR", dblPitchL = "dblPitchL", dblPitchR = "dblPitchR",
    widWidth = "widWidth", widBal = "widBal", widMicro = "widMicro", widSpread = "widSpread",
    widInt = "widInt", widMono = "widMono",
    inGain = "inGain", outGain = "outGain", mix = "mix", bypass = "bypass";

inline juce::String note (int i) { return "note" + juce::String (i); }
}

//==============================================================================
inline const juce::StringArray& noteNames()
{
    static const juce::StringArray n { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n;
}

inline const juce::StringArray& scaleNames()
{
    static const juce::StringArray n { "Chromatic", "Major", "Minor", "Harmonic Minor", "Pentatonic Major",
                                       "Pentatonic Minor", "Blues", "Dorian", "Phrygian", "Lydian",
                                       "Mixolydian", "Custom" };
    return n;
}

// Returns a 12-bit mask of allowed absolute pitch classes (bit 0 = C).
inline int scaleMask (int scaleIdx, int key, int customMask)
{
    auto m = [] (std::initializer_list<int> iv) { int r = 0; for (int i : iv) r |= 1 << i; return r; };
    static const int rel[] = {
        0xFFF,                          // Chromatic
        m ({ 0, 2, 4, 5, 7, 9, 11 }),   // Major
        m ({ 0, 2, 3, 5, 7, 8, 10 }),   // Minor
        m ({ 0, 2, 3, 5, 7, 8, 11 }),   // Harmonic minor
        m ({ 0, 2, 4, 7, 9 }),          // Pentatonic major
        m ({ 0, 3, 5, 7, 10 }),         // Pentatonic minor
        m ({ 0, 3, 5, 6, 7, 10 }),      // Blues
        m ({ 0, 2, 3, 5, 7, 9, 10 }),   // Dorian
        m ({ 0, 1, 3, 5, 7, 8, 10 }),   // Phrygian
        m ({ 0, 2, 4, 6, 7, 9, 11 }),   // Lydian
        m ({ 0, 2, 4, 5, 7, 9, 10 })    // Mixolydian
    };
    if (scaleIdx >= 11) return customMask != 0 ? customMask : 0xFFF;
    int out = 0;
    for (int i = 0; i < 12; ++i)
        if ((rel[juce::jlimit (0, 10, scaleIdx)] >> i) & 1) out |= 1 << ((i + key) % 12);
    return out;
}

//==============================================================================
inline std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& id, const juce::String& name,
                                                             float lo, float hi, float def,
                                                             const juce::String& unit, int decimals, float interval = 0.f)
{
    auto attrs = juce::AudioParameterFloatAttributes()
        .withStringFromValueFunction ([unit, decimals] (float v, int) { return juce::String (v, decimals) + unit; })
        .withValueFromStringFunction ([] (const juce::String& t) { return t.retainCharacters ("-0123456789.").getFloatValue(); });
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                                                        juce::NormalisableRange<float> (lo, hi, interval), def, attrs);
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace PID;
    juce::AudioProcessorValueTreeState::ParameterLayout L;

    // --- main tuning --------------------------------------------------------
    L.add (makeFloat (retune,     "Retune Speed",     -10.f, 100.f, 70.f, "",     1, 0.1f));
    L.add (makeFloat (humanize,   "Humanize",           0.f, 100.f,  0.f, " %",   0, 0.1f));
    L.add (makeFloat (flex,       "Flex-Tune",          0.f, 100.f,  0.f, " %",   0, 0.1f));
    L.add (makeFloat (tracking,   "Tracking",           0.f, 100.f, 60.f, " %",   0, 0.1f));
    L.add (makeFloat (correction, "Correction Amount",  0.f, 100.f, 100.f, " %",  0, 0.1f));
    L.add (makeFloat (natural,    "Naturalness",        0.f, 100.f,  0.f, " %",   0, 0.1f));
    L.add (makeFloat (vibrato,    "Vibrato Control",    0.f, 200.f,  0.f, " %",   0, 0.5f));
    L.add (makeFloat (transition, "Note Transition",    0.f, 300.f,  0.f, " ms",  0, 0.5f));

    L.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { key, 1 }, "Key", noteNames(), 0));
    L.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { scale, 1 }, "Scale", scaleNames(), 0));
    for (int i = 0; i < 12; ++i)
    {
        const bool inMajor = (1 << i) & 0b101010110101;   // C major as the starting custom scale
        L.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { note (i), 1 }, "Custom " + noteNames()[i], inMajor));
    }
    L.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { midiTarget, 1 }, "MIDI Target", true));
    L.add (makeFloat (refPitch, "Pitch Reference", 415.f, 466.f, 440.f, " Hz", 1, 0.1f));
    L.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { range, 1 }, "Vocal Range",
           juce::StringArray { "Low (65-450 Hz)", "Standard (80-900 Hz)", "High (130-1200 Hz)" }, 1));

    // --- pitch & formant ----------------------------------------------------
    L.add (makeFloat (pitch,   "Pitch",   -24.f, 24.f, 0.f, " st", 2, 0.01f));
    L.add (makeFloat (fine,    "Fine",   -100.f, 100.f, 0.f, " ct", 1, 0.1f));
    L.add (makeFloat (formant, "Formant", -12.f, 12.f, 0.f, " st", 2, 0.01f));
    L.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { preserve, 1 }, "Preserve Formants", true));

    // --- doubler ------------------------------------------------------------
    L.add (makeFloat (dblDetune, "Dbl Detune",     0.f,  50.f, 10.f, " ct", 1, 0.1f));
    L.add (makeFloat (dblDelay,  "Dbl Delay",      0.f,  60.f, 15.f, " ms", 1, 0.1f));
    L.add (makeFloat (dblSep,    "Dbl Separation", 0.f, 100.f, 70.f, " %",  0, 0.5f));
    L.add (makeFloat (dblInt,    "Dbl Intensity",  0.f, 100.f,  0.f, " %",  0, 0.5f));
    L.add (makeFloat (dblVolL,   "Dbl Vol L",    -24.f,   6.f, -3.f, " dB", 1, 0.1f));
    L.add (makeFloat (dblVolR,   "Dbl Vol R",    -24.f,   6.f, -3.f, " dB", 1, 0.1f));
    L.add (makeFloat (dblPitchL, "Dbl Pitch L",  -12.f,  12.f,  0.f, " st", 2, 0.01f));
    L.add (makeFloat (dblPitchR, "Dbl Pitch R",  -12.f,  12.f,  0.f, " st", 2, 0.01f));

    // --- widener ------------------------------------------------------------
    L.add (makeFloat (widWidth,  "Width",         0.f, 200.f, 100.f, " %",  0, 0.5f));
    L.add (makeFloat (widBal,    "Balance",    -100.f, 100.f,   0.f, " %",  0, 0.5f));
    L.add (makeFloat (widMicro,  "Micro-Delay",    0.f,  30.f,   0.f, " ms", 1, 0.1f));
    L.add (makeFloat (widSpread, "Spread",         0.f, 100.f,   0.f, " %",  0, 0.5f));
    L.add (makeFloat (widInt,    "Width Intensity", 0.f, 100.f,  0.f, " %",  0, 0.5f));
    L.add (makeFloat (widMono,   "Mono Compat",    0.f, 100.f,  50.f, " %",  0, 0.5f));

    // --- I/O ------------------------------------------------------------------
    L.add (makeFloat (inGain,  "Input",  -24.f, 24.f, 0.f, " dB", 1, 0.1f));
    L.add (makeFloat (outGain, "Output", -24.f, 24.f, 0.f, " dB", 1, 0.1f));
    L.add (makeFloat (mix,     "Mix",      0.f, 100.f, 100.f, " %", 0, 0.5f));
    L.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { bypass, 1 }, "Bypass", false));
    return L;
}

//==============================================================================
// Parameters a preset is allowed to touch (everything "sound design").  Key, scale,
// custom notes, MIDI mode, pitch reference, vocal range and the I/O section are
// left alone so loading a preset never changes your song's key or your gain staging.
inline const juce::StringArray& presetResetIDs()
{
    using namespace PID;
    static const juce::StringArray ids {
        retune, humanize, flex, tracking, correction, natural, vibrato, transition,
        pitch, fine, formant, preserve,
        dblDetune, dblDelay, dblSep, dblInt, dblVolL, dblVolR, dblPitchL, dblPitchR,
        widWidth, widBal, widMicro, widSpread, widInt, widMono };
    return ids;
}

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    using namespace PID;
    static const std::vector<FactoryPreset> P = {
        { "Hard Auto-Tune", { { retune, 100 }, { tracking, 75 }, { correction, 100 }, { vibrato, 0 } } },

        { "Melodic Rap", { { retune, 35 }, { humanize, 40 }, { flex, 25 }, { tracking, 60 }, { correction, 90 },
                           { natural, 25 }, { vibrato, 60 }, { transition, 25 },
                           { dblInt, 25 }, { dblDetune, 8 }, { dblDelay, 14 }, { dblSep, 60 },
                           { widWidth, 130 }, { widInt, 30 }, { widMono, 60 } } },

        { "Underground Rap", { { retune, 55 }, { humanize, 15 }, { flex, 10 }, { tracking, 65 }, { natural, 10 },
                               { vibrato, 20 }, { transition, 8 }, { formant, -1.5f },
                               { dblInt, 40 }, { dblDelay, 22 }, { dblDetune, 14 }, { dblVolL, -4 }, { dblVolR, -4 },
                               { widWidth, 120 }, { widInt, 30 } } },

        { "Pluggnb", { { retune, 70 }, { humanize, 20 }, { tracking, 70 }, { natural, 5 }, { vibrato, 30 },
                       { transition, 12 }, { formant, 1.0f },
                       { dblInt, 55 }, { dblDetune, 18 }, { dblDelay, 18 }, { dblSep, 85 },
                       { widWidth, 160 }, { widInt, 60 }, { widMicro, 8 }, { widSpread, 40 } } },

        { "Rage Vocals", { { retune, 95 }, { tracking, 80 }, { formant, 2.5f }, { preserve, 0 },
                           { dblInt, 70 }, { dblDetune, 22 }, { dblDelay, 10 }, { dblSep, 100 },
                           { dblPitchL, -12 }, { dblVolL, -10 }, { dblPitchR, 12 }, { dblVolR, -12 },
                           { widWidth, 180 }, { widInt, 70 }, { widSpread, 60 } } },

        { "Hyperpop", { { retune, -3 }, { tracking, 85 }, { formant, 4 }, { preserve, 0 },
                        { dblInt, 60 }, { dblDetune, 25 }, { dblDelay, 8 }, { dblPitchL, 12 }, { dblVolL, -9 },
                        { dblPitchR, -5 }, { dblVolR, -11 },
                        { widWidth, 170 }, { widInt, 70 }, { widSpread, 50 }, { widMicro, 6 } } },

        { "Robotic Vocals", { { retune, 100 }, { tracking, 90 }, { formant, -3 }, { preserve, 1 },
                              { dblInt, 30 }, { dblDetune, 3 }, { dblDelay, 3 }, { dblSep, 40 },
                              { widWidth, 100 } } },

        { "Natural Tuning", { { retune, 22 }, { humanize, 60 }, { flex, 55 }, { tracking, 45 }, { correction, 80 },
                              { natural, 70 }, { vibrato, 100 }, { transition, 60 } } },

        { "Extreme Pitch Correction", { { retune, 100 }, { tracking, 100 }, { correction, 100 }, { vibrato, 0 },
                                        { flex, 0 }, { natural, 0 }, { humanize, 0 }, { transition, 0 } } },

        { "Wide Vocals", { { retune, 45 }, { humanize, 30 }, { tracking, 60 }, { natural, 20 }, { vibrato, 50 },
                           { dblInt, 35 }, { dblDetune, 10 }, { dblDelay, 20 }, { dblSep, 70 },
                           { widWidth, 190 }, { widInt, 85 }, { widSpread, 55 }, { widMicro, 10 }, { widMono, 55 } } },

        { "Doubled Vocals", { { retune, 40 }, { humanize, 25 }, { natural, 20 }, { vibrato, 70 },
                              { dblInt, 80 }, { dblDetune, 12 }, { dblDelay, 16 }, { dblSep, 100 },
                              { dblVolL, -3 }, { dblVolR, -3 },
                              { widWidth, 110 }, { widInt, 40 } } },
    };
    return P;
}
