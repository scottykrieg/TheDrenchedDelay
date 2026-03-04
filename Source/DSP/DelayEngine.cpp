// DelayEngine.cpp
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

    dcBlockers[0].reset();
    dcBlockers[1].reset();

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

    dcBlockers[0].reset();
    dcBlockers[1].reset();

    isFrozen = false;
}

//==============================================================================
void DelayEngine::process(juce::AudioBuffer<float>& buffer)
{
    const int   numSamples = buffer.getNumSamples();
    const float sr = (float)sampleRate;

    auto raw = [&](const juce::String& id) -> float {
        return *apvts.getRawParameterValue(id);
    };

    const bool  lockHold = raw(ParamID::lockHold) > 0.5f;
    const bool  phaseInv = raw(ParamID::phaseInvert) > 0.5f;
    const float mix = raw(ParamID::mix);
    const float feedback = raw(ParamID::feedback);
    const float gridOffsetMs = raw(ParamID::gridOffset);
    const bool  syncEnabled = raw(ParamID::syncEnabled) > 0.5f;

    float delayMs = (overrideDelayMs.load() > 0.f && syncEnabled)
        ? overrideDelayMs.load()
        : raw(ParamID::delayTimeMs);
    delayMs = juce::jlimit(1.f, kMaxDelayMs - 20.f, delayMs);

    float delaySamps = delayMs * 0.001f * sr;
    float gridOffsetSamps = gridOffsetMs * 0.001f * sr;

    if (lockHold && !isFrozen)
    {
        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < maxDelaySamps; ++s)
                frozenBuf.setSample(c, s, delayBuf.getBuffer().getSample(c, s));
        frozenWritePos = delayBuf.getWritePos();
        isFrozen = true;
    }
    else if (!lockHold)
    {
        isFrozen = false;
    }

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

    const bool harmCascade = (int)raw(ParamID::harmMode) == 1;
    const int  harmSemi = (int)raw(ParamID::harmInterval);

    if (reverbOn)
        reverb->setParams(raw(ParamID::reverbSize), raw(ParamID::reverbDamping));

    // -------------------------------------------------------------------------
    for (int s = 0; s < numSamples; ++s)
    {
        const float dryL = buffer.getSample(0, s);
        const float dryR = buffer.getSample(1, s);

        // --- Read from delay buffer ---
        float wetL, wetR;

        if (isFrozen)
        {
            int rPos = (frozenWritePos - (int)delaySamps + maxDelaySamps * 2) % maxDelaySamps;
            wetL = frozenBuf.getSample(0, rPos);
            wetR = frozenBuf.getSample(1, rPos);
        }
        else
        {
            wetL = delayBuf.read(0, delaySamps + gridOffsetSamps);
            wetR = delayBuf.read(1, delaySamps + gridOffsetSamps);
        }

        // --- Reverse ---
        if (reverseOn)
        {
            wetL = reverser->process(wetL, 0, delayMs);
            wetR = reverser->process(wetR, 1, delayMs);
        }

        // --- Effect chain ---
        wetL = processEffectChain(wetL, 0, sr, bcOn, satOn, filtOn,
            chorusOn, phaserOn, tremOn,
            harmOn, harmCascade, ghostOn, false, apvts);
        wetR = processEffectChain(wetR, 1, sr, bcOn, satOn, filtOn,
            chorusOn, phaserOn, tremOn,
            harmOn, harmCascade, ghostOn, false, apvts);

        // --- Reverb (stereo, post-chain) ---
        if (reverbOn)
            reverb->processBlock(wetL, wetR, raw(ParamID::reverbMix));

        // --- Phase invert ---
        if (phaseInv) { wetL = -wetL; wetR = -wetR; }

        // --- Static Harmonizer ---
        if (harmOn && !harmCascade)
        {
            const float hMix = raw(ParamID::harmMix);
            wetL = harmonizer->process(wetL, 0, harmSemi, hMix);
            wetR = harmonizer->process(wetR, 1, harmSemi, hMix);
        }

        // --- Write to delay buffer ---
        // Soft-clip then DC-block before writing back. This is what prevents
        // the runaway explosion when bitcrusher/saturation boost the signal:
        // the soft clipper keeps the magnitude ≤1, and the DC blocker removes
        // any offset that saturation asymmetries inject on every loop around.
        if (!isFrozen)
        {
            float fbL = feedbackSoftClip(dryL + wetL * feedback);
            float fbR = feedbackSoftClip(dryR + wetR * feedback);
            fbL = dcBlockers[0].process(fbL);
            fbR = dcBlockers[1].process(fbR);

            delayBuf.writeAndAdvance(0, fbL);
            delayBuf.writeAndAdvance(1, fbR);
            delayBuf.advance();
        }

        // --- Mix ---
        const float outL = juce::jlimit(-1.f, 1.f, dryL + (wetL - dryL) * mix);
        const float outR = juce::jlimit(-1.f, 1.f, dryR + (wetR - dryR) * mix);

        buffer.setSample(0, s, outL);
        buffer.setSample(1, s, outR);
    }
}

//==============================================================================
// Smooth cubic soft-clipper for the feedback path.
//
// Response:
//   |x| ≤ knee        → linear pass-through
//   knee < |x| ≤ 1.0  → cubic Hermite curve, slopes to zero at output = 1
//   |x| > 1.0         → hard-clip (catches denormals / very extreme inputs)
//
// knee = 0.82 gives ~2.7 dB of headroom before the soft region begins,
// which is enough that normal effect processing at unity gain is unaffected,
// while stacked drive/crush at high feedback is contained.
float DelayEngine::feedbackSoftClip(float x) noexcept
{
    constexpr float knee = 0.82f;
    const float     ax = std::abs(x);

    if (ax <= knee)
        return x;

    if (ax >= 1.f)
        return (x > 0.f) ? 1.f : -1.f;

    const float sign = (x > 0.f) ? 1.f : -1.f;
    // Normalise the over-threshold portion to [0, 1]
    const float t = (ax - knee) / (1.f - knee);
    // Cubic Hermite: starts at 1 (slope 1), ends at 0 (slope 0)
    // maps to output range [knee, 1]
    const float shaped = t * t * (3.f - 2.f * t);           // smoothstep
    return sign * (knee + (1.f - knee) * shaped);
}

//==============================================================================
float DelayEngine::processEffectChain(float sample, int ch, float sr,
    bool bcOn, bool satOn, bool filtOn,
    bool chorusOn, bool phaserOn, bool tremOn,
    bool harmOn, bool harmCascade,
    bool ghostOn, bool /*reverbOn*/,
    juce::AudioProcessorValueTreeState& apvtsRef)
{
    (void)sr;
    auto raw = [&](const juce::String& id) -> float {
        return *apvtsRef.getRawParameterValue(id);
    };

    if (bcOn)
        sample = bitcrusher->process(sample, ch,
            raw(ParamID::bcBits), raw(ParamID::bcSampleRateDiv));

    if (satOn)
        sample = saturator->process(sample, raw(ParamID::satDrive));

    if (filtOn)
        sample = resonantFilter->process(sample, ch,
            (int)raw(ParamID::filtType),
            raw(ParamID::filtCutoff),
            raw(ParamID::filtResonance));

    if (chorusOn)
        sample = chorus->process(sample, ch,
            raw(ParamID::chorusRate),
            raw(ParamID::chorusDepth),
            raw(ParamID::chorusMix));

    if (phaserOn)
        sample = phaser->process(sample, ch,
            raw(ParamID::phaserRate),
            raw(ParamID::phaserDepth),
            raw(ParamID::phaserFeedback));

    if (tremOn)
        sample = tremolo->process(sample, ch,
            raw(ParamID::tremRate),
            raw(ParamID::tremDepth));

    if (harmOn && harmCascade)
        sample = harmonizer->process(sample, ch,
            (int)raw(ParamID::harmInterval),
            raw(ParamID::harmMix));

    if (ghostOn)
        sample = ghostDelay->process(sample, ch,
            raw(ParamID::ghostTime),
            raw(ParamID::ghostFeedback));

    return sample;
}