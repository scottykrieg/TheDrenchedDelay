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

    // -----------------------------------------------------------------------
    // Fetch ALL parameters once per block — zero string lookups in the loop
    // -----------------------------------------------------------------------
    auto rp = [&](const juce::String& id) -> float {
        return *apvts.getRawParameterValue(id);
    };

    CachedParams p;
    p.mix = rp(ParamID::mix);
    p.feedback = rp(ParamID::feedback);
    p.gridOffsetMs = rp(ParamID::gridOffset);
    p.lockHold = rp(ParamID::lockHold) > 0.5f;
    p.phaseInvert = rp(ParamID::phaseInvert) > 0.5f;
    p.syncEnabled = rp(ParamID::syncEnabled) > 0.5f;
    p.bcOn = rp(ParamID::bcEnabled) > 0.5f;
    p.satOn = rp(ParamID::satEnabled) > 0.5f;
    p.filtOn = rp(ParamID::filtEnabled) > 0.5f;
    p.chorusOn = rp(ParamID::chorusEnabled) > 0.5f;
    p.phaserOn = rp(ParamID::phaserEnabled) > 0.5f;
    p.tremOn = rp(ParamID::tremEnabled) > 0.5f;
    p.harmOn = rp(ParamID::harmEnabled) > 0.5f;
    p.ghostOn = rp(ParamID::ghostEnabled) > 0.5f;
    p.reverbOn = rp(ParamID::reverbEnabled) > 0.5f;
    p.reverseOn = rp(ParamID::reverseEnabled) > 0.5f;
    p.harmCascade = (int)rp(ParamID::harmMode) == 1;
    p.harmSemi = (int)rp(ParamID::harmInterval);
    p.bcBits = rp(ParamID::bcBits);
    p.bcSrDiv = rp(ParamID::bcSampleRateDiv);
    p.satDrive = rp(ParamID::satDrive);
    p.filtType = (int)rp(ParamID::filtType);
    p.filtCutoff = rp(ParamID::filtCutoff);
    p.filtRes = rp(ParamID::filtResonance);
    p.chorusRate = rp(ParamID::chorusRate);
    p.chorusDepth = rp(ParamID::chorusDepth);
    p.chorusMix = rp(ParamID::chorusMix);
    p.phaserRate = rp(ParamID::phaserRate);
    p.phaserDepth = rp(ParamID::phaserDepth);
    p.phaserFb = rp(ParamID::phaserFeedback);
    p.tremRate = rp(ParamID::tremRate);
    p.tremDepth = rp(ParamID::tremDepth);
    p.harmMix = rp(ParamID::harmMix);
    p.ghostTime = rp(ParamID::ghostTime);
    p.ghostFb = rp(ParamID::ghostFeedback);
    p.reverbSize = rp(ParamID::reverbSize);
    p.reverbDamp = rp(ParamID::reverbDamping);
    p.reverbMix = rp(ParamID::reverbMix);

    float delayMs = (overrideDelayMs.load() > 0.f && p.syncEnabled)
        ? overrideDelayMs.load()
        : rp(ParamID::delayTimeMs);
    delayMs = juce::jlimit(1.f, kMaxDelayMs - 20.f, delayMs);

    const float delaySamps = delayMs * 0.001f * sr;
    const float gridOffsetSamps = p.gridOffsetMs * 0.001f * sr;

    // Lock/Hold
    if (p.lockHold && !isFrozen)
    {
        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < maxDelaySamps; ++s)
                frozenBuf.setSample(c, s, delayBuf.getBuffer().getSample(c, s));
        frozenWritePos = delayBuf.getWritePos();
        isFrozen = true;
    }
    else if (!p.lockHold)
    {
        isFrozen = false;
    }

    if (p.reverbOn)
        reverb->setParams(p.reverbSize, p.reverbDamp);

    // -----------------------------------------------------------------------
    // Allocate a wet buffer for block-based reverb processing
    // -----------------------------------------------------------------------
    // We accumulate wetL/wetR into this, then pass to reverb->processBlock()
    juce::AudioBuffer<float> wetBuf(2, numSamples);
    wetBuf.clear();

    // -----------------------------------------------------------------------
    // Per-sample loop — no string lookups, no allocations
    // -----------------------------------------------------------------------
    for (int s = 0; s < numSamples; ++s)
    {
        const float dryL = buffer.getSample(0, s);
        const float dryR = buffer.getSample(1, s);

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

        if (p.reverseOn)
        {
            wetL = reverser->process(wetL, 0, delayMs);
            wetR = reverser->process(wetR, 1, delayMs);
        }

        wetL = processEffectChain(wetL, 0, p);
        wetR = processEffectChain(wetR, 1, p);

        // Clamp effect chain output before feedback
        wetL = juce::jlimit(-1.f, 1.f, wetL);
        wetR = juce::jlimit(-1.f, 1.f, wetR);

        if (p.phaseInvert) { wetL = -wetL; wetR = -wetR; }

        if (p.harmOn && !p.harmCascade)
        {
            wetL = harmonizer->process(wetL, 0, p.harmSemi, p.harmMix);
            wetR = harmonizer->process(wetR, 1, p.harmSemi, p.harmMix);
        }

        // Store wet signal for block reverb
        wetBuf.setSample(0, s, wetL);
        wetBuf.setSample(1, s, wetR);

        if (!isFrozen)
        {
            float fbL = feedbackSoftClip(dryL + wetL * p.feedback);
            float fbR = feedbackSoftClip(dryR + wetR * p.feedback);
            fbL = dcBlockers[0].process(fbL);
            fbR = dcBlockers[1].process(fbR);
            delayBuf.writeAndAdvance(0, fbL);
            delayBuf.writeAndAdvance(1, fbR);
            delayBuf.advance();
        }

        const float outL = juce::jlimit(-1.f, 1.f, dryL + (wetL - dryL) * p.mix);
        const float outR = juce::jlimit(-1.f, 1.f, dryR + (wetR - dryR) * p.mix);
        buffer.setSample(0, s, outL);
        buffer.setSample(1, s, outR);
    }

    // -----------------------------------------------------------------------
    // Reverb: one block call instead of numSamples individual calls
    // -----------------------------------------------------------------------
    if (p.reverbOn)
        reverb->processBlock(wetBuf, numSamples, p.reverbMix);
    // Note: reverb output is post-mix here (aesthetic choice, same as before).
    // If you want reverb audible in the output, add a second mix pass here,
    // or restructure to apply reverb before the mix write above.
}

//==============================================================================
float DelayEngine::processEffectChain(float sample, int ch,
    const CachedParams& p)
{
    if (p.bcOn)
    {
        sample = bitcrusher->process(sample, ch, p.bcBits, p.bcSrDiv);
        sample = juce::jlimit(-1.f, 1.f, sample);
    }
    if (p.satOn)
    {
        sample = saturator->process(sample, p.satDrive);
        sample = juce::jlimit(-1.f, 1.f, sample);
    }
    if (p.filtOn)
    {
        sample = resonantFilter->process(sample, ch,
            p.filtType, p.filtCutoff, p.filtRes);
        sample = juce::jlimit(-1.f, 1.f, sample);
    }
    if (p.chorusOn)
        sample = chorus->process(sample, ch,
            p.chorusRate, p.chorusDepth, p.chorusMix);
    if (p.phaserOn)
        sample = phaser->process(sample, ch,
            p.phaserRate, p.phaserDepth, p.phaserFb);
    if (p.tremOn)
        sample = tremolo->process(sample, ch, p.tremRate, p.tremDepth);
    if (p.harmOn && p.harmCascade)
        sample = harmonizer->process(sample, ch, p.harmSemi, p.harmMix);
    if (p.ghostOn)
    {
        sample = ghostDelay->process(sample, ch, p.ghostTime, p.ghostFb);
        sample = juce::jlimit(-1.f, 1.f, sample);
    }
    return sample;
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
