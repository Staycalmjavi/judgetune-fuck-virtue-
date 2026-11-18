#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI.h"

class JudgeTuneAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit JudgeTuneAudioProcessorEditor (JudgeTuneAudioProcessor&);
    ~JudgeTuneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshPresetBox();
    void stepPreset (int dir);
    void promptSavePreset();
    void showSettingsMenu();
    void applyDetectedKey();
    void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String& title);
    void drawGavel (juce::Graphics&, juce::Rectangle<float>);

    JudgeLookAndFeel laf;           // must outlive every child component
    JudgeTuneAudioProcessor& proc;

    SnakeVisual snake;
    PianoKeyboard keyboard;
    PitchTrace trace;
    LevelMeter meterIn, meterOut;
    float dbIn = -60.f, dbOut = -60.f;

    // main tuning
    Knob kRetune, kHumanize, kFlex, kCorrection, kTracking, kNatural, kTransition, kVibrato, kFormantMain;
    // pitch & formant
    Knob kPitch, kFine, kFormant;
    // doubler
    Knob kDblDetune, kDblDelay, kDblSep, kDblInt, kDblVolL, kDblVolR, kDblPitchL, kDblPitchR;
    // widener
    Knob kWidWidth, kWidBal, kWidMicro, kWidSpread, kWidInt, kWidMono;
    // I/O + key
    Knob kIn, kOut, kMix, kRef;

    juce::ComboBox presetBox, keyBox, scaleBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, undoBtn { "UNDO" }, redoBtn { "REDO" },
                     abA { "A" }, abB { "B" }, settingsBtn { "SETTINGS" }, detectBtn { "DETECT KEY" };
    juce::ToggleButton preserveBtn { "PRESERVE FORMANTS" }, midiBtn { "MIDI TARGET" }, bypassBtn { "BYPASS" };
    juce::ToggleButton noteBtn[12];
    juce::Label keyInfo;

    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<BA> preserveAtt, midiAtt, bypassAtt, noteAtt[12];
    std::unique_ptr<CA> keyAtt, scaleAtt;

    juce::Array<juce::File> userPresets;
    juce::Rectangle<int> rDoubler, rWidener, rTuning, rPF, rKey, rIO;
    int tick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JudgeTuneAudioProcessorEditor)
};
