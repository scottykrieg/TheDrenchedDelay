#include <JuceHeader.h>
#include "DelayEngine.h"
#include "Effects.h"
#include "../PluginProcessor.h"

//==============================================================================
DelayEngine::DelayEngine(juce::AudioProcessorValueTreeState& a)
    : apvts(a)
{
    bitcrusher = std::make_unique<Bitcrusher>();
    saturator = std::make_unique<Saturator>();
    resonantFilter = std::make_unique<ResonantFilter>();
    chorus = std::make_unique<ChorusVibrato>();
    phaser = std::make_unique<Phaser>();
    tremolo = std::make_unique<Tremolo>();
    harmonizer = std::make_unique<Harmonizer>();
    ghostDelay = std::make_unique<GhostDelay>();
    reverb = std::make_unique<ReverbEffect>();
    reverser = std::make_unique<ReverseBuffer>();
}

DelayEngine::~DelayEngine() = default;

//==============================================================================
void DelayEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    maxDelaySamps = (int)(kMaxDelayMs * 0.001 * sampleRate) + 4;

    delayBuf.prepare(2, maxDelaySamps);
    frozenBuf.setSize(2, maxDelaySamps, false, true, true);

    for (int c = 0; c < 2; ++c)
    {
        revBuf[c].assign(maxDelaySamps, 0.f);
        revWritePtr[c] = 0;
        revSize[c] = maxDelaySamps;
    }

    bitcrusher->prepare(sampleRate);
    saturator->prepare(sampleRate);
    resonantFilter->prepare(sampleRate);
    chorus->prepare(sampleRate);
    phaser->prepare(sampleRate);
    tremolo->prepare(sampleRate);
    harmonizer->prepare(sampleRate);
    ghostDelay->prepare(sampleRate);
    reverb->prepare(sampleRate);
    reverser->prepare(sampleRate);

    isFrozen = false;
    overrideDelayMs.store(-1.f);
}

void DelayEngine::reset()
{
    delayBuf.reset();
    frozenBuf.clear();
    bitcrusher->reset();
    resonantFilter->reset();
    chorus->reset();
    phaser->reset();
    tremolo->reset();
    harmonizer->reset();
    ghostDelay->reset();
    reverb->reset();
    reverser->reset();
    isFrozen = false;
}

//==============================================================================
void DelayEngine::process(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const float sr = (float)sampleRate;

    // -------------------------------------------------------------------------
    // Read parameters (atomic raw values — thread-safe)
    // -------------------------------------------------------------------------
    auto raw = [&](const juce::String& id) -> float {
        return *apvts.getRawParameterValue(id);
    };

    const bool  lockHold = raw(ParamID::lockHold) > 0.5f;
    const bool  phaseInv = raw(ParamID::phaseInvert) > 0.5f;
    const float mix = raw(ParamID::mix);
    const float feedback = raw(ParamID::feedback);
    const float gridOffsetMs = raw(ParamID::gridOffset);
    const bool  syncEnabled = raw(ParamID::syncEnabled) > 0.5f;

    // Delay time
    float delayMs = (overrideDelayMs.load() > 0.f && syncEnabled)
        ? overrideDelayMs.load()
        : raw(ParamID::delayTimeMs);
    delayMs = juce::jlimit(1.f, kMaxDelayMs - 20.f, delayMs);

    // Convert to samples (with grid offset)
    float delaySamps = delayMs * 0.001f * sr;
    float gridOffsetSamps = gridOffsetMs * 0.001f * sr;

    // Lock/Hold: snapshot on transition
    if (lockHold && !isFrozen)
    {
        // Copy current delay buffer
        for (int c = 0; c < 2; ++c)
        {
            for (int s = 0; s < maxDelaySamps; ++s)
                frozenBuf.setSample(c, s, delayBuf.getBuffer().getSample(c, s));
        }
        frozenWritePos = delayBuf.getWritePos();
        isFrozen = true;
    }
    else if (!lockHold)
    {
        isFrozen = false;
    }

    // Effect enables
    const bool bcOn = raw(ParamID::bcEnabled) > 0.5f;
    const bool satOn = raw(ParamID::satEnabled) > 0.5f;
    const bool filtOn = raw(ParamID::filtEnabled) > 0.5f;
    const bool chorusOn = raw(ParamID::chorusEnabled) > 0.5f;
    const bool phaserOn = raw(ParamID::phaserEnabled) > 0.5f;
    const bool tremOn = raw(ParamID::tremEnabled) > 0.5f;
    const bool harmOn = raw(ParamID::harmEnabled) > 0.5f;
    const bool ghostOn = raw(ParamID::ghostEnabled) > 0.5f;
    const bool reverbOn = raw(ParamID::reverbEnabled) > 0.5f;
    const bool reverseOn = raw(ParamID::reverseEnabled) > 0.5f;

    // Harmonizer mode: 0=Static (post-loop), 1=Cascade (in-loop)
    const bool harmCascade = (int)raw(ParamID::harmMode) == 1;
    const int  harmSemi = (int)raw(ParamID::harmInterval);

    // Reverb params (update once per block)
    if (reverbOn)
        reverb->setParams(raw(ParamID::reverbSize), raw(ParamID::reverbDamping));

    // Tremolo sync
    if (tremOn && raw(ParamID::tremSync) > 0.5f)
    {
        // Use BPM from processor — read via apvts listener or another mechanism.
        // For now default to tremolo rate param.
    }

    // -------------------------------------------------------------------------
    // Sample loop
    // -------------------------------------------------------------------------
    for (int s = 0; s < numSamples; ++s)
    {
        const float dryL = buffer.getSample(0, s);
        const float dryR = buffer.getSample(1, s);

        // ------ Read from delay buffer ------
        float wetL, wetR;

        if (isFrozen)
        {
            // Read frozen snapshot circularly
            int rPos = (frozenWritePos - (int)delaySamps + maxDelaySamps * 2) % maxDelaySamps;
            wetL = frozenBuf.getSample(0, rPos);
            wetR = frozenBuf.getSample(1, rPos);
        }
        else
        {
            wetL = delayBuf.read(0, delaySamps + gridOffsetSamps);
            wetR = delayBuf.read(1, delaySamps + gridOffsetSamps);
        }

        // ------ Reverse effect (pre-mix, on wet signal) ------
        if (reverseOn)
        {
            wetL = reverser->process(wetL, 0, delayMs);
            wetR = reverser->process(wetR, 1, delayMs);
        }

        // ------ Effect chain on the wet (repeat) signal ------
        wetL = processEffectChain(wetL, 0, sr, bcOn, satOn, filtOn,
            chorusOn, phaserOn, tremOn,
            harmOn, harmCascade, ghostOn, false, apvts);
        wetR = processEffectChain(wetR, 1, sr, bcOn, satOn, filtOn,
            chorusOn, phaserOn, tremOn,
            harmOn, harmCascade, ghostOn, false, apvts);

        // ------ Reverb (stereo, post-chain) ------
        if (reverbOn)
        {
            float reverbMix = raw(ParamID::reverbMix);
            reverb->processBlock(wetL, wetR, reverbMix);
        }

        // ------ Phase Invert ------
        if (phaseInv) { wetL = -wetL; wetR = -wetR; }

        // ------ Static Harmonizer (post-loop) ------
        if (harmOn && !harmCascade)
        {
            float hMix = raw(ParamID::harmMix);
            wetL = harmonizer->process(wetL, 0, harmSemi, hMix);
            wetR = harmonizer->process(wetR, 1, harmSemi, hMix);
        }

        // ------ Write to delay buffer (input + feedback) ------
        if (!isFrozen)
        {
            delayBuf.writeAndAdvance(0, dryL + wetL * feedback);
            delayBuf.writeAndAdvance(1, dryR + wetR * feedback);
            delayBuf.advance();
        }

        // ------ Mix ------
        float outL = dryL + (wetL - dryL) * mix;
        float outR = dryR + (wetR - dryR) * mix;

        // Safety limiter — hard-clip at ±1 to prevent digital explosions
        outL = juce::jlimit(-1.f, 1.f, outL);
        outR = juce::jlimit(-1.f, 1.f, outR);

        buffer.setSample(0, s, outL);
        buffer.setSample(1, s, outR);
    }
}

//==============================================================================
float DelayEngine::processEffectChain(float sample, int ch, float sr,
    bool bcOn, bool satOn, bool filtOn,
    bool chorusOn, bool phaserOn, bool tremOn,
    bool harmOn, bool harmCascade,
    bool ghostOn, bool /*reverbOn*/,
    juce::AudioProcessorValueTreeState& apvtsRef)
{
    (void)sr; // sample rate available if needed by future effects
    auto raw = [&](const juce::String& id) -> float {
        return *apvtsRef.getRawParameterValue(id);
    };

    // 1. Bitcrusher
    if (bcOn)
        sample = bitcrusher->process(sample, ch,
            raw(ParamID::bcBits),
            raw(ParamID::bcSampleRateDiv));

    // 2. Saturation
    if (satOn)
        sample = saturator->process(sample, raw(ParamID::satDrive));

    // 3. Filter
    if (filtOn)
        sample = resonantFilter->process(sample, ch,
            (int)raw(ParamID::filtType),
            raw(ParamID::filtCutoff),
            raw(ParamID::filtResonance));

    // 4. Chorus / Vibrato
    if (chorusOn)
        sample = chorus->process(sample, ch,
            raw(ParamID::chorusRate),
            raw(ParamID::chorusDepth),
            raw(ParamID::chorusMix));

    // 5. Phaser
    if (phaserOn)
        sample = phaser->process(sample, ch,
            raw(ParamID::phaserRate),
            raw(ParamID::phaserDepth),
            raw(ParamID::phaserFeedback));

    // 6. Tremolo
    if (tremOn)
        sample = tremolo->process(sample, ch,
            raw(ParamID::tremRate),
            raw(ParamID::tremDepth));

    // 7. Harmonizer in Cascade mode (in feedback loop)
    if (harmOn && harmCascade)
        sample = harmonizer->process(sample, ch,
            (int)raw(ParamID::harmInterval),
            raw(ParamID::harmMix));

    // 8. Ghost Delay
    if (ghostOn)
        sample = ghostDelay->process(sample, ch,
            raw(ParamID::ghostTime),
            raw(ParamID::ghostFeedback));

    return sample;
}