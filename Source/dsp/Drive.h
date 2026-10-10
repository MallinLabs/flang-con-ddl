#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <juce_dsp/juce_dsp.h>

namespace flang::dsp
{

//==============================================================================
enum class DriveShape { Soft, Hard, Fold, Tube };

//==============================================================================
/**
    Etapa de distorsion: waveshaper + oversampling antialiasing + tono +
    DC block + dry/wet.

    Modo `Tube`: waveshaping **asimetrico** (genera armonicos pares = calidez),
    con pre high-pass (aprieta graves) y un presence shelf (~3.5 kHz) para
    conservar el brillo clasico sin harshness. El knob `Warmth` controla la
    asimetria.

    Nucleo puro (no depende del host). Oversampling: 1x / 2x / 4x / 8x.
*/
class Drive
{
public:
    Drive() = default;

    static constexpr int kNumOversamplingModes = 4; // 1x, 2x, 4x, 8x

    //==========================================================================
    void prepare (double newSampleRate, int numChannels, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        channels   = juce::jmax (1, numChannels);
        maxBlock   = juce::jmax (1, maxBlockSize);

        oversamplers.clear();
        oversamplers.reserve ((size_t) kNumOversamplingModes);

        for (int mode = 0; mode < kNumOversamplingModes; ++mode)
        {
            auto os = std::make_unique<juce::dsp::Oversampling<float>> (
                (size_t) channels,
                (size_t) mode,
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                true, false);
            os->initProcessing ((size_t) maxBlock);
            oversamplers.push_back (std::move (os));
        }

        activeMode = juce::jlimit (0, kNumOversamplingModes - 1, activeMode);

        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) maxBlock, (juce::uint32) channels };
        preHP.prepare (spec);
        presence.prepare (spec);

        *preHP.state = *juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sampleRate, 90.0f);
        *presence.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                              sampleRate, 3500.0f, 0.8f, juce::Decibels::decibelsToGain (3.0f));

        dryBuffer.setSize (channels, maxBlock);
        wetBuffer.setSize (channels, maxBlock);
        toneZ.assign ((size_t) channels, 0.0f);
        wdcX1.assign ((size_t) channels, 0.0f);
        wdcY1.assign ((size_t) channels, 0.0f);

        driveGain.reset (sampleRate, 0.02);
        driveGain.setCurrentAndTargetValue (1.0f);

        updateCoefficients();
        reset();
    }

    void reset()
    {
        for (auto& os : oversamplers)
            os->reset();

        preHP.reset();
        presence.reset();

        std::fill (toneZ.begin(), toneZ.end(), 0.0f);
        std::fill (wdcX1.begin(), wdcX1.end(), 0.0f);
        std::fill (wdcY1.begin(), wdcY1.end(), 0.0f);
    }

    //==========================================================================
    void setShape (DriveShape s) noexcept            { shape = s; }
    void setDriveAmount (float normalized) noexcept  { driveAmount = juce::jlimit (0.0f, 1.0f, normalized); }
    void setWarmth (float normalized) noexcept       { warmth = juce::jlimit (0.0f, 1.0f, normalized); }
    void setMix (float normalized) noexcept          { mix = juce::jlimit (0.0f, 1.0f, normalized); }

    void setTone (float normalized) noexcept
    {
        const float t = juce::jlimit (0.0f, 1.0f, normalized);
        if (! juce::exactlyEqual (t, toneAmount)) { toneAmount = t; updateCoefficients(); }
    }

    void setOversamplingMode (int mode) noexcept { activeMode = juce::jlimit (0, kNumOversamplingModes - 1, mode); }
    int  getOversamplingMode() const noexcept    { return activeMode; }

    int  getLatencySamples() const noexcept
    {
        return oversamplers.empty()
                   ? 0
                   : (int) oversamplers[(size_t) activeMode]->getLatencyInSamples();
    }

    //==========================================================================
    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int numSamples = buffer.getNumSamples();
        if (numSamples == 0 || oversamplers.empty())
            return;

        const int numCh = juce::jmin (channels, buffer.getNumChannels());
        if (numCh <= 0)
            return;

        driveGain.setTargetValue (juce::Decibels::decibelsToGain (driveAmount * 30.0f));

        const bool tube = (shape == DriveShape::Tube);

        // 1. dry + copia wet.
        for (int ch = 0; ch < numCh; ++ch)
        {
            dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
            wetBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
        }

        // 2. Pre high-pass (solo Tube): aprieta los graves antes del shaper.
        if (tube)
            processFilter (preHP, numCh, numSamples);

        // 3. Oversample -> ganancia + shaping.
        auto& os = *oversamplers[(size_t) activeMode];
        auto block = juce::dsp::AudioBlock<float> (wetBuffer)
                         .getSubsetChannelBlock (0, (size_t) numCh)
                         .getSubBlock (0, (size_t) numSamples);
        auto upBlock = os.processSamplesUp (block);

        const int upChannels = (int) upBlock.getNumChannels();
        const int upSamples  = (int) upBlock.getNumSamples();

        for (int i = 0; i < upSamples; ++i)
        {
            const float g = driveGain.getNextValue();

            for (int ch = 0; ch < upChannels; ++ch)
            {
                auto* d = upBlock.getChannelPointer ((size_t) ch);
                d[i] = shape_ (d[i] * g);
            }
        }

        os.processSamplesDown (block);

        // 4. Presence shelf (solo Tube): brillo clasico sin harshness.
        if (tube)
            processFilter (presence, numCh, numSamples);

        // 5. Makeup + tono + DC block + mix.
        const float makeup = 1.0f / std::sqrt (juce::jmax (1.0f, driveGain.getTargetValue()));

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d   = buffer.getWritePointer (ch);
            const auto* dry = dryBuffer.getReadPointer (ch);
            const auto* wet = wetBuffer.getReadPointer (ch);
            const auto c = (size_t) ch;

            for (int i = 0; i < numSamples; ++i)
            {
                float w = wet[i] * makeup;

                toneZ[c] += toneCoeff * (w - toneZ[c]);
                w = toneZ[c];

                const float y = w - wdcX1[c] + dcR * wdcY1[c];
                wdcX1[c] = w;
                wdcY1[c] = y;

                d[i] = dry[i] + mix * (y - dry[i]);
            }
        }
    }

private:
    //==========================================================================
    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                  juce::dsp::IIR::Coefficients<float>>;

    template <typename FilterType>
    void processFilter (FilterType& filter, int numCh, int numSamples) noexcept
    {
        auto block = juce::dsp::AudioBlock<float> (wetBuffer)
                         .getSubsetChannelBlock (0, (size_t) numCh)
                         .getSubBlock (0, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        filter.process (ctx);
    }

    float shape_ (float x) const noexcept
    {
        switch (shape)
        {
            case DriveShape::Hard: return juce::jlimit (-1.0f, 1.0f, x);
            case DriveShape::Fold: return foldback_ (x);
            case DriveShape::Tube:
            {
                // Asimetria: el bias corre el punto de operacion -> armonicos pares.
                const float bias = warmth * 0.8f;
                return std::tanh (x + bias) - std::tanh (bias);
            }
            case DriveShape::Soft:
            default:               return std::tanh (x);
        }
    }

    static float foldback_ (float x) noexcept
    {
        if (x > 1.0f || x < -1.0f)
            x = std::fabs (std::fabs (std::fmod (x - 1.0f, 4.0f)) - 2.0f) - 1.0f;
        return x;
    }

    void updateCoefficients()
    {
        const float cutoff = 500.0f * std::pow (20000.0f / 500.0f, toneAmount);
        const float fc     = juce::jmin (cutoff, (float) (sampleRate * 0.45));
        toneCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * fc / (float) sampleRate);
        dcR       = 1.0f - juce::MathConstants<float>::twoPi * 20.0f / (float) sampleRate;
    }

    //==========================================================================
    double sampleRate = 44100.0;
    int channels  = 2;
    int maxBlock  = 512;
    int activeMode = 2;

    DriveShape shape = DriveShape::Tube;
    float driveAmount = 0.0f;
    float warmth      = 0.5f;
    float mix         = 0.0f;
    float toneAmount  = 1.0f;
    float toneCoeff   = 1.0f;
    float dcR         = 0.995f;

    std::vector<std::unique_ptr<juce::dsp::Oversampling<float>>> oversamplers;
    juce::AudioBuffer<float> dryBuffer, wetBuffer;
    std::vector<float> toneZ, wdcX1, wdcY1;
    Filter preHP, presence;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driveGain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Drive)
};

} // namespace flang::dsp
