#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
// BITCRUSHER
// Reduces effective sample rate and bit depth of the signal
//==============================================================================
class Bitcrusher
{
public:
    void prepare(double sr) { sampleRate = sr; }
    void reset() { phaseAcc[0] = phaseAcc[1] = 0.f; held[0] = held[1] = 0.f; }

    float process(float sample, int ch, float bits, float srDiv)
    {
        // Sample rate reduction (zero-order hold)
        phaseAcc[ch] += 1.f / juce::jmax(1.f, srDiv);
        if (phaseAcc[ch] >= 1.f)
        {
            phaseAcc[ch] -= 1.f;
            // Bit depth quantisation
            float levels = std::pow(2.f, juce::jlimit(1.f, 16.f, bits)) - 1.f;
            held[ch] = std::round(sample * levels) / levels;
        }
        return held[ch];
    }

private:
    double sampleRate = 44100.0;
    float phaseAcc[2] = {};
    float held[2] = {};
};

//==============================================================================
// SATURATOR
// Soft-clip waveshaper with drive control
//==============================================================================
class Saturator
{
public:
    void prepare(double /*sr*/) {}
    void reset() {}

    float process(float sample, float drive)
    {
        // drive: 0..1  → boost 1..20x before soft clip
        float gain = 1.f + drive * 19.f;
        float x = sample * gain;
        // Soft clip: tanh Padé approximation
        float ax = std::abs(x);
        float sign = x >= 0.f ? 1.f : -1.f;
        float shaped = (ax < 1.f) ? (x - x * x * x / 3.f)
            : sign * 0.6667f;
        // Divide by gain (not sqrt(gain)) to restore unity gain for small signals.
        // sqrt(gain) was leaving a net amplification of sqrt(gain) ≈ 4.5x at
        // full drive, which compounded catastrophically in the feedback loop.
        return shaped / gain;
    }
};

//==============================================================================
// RESONANT FILTER  (State-Variable Filter for stable resonance at high Q)
//==============================================================================
class ResonantFilter
{
public:
    void prepare(double sr)
    {
        sampleRate = sr;
        resetState();
    }

    void reset() { resetState(); }

    // type: 0=LP, 1=HP, 2=BP
    float process(float input, int channel, int type,
        float cutoffHz, float resonance)
    {
        // Clamp for safety
        cutoffHz = juce::jlimit(20.f, (float)(sampleRate * 0.49), cutoffHz);
        resonance = juce::jlimit(0.5f, 10.f, resonance);

        float g = std::tan(juce::MathConstants<float>::pi * cutoffHz / (float)sampleRate);
        float k = 1.f / resonance;
        float a1 = 1.f / (1.f + g * (g + k));
        float a2 = g * a1;
        float a3 = g * a2;

        auto& s1 = state1[channel];
        auto& s2 = state2[channel];

        float v3 = input - s2;
        float v1 = a1 * s1 + a2 * v3;
        float v2 = s2 + a2 * s1 + a3 * v3;

        s1 = 2.f * v1 - s1;
        s2 = 2.f * v2 - s2;

        switch (type)
        {
        case 1:  return input - k * v1 - v2;  // HP
        case 2:  return k * v1;                // BP
        default: return v2;                    // LP
        }
    }

private:
    double sampleRate = 44100.0;
    float  state1[2] = {};
    float  state2[2] = {};

    void resetState()
    {
        for (int i = 0; i < 2; ++i) { state1[i] = state2[i] = 0.f; }
    }
};

//==============================================================================
// CHORUS / VIBRATO  — modulated delay-based effect
//==============================================================================
class ChorusVibrato
{
public:
    void prepare(double sr)
    {
        sampleRate = sr;
        int bufSz = (int)(sr * 0.04); // 40ms max depth
        for (int c = 0; c < 2; ++c)
        {
            buf[c].assign(bufSz, 0.f);
            wPtr[c] = 0;
            bufSize = bufSz;
        }
        lfoPhase[0] = 0.f;
        lfoPhase[1] = juce::MathConstants<float>::pi; // stereo spread
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            std::fill(buf[c].begin(), buf[c].end(), 0.f);
            wPtr[c] = 0;
            lfoPhase[c] = c == 0 ? 0.f : juce::MathConstants<float>::pi;
        }
    }

    float process(float input, int ch, float rateHz, float depth, float mix)
    {
        // LFO
        float lfo = std::sin(lfoPhase[ch]);
        float incr = juce::MathConstants<float>::twoPi * rateHz / (float)sampleRate;
        lfoPhase[ch] = std::fmod(lfoPhase[ch] + incr,
            juce::MathConstants<float>::twoPi);

        // Modulated delay: 1ms..20ms
        float maxDepthSamps = depth * 0.02f * (float)sampleRate;
        float delaySamps = maxDepthSamps * (1.f + lfo);

        // Write
        buf[ch][wPtr[ch]] = input;

        // Read with interpolation
        float rPosF = (float)wPtr[ch] - delaySamps;
        while (rPosF < 0.f) rPosF += (float)bufSize;
        int r0 = (int)rPosF % bufSize;
        float fr = rPosF - (float)(int)rPosF;
        int r1 = (r0 + 1) % bufSize;
        float wet = buf[ch][r0] * (1.f - fr) + buf[ch][r1] * fr;

        wPtr[ch] = (wPtr[ch] + 1) % bufSize;

        return input * (1.f - mix) + wet * mix;
    }

private:
    double sampleRate = 44100.0;
    std::vector<float> buf[2];
    int wPtr[2] = {};
    int bufSize = 0;
    float lfoPhase[2] = {};
};

//==============================================================================
// PHASER — all-pass based (4-stage)
//==============================================================================
class Phaser
{
public:
    void prepare(double sr) { sampleRate = sr; }
    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            lfoPhase[c] = c == 0 ? 0.f : 0.5f;
            for (int s = 0; s < 4; ++s) apState[c][s] = 0.f;
            fbState[c] = 0.f;
        }
    }

    float process(float input, int ch, float rateHz, float depth, float feedback)
    {
        float incr = juce::MathConstants<float>::twoPi * rateHz / (float)sampleRate;
        lfoPhase[ch] = std::fmod(lfoPhase[ch] + incr,
            juce::MathConstants<float>::twoPi);

        float lfo = 0.5f + 0.5f * std::sin(lfoPhase[ch]);
        float minHz = 300.f, maxHz = 3000.f;
        float fHz = minHz + depth * lfo * (maxHz - minHz);
        float g = std::tan(juce::MathConstants<float>::pi * fHz / (float)sampleRate);
        float coeff = (g - 1.f) / (g + 1.f);

        float x = input + feedback * fbState[ch];
        float y = x;
        for (int s = 0; s < 4; ++s)
        {
            float yn = coeff * y + apState[ch][s];
            apState[ch][s] = y - coeff * yn;
            y = yn;
        }
        fbState[ch] = y;
        return 0.5f * (input + y);
    }

private:
    double sampleRate = 44100.0;
    float lfoPhase[2] = {};
    float apState[2][4] = {};
    float fbState[2] = {};
};

//==============================================================================
// TREMOLO — LFO amplitude modulation
//==============================================================================
class Tremolo
{
public:
    void prepare(double sr) { sampleRate = sr; }
    void reset()
    {
        lfoPhase[0] = 0.f;
        lfoPhase[1] = juce::MathConstants<float>::pi * 0.5f; // subtle stereo spread
    }

    float process(float input, int ch, float rateHz, float depth)
    {
        float incr = juce::MathConstants<float>::twoPi * rateHz / (float)sampleRate;
        lfoPhase[ch] = std::fmod(lfoPhase[ch] + incr,
            juce::MathConstants<float>::twoPi);

        float lfo = 0.5f + 0.5f * std::sin(lfoPhase[ch]);
        float amp = 1.f - depth * (1.f - lfo);
        return input * amp;
    }

    void setSyncedRate(double bpm, int divIdx)
    {
        static const double divs[] = { 1.0, 2.0, 4.0, 8.0, 16.0, 32.0 };
        if (divIdx < 0 || divIdx > 5) divIdx = 2;
        double msPerBeat = 60000.0 / bpm;
        double periodMs = msPerBeat * 4.0 / divs[divIdx];
        syncRate = 1000.0 / periodMs;
    }

    double syncRate = 0.0;

private:
    double sampleRate = 44100.0;
    float  lfoPhase[2] = {};
};

//==============================================================================
// HARMONIZER — cache pitch ratio, use float math
//==============================================================================
class Harmonizer
{
public:
    static constexpr int GRAIN = 2048;
    static constexpr int BUF_LEN = GRAIN * 8;

    void prepare(double sr)
    {
        sampleRate = sr;
        for (int c = 0; c < 2; ++c)
        {
            inputBuf[c].assign(BUF_LEN, 0.f);
            writePos[c] = 0;
            grainPhase[c] = 0.f;
            grain2Phase[c] = 0.5f;
        }
        cachedSemitones = INT_MIN; // force recalc on first use
        cachedRatio = 1.f;
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            std::fill(inputBuf[c].begin(), inputBuf[c].end(), 0.f);
            writePos[c] = 0;
            grainPhase[c] = 0.f;
            grain2Phase[c] = 0.5f;
        }
    }

    float process(float input, int ch, int semitones, float mix)
    {
        if (semitones == 0) return input;

        // Only recompute pow() when the knob actually changes
        if (semitones != cachedSemitones)
        {
            cachedSemitones = semitones;
            cachedRatio = (float)std::pow(2.0, semitones / 12.0);
        }

        inputBuf[ch][writePos[ch]] = input;
        writePos[ch] = (writePos[ch] + 1) % BUF_LEN;

        const float phaseInc = 1.f / (float)GRAIN;
        grainPhase[ch] = std::fmod(grainPhase[ch] + phaseInc, 1.f);
        grain2Phase[ch] = std::fmod(grain2Phase[ch] + phaseInc, 1.f);

        auto readSample = [&](float phase) -> float
        {
            const float window = 0.5f - 0.5f * std::cos(
                juce::MathConstants<float>::twoPi * phase);
            const float readOffset = (1.f - phase) * (float)GRAIN * cachedRatio;
            const float safeOffset = juce::jlimit(1.f, (float)(BUF_LEN - 1), readOffset);

            float rPosF = (float)writePos[ch] - safeOffset;
            while (rPosF < 0.f)            rPosF += (float)BUF_LEN;
            while (rPosF >= (float)BUF_LEN) rPosF -= (float)BUF_LEN;

            const int   r0 = (int)rPosF;
            const float fr = rPosF - (float)r0;
            const int   r1 = (r0 + 1) % BUF_LEN;
            return (inputBuf[ch][r0] * (1.f - fr) + inputBuf[ch][r1] * fr) * window;
        };

        const float wet = readSample(grainPhase[ch]) + readSample(grain2Phase[ch]);
        return input * (1.f - mix) + wet * mix;
    }

private:
    double sampleRate = 44100.0;
    int    cachedSemitones = INT_MIN;
    float  cachedRatio = 1.f;

    std::vector<float> inputBuf[2];
    int   writePos[2] = {};
    float grainPhase[2] = {};
    float grain2Phase[2] = { 0.5f, 0.5f };
};


//==============================================================================
// GHOST DELAY — simple secondary delay for multi-tap patterns
//==============================================================================
class GhostDelay
{
public:
    void prepare(double sr)
    {
        sampleRate = sr;
        int maxSamps = (int)(sr * 1.1);
        for (int c = 0; c < 2; ++c)
        {
            buf[c].assign(maxSamps, 0.f);
            wPtr[c] = 0;
            bufSize = maxSamps;
        }
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            std::fill(buf[c].begin(), buf[c].end(), 0.f);
            wPtr[c] = 0;
        }
    }

    float process(float input, int ch, float timeMs, float feedback)
    {
        float delaySamps = (float)(timeMs * 0.001 * sampleRate);
        delaySamps = juce::jlimit(1.f, (float)(bufSize - 1), delaySamps);

        // Read
        float rPosF = (float)wPtr[ch] - delaySamps;
        while (rPosF < 0.f) rPosF += (float)bufSize;
        int   r0 = (int)rPosF % bufSize;
        float fr = rPosF - (float)(int)rPosF;
        int   r1 = (r0 + 1) % bufSize;
        float wetOut = buf[ch][r0] * (1.f - fr) + buf[ch][r1] * fr;

        // Write with feedback — clamped to prevent the ghost's own internal
        // loop from diverging independently of the main delay feedback level.
        buf[ch][wPtr[ch]] = juce::jlimit(-1.f, 1.f, input + wetOut * feedback);
        wPtr[ch] = (wPtr[ch] + 1) % bufSize;

        return (input + wetOut) * 0.5f;
    }

private:
    double sampleRate = 44100.0;
    std::vector<float> buf[2];
    int wPtr[2] = {};
    int bufSize = 0;
};


//==============================================================================
// REVERB — block-based, no per-sample allocation
//==============================================================================
class ReverbEffect
{
public:
    void prepare(double sr)
    {
        spec.sampleRate = sr;
        spec.maximumBlockSize = 4096;
        spec.numChannels = 2;
        rev.prepare(spec);
        // Pre-allocate the working buffer at max block size
        workBuf.setSize(2, 4096, false, true, true);
    }

    void reset() { rev.reset(); }

    void setParams(float size, float damping)
    {
        juce::dsp::Reverb::Parameters p;
        p.roomSize = size;
        p.damping = damping;
        p.wetLevel = 1.f;
        p.dryLevel = 0.f;
        p.width = 1.f;
        rev.setParameters(p);
    }

    // Call once per block instead of once per sample.
    // dryBuf contains the pre-reverb wet signal; this overwrites it in-place
    // blended to mix. numSamples must be <= 4096.
    void processBlock(juce::AudioBuffer<float>& buf, int numSamples, float mix)
    {
        // Copy into work buffer
        workBuf.copyFrom(0, 0, buf, 0, 0, numSamples);
        workBuf.copyFrom(1, 0, buf, 1, 0, numSamples);

        juce::dsp::AudioBlock<float> blk(workBuf.getArrayOfWritePointers(),
            2, 0, (size_t)numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx(blk);
        rev.process(ctx);

        // Mix back
        for (int s = 0; s < numSamples; ++s)
        {
            buf.setSample(0, s, buf.getSample(0, s) * (1.f - mix)
                + workBuf.getSample(0, s) * mix);
            buf.setSample(1, s, buf.getSample(1, s) * (1.f - mix)
                + workBuf.getSample(1, s) * mix);
        }
    }

private:
    juce::dsp::Reverb      rev;
    juce::dsp::ProcessSpec spec {};
    juce::AudioBuffer<float> workBuf;
};
//==============================================================================
// REVERSE BUFFER — collects delay-sized chunks and plays them backwards
//==============================================================================
class ReverseBuffer
{
public:
    void prepare(double sr) { sampleRate = sr; }
    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            revBuf[c].clear();
            playBuf[c].clear();
            playPtr[c] = 0;
        }
    }

    // delayMs is the current delay time — we reverse in that window
    float process(float input, int ch, float delayMs)
    {
        int windowSamps = juce::jmax(1, (int)(delayMs * 0.001 * sampleRate));

        revBuf[ch].push_back(input);
        if ((int)revBuf[ch].size() >= windowSamps)
        {
            // Reverse and move to play buffer
            playBuf[ch].assign(revBuf[ch].rbegin(), revBuf[ch].rend());
            revBuf[ch].clear();
            playPtr[ch] = 0;
        }

        float out = 0.f;
        if (playPtr[ch] < (int)playBuf[ch].size())
        {
            out = playBuf[ch][playPtr[ch]];
            ++playPtr[ch];
        }
        return out;
    }

private:
    double sampleRate = 44100.0;
    std::vector<float> revBuf[2];
    std::vector<float> playBuf[2];
    int playPtr[2] = {};
};