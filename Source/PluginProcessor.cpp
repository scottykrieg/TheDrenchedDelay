#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
static juce::AudioProcessorValueTreeState::ParameterLayout createParams()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto pct = [](float v, int) { return juce::String(v * 100.f, 1) + "%"; };
    auto ms = [](float v, int) { return juce::String(v, 1) + " ms"; };
    auto db = [](float v, int) { return juce::String(v, 1) + " dB"; };
    auto hz = [](float v, int) { return juce::String(v, 0) + " Hz"; };
    (void)pct;

    // --- Core Delay ---
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::delayTimeMs, "Delay Time",
        juce::NormalisableRange<float>(1.f, 2000.f, 0.1f, 0.4f), 250.f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(ms)));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::feedback, "Feedback",
        juce::NormalisableRange<float>(0.f, 0.98f, 0.001f), 0.4f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String(v * 100.f, 1) + "%"; })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::mix, "Mix",
        juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.5f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String(v * 100.f, 1) + "%"; })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::inputGain, "Input Gain",
        juce::NormalisableRange<float>(-24.f, 24.f, 0.1f), 0.f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(db)));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::outputLevel, "Output Level",
        juce::NormalisableRange<float>(-24.f, 12.f, 0.1f), 0.f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(db)));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamID::phaseInvert, "Phase Invert", false));

    // --- Sync / Rhythm ---
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamID::syncEnabled, "Sync", false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamID::noteDivision, "Note Division",
        juce::StringArray{"1/1", "1/2", "1/4", "1/8", "1/16", "1/32"}, 2));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamID::modifier, "Modifier",
        juce::StringArray{"Normal", "Dotted", "Triplet"}, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::gridOffset, "Grid Offset",
        juce::NormalisableRange<float>(-20.f, 20.f, 0.1f), 0.f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(ms)));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamID::lockHold, "Lock/Hold", false));

    // --- Bitcrusher ---
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamID::bcEnabled, "Bitcrusher", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::bcBits, "Bit Depth",
        juce::NormalisableRange<float>(2.f, 16.f, 0.5f), 8.f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::bcSampleRateDiv, "Sample Rate Div",
        juce::NormalisableRange<float>(1.f, 32.f, 0.5f), 1.f));

    // --- Saturation ---
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamID::satEnabled, "Saturation", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::satDrive, "Drive",
        juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.3f));

    // --- Resonant Filter ---
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamID::filtEnabled, "Filter", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamID::filtType, "Filter Type",
        juce::StringArray{"Low Pass", "High Pass", "Band Pass"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::filtCutoff, "Filter Cutoff",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 2000.f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(hz)));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::filtResonance, "Resonance",
        juce::NormalisableRange<float>(0.5f, 10.f, 0.01f), 0.707f));

    // --- Chorus ---
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamID::chorusEnabled, "Chorus", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::chorusRate, "Chorus Rate",
        juce::NormalisableRange<float>(0.01f, 10.f, 0.01f, 0.5f), 0.5f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 2) + " Hz"; })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::chorusDepth, "Chorus Depth",
        juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.3f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::chorusMix, "Chorus Mix",
        juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.5f));

    // --- Phaser ---
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamID::phaserEnabled, "Phaser", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::phaserRate, "Phaser Rate",
        juce::NormalisableRange<float>(0.01f, 5.f, 0.01f, 0.5f), 0.2f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 2) + " Hz"; })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::phaserDepth, "Phaser Depth",
        juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::phaserFeedback, "Phaser Feedback",
        juce::NormalisableRange<float>(0.f, 0.95f, 0.001f), 0.5f));

    // --- Tremolo ---
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamID::tremEnabled, "Tremolo", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::tremRate, "Tremolo Rate",
        juce::NormalisableRange<float>(0.1f, 20.f, 0.01f, 0.5f), 4.f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 2) + " Hz"; })));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::tremDepth, "Tremolo Depth",
        juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.7f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamID::tremSync, "Tremolo Sync", false));

    // --- Harmonizer ---
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamID::harmEnabled, "Harmonizer", false));
    // Index 12 = Unison (0 semitones), range -12 to +12 semitones = indices 0..24
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        ParamID::harmInterval, "Interval", -24, 24, 12)); // semitones (stored directly)
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamID::harmMode, "Harmonizer Mode",
        juce::StringArray{"Static", "Cascade"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::harmMix, "Harmonizer Mix",
        juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.5f));

    // --- Ghost Delay ---
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamID::ghostEnabled, "Ghost Delay", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::ghostTime, "Ghost Time",
        juce::NormalisableRange<float>(1.f, 1000.f, 0.1f, 0.4f), 125.f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(ms)));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::ghostFeedback, "Ghost Feedback",
        juce::NormalisableRange<float>(0.f, 0.95f, 0.001f), 0.3f));

    // --- Reverb ---
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamID::reverbEnabled, "Reverb", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::reverbSize, "Reverb Size",
        juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::reverbDamping, "Reverb Damping",
        juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::reverbMix, "Reverb Mix",
        juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.3f));

    // --- Reverse ---
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamID::reverseEnabled, "Reverse", false));

    return { params.begin(), params.end() };
}

//==============================================================================
ChromaDelayAudioProcessor::ChromaDelayAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "ChromaDelayState", createParams())
{
    // Register as listener for all parameters
    for (auto& id : {
        ParamID::delayTimeMs, ParamID::feedback, ParamID::mix,
        ParamID::inputGain, ParamID::outputLevel, ParamID::phaseInvert,
        ParamID::syncEnabled, ParamID::noteDivision, ParamID::modifier,
        ParamID::gridOffset, ParamID::lockHold,
        ParamID::bcEnabled, ParamID::bcBits, ParamID::bcSampleRateDiv,
        ParamID::satEnabled, ParamID::satDrive,
        ParamID::filtEnabled, ParamID::filtType, ParamID::filtCutoff, ParamID::filtResonance,
        ParamID::chorusEnabled, ParamID::chorusRate, ParamID::chorusDepth, ParamID::chorusMix,
        ParamID::phaserEnabled, ParamID::phaserRate, ParamID::phaserDepth, ParamID::phaserFeedback,
        ParamID::tremEnabled, ParamID::tremRate, ParamID::tremDepth, ParamID::tremSync,
        ParamID::harmEnabled, ParamID::harmInterval, ParamID::harmMode, ParamID::harmMix,
        ParamID::ghostEnabled, ParamID::ghostTime, ParamID::ghostFeedback,
        ParamID::reverbEnabled, ParamID::reverbSize, ParamID::reverbDamping, ParamID::reverbMix,
        ParamID::reverseEnabled })
    {
        apvts.addParameterListener(id, this);
    }

    delayEngine = std::make_unique<DelayEngine>(apvts);
}

ChromaDelayAudioProcessor::~ChromaDelayAudioProcessor()
{
    // Unregister listeners
    for (auto& id : {
        ParamID::delayTimeMs, ParamID::feedback, ParamID::mix,
        ParamID::inputGain, ParamID::outputLevel, ParamID::phaseInvert,
        ParamID::syncEnabled, ParamID::noteDivision, ParamID::modifier,
        ParamID::gridOffset, ParamID::lockHold,
        ParamID::bcEnabled, ParamID::bcBits, ParamID::bcSampleRateDiv,
        ParamID::satEnabled, ParamID::satDrive,
        ParamID::filtEnabled, ParamID::filtType, ParamID::filtCutoff, ParamID::filtResonance,
        ParamID::chorusEnabled, ParamID::chorusRate, ParamID::chorusDepth, ParamID::chorusMix,
        ParamID::phaserEnabled, ParamID::phaserRate, ParamID::phaserDepth, ParamID::phaserFeedback,
        ParamID::tremEnabled, ParamID::tremRate, ParamID::tremDepth, ParamID::tremSync,
        ParamID::harmEnabled, ParamID::harmInterval, ParamID::harmMode, ParamID::harmMix,
        ParamID::ghostEnabled, ParamID::ghostTime, ParamID::ghostFeedback,
        ParamID::reverbEnabled, ParamID::reverbSize, ParamID::reverbDamping, ParamID::reverbMix,
        ParamID::reverseEnabled })
    {
        apvts.removeParameterListener(id, this);
    }
}

//==============================================================================
void ChromaDelayAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32)samplesPerBlock;
    spec.numChannels = 2;

    inputGainDSP.prepare(spec);
    outputGainDSP.prepare(spec);
    inputGainDSP.setRampDurationSeconds(0.005);
    outputGainDSP.setRampDurationSeconds(0.005);

    delayEngine->prepare(spec);
}

void ChromaDelayAudioProcessor::releaseResources()
{
    delayEngine->reset();
}

bool ChromaDelayAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    return true;
}

//==============================================================================
void ChromaDelayAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;

    // Pull BPM from host playhead
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm())
                currentBpm.store(*bpm);
        }
    }

    // Update sync delay time if needed
    bool synced = *apvts.getRawParameterValue(ParamID::syncEnabled) > 0.5f;
    if (synced)
    {
        int divIdx = (int)*apvts.getRawParameterValue(ParamID::noteDivision);
        int modIdx = (int)*apvts.getRawParameterValue(ParamID::modifier);
        float syncedMs = (float)calcSyncDelayMs(currentBpm.load(), divIdx, modIdx);
        float offset = *apvts.getRawParameterValue(ParamID::gridOffset);
        delayEngine->setDelayTimeMsDirect(juce::jlimit(1.f, 2000.f, syncedMs + offset));
    }

    // Input gain
    float inGainDb = *apvts.getRawParameterValue(ParamID::inputGain);
    float outGainDb = *apvts.getRawParameterValue(ParamID::outputLevel);
    inputGainDSP.setGainDecibels(inGainDb);
    outputGainDSP.setGainDecibels(outGainDb);

    auto block = juce::dsp::AudioBlock<float>(buffer);
    auto ctx = juce::dsp::ProcessContextReplacing<float>(block);

    inputGainDSP.process(ctx);
    delayEngine->process(buffer);
    outputGainDSP.process(ctx);
}

//==============================================================================
double ChromaDelayAudioProcessor::calcSyncDelayMs(double bpm,
    int divisionIndex,
    int modifierIndex) const
{
    // divisionIndex: 0=1/1, 1=1/2, 2=1/4, 3=1/8, 4=1/16, 5=1/32
    static const double divisors[] = { 1.0, 2.0, 4.0, 8.0, 16.0, 32.0 };
    if (divisionIndex < 0 || divisionIndex > 5) divisionIndex = 2;
    double beatsPerBar = 1.0 / divisors[divisionIndex];
    double msPerBeat = 60000.0 / bpm;
    double baseMs = msPerBeat * beatsPerBar * 4.0; // relative to whole note

    switch (modifierIndex)
    {
    case 1:  return baseMs * 1.5;           // Dotted
    case 2:  return baseMs * (2.0 / 3.0);   // Triplet
    default: return baseMs;                  // Normal
    }
}

//==============================================================================
void ChromaDelayAudioProcessor::tapTempo()
{
    using namespace std::chrono;
    auto now = juce::Time::currentTimeMillis();

    // Shift tap history
    for (int i = TAP_HISTORY - 1; i > 0; --i)
        tapTimes[i] = tapTimes[i - 1];
    tapTimes[0] = now;
    tapCount = juce::jmin(tapCount + 1, TAP_HISTORY);

    if (tapCount < 2) return;

    // Average intervals
    double totalMs = 0.0;
    int count = 0;
    for (int i = 0; i < tapCount - 1; ++i)
    {
        double interval = (double)(tapTimes[i] - tapTimes[i + 1]);
        if (interval > 50.0 && interval < 3000.0)
        {
            totalMs += interval;
            ++count;
        }
    }
    if (count == 0) return;

    double avgMs = totalMs / count;
    currentBpm.store(60000.0 / avgMs);

    // Apply to delay time
    bool synced = *apvts.getRawParameterValue(ParamID::syncEnabled) > 0.5f;
    if (!synced)
    {
        // Set delay time directly in ms
        float clampedMs = (float)juce::jlimit(1.0, 2000.0, avgMs);
        if (auto* param = apvts.getParameter(ParamID::delayTimeMs))
        {
            float norm = param->convertTo0to1(clampedMs);
            param->setValueNotifyingHost(norm);
        }
    }
}

//==============================================================================
void ChromaDelayAudioProcessor::parameterChanged(const juce::String& /*parameterID*/,
    float /*newValue*/)
{
    // DelayEngine reads directly from apvts raw values — no extra work needed here.
    // Hook available for future caching optimisations.
}

//==============================================================================
void ChromaDelayAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ChromaDelayAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChromaDelayAudioProcessor();
}