#pragma once

// In Projucer projects, <JuceHeader.h> is a precompiled header and must only
// be included from .cpp files. Headers include specific module headers instead.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "DSP/DelayEngine.h"

//==============================================================================
// Parameter IDs — shared between C++ and TypeScript via the bridge
//==============================================================================
namespace ParamID
{
    // Core Delay
    static const juce::String delayTimeMs = "delayTimeMs";
    static const juce::String feedback = "feedback";
    static const juce::String mix = "mix";
    static const juce::String inputGain = "inputGain";
    static const juce::String outputLevel = "outputLevel";
    static const juce::String phaseInvert = "phaseInvert";

    // Sync / Rhythm
    static const juce::String syncEnabled = "syncEnabled";
    static const juce::String noteDivision = "noteDivision";
    static const juce::String modifier = "modifier";
    static const juce::String gridOffset = "gridOffset";

    // Lock / Hold
    static const juce::String lockHold = "lockHold";

    // Bitcrusher
    static const juce::String bcEnabled = "bcEnabled";
    static const juce::String bcBits = "bcBits";
    static const juce::String bcSampleRateDiv = "bcSampleRateDiv";

    // Saturation
    static const juce::String satEnabled = "satEnabled";
    static const juce::String satDrive = "satDrive";

    // Resonant Filter
    static const juce::String filtEnabled = "filtEnabled";
    static const juce::String filtType = "filtType";    // 0=LP, 1=HP, 2=BP
    static const juce::String filtCutoff = "filtCutoff";
    static const juce::String filtResonance = "filtResonance";

    // Chorus / Vibrato
    static const juce::String chorusEnabled = "chorusEnabled";
    static const juce::String chorusRate = "chorusRate";
    static const juce::String chorusDepth = "chorusDepth";
    static const juce::String chorusMix = "chorusMix";

    // Phaser
    static const juce::String phaserEnabled = "phaserEnabled";
    static const juce::String phaserRate = "phaserRate";
    static const juce::String phaserDepth = "phaserDepth";
    static const juce::String phaserFeedback = "phaserFeedback";

    // Tremolo
    static const juce::String tremEnabled = "tremEnabled";
    static const juce::String tremRate = "tremRate";
    static const juce::String tremDepth = "tremDepth";
    static const juce::String tremSync = "tremSync";

    // Harmonizer
    static const juce::String harmEnabled = "harmEnabled";
    static const juce::String harmInterval = "harmInterval";  // -24 to +24 semitones (choice index)
    static const juce::String harmMode = "harmMode";      // 0=Static, 1=Cascade
    static const juce::String harmMix = "harmMix";

    // Ghost Delay
    static const juce::String ghostEnabled = "ghostEnabled";
    static const juce::String ghostTime = "ghostTime";
    static const juce::String ghostFeedback = "ghostFeedback";

    // Reverb
    static const juce::String reverbEnabled = "reverbEnabled";
    static const juce::String reverbSize = "reverbSize";
    static const juce::String reverbDamping = "reverbDamping";
    static const juce::String reverbMix = "reverbMix";

    // Reverse
    static const juce::String reverseEnabled = "reverseEnabled";
}

//==============================================================================
class ChromaDelayAudioProcessor : public juce::AudioProcessor,
    public juce::AudioProcessorValueTreeState::Listener
{
public:
    ChromaDelayAudioProcessor();
    ~ChromaDelayAudioProcessor() override;

    //==========================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==========================================================================
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    //==========================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==========================================================================
    // Tap Tempo — called from editor (UI button)
    void tapTempo();

    // Parameter listener
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // Expose APVTS publicly for editor/bridge access
    juce::AudioProcessorValueTreeState apvts;

    // BPM accessor (from host or tap)
    double getCurrentBpm() const { return currentBpm.load(); }
    double getSampleRate() const { return currentSampleRate; }

private:
    //==========================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==========================================================================
    std::unique_ptr<DelayEngine> delayEngine;

    // Gain utilities
    juce::dsp::Gain<float> inputGainDSP;
    juce::dsp::Gain<float> outputGainDSP;

    // BPM state
    std::atomic<double> currentBpm { 120.0 };
    double currentSampleRate = 44100.0;
    int    currentBlockSize = 512;

    // Tap Tempo state
    static constexpr int TAP_HISTORY = 4;
    std::array<juce::int64, TAP_HISTORY> tapTimes {};
    int tapCount = 0;

    // Sync helper
    double calcSyncDelayMs(double bpm, int divisionIndex, int modifierIndex) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChromaDelayAudioProcessor)
};