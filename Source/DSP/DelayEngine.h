// DelayEngine.h
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

class Bitcrusher;
class Saturator;
class ResonantFilter;
class ChorusVibrato;
class Phaser;
class Tremolo;
class Harmonizer;
class GhostDelay;
class ReverbEffect;
class ReverseBuffer;

//==============================================================================
/// Simple one-pole DC blocker (~10 Hz high-pass).
/// Removes DC offset that accumulates in feedback loops with saturation.
struct DcBlocker
{
    float x1 = 0.f, y1 = 0.f;

    float process(float x) noexcept
    {
        const float y = x - x1 + 0.9975f * y1;
        x1 = x;
        y1 = y;
        return y;
    }

    void reset() noexcept { x1 = y1 = 0.f; }
};

//==============================================================================
class CircularDelayBuffer
{
public:
    void prepare(int numChannels, int maxSamples)
    {
        buffer.setSize(numChannels, maxSamples, false, true, true);
        writePos = 0;
        bufferSize = maxSamples;
    }

    void reset() { buffer.clear(); writePos = 0; }

    void writeAndAdvance(int ch, float sample)
    {
        buffer.setSample(ch, writePos, sample);
    }

    void advance() { writePos = (writePos + 1) % bufferSize; }

    float read(int ch, float delayInSamples) const
    {
        float readPosF = (float)writePos - delayInSamples;
        while (readPosF < 0.f)               readPosF += (float)bufferSize;
        while (readPosF >= (float)bufferSize)  readPosF -= (float)bufferSize;

        const int   r0 = (int)readPosF;
        const float frac = readPosF - (float)r0;
        const int   r1 = (r0 + 1) % bufferSize;

        return buffer.getSample(ch, r0) * (1.f - frac)
            + buffer.getSample(ch, r1) * frac;
    }

    int getWritePos()  const { return writePos; }
    int getBufferSize() const { return bufferSize; }
    const juce::AudioBuffer<float>& getBuffer() const { return buffer; }

private:
    juce::AudioBuffer<float> buffer;
    int writePos = 0;
    int bufferSize = 0;
};

//==============================================================================
class DelayEngine
{
public:
    explicit DelayEngine(juce::AudioProcessorValueTreeState& apvts);
    ~DelayEngine();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

    void setDelayTimeMsDirect(float ms) { overrideDelayMs = ms; }

private:
    juce::AudioProcessorValueTreeState& apvts;

    static constexpr float kMaxDelayMs = 2020.f;
    static constexpr float kMaxGhostMs = 1020.f;

    double sampleRate = 44100.0;
    int    maxDelaySamps = 0;

    CircularDelayBuffer delayBuf;
    std::atomic<float>  overrideDelayMs { -1.f };

    juce::AudioBuffer<float> frozenBuf;
    bool isFrozen = false;
    int  frozenWritePos = 0;

    std::array<std::vector<float>, 2> revBuf;
    std::array<int, 2>                revWritePtr {};
    std::array<int, 2>                revSize {};

    // ---- Feedback stability ----
    std::array<DcBlocker, 2> dcBlockers;
    static float feedbackSoftClip(float x) noexcept;

    // ---- Effects (owned) ----
    std::unique_ptr<Bitcrusher>     bitcrusher;
    std::unique_ptr<Saturator>      saturator;
    std::unique_ptr<ResonantFilter> resonantFilter;
    std::unique_ptr<ChorusVibrato>  chorus;
    std::unique_ptr<Phaser>         phaser;
    std::unique_ptr<Tremolo>        tremolo;
    std::unique_ptr<Harmonizer>     harmonizer;
    std::unique_ptr<GhostDelay>     ghostDelay;
    std::unique_ptr<ReverbEffect>   reverb;
    std::unique_ptr<ReverseBuffer>  reverser;

    float processEffectChain(float sample, int channel, float sampleRateF,
        bool bcOn, bool satOn, bool filtOn,
        bool chorusOn, bool phaserOn, bool tremOn,
        bool harmOn, bool harmCascade,
        bool ghostOn, bool reverbOn,
        juce::AudioProcessorValueTreeState& apvts);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayEngine)
};