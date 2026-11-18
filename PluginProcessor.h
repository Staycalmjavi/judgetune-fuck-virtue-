#pragma once
#include <JuceHeader.h>
#include <bitset>
#include "Params.h"
#include "TunerEngine.h"
#include "Effects.h"

class JudgeTuneAudioProcessor : public juce::AudioProcessor
{
public:
    JudgeTuneAudioProcessor();
    ~JudgeTuneAudioProcessor() override = default;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "JUDGETUNE"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // State / undo / A-B
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;
    void switchAB();
    int  getCurrentSlot() const { return currentSlot; }

    // Presets
    void loadFactoryPreset (int index);
    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::File& f);
    juce::Array<juce::File> getUserPresetFiles() const;
    static juce::File getUserPresetDir();

    // Key detection (pitch-class histogram + Krumhansl-Schmuckler)
    bool detectKey (int& key, bool& isMinor, float& confidence) const;

    // Visualisation (read from the GUI thread)
    float getVisDet() const    { return engine.visDet.load(); }
    float getVisTgt() const    { return engine.visTgt.load(); }
    float getVisOut() const    { return engine.visOut.load(); }
    float getVisVoiced() const { return engine.visVoiced.load(); }
    std::atomic<float> meterIn { 0.f }, meterOut { 0.f };
    std::atomic<int>   uiAllowedMask { 0xFFF }, uiHeldMask { 0 };

private:
    struct Raw
    {
        std::atomic<float> *retune, *humanize, *flex, *tracking, *correction, *natural, *vibrato, *transition,
            *key, *scale, *midiTarget, *refPitch, *range, *pitch, *fine, *formant, *preserve,
            *dblDetune, *dblDelay, *dblSep, *dblInt, *dblVolL, *dblVolR, *dblPitchL, *dblPitchR,
            *widWidth, *widBal, *widMicro, *widSpread, *widInt, *widMono, *inGain, *outGain, *mix, *bypass;
        std::array<std::atomic<float>*, 12> notes;
    } raw {};

    TunerEngine engine;
    Doubler doubler;
    Widener widener;
    jt::DelayBuf dryL, dryR;
    juce::SmoothedValue<float> smIn, smOut, smMix, smBypass;
    std::bitset<128> held;
    juce::ValueTree abSlots[2];
    int currentSlot = 0;
    double sr = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JudgeTuneAudioProcessor)
};
