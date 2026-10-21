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

    Los parametros que mueven senal (cantidad, tiempo y feedback del eco) estan
    suavizados: moverlos de golpe no produce clicks, y el tiempo "glidea" estilo
    cinta (pitch bend) en vez de saltar.

    **Manejo de nivel**: cada rama es un crossfade dry/wet (no una suma encima del
    dry) y el tap del eco se normaliza por `1 - feedback`, que es la suma de la
    cola de repeticiones. Sin eso el eco amplificaba hasta 4.4x (Delay 1.0 con
    Repeats 0.72) y los presets con espacio al palo se escuchaban reventados. En
    ruteo paralelo los dos returns se reparten el presupuesto del wet.

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

        delayAmountSm.reset (sampleRate, 0.02);
        delayAmountSm.setCurrentAndTargetValue (delayAmount);
        delayFeedbackSm.reset (sampleRate, 0.02);
        delayFeedbackSm.setCurrentAndTargetValue (delayFeedback);
        delaySamplesSm.reset (sampleRate, 0.08);   // glide mas lento para el tiempo
        delaySamplesSm.setCurrentAndTargetValue (delaySeconds * (float) sampleRate);

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
    void setRouting (SpaceRouting r) noexcept { routing = r; }

    void setDelayAmount (float a) noexcept
    {
        delayAmount = juce::jlimit (0.0f, 1.0f, a);
        delayAmountSm.setTargetValue (delayAmount);
    }

    void setDelayFeedback (float f) noexcept
    {
        delayFeedback = juce::jlimit (0.0f, 0.9f, f);
        delayFeedbackSm.setTargetValue (delayFeedback);
    }

    void setDelayTimeMs (float ms) noexcept
    {
        delaySeconds = juce::jlimit (0.02f, kMaxDelaySeconds, ms * 0.001f);
        delaySamplesSm.setTargetValue (delaySeconds * (float) sampleRate);
    }

    //==========================================================================
    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh      = juce::jmin (channels, buffer.getNumChannels());

        if (numSamples == 0 || numCh == 0)
            return;

        // Trozos de maxBlock: bufA/bufB (ruteo paralelo) estan dimensionados a
        // maxBlock y el host puede mandar un bloque mas grande.
        for (int offset = 0; offset < numSamples; offset += maxBlock)
            processChunk (buffer, offset, juce::jmin (maxBlock, numSamples - offset), numCh);
    }

private:
    void processChunk (juce::AudioBuffer<float>& buffer, int offset, int numSamples, int numCh) noexcept
    {
        const float wr = reverbAmount;
        const float wd = delayAmount;
        juce::ignoreUnused (wd);

        switch (routing)
        {
            case SpaceRouting::ReverbIntoEcho:
                applyReverb (buffer, offset, numSamples, numCh, 1.0f - wr, wr);
                applyDelay  (buffer, offset, numSamples, numCh, true, 1.0f);
                break;

            case SpaceRouting::EchoIntoReverb:
                applyDelay  (buffer, offset, numSamples, numCh, true, 1.0f);
                applyReverb (buffer, offset, numSamples, numCh, 1.0f - wr, wr);
                break;

            case SpaceRouting::Parallel:
            default:
            {
                // Los dos returns se reparten el presupuesto del wet y el dry se
                // queda con lo que sobra: la suma nunca supera la entrada.
                const float sum  = wr + wd;
                const float norm = sum > 1.0f ? 1.0f / sum : 1.0f;
                const float kr   = wr * norm;
                const float kd   = wd * norm;
                const float dry  = juce::jmax (0.0f, 1.0f - kr - kd);

                for (int ch = 0; ch < numCh; ++ch)
                {
                    bufA.copyFrom (ch, 0, buffer, ch, offset, numSamples);
                    bufB.copyFrom (ch, 0, buffer, ch, offset, numSamples);
                }

                applyReverb (bufA, 0, numSamples, numCh, 0.0f, kr);  // solo wet
                applyDelay  (bufB, 0, numSamples, numCh, false, kd); // solo wet

                for (int ch = 0; ch < numCh; ++ch)
                {
                    auto* d = buffer.getWritePointer (ch) + offset;
                    const auto* a = bufA.getReadPointer (ch);
                    const auto* b = bufB.getReadPointer (ch);

                    for (int i = 0; i < numSamples; ++i)
                        d[i] = dry * d[i] + a[i] + b[i];
                }
                break;
            }
        }
    }

    //==========================================================================
    static constexpr float kMaxDelaySeconds = 1.0f;
    static constexpr float kFeedbackLpHz    = 4000.0f;

    void applyReverb (juce::AudioBuffer<float>& buffer, int offset, int numSamples, int numCh,
                      float dryAmount, float wetAmount) noexcept
    {
        juce::dsp::Reverb::Parameters p;
        p.roomSize = reverbDecay;
        p.damping  = 0.45f;
        p.width    = 1.0f;
        // juce::Reverb escala: dryGain = dryLevel*2, wet = wetLevel*3.
        p.dryLevel = juce::jlimit (0.0f, 1.0f, dryAmount) * 0.5f;
        p.wetLevel = juce::jlimit (0.0f, 1.0f, wetAmount) / 3.0f;
        reverb.setParameters (p);

        auto block = juce::dsp::AudioBlock<float> (buffer)
                         .getSubsetChannelBlock (0, (size_t) numCh)
                         .getSubBlock ((size_t) offset, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverb.process (ctx);
    }

    void applyDelay (juce::AudioBuffer<float>& buffer, int offset, int numSamples, int numCh,
                     bool includeDry, float branchWeight) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float amt = delayAmountSm.getNextValue();
            const float fb  = delayFeedbackSm.getNextValue();
            const float dSamples = juce::jlimit (1.0f, (float) (delayBufferSize - 2),
                                                 delaySamplesSm.getNextValue());

            const float wetGain = branchWeight * amt;
            const float dryGain = includeDry ? juce::jmax (0.0f, 1.0f - wetGain) : 0.0f;

            // La cola de repeticiones suma 1/(1-fb): se normaliza el tap para que
            // el wet no crezca con los repeats (antes: hasta 4.4x).
            const float tapNorm = 1.0f - fb;

            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* d = buffer.getWritePointer (ch) + offset;
                const float in = d[i];

                const float delayed = readDelay (ch, dSamples);

                feedbackLp[(size_t) ch] += feedbackCoef * (delayed - feedbackLp[(size_t) ch]);
                writeDelay (ch, in + fb * feedbackLp[(size_t) ch]);

                d[i] = in * dryGain + wetGain * tapNorm * delayed;
            }
        }
    }

    float readDelay (int ch, float delaySamples) const noexcept
    {
        float pos = (float) delayWrite[(size_t) ch] - delaySamples;

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

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delayAmountSm { 0.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delayFeedbackSm { 0.35f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> delaySamplesSm { 0.0f };

    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> bufA, bufB;
    std::vector<std::vector<float>> delayBuffers;
    std::vector<int> delayWrite;
    std::vector<float> feedbackLp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpaceFX)
};

} // namespace flang::dsp
