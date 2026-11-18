#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
JudgeTuneAudioProcessor::JudgeTuneAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "JUDGETUNE", createParameterLayout())
{
    auto P = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
    using namespace PID;
    raw.retune = P (retune);   raw.humanize = P (humanize);   raw.flex = P (flex);
    raw.tracking = P (tracking); raw.correction = P (correction); raw.natural = P (natural);
    raw.vibrato = P (vibrato); raw.transition = P (transition);
    raw.key = P (key); raw.scale = P (scale); raw.midiTarget = P (midiTarget);
    raw.refPitch = P (refPitch); raw.range = P (range);
    raw.pitch = P (pitch); raw.fine = P (fine); raw.formant = P (formant); raw.preserve = P (preserve);
    raw.dblDetune = P (dblDetune); raw.dblDelay = P (dblDelay); raw.dblSep = P (dblSep); raw.dblInt = P (dblInt);
    raw.dblVolL = P (dblVolL); raw.dblVolR = P (dblVolR); raw.dblPitchL = P (dblPitchL); raw.dblPitchR = P (dblPitchR);
    raw.widWidth = P (widWidth); raw.widBal = P (widBal); raw.widMicro = P (widMicro);
    raw.widSpread = P (widSpread); raw.widInt = P (widInt); raw.widMono = P (widMono);
    raw.inGain = P (inGain); raw.outGain = P (outGain); raw.mix = P (mix); raw.bypass = P (bypass);
    for (int i = 0; i < 12; ++i) raw.notes[(size_t) i] = P (note (i));
}

bool JudgeTuneAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    return l.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        || l.getMainInputChannelSet() == juce::AudioChannelSet::mono();
}

void JudgeTuneAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    engine.prepare (sr);
    doubler.prepare (sr);
    widener.prepare (sr);
    const int lat = engine.getLatency();
    dryL.prepare (lat + 8); dryR.prepare (lat + 8);
    setLatencySamples (lat);
    for (auto* s : { &smIn, &smOut, &smMix, &smBypass }) s->reset (sr, 0.03);
    smIn.setCurrentAndTargetValue (1.f); smOut.setCurrentAndTargetValue (1.f);
    smMix.setCurrentAndTargetValue (1.f); smBypass.setCurrentAndTargetValue (0.f);
    held.reset();
}

//==============================================================================
void JudgeTuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = buffer.getNumChannels(), numSamples = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // --- MIDI: held notes become the tuning targets --------------------------
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())            held.set ((size_t) m.getNoteNumber());
        else if (m.isNoteOff())      held.reset ((size_t) m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) held.reset();
    }
    int heldMask = 0;
    for (int i = 0; i < 128; ++i) if (held.test ((size_t) i)) heldMask |= 1 << (i % 12);
    uiHeldMask.store (heldMask);

    // --- parameter snapshot --------------------------------------------------
    TuneParams tp;
    tp.retune = *raw.retune; tp.humanize = *raw.humanize; tp.flex = *raw.flex; tp.tracking = *raw.tracking;
    tp.correction = *raw.correction; tp.natural = *raw.natural; tp.vibrato = *raw.vibrato;
    tp.transition = *raw.transition;
    tp.shiftSemis = *raw.pitch + *raw.fine * 0.01f;
    tp.formantSemis = *raw.formant;
    tp.preserve = *raw.preserve > 0.5f;
    tp.refHz = *raw.refPitch;

    int custom = 0;
    for (int i = 0; i < 12; ++i) if (*raw.notes[(size_t) i] > 0.5f) custom |= 1 << i;
    tp.mask = scaleMask ((int) *raw.scale, (int) *raw.key, custom);
    tp.heldMask = (*raw.midiTarget > 0.5f) ? heldMask : 0;
    uiAllowedMask.store (tp.heldMask != 0 ? tp.heldMask : tp.mask);

    switch ((int) *raw.range)
    {
        case 0:  tp.fmin = 65.f;  tp.fmax = 450.f;  break;
        case 2:  tp.fmin = 130.f; tp.fmax = 1200.f; break;
        default: tp.fmin = 80.f;  tp.fmax = 900.f;  break;
    }

    Doubler::Params dp;
    dp.detuneCents = *raw.dblDetune; dp.delayMs = *raw.dblDelay; dp.separation = *raw.dblSep * 0.01f;
    dp.intensity = *raw.dblInt * 0.01f;
    dp.gainL = juce::Decibels::decibelsToGain (*raw.dblVolL); dp.gainR = juce::Decibels::decibelsToGain (*raw.dblVolR);
    dp.pitchL = *raw.dblPitchL; dp.pitchR = *raw.dblPitchR;
    doubler.setParams (dp);

    Widener::Params wp;
    wp.width = *raw.widWidth * 0.01f; wp.balance = *raw.widBal * 0.01f; wp.microMs = *raw.widMicro;
    wp.spread = *raw.widSpread * 0.01f; wp.intensity = *raw.widInt * 0.01f; wp.mono = *raw.widMono * 0.01f;
    widener.setParams (wp);

    smIn.setTargetValue (juce::Decibels::decibelsToGain (*raw.inGain));
    smOut.setTargetValue (juce::Decibels::decibelsToGain (*raw.outGain));
    smMix.setTargetValue (*raw.mix * 0.01f);
    smBypass.setTargetValue (*raw.bypass > 0.5f ? 1.f : 0.f);

    const int lat = engine.getLatency();
    float* chL = buffer.getWritePointer (0);
    float* chR = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    float peakIn = 0.f, peakOut = 0.f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float g = smIn.getNextValue();
        float l = chL[i] * g;
        float r = (chR != nullptr ? chR[i] : chL[i]) * g;
        peakIn = std::max (peakIn, std::max (std::abs (l), std::abs (r)));

        // latency-aligned dry path
        dryL.write (l); dryR.write (r);
        const float dl = dryL.read ((float) lat), dr = dryR.read ((float) lat);

        // tuner -> doubler -> widener
        const float tuned = engine.process (0.5f * (l + r), tp);
        float wl, wr;
        doubler.process (tuned, wl, wr);
        widener.process (wl, wr);

        const float mix = smMix.getNextValue(), byp = smBypass.getNextValue(), og = smOut.getNextValue();
        float oL = dl * (1.f - mix) + wl * mix;
        float oR = dr * (1.f - mix) + wr * mix;
        oL = (oL * (1.f - byp) + dl * byp) * og;
        oR = (oR * (1.f - byp) + dr * byp) * og;

        chL[i] = oL;
        if (chR != nullptr) chR[i] = oR;
        peakOut = std::max (peakOut, std::max (std::abs (oL), std::abs (oR)));
    }
    meterIn.store (peakIn);
    meterOut.store (peakOut);
}

//==============================================================================
bool JudgeTuneAudioProcessor::detectKey (int& key, bool& isMinor, float& confidence) const
{
    float h[12]; float tot = 0.f;
    for (int i = 0; i < 12; ++i) { h[i] = engine.pcHist[(size_t) i].load(); tot += h[i]; }
    if (tot < 120.f) return false;   // need roughly 0.6 s of voiced material

    static const float majorProf[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    static const float minorProf[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

    auto corr = [] (const float* a, const float* b)
    {
        float ma = 0, mb = 0;
        for (int i = 0; i < 12; ++i) { ma += a[i]; mb += b[i]; }
        ma /= 12; mb /= 12;
        float num = 0, da = 0, db = 0;
        for (int i = 0; i < 12; ++i) { num += (a[i] - ma) * (b[i] - mb); da += (a[i] - ma) * (a[i] - ma); db += (b[i] - mb) * (b[i] - mb); }
        return num / (std::sqrt (da * db) + 1e-9f);
    };

    float best = -2.f, second = -2.f; int bk = 0; bool bm = false;
    for (int k = 0; k < 12; ++k)
    {
        float rot[12];
        for (int i = 0; i < 12; ++i) rot[i] = h[(i + k) % 12];
        for (int mode = 0; mode < 2; ++mode)
        {
            const float c = corr (rot, mode == 0 ? majorProf : minorProf);
            if (c > best) { second = best; best = c; bk = k; bm = (mode == 1); }
            else if (c > second) second = c;
        }
    }
    key = bk; isMinor = bm; confidence = juce::jlimit (0.f, 1.f, (best - second) * 6.f + 0.3f);
    return true;
}

//==============================================================================
void JudgeTuneAudioProcessor::switchAB()
{
    abSlots[currentSlot] = apvts.copyState();
    currentSlot ^= 1;
    if (abSlots[currentSlot].isValid())
        apvts.replaceState (abSlots[currentSlot].createCopy());
    else
        abSlots[currentSlot] = apvts.copyState();
}

void JudgeTuneAudioProcessor::loadFactoryPreset (int index)
{
    const auto& presets = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;

    undoManager.beginNewTransaction ("Load preset");
    for (const auto& id : presetResetIDs())
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->getDefaultValue());

    for (const auto& [id, value] : presets[(size_t) index].values)
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
}

juce::File JudgeTuneAudioProcessor::getUserPresetDir()
{
    auto d = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                 .getChildFile ("JUDGETUNE").getChildFile ("Presets");
    d.createDirectory();
    return d;
}

juce::Array<juce::File> JudgeTuneAudioProcessor::getUserPresetFiles() const
{
    return getUserPresetDir().findChildFiles (juce::File::findFiles, false, "*.jtpreset");
}

bool JudgeTuneAudioProcessor::saveUserPreset (const juce::String& name)
{
    auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) return false;
    auto xml = apvts.copyState().createXml();
    return xml != nullptr && xml->writeTo (getUserPresetDir().getChildFile (clean + ".jtpreset"));
}

bool JudgeTuneAudioProcessor::loadUserPreset (const juce::File& f)
{
    if (auto xml = juce::XmlDocument::parse (f))
        if (xml->hasTagName (apvts.state.getType()))
        {
            undoManager.beginNewTransaction ("Load preset");
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            return true;
        }
    return false;
}

void JudgeTuneAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void JudgeTuneAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* JudgeTuneAudioProcessor::createEditor() { return new JudgeTuneAudioProcessorEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new JudgeTuneAudioProcessor(); }
