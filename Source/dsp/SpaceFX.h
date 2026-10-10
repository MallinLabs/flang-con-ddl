#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include <juce_dsp/juce_dsp.h>

namespace flang::dsp
{

//==============================================================================
enum class SpaceRouting { ReverbIntoEcho, EchoIntoReverb, Parallel };

//==============================================================================
/**
    Etapa final de espacio: reverb (Freeverb) + delay/echo, con ruteo conmutable.

    Controles: cantidad/decay de reverb, cantidad/tiempo/feedback del eco, y orden.
    Nucleo puro: no depende del host.
*/
class SpaceFX
{
public:
    SpaceFX() = default;

    //==========================================================================
    void prepare (double newSampleRate, int numChannels, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        channels   = juce::jmax (1, numChannels);
        maxBlock   = juce::jmax (1, maxBlockSize);

        juce::dsp::ProcessSpec spec { newSampleRate, (juce::uint32) maxBlock, (juce::uint32) channels };
        reverb.prepare (spec);

        delayBufferSize = (int) std::ceil (kMaxDelaySeconds * sampleRate) + 4;
        delayBuffers.assign ((size_t) channels, std::vector<float> ((size_t) delayBufferSize, 0.0f));
        delayWrite.assign ((size_t) channels, 0);
        feedbackLp.assign ((size_t) channels, 0.0f);
        feedbackCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * kFeedbackLpHz / (float) sampleRate);

        bufA.setSize (channels, maxBlock);
        bufB.setSize (channels, maxBlock);

        reset();
    }

    void reset()
    {
        reverb.reset();

        for (auto& b : delayBuffers)
            std::fill (b.begin(), b.end(), 0.0f);

        std::fill (delayWrite.begin(), delayWrite.end(), 0);
        std::fill (feedbackLp.begin(), feedbackLp.end(), 0.0f);
    }

    //==========================================================================
    void setReverbAmount (float a) noexcept   { reverbAmount = juce::jlimit (0.0f, 1.0f, a); }
    void setReverbDecay (float d) noexcept    { reverbDecay  = juce::jlimit (0.0f, 1.0f, d); }
    void setDelayAmount (float a) noexcept    { delayAmount  = juce::jlimit (0.0f, 1.0f, a); }
    void setDelayFeedback (float f) noexcept  { delayFeedback = juce::jlimit (0.0f, 0.9f, f); }
    void setDelayTimeMs (float ms) noexcept   { delaySeconds = juce::jlimit (0.02f, kMaxDelaySeconds, ms * 0.001f); }
    void setRouting (SpaceRouting r) noexcept { routing = r; }

    //==========================================================================
    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh      = juce::jmin (channels, buffer.getNumChannels());

        if (numSamples == 0 || numCh == 0)
            return;

        switch (routing)
        {
            case SpaceRouting::ReverbIntoEcho:
                applyReverb (buffer, numSamples, numCh, 1.0f, reverbAmount);
                applyDelay (buffer, numSamples, numCh, delayAmount);
                break;

            case SpaceRouting::EchoIntoReverb:
                applyDelay (buffer, numSamples, numCh, delayAmount);
                applyReverb (buffer, numSamples, numCh, 1.0f, reverbAmount);
                break;

            case SpaceRouting::Parallel:
            default:
            {
                for (int ch = 0; ch < numCh; ++ch)
                {
                    bufA.copyFrom (ch, 0, buffer, ch, 0, numSamples);
                    bufB.copyFrom (ch, 0, buffer, ch, 0, numSamples);
                }

                applyReverb (bufA, numSamples, numCh, 0.0f, reverbAmount); // solo wet
                applyDelay  (bufB, numSamples, numCh, delayAmount);        // dry + wet

                for (int ch = 0; ch < numCh; ++ch)
                {
                    auto* d = buffer.getWritePointer (ch);
                    const auto* a = bufA.getReadPointer (ch);
                    const auto* b = bufB.getReadPointer (ch);

                    for (int i = 0; i < numSamples; ++i)
                        d[i] = a[i] + b[i];
                }
                break;
            }
        }
    }

private:
    //==========================================================================
    static constexpr float kMaxDelaySeconds = 1.0f;
    static constexpr float kFeedbackLpHz    = 4000.0f;

    void applyReverb (juce::AudioBuffer<float>& buffer, int numSamples, int numCh,
                      float dry, float wet) noexcept
    {
        juce::dsp::Reverb::Parameters p;
        p.roomSize = reverbDecay;
        p.damping  = 0.45f;
        p.width    = 1.0f;
        // juce::Reverb escala: dryGain = dryLevel*2, wet = wetLevel*3.
        p.dryLevel = dry * 0.5f;
        p.wetLevel = wet / 3.0f;
        reverb.setParameters (p);

        auto block = juce::dsp::AudioBlock<float> (buffer)
                         .getSubsetChannelBlock (0, (size_t) numCh)
                         .getSubBlock (0, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverb.process (ctx);
    }

    void applyDelay (juce::AudioBuffer<float>& buffer, int numSamples, int numCh, float amount) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                const float in = d[i];

                const float delayed = readDelay (ch);

                feedbackLp[(size_t) ch] += feedbackCoef * (delayed - feedbackLp[(size_t) ch]);
                writeDelay (ch, in + delayFeedback * feedbackLp[(size_t) ch]);

                d[i] = in + amount * delayed;
            }
        }
    }

    float readDelay (int ch) const noexcept
    {
        float pos = (float) delayWrite[(size_t) ch] - delaySeconds * (float) sampleRate;

        while (pos < 0.0f)
            pos += (float) delayBufferSize;

        const int i0 = (int) pos;
        const int i1 = (i0 + 1) % delayBufferSize;
        const float frac = pos - (float) i0;
        const auto* b = delayBuffers[(size_t) ch].data();

        return b[i0] + frac * (b[i1] - b[i0]);
    }

    void writeDelay (int ch, float value) noexcept
    {
        delayBuffers[(size_t) ch][(size_t) delayWrite[(size_t) ch]] = value;
        delayWrite[(size_t) ch] = (delayWrite[(size_t) ch] + 1) % delayBufferSize;
    }

    double sampleRate = 44100.0;
    int channels   = 2;
    int maxBlock   = 512;
    int delayBufferSize = 0;

    float reverbAmount  = 0.0f;
    float reverbDecay   = 0.5f;
    float delayAmount   = 0.0f;
    float delayFeedback = 0.35f;
    float delaySeconds  = 0.35f;
    SpaceRouting routing = SpaceRouting::ReverbIntoEcho;
    float feedbackCoef = 0.5f;

    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> bufA, bufB;
    std::vector<std::vector<float>> delayBuffers;
    std::vector<int> delayWrite;
    std::vector<float> feedbackLp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceFX)
};

} // namespace flang::dsp
