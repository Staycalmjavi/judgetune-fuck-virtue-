#include "PluginEditor.h"

using namespace PID;

JudgeTuneAudioProcessorEditor::JudgeTuneAudioProcessorEditor (JudgeTuneAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), snake (p), keyboard (p),
      kRetune (p.apvts, retune, "RETUNE SPEED"),
      kHumanize (p.apvts, humanize, "HUMANIZE"), kFlex (p.apvts, flex, "FLEX-TUNE"),
      kCorrection (p.apvts, correction, "CORRECTION"), kTracking (p.apvts, tracking, "TRACKING"),
      kNatural (p.apvts, natural, "NATURALNESS"), kTransition (p.apvts, transition, "TRANSITION"),
      kVibrato (p.apvts, vibrato, "VIBRATO"), kFormantMain (p.apvts, formant, "FORMANT"),
      kPitch (p.apvts, pitch, "PITCH"), kFine (p.apvts, fine, "FINE"), kFormant (p.apvts, formant, "FORMANT"),
      kDblDetune (p.apvts, dblDetune, "DETUNE"), kDblDelay (p.apvts, dblDelay, "DELAY"),
      kDblSep (p.apvts, dblSep, "SEPARATION"), kDblInt (p.apvts, dblInt, "INTENSITY"),
      kDblVolL (p.apvts, dblVolL, "VOL L"), kDblVolR (p.apvts, dblVolR, "VOL R"),
      kDblPitchL (p.apvts, dblPitchL, "PITCH L"), kDblPitchR (p.apvts, dblPitchR, "PITCH R"),
      kWidWidth (p.apvts, widWidth, "WIDTH"), kWidBal (p.apvts, widBal, "BALANCE"),
      kWidMicro (p.apvts, widMicro, "MICRO-DELAY"), kWidSpread (p.apvts, widSpread, "SPREAD"),
      kWidInt (p.apvts, widInt, "INTENSITY"), kWidMono (p.apvts, widMono, "MONO COMPAT"),
      kIn (p.apvts, inGain, "INPUT"), kOut (p.apvts, outGain, "OUTPUT"), kMix (p.apvts, mix, "WET / DRY"),
      kRef (p.apvts, refPitch, "PITCH REF")
{
    setLookAndFeel (&laf);
    setSize (1320, 860);

    for (auto* c : std::initializer_list<juce::Component*> {
             &snake, &keyboard, &trace, &meterIn, &meterOut,
             &kRetune, &kHumanize, &kFlex, &kCorrection, &kTracking, &kNatural, &kTransition, &kVibrato, &kFormantMain,
             &kPitch, &kFine, &kFormant,
             &kDblDetune, &kDblDelay, &kDblSep, &kDblInt, &kDblVolL, &kDblVolR, &kDblPitchL, &kDblPitchR,
             &kWidWidth, &kWidBal, &kWidMicro, &kWidSpread, &kWidInt, &kWidMono,
             &kIn, &kOut, &kMix, &kRef,
             &presetBox, &keyBox, &scaleBox, &prevBtn, &nextBtn, &saveBtn, &undoBtn, &redoBtn, &abA, &abB,
             &settingsBtn, &detectBtn, &preserveBtn, &midiBtn, &bypassBtn, &keyInfo })
        addAndMakeVisible (c);

    keyBox.addItemList (noteNames(), 1);
    scaleBox.addItemList (scaleNames(), 1);
    keyAtt   = std::make_unique<CA> (proc.apvts, key, keyBox);
    scaleAtt = std::make_unique<CA> (proc.apvts, scale, scaleBox);
    preserveAtt = std::make_unique<BA> (proc.apvts, preserve, preserveBtn);
    midiAtt     = std::make_unique<BA> (proc.apvts, midiTarget, midiBtn);
    bypassAtt   = std::make_unique<BA> (proc.apvts, bypass, bypassBtn);
    for (int i = 0; i < 12; ++i)
    {
        noteBtn[i].setButtonText (noteNames()[i]);
        addAndMakeVisible (noteBtn[i]);
        noteAtt[i] = std::make_unique<BA> (proc.apvts, note (i), noteBtn[i]);
    }

    keyInfo.setFont (JT::font (12.f));
    keyInfo.setColour (juce::Label::textColourId, JT::textDim);
    keyInfo.setText ("Detected key: sing a few notes...", juce::dontSendNotification);

    presetBox.setTextWhenNothingSelected ("Select preset...");
    refreshPresetBox();
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id <= 0) return;
        if (id < 1000) proc.loadFactoryPreset (id - 1);
        else if (id - 1000 < userPresets.size()) proc.loadUserPreset (userPresets[id - 1000]);
    };
    prevBtn.onClick = [this] { stepPreset (-1); };
    nextBtn.onClick = [this] { stepPreset (+1); };
    saveBtn.onClick = [this] { promptSavePreset(); };
    undoBtn.onClick = [this] { proc.undoManager.undo(); };
    redoBtn.onClick = [this] { proc.undoManager.redo(); };
    abA.setClickingTogglesState (false); abB.setClickingTogglesState (false);
    abA.onClick = [this] { if (proc.getCurrentSlot() != 0) proc.switchAB(); };
    abB.onClick = [this] { if (proc.getCurrentSlot() != 1) proc.switchAB(); };
    settingsBtn.onClick = [this] { showSettingsMenu(); };
    detectBtn.onClick = [this] { applyDetectedKey(); };

    startTimerHz (30);
}

JudgeTuneAudioProcessorEditor::~JudgeTuneAudioProcessorEditor() { stopTimer(); setLookAndFeel (nullptr); }

//==============================================================================
void JudgeTuneAudioProcessorEditor::refreshPresetBox()
{
    const int keep = presetBox.getSelectedId();
    presetBox.clear (juce::dontSendNotification);
    presetBox.addSectionHeading ("Factory");
    int id = 1;
    for (const auto& f : factoryPresets()) presetBox.addItem (f.name, id++);
    userPresets = proc.getUserPresetFiles();
    if (! userPresets.isEmpty())
    {
        presetBox.addSectionHeading ("User");
        for (int i = 0; i < userPresets.size(); ++i)
            presetBox.addItem (userPresets[i].getFileNameWithoutExtension(), 1000 + i);
    }
    presetBox.setSelectedId (keep, juce::dontSendNotification);
}

void JudgeTuneAudioProcessorEditor::stepPreset (int dir)
{
    const int n = (int) factoryPresets().size();
    int cur = presetBox.getSelectedId();
    if (cur <= 0 || cur >= 1000) cur = dir > 0 ? 0 : n + 1;
    int next = cur + dir;
    if (next < 1) next = n;
    if (next > n) next = 1;
    presetBox.setSelectedId (next, juce::sendNotification);
}

void JudgeTuneAudioProcessorEditor::promptSavePreset()
{
    auto* aw = new juce::AlertWindow ("Save preset", "Preset name:", juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", "My Preset");
    aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int result)
    {
        if (result == 1 && proc.saveUserPreset (aw->getTextEditorContents ("name")))
            refreshPresetBox();
    }), true);
}

void JudgeTuneAudioProcessorEditor::showSettingsMenu()
{
    juce::PopupMenu m;
    const int cur = (int) std::lround (proc.apvts.getParameter (PID::range)->convertFrom0to1 (proc.apvts.getParameter (PID::range)->getValue()));
    m.addSectionHeader ("Vocal range (pitch detector)");
    m.addItem (1, "Low voice  (65-450 Hz)",  true, cur == 0);
    m.addItem (2, "Standard   (80-900 Hz)",  true, cur == 1);
    m.addItem (3, "High voice (130-1200 Hz)", true, cur == 2);
    m.addSeparator();
    m.addItem (10, "Reset all parameters");
    m.addItem (11, "Open user preset folder");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsBtn), [this] (int r)
    {
        if (r >= 1 && r <= 3)
        {
            auto* p = proc.apvts.getParameter (PID::range);
            p->setValueNotifyingHost (p->convertTo0to1 ((float) (r - 1)));
        }
        else if (r == 10)
        {
            proc.undoManager.beginNewTransaction ("Reset all");
            for (auto* p : proc.getParameters()) p->setValueNotifyingHost (p->getDefaultValue());
        }
        else if (r == 11)
            JudgeTuneAudioProcessor::getUserPresetDir().revealToUser();
    });
}

void JudgeTuneAudioProcessorEditor::applyDetectedKey()
{
    int k; bool isMinor; float conf;
    if (! proc.detectKey (k, isMinor, conf))
    {
        keyInfo.setText ("Not enough vocal material yet - sing a phrase first.", juce::dontSendNotification);
        return;
    }
    proc.undoManager.beginNewTransaction ("Detect key");
    auto* kp = proc.apvts.getParameter (PID::key);
    auto* sp = proc.apvts.getParameter (PID::scale);
    kp->setValueNotifyingHost (kp->convertTo0to1 ((float) k));
    sp->setValueNotifyingHost (sp->convertTo0to1 (isMinor ? 2.f : 1.f));
}

//==============================================================================
void JudgeTuneAudioProcessorEditor::timerCallback()
{
    ++tick;
    keyboard.setLive (proc.getVisDet(), proc.getVisTgt(), proc.getVisOut(), proc.uiAllowedMask.load(), proc.uiHeldMask.load());
    trace.push (proc.getVisDet(), proc.getVisOut());

    dbIn  = std::max (juce::Decibels::gainToDecibels (proc.meterIn.load(),  -60.f), dbIn  - 1.8f);
    dbOut = std::max (juce::Decibels::gainToDecibels (proc.meterOut.load(), -60.f), dbOut - 1.8f);
    meterIn.setDb (dbIn); meterOut.setDb (dbOut);

    undoBtn.setEnabled (proc.undoManager.canUndo());
    redoBtn.setEnabled (proc.undoManager.canRedo());
    abA.setToggleState (proc.getCurrentSlot() == 0, juce::dontSendNotification);
    abB.setToggleState (proc.getCurrentSlot() == 1, juce::dontSendNotification);

    if (tick % 15 == 0)
    {
        int k; bool isMinor; float conf;
        if (proc.detectKey (k, isMinor, conf))
            keyInfo.setText ("Detected key: " + noteNames()[k] + (isMinor ? " isMinor" : " major")
                                 + "   (" + juce::String (juce::roundToInt (conf * 100.f)) + "% sure)", juce::dontSendNotification);
    }
}

//==============================================================================
void JudgeTuneAudioProcessorEditor::drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
{
    g.setColour (JT::panel);
    g.fillRoundedRectangle (r.toFloat(), 6.f);
    g.setColour (JT::charcoal.withAlpha (0.8f));
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.f, 1.f);
    g.setColour (JT::pink);
    g.fillRect (r.getX() + 12, r.getY() + 12, 10, 2);
    g.setColour (JT::textDim);
    g.setFont (JT::font (11.f, true).withExtraKerningFactor (0.18f));
    g.drawText (title, r.getX() + 28, r.getY() + 5, r.getWidth() - 40, 16, juce::Justification::centredLeft);
}

void JudgeTuneAudioProcessorEditor::drawGavel (juce::Graphics& g, juce::Rectangle<float> r)
{
    const float s = r.getWidth() / 32.f;
    const auto c = r.getCentre();
    juce::Path handle, head;
    handle.addRoundedRectangle (-1.6f, -4.f, 3.2f, 20.f, 1.6f);
    head.addRoundedRectangle (-10.f, -12.f, 20.f, 9.f, 2.f);
    const auto xf = juce::AffineTransform::scale (s).rotated (-0.7f).translated (c.x, c.y - 2.f);
    handle.applyTransform (xf);
    head.applyTransform (xf);
    g.setColour (JT::pink);
    g.fillPath (handle);
    g.fillPath (head);
    g.setColour (JT::pinkDim);
    g.fillRoundedRectangle (r.getX() - 3.f, r.getBottom() + 2.f, 24.f, 3.f, 1.5f);   // sound block
}

void JudgeTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (JT::bg);
    // top bar
    g.setColour (JT::panel);
    g.fillRect (0, 0, getWidth(), 56);
    g.setColour (JT::charcoal);
    g.drawHorizontalLine (56, 0.f, (float) getWidth());

    drawGavel (g, { 20.f, 8.f, 30.f, 30.f });
    g.setColour (JT::text);
    g.setFont (JT::font (26.f, true).withExtraKerningFactor (0.28f));
    g.drawText ("JUDGETUNE", 62, 8, 260, 34, juce::Justification::centredLeft);
    g.setColour (JT::pink);
    g.fillRect (64, 44, 40, 2);

    drawPanel (g, rDoubler, "DOUBLER");
    drawPanel (g, rWidener, "WIDENER");
    drawPanel (g, rTuning,  "TUNING");
    drawPanel (g, rPF,      "PITCH & FORMANT");
    drawPanel (g, rKey,     "KEY / SCALE");
    drawPanel (g, rIO,      "INPUT / OUTPUT");

    g.setColour (JT::textDim);
    g.setFont (JT::font (10.f, true));
    g.drawText ("IN",  rIO.getX() + 272, rIO.getY() + 16, 30, 12, juce::Justification::centred);
    g.drawText ("OUT", rIO.getX() + 302, rIO.getY() + 16, 30, 12, juce::Justification::centred);
    g.setFont (JT::font (11.f));
    g.drawText ("Click keys to build a custom scale.   Pink dot = in scale.   Outline = held MIDI note.",
                rKey.getX() + 104, rKey.getY() + 140, 320, 40, juce::Justification::topLeft);
    g.setColour (JT::pinkDim);
    g.drawText ("detected", rPF.getX() + 272, rPF.getBottom() - 16, 60, 12, juce::Justification::left);
    g.setColour (JT::text);
    g.drawText ("corrected", rPF.getX() + 338, rPF.getBottom() - 16, 70, 12, juce::Justification::left);
}

void JudgeTuneAudioProcessorEditor::resized()
{
    const int W = getWidth();

    // top bar
    prevBtn.setBounds (360, 14, 28, 28);   presetBox.setBounds (392, 14, 250, 28);
    nextBtn.setBounds (646, 14, 28, 28);   saveBtn.setBounds (680, 14, 60, 28);
    undoBtn.setBounds (770, 14, 62, 28);   redoBtn.setBounds (836, 14, 62, 28);
    abA.setBounds (930, 14, 34, 28);       abB.setBounds (968, 14, 34, 28);
    settingsBtn.setBounds (W - 14 - 100, 14, 100, 28);

    rDoubler = { 14, 68, 304, 230 };
    rWidener = { 14, 308, 304, 212 };
    rTuning  = { 1002, 68, 304, 452 };
    rPF      = { 14, 532, 426, 190 };
    rKey     = { 452, 532, 436, 190 };
    rIO      = { 900, 532, 406, 190 };

    snake.setBounds (330, 64, 660, 462);
    keyboard.setBounds (14, 732, W - 28, 120);

    auto grid = [] (juce::Rectangle<int> panel, int y0, int cols, int cw, int ch, std::initializer_list<Knob*> ks)
    {
        int i = 0;
        for (auto* k : ks)
            k->setBounds (panel.getX() + 8 + (i % cols) * cw, panel.getY() + y0 + (i / cols) * ch, cw, ch), ++i;
    };

    grid (rDoubler, 26, 4, 72, 98, { &kDblDetune, &kDblDelay, &kDblSep, &kDblInt, &kDblVolL, &kDblVolR, &kDblPitchL, &kDblPitchR });
    grid (rWidener, 26, 4, 72, 98, { &kWidWidth, &kWidBal, &kWidMicro, &kWidSpread, &kWidInt, &kWidMono });

    kRetune.setBounds (rTuning.getX() + 57, rTuning.getY() + 24, 190, 206);
    grid (rTuning, 238, 4, 72, 98, { &kHumanize, &kFlex, &kCorrection, &kTracking, &kNatural, &kTransition, &kVibrato, &kFormantMain });

    grid (rPF, 26, 3, 84, 118, { &kPitch, &kFine, &kFormant });
    preserveBtn.setBounds (rPF.getX() + 12, rPF.getY() + 152, 236, 26);
    trace.setBounds (rPF.getX() + 272, rPF.getY() + 28, 142, 128);

    keyBox.setBounds   (rKey.getX() + 10,  rKey.getY() + 28, 80, 26);
    scaleBox.setBounds (rKey.getX() + 98,  rKey.getY() + 28, 150, 26);
    midiBtn.setBounds  (rKey.getX() + 256, rKey.getY() + 28, 90, 26);
    detectBtn.setBounds (rKey.getX() + 352, rKey.getY() + 28, 74, 26);
    for (int i = 0; i < 12; ++i)
        noteBtn[i].setBounds (rKey.getX() + 10 + i * 34, rKey.getY() + 66, 33, 24);
    kRef.setBounds (rKey.getX() + 10, rKey.getY() + 98, 84, 88);
    keyInfo.setBounds (rKey.getX() + 104, rKey.getY() + 104, 320, 22);

    grid (rIO, 26, 3, 84, 118, { &kIn, &kOut, &kMix });
    bypassBtn.setBounds (rIO.getX() + 12, rIO.getY() + 152, 120, 26);
    meterIn.setBounds  (rIO.getX() + 280, rIO.getY() + 32, 14, 146);
    meterOut.setBounds (rIO.getX() + 310, rIO.getY() + 32, 14, 146);
}
