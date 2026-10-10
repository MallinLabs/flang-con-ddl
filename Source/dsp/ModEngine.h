#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include <juce_dsp/juce_dsp.h>

namespace flang::dsp
{

//==============================================================================
/**
    Motor de modulacion: delay line modulada por un LFO.

    Corazon del "chorus" estilo ampli clasico: la senal seca se mezcla con una
    copia retardada cuyo delay se mueve con el LFO. El truco estelar es el
    **desfase del LFO entre canales** (spread), que genera el ancho estereo 3D.

    El delay modulado por canal esta **suavizado**: mover depth/spread/mix de
    golpe no clickea (el delay glidea).

    Nucleo puro: no depende del host.
*/
class ModEngine
{
public:
    ModEngine() = default;

    //==========================================================================
    void prepare (double newSampleRate, int numChannels, int maxBlockSize)
    {
        juce::ignoreUnused (maxBlockSize);

        sampleRate = newSampleRate;
        channels   = juce::jmax (1, numChannels);

        bufferSize = (int) std::ceil (kMaxDelayMs * 0.001 * sampleRate) + 4;

        buffers.assign ((size_t) channels, std::vector<float> ((size_t) bufferSize, 0.0f));
        writeIndex.assign ((size_t) channels, 0);
        channelPhaseOffset.assign ((size_t) channels, 0.0);

        delaySamplesSm.assign ((size_t) channels,
                               juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>());
        for (auto& s : delaySamplesSm)
        {
            s.reset (sampleRate, 0.005);
            s.setCurrentAndTargetValue (centreDelayMs * 0.001f * (float) sampleRate);
        }

        mixSm.reset (sampleRate, 0.02);
        mixSm.setCurrentAndTargetValue (mix);
        feedbackSm.reset (sampleRate, 0.02);
        feedbackSm.setCurrentAndTargetValue (feedback);

        updatePhaseOffsets();
        reset();
    }

    void reset()
    {
        for (auto& b : buffers)
            std::fill (b.begin(), b.end(), 0.0f);

        std::fill (writeIndex.begin(), writeIndex.end(), 0);
        phase = 0.0;
    }

    //==========================================================================
    void setRate (float hz) noexcept          { rate = juce::jlimit (0.0f, 20.0f, hz); }
    void setDepth (float normalized) noexcept { depth = juce::jlimit (0.0f, 1.0f, normalized); }
    void setCentreDelayMs (float ms) noexcept { centreDelayMs = juce::jlimit (0.1f, (float) kMaxDelayMs - 1.0f, ms); }

    void setMix (float normalized) noexcept
    {
        mix = juce::jlimit (0.0f, 1.0f, normalized);
        mixSm.setTargetValue (mix);
    }

    void setFeedback (float f) noexcept
    {
        feedback = juce::jlimit (-0.95f, 0.95f, f);
        feedbackSm.setTargetValue (feedback);
    }

    void setSpread (float normalized) noexcept
    {
        const float s = juce::jlimit (0.0f, 1.0f, normalized);
        if (! juce::exactlyEqual (s, spread))
        {
            spread = s;
            updatePhaseOffsets();
        }
    }

    //==========================================================================
    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh      = juce::jmin (channels, buffer.getNumChannels());

        if (numSamples == 0 || numCh == 0)
            return;

        const double phaseIncrement = (double) rate / sampleRate;
        const float  depthMs        = depth * kMaxDepthMs;

        for (int i = 0; i < numSamples; ++i)
        {
            const float mixValue = mixSm.getNextValue();
            const float fbValue  = feedbackSm.getNextValue();

            for (int ch = 0; ch < numCh; ++ch)
            {
                const auto lfo = (float) std::sin (juce::MathConstants<double>::twoPi
                                                   * (phase + channelPhaseOffset[(size_t) ch]));

                const float targetMs = juce::jlimit (0.1f, (float) kMaxDelayMs - 1.0f,
                                                     centreDelayMs + depthMs * lfo);
                delaySamplesSm[(size_t) ch].setTargetValue (targetMs * 0.001f * (float) sampleRate);
                const float dSamples = delaySamplesSm[(size_t) ch].getNextValue();

                auto* d = buffer.getWritePointer (ch);
                const float dry = d[i];

                const float delayed = readDelay (ch, juce::jlimit (1.0f, (float) (bufferSize - 2), dSamples));
                write (ch, dry + fbValue * delayed);

                d[i] = dry + mixValue * (delayed - dry);
            }

            phase += phaseIncrement;
            if (phase >= 1.0)
                phase -= 1.0;
        }
    }

private:
    //==========================================================================
    static constexpr double kMaxDelayMs = 50.0;
    static constexpr float  kMaxDepthMs = 8.0f;

    void updatePhaseOffsets()
    {
        for (int ch = 0; ch < channels; ++ch)
            channelPhaseOffset[(size_t) ch] = ((ch % 2) == 0) ? 0.0 : (double) spread * 0.5;
    }

    float readDelay (int ch, float delaySamples) const noexcept
    {
        float readPos = (float) writeIndex[(size_t) ch] - delaySamples;

        while (readPos < 0.0f)
            readPos += (float) bufferSize;

        const int i0 = (int) readPos;
        const int i1 = (i0 + 1) % bufferSize;
        const float frac = readPos - (float) i0;
        const auto* b = buffers[(size_t) ch].data();

        return b[i0] + frac * (b[i1] - b[i0]);
    }

    void write (int ch, float value) noexcept
    {
        buffers[(size_t) ch][(size_t) writeIndex[(size_t) ch]] = value;
        writeIndex[(size_t) ch] = (writeIndex[(size_t) ch] + 1) % bufferSize;
    }

    double sampleRate = 44100.0;
    int channels   = 2;
    int bufferSize = 0;

    float rate         = 0.6f;
    float depth        = 0.0f;
    float mix          = 0.0f;
    float spread       = 1.0f;
    float feedback     = 0.0f;
    float centreDelayMs = 15.0f;

    double phase = 0.0;
    std::vector<double> channelPhaseOffset;
    std::vector<std::vector<float>> buffers;
    std::vector<int> writeIndex;

    std::vector<juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>> delaySamplesSm;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSm { 0.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> feedbackSm { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModEngine)
};

} // namespace flang::dsp
