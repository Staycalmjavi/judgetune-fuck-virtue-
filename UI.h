#pragma once
#include <JuceHeader.h>
#include "Params.h"
#include "PluginProcessor.h"

//==============================================================================
// Palette: matte black, matte pink, charcoal. No neon, no glow.
namespace JT
{
inline const juce::Colour bg        { 0xff0a0a0b };
inline const juce::Colour panel     { 0xff131315 };
inline const juce::Colour panelHi   { 0xff1b1b1f };
inline const juce::Colour charcoal  { 0xff2b2b30 };
inline const juce::Colour charcoalHi{ 0xff3a3a41 };
inline const juce::Colour pink      { 0xffc4738d };   // matte pink
inline const juce::Colour pinkHi    { 0xffdc9bb0 };   // muted highlight
inline const juce::Colour pinkDim   { 0xff6e3f50 };
inline const juce::Colour text      { 0xffeceaec };
inline const juce::Colour textDim   { 0xff8f8f97 };

inline juce::Font font (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

inline juce::String noteName (float midi)
{
    const int n = (int) std::lround (midi);
    return noteNames()[((n % 12) + 12) % 12] + juce::String (n / 12 - 1);
}
}

//==============================================================================
class JudgeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    JudgeLookAndFeel()
    {
        using juce::Slider; using juce::Label; using juce::ComboBox; using juce::PopupMenu; using juce::TextButton;
        setColour (juce::ResizableWindow::backgroundColourId, JT::bg);
        setColour (Slider::textBoxTextColourId, JT::text);
        setColour (Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (Slider::textBoxHighlightColourId, JT::pinkDim);
        setColour (Label::textColourId, JT::text);
        setColour (ComboBox::backgroundColourId, JT::panelHi);
        setColour (ComboBox::textColourId, JT::text);
        setColour (ComboBox::outlineColourId, JT::charcoal);
        setColour (ComboBox::arrowColourId, JT::pink);
        setColour (PopupMenu::backgroundColourId, JT::panel);
        setColour (PopupMenu::textColourId, JT::text);
        setColour (PopupMenu::highlightedBackgroundColourId, JT::pinkDim);
        setColour (PopupMenu::highlightedTextColourId, JT::text);
        setColour (PopupMenu::headerTextColourId, JT::pink);
        setColour (TextButton::buttonColourId, JT::panelHi);
        setColour (TextButton::buttonOnColourId, JT::pinkDim);
        setColour (TextButton::textColourOffId, JT::text);
        setColour (TextButton::textColourOnId, JT::text);
        setColour (juce::AlertWindow::backgroundColourId, JT::panel);
        setColour (juce::AlertWindow::textColourId, JT::text);
        setColour (juce::TextEditor::backgroundColourId, JT::panelHi);
        setColour (juce::TextEditor::textColourId, JT::text);
        setColour (juce::TextEditor::outlineColourId, JT::charcoal);
        setColour (juce::TextEditor::focusedOutlineColourId, JT::pink);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.f);
        const float r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto  c = bounds.getCentre();
        const float ringW = std::max (2.f, r * 0.11f);
        const float rr = r - ringW * 0.5f;

        juce::Path track;
        track.addCentredArc (c.x, c.y, rr, rr, 0.f, a0, a1, true);
        g.setColour (JT::charcoal);
        g.strokePath (track, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        const float ang = a0 + pos * (a1 - a0);
        float from = a0, to = ang;
        if (s.getMinimum() < 0.0 && std::abs (s.getMinimum() + s.getMaximum()) < 1e-6)   // symmetric => bipolar
        {
            const float zero = a0 + 0.5f * (a1 - a0);
            from = std::min (zero, ang); to = std::max (zero, ang);
        }
        if (to - from > 0.001f)
        {
            juce::Path val;
            val.addCentredArc (c.x, c.y, rr, rr, 0.f, from, to, true);
            g.setColour (s.isEnabled() ? JT::pink : JT::pinkDim);
            g.strokePath (val, juce::PathStrokeType (ringW, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }

        const float br = rr - ringW * 0.95f;      // matte black knob body
        juce::ColourGradient body (juce::Colour (0xff232327), c.x - br * 0.45f, c.y - br * 0.55f,
                                   juce::Colour (0xff09090a), c.x + br * 0.55f, c.y + br * 0.65f, true);
        g.setGradientFill (body);
        g.fillEllipse (c.x - br, c.y - br, br * 2.f, br * 2.f);
        g.setColour (JT::charcoalHi.withAlpha (0.55f));
        g.drawEllipse (c.x - br, c.y - br, br * 2.f, br * 2.f, 1.f);

        juce::Path ind;
        const float s0 = br * 0.38f, s1 = br * 0.86f;
        ind.startNewSubPath (c.x + s0 * std::sin (ang), c.y - s0 * std::cos (ang));
        ind.lineTo          (c.x + s1 * std::sin (ang), c.y - s1 * std::cos (ang));
        g.setColour (JT::pink);
        g.strokePath (ind, juce::PathStrokeType (std::max (2.f, br * 0.09f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hover, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.f);
        const bool on = b.getToggleState();
        g.setColour (on ? JT::pinkDim : (hover ? JT::charcoal : JT::panelHi));
        g.fillRoundedRectangle (r, 4.f);
        g.setColour (on ? JT::pink : JT::charcoal);
        g.drawRoundedRectangle (r, 4.f, 1.f);
        g.setColour (on ? JT::text : JT::textDim);
        g.setFont (JT::font (11.f, true));
        g.drawText (b.getButtonText(), r, juce::Justification::centred);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.f);
        const bool on = b.getToggleState();
        g.setColour (on ? JT::pinkDim : (down ? JT::charcoalHi : (hover ? JT::charcoal : JT::panelHi)));
        g.fillRoundedRectangle (r, 4.f);
        g.setColour (on ? JT::pink : JT::charcoal);
        g.drawRoundedRectangle (r, 4.f, 1.f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override { return JT::font (12.f, true); }
    juce::Font getComboBoxFont (juce::ComboBox&) override          { return JT::font (13.f); }
};

//==============================================================================
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& labelText)
    {
        label.setText (labelText, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (JT::font (10.5f, true).withExtraKerningFactor (0.06f));
        label.setColour (juce::Label::textColourId, JT::textDim);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);
        addAndMakeVisible (slider);

        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (220);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);
        if (auto* p = apvts.getParameter (paramID))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    }

    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds (b.removeFromTop (14));
        slider.setBounds (b);
    }

private:
    juce::Label label;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
class LevelMeter : public juce::Component
{
public:
    void setDb (float d) { if (std::abs (d - db) > 0.05f) { db = d; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (JT::bg);  g.fillRoundedRectangle (r, 2.f);
        const int segs = 28;
        const float segH = r.getHeight() / (float) segs;
        const int lit = (int) std::lround (juce::jlimit (0.f, 1.f, (db + 60.f) / 60.f) * (float) segs);
        for (int i = 0; i < segs; ++i)
        {
            auto seg = juce::Rectangle<float> (r.getX() + 1.f, r.getBottom() - (float) (i + 1) * segH + 1.f, r.getWidth() - 2.f, segH - 1.5f);
            const bool on = i < lit;
            juce::Colour col = i >= segs - 3 ? JT::pinkHi : (i >= segs - 8 ? JT::pink : JT::pinkDim);
            g.setColour (on ? col : JT::charcoal.withAlpha (0.5f));
            g.fillRect (seg);
        }
    }

private:
    float db = -60.f;
};

//==============================================================================
// Scrolling detected-vs-corrected pitch trace
class PitchTrace : public juce::Component
{
public:
    PitchTrace() { for (int i = 0; i < N; ++i) det[i] = out[i] = -1.f; }

    void push (float d, float o) { det[idx] = d; out[idx] = o; idx = (idx + 1) % N; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (JT::bg); g.fillRoundedRectangle (r, 4.f);
        g.setColour (JT::charcoal); g.drawRoundedRectangle (r.reduced (0.5f), 4.f, 1.f);

        float sum = 0.f; int cnt = 0;
        for (int i = 0; i < N; ++i) if (out[i] > 0.f) { sum += out[i]; ++cnt; }
        const float centre = cnt > 0 ? sum / (float) cnt : 60.f;
        const float pxPerSemi = r.getHeight() / 14.f;

        g.setColour (JT::charcoal.withAlpha (0.6f));
        for (int s = -7; s <= 7; ++s)
        {
            const float y = r.getCentreY() - ((float) s) * pxPerSemi;
            g.drawHorizontalLine ((int) y, r.getX() + 2.f, r.getRight() - 2.f);
        }

        auto trace = [&] (const float* v, juce::Colour col, float thick)
        {
            juce::Path p; bool pen = false;
            for (int i = 0; i < N; ++i)
            {
                const float val = v[(idx + i) % N];
                if (val < 0.f) { pen = false; continue; }
                const float x = r.getX() + 2.f + (r.getWidth() - 4.f) * (float) i / (float) (N - 1);
                const float y = juce::jlimit (r.getY() + 2.f, r.getBottom() - 2.f, r.getCentreY() - (val - centre) * pxPerSemi);
                if (! pen) { p.startNewSubPath (x, y); pen = true; } else p.lineTo (x, y);
            }
            g.setColour (col);
            g.strokePath (p, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        trace (det, JT::pinkDim, 1.4f);
        trace (out, JT::text, 1.6f);
    }

private:
    static constexpr int N = 150;
    float det[N], out[N];
    int idx = 0;
};

//==============================================================================
// 88-key keyboard: shows detected + corrected note, scale membership, held MIDI keys.
// Click a key to toggle its pitch class in the Custom scale.
class PianoKeyboard : public juce::Component
{
public:
    explicit PianoKeyboard (JudgeTuneAudioProcessor& p) : proc (p)
    {
        int wi = 0;
        for (int m = 21; m <= 108; ++m)
        {
            whiteIndex[m] = isBlack (m) ? -1 : wi;
            if (! isBlack (m)) ++wi;
        }
        numWhite = wi;
    }

    void setLive (float det, float tgt, float out, int mask, int heldMask)
    {
        liveDet = det >= 0.f ? (int) std::lround (det) : -1;
        liveTgt = tgt >= 0.f && det >= 0.f ? (int) std::lround (tgt) : -1;
        liveOut = out >= 0.f ? (int) std::lround (out) : -1;
        allowed = mask; held = heldMask;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        for (int m = 21; m <= 108; ++m) if (! isBlack (m)) drawKey (g, m);
        for (int m = 21; m <= 108; ++m) if (isBlack (m))  drawKey (g, m);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int pass = 0; pass < 2; ++pass)            // black keys first
            for (int m = 21; m <= 108; ++m)
                if (isBlack (m) == (pass == 0) && keyRect (m).contains (e.position))
                {
                    toggleCustom (m % 12);
                    return;
                }
    }

private:
    static bool isBlack (int m) { const int pc = m % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }

    juce::Rectangle<float> keyRect (int m) const
    {
        const float wk = (float) getWidth() / (float) numWhite;
        const float bw = wk * 0.62f, bh = (float) getHeight() * 0.62f;
        if (! isBlack (m))
            return { (float) whiteIndex[m] * wk, 0.f, wk, (float) getHeight() };
        const float x = (float) (whiteIndex[m - 1] + 1) * wk - bw * 0.5f;
        return { x, 0.f, bw, bh };
    }

    void drawKey (juce::Graphics& g, int m)
    {
        const auto r = keyRect (m);
        const bool black = isBlack (m);
        const int pc = m % 12;
        const bool inScale = (allowed >> pc) & 1;

        juce::Colour base = black ? juce::Colour (0xff0d0d0f) : juce::Colour (0xffb9b6bb);
        g.setColour (base);
        g.fillRect (r.reduced (black ? 0.f : 0.5f, 0.f));

        if (m == liveDet)       { g.setColour (JT::pinkDim.withAlpha (black ? 0.95f : 0.80f)); g.fillRect (r.reduced (black ? 0.f : 0.5f, 0.f)); }
        if (m == liveOut)       { g.setColour (JT::pink);                                       g.fillRect (r.reduced (black ? 0.f : 0.5f, 0.f)); }
        if ((held >> pc) & 1)   { g.setColour (JT::text.withAlpha (0.9f)); g.drawRect (r.reduced (1.5f), 2.f); }

        g.setColour (juce::Colour (0xff050506));
        g.drawRect (r, 1.f);

        if (inScale)
        {
            g.setColour (m == liveOut || m == liveDet ? JT::text : JT::pink);
            g.fillEllipse (r.getCentreX() - 2.2f, r.getBottom() - (black ? r.getHeight() * 0.14f : 12.f), 4.4f, 4.4f);
        }
        if (pc == 0 && ! black)
        {
            g.setColour (juce::Colour (0xff3a3a41));
            g.setFont (JT::font (9.f, true));
            g.drawText ("C" + juce::String (m / 12 - 1), r.withTrimmedTop (r.getHeight() - 26.f).withHeight (12.f), juce::Justification::centred);
        }
    }

    void toggleCustom (int pc)
    {
        auto& apvts = proc.apvts;
        auto setBool = [&apvts] (int i, bool v)
        {
            if (auto* q = apvts.getParameter (PID::note (i))) q->setValueNotifyingHost (v ? 1.f : 0.f);
        };

        auto* sp = apvts.getParameter (PID::scale);
        const bool isCustom = sp != nullptr && (int) std::lround (sp->convertFrom0to1 (sp->getValue())) == 11;
        proc.undoManager.beginNewTransaction ("Edit custom scale");

        if (isCustom)
        {
            const auto* cur = apvts.getRawParameterValue (PID::note (pc));
            setBool (pc, *cur < 0.5f);
        }
        else
        {
            // Switching to Custom: seed it from the scale that was active, then toggle the clicked note.
            const int cur = proc.uiAllowedMask.load();
            for (int i = 0; i < 12; ++i) setBool (i, ((cur >> i) & 1) != 0);
            setBool (pc, ((cur >> pc) & 1) == 0);
            if (sp != nullptr) sp->setValueNotifyingHost (sp->convertTo0to1 (11.f));
        }
    }

    JudgeTuneAudioProcessor& proc;
    int whiteIndex[128] {};
    int numWhite = 52;
    int liveDet = -1, liveTgt = -1, liveOut = -1, allowed = 0xFFF, held = 0;
};

//==============================================================================
// The centrepiece: the double Ouroboros.  Two serpents, each biting the other's tail.
// Geometry matches Assets/make_ouroboros.py.  It undulates with input level, quickens
// with correction amount, and a ring of 12 pitch classes shows detected vs corrected pitch.
class SnakeVisual : public juce::Component, private juce::Timer
{
public:
    explicit SnakeVisual (JudgeTuneAudioProcessor& p) : proc (p) { startTimerHz (60); }

    void resized() override
    {
        const float R = std::min (getWidth(), getHeight()) * 0.5f - 2.f;
        const auto c = getLocalBounds().toFloat().getCentre();
        juce::Path circle;
        circle.addEllipse (c.x - R * 0.72f, c.y - R * 0.72f, R * 1.44f, R * 1.44f);
        dashed.clear();
        const float dashes[] = { 3.f, 6.f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, circle, dashes, 2);
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const auto c = b.getCentre();
        const float R = std::min (b.getWidth(), b.getHeight()) * 0.5f - 2.f;
        const float pi = juce::MathConstants<float>::pi, twoPi = juce::MathConstants<float>::twoPi;
        auto P = [&] (float a, float r) { return juce::Point<float> (c.x + r * std::cos (a), c.y + r * std::sin (a)); };
        const int mask = proc.uiAllowedMask.load();

        // --- pitch ring: ticks + note labels --------------------------------
        for (int k = 0; k < 120; ++k)
        {
            const float a = -pi * 0.5f + (float) k / 120.f * twoPi;
            const bool major = (k % 10) == 0;
            g.setColour (major ? JT::pink.withAlpha (0.85f) : JT::pinkDim.withAlpha (0.7f));
            g.drawLine ({ P (a, R * 0.84f), P (a, R * (major ? 0.885f : 0.857f)) }, major ? 1.4f : 1.f);
        }
        g.setFont (JT::font (std::max (10.f, R * 0.052f), true));
        for (int pc = 0; pc < 12; ++pc)
        {
            const float a = -pi * 0.5f + (float) pc / 12.f * twoPi;
            const bool on = (mask >> pc) & 1;
            g.setColour (on ? JT::text : JT::textDim.withAlpha (0.45f));
            g.drawText (noteNames()[pc], juce::Rectangle<float> (30.f, 16.f).withCentre (P (a, R * 0.945f)), juce::Justification::centred);
        }
        g.setColour (JT::charcoal);
        g.drawEllipse (c.x - R * 0.80f, c.y - R * 0.80f, R * 1.6f, R * 1.6f, 1.2f);

        // --- slow rotating dashed ring --------------------------------------
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (ringRot, c.x, c.y));
        g.setColour (JT::pinkDim.withAlpha (0.85f));
        g.fillPath (dashed);
        g.restoreState();

        // --- correction arc + markers ----------------------------------------
        if (voiced)
        {
            const float rr = R * 0.80f;
            const float d = wrapPc (outPc - detPc);
            float from = detPc / 12.f * twoPi, to = from + d / 12.f * twoPi;
            if (from > to) std::swap (from, to);
            if (to - from > 0.002f)
            {
                juce::Path arc;
                arc.addCentredArc (c.x, c.y, rr, rr, 0.f, from, to, true);
                g.setColour (JT::pink);
                g.strokePath (arc, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            auto dp = P (-pi * 0.5f + detPc / 12.f * twoPi, rr);
            auto op = P (-pi * 0.5f + outPc / 12.f * twoPi, rr);
            g.setColour (JT::bg);            g.fillEllipse (juce::Rectangle<float> (11.f, 11.f).withCentre (dp));
            g.setColour (JT::pinkHi);        g.drawEllipse (juce::Rectangle<float> (11.f, 11.f).withCentre (dp), 1.6f);
            g.setColour (JT::text);          g.fillEllipse (juce::Rectangle<float> (9.f, 9.f).withCentre (op));
        }

        // --- the Ouroboros: bold single-line serpent, mouth open, swallowing its own tail ---
        drawOuroboros (g, c, R * 0.50f, 0.010f + 0.030f * level + 0.020f * corrS, R * 0.02f + 1.2f * level);

        // --- centre read-out ----------------------------------------------------
        g.setColour (voiced ? JT::text : JT::textDim);
        g.setFont (JT::font (R * 0.15f, true));
        g.drawText (voiced ? JT::noteName (detMidi) : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")),
                    juce::Rectangle<float> (R * 0.6f, R * 0.18f).withCentre ({ c.x, c.y - R * 0.04f }), juce::Justification::centred);
        g.setFont (JT::font (R * 0.05f, true));
        g.setColour (JT::textDim);
        if (voiced)
        {
            const float cents = (detMidi - std::round (detMidi)) * 100.f;
            g.drawText (juce::String (cents, 0) + " ct   >   " + JT::noteName (outMidi),
                        juce::Rectangle<float> (R * 0.6f, R * 0.07f).withCentre ({ c.x, c.y + R * 0.10f }), juce::Justification::centred);
        }
        else
            g.drawText ("LISTENING", juce::Rectangle<float> (R * 0.6f, R * 0.07f).withCentre ({ c.x, c.y + R * 0.10f }), juce::Justification::centred);
    }

private:
    // Unit space: ring mid-radius 1, y down.  Same numbers as Assets/make_ouroboros.py.
    static juce::Array<juce::Point<float>> smoothOpen (const juce::Array<juce::Point<float>>& in, int iterations)
    {
        juce::Array<juce::Point<float>> pts (in);
        for (int it = 0; it < iterations; ++it)
        {
            juce::Array<juce::Point<float>> q;
            q.add (pts.getFirst());
            for (int i = 0; i + 1 < pts.size(); ++i)
            {
                const auto a = pts[i], b = pts[i + 1];
                q.add (a * 0.75f + b * 0.25f);
                q.add (a * 0.25f + b * 0.75f);
            }
            q.add (pts.getLast());
            pts = q;
        }
        return pts;
    }

    void drawOuroboros (juce::Graphics& g, juce::Point<float> c, float S, float amp, float thick)
    {
        constexpr int   N = 220;
        constexpr float HW = 0.16f, A0 = -72.f, A1 = 262.f;
        const float deg2rad = juce::MathConstants<float>::pi / 180.f;
        auto pol = [deg2rad] (float deg, float r) { return juce::Point<float> (r * std::cos (deg * deg2rad), r * std::sin (deg * deg2rad)); };
        auto toS = [&] (juce::Point<float> p) { return juce::Point<float> (c.x + p.x * S, c.y + p.y * S); };

        // body: band whose width tapers to a point inside the mouth
        juce::Path body;
        juce::Array<juce::Point<float>> inner;
        for (int i = 0; i <= N; ++i)
        {
            const float t = (float) i / (float) N;
            const float deg = A0 + (A1 - A0) * t;
            const float env = std::min (1.f, t * 6.f) * std::min (1.f, (1.f - t) * 6.f);
            const float r = 1.f + amp * env * std::sin (t * 10.f - phase);
            const float u = std::max (0.f, (t - 0.78f) / 0.22f);
            const float w = HW * std::max (0.02f, std::pow (std::max (0.f, std::cos (u * juce::MathConstants<float>::halfPi)), 0.9f));
            const auto o = toS (pol (deg, r + w));
            if (i == 0) body.startNewSubPath (o); else body.lineTo (o);
            inner.add (toS (pol (deg, r - w)));
        }
        for (int i = N; i >= 0; --i) body.lineTo (inner[i]);
        body.closeSubPath();
        g.setColour (JT::bg);   g.fillPath (body);
        g.setColour (JT::pink); g.strokePath (body, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // head: open outline (the neck edge is left open so it flows into the body), filled as a closed shape
        juce::Array<juce::Point<float>> h;
        h.add (pol (-50.f, 1.16f)); h.add (pol (-60.f, 1.16f));
        h.add ({ 0.40f, -1.18f }); h.add ({ 0.18f, -1.27f }); h.add ({ -0.06f, -1.30f }); h.add ({ -0.24f, -1.27f });
        h.add ({ -0.33f, -1.20f }); h.add ({ -0.31f, -1.13f });                                   // snout
        h.add ({ -0.10f, -1.07f }); h.add ({ 0.12f, -1.02f }); h.add ({ 0.28f, -0.99f });         // upper jaw -> mouth corner
        h.add ({ 0.12f, -0.95f });  h.add ({ -0.08f, -0.91f }); h.add ({ -0.26f, -0.89f });       // lower jaw edge -> tip
        h.add ({ -0.20f, -0.83f }); h.add (pol (-90.f, 0.84f)); h.add (pol (-70.f, 0.84f)); h.add (pol (-50.f, 0.84f));
        const auto sm = smoothOpen (h, 3);

        juce::Path headStroke;
        for (int i = 0; i < sm.size(); ++i)
        {
            const auto p = toS (sm[i]);
            if (i == 0) headStroke.startNewSubPath (p); else headStroke.lineTo (p);
        }
        juce::Path headFill (headStroke);
        headFill.closeSubPath();
        g.setColour (JT::bg);   g.fillPath (headFill);
        g.setColour (JT::pink); g.strokePath (headStroke, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path eye;
        eye.startNewSubPath (toS ({ -0.17f, -1.185f }));
        eye.lineTo (toS ({ -0.07f, -1.215f }));
        g.strokePath (eye, juce::PathStrokeType (thick * 0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    static float wrapPc (float d) { while (d > 6.f) d -= 12.f; while (d < -6.f) d += 12.f; return d; }

    void timerCallback() override
    {
        detMidi = proc.getVisDet(); outMidi = proc.getVisOut();
        voiced = proc.getVisVoiced() > 0.5f && detMidi >= 0.f;

        const float lvl = juce::jlimit (0.f, 1.f, proc.meterIn.load() * 2.5f);
        level = std::max (lvl, level * 0.93f);
        const float corr = voiced ? juce::jlimit (0.f, 1.f, std::abs (outMidi - detMidi) * 0.5f) : 0.f;
        corrS += (corr - corrS) * 0.15f;

        phase  += 0.05f + 0.30f * level + 0.20f * corrS;
        rot    += 0.0015f + 0.004f * level;
        ringRot -= 0.0035f + 0.012f * corrS;

        if (voiced)
        {
            const float dPc = std::fmod (detMidi, 12.f), oPc = std::fmod (outMidi, 12.f);
            if (! wasVoiced) { detPc = dPc; outPc = oPc; }
            detPc += wrapPc (dPc - detPc) * 0.35f;  outPc += wrapPc (oPc - outPc) * 0.35f;
            detPc = std::fmod (detPc + 12.f, 12.f); outPc = std::fmod (outPc + 12.f, 12.f);
        }
        wasVoiced = voiced;
        repaint();
    }

    JudgeTuneAudioProcessor& proc;
    juce::Path dashed;
    bool voiced = false, wasVoiced = false;
    float detMidi = -1.f, outMidi = -1.f, detPc = 0.f, outPc = 0.f;
    float level = 0.f, corrS = 0.f, phase = 0.f, rot = -juce::MathConstants<float>::halfPi - 0.2f, ringRot = 0.f;
};
