#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "dsp/CabinetIR.h"

//==============================================================================
namespace
{
    constexpr double kSr = 48000.0;

    void fillSine (juce::AudioBuffer<float>& buffer, float freq, float amp)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                d[i] = amp * std::sin (2.0f * juce::MathConstants<float>::pi * freq
                                       * (float) i / (float) kSr);
        }
    }

    float rms (const juce::AudioBuffer<float>& buffer, int from)
    {
        double sum = 0.0;
        const int n = buffer.getNumSamples() - from;
        for (int i = from; i < buffer.getNumSamples(); ++i)
            sum += (double) buffer.getSample (0, i) * buffer.getSample (0, i);
        return (float) std::sqrt (sum / juce::jmax (1, n));
    }
}

//==============================================================================
TEST_CASE ("Cab con mix=0 es seco", "[dsp][cab]")
{
    flang::dsp::CabinetIR cab;
    cab.prepare (kSr, 2, 512);
    cab.setImpulseResponse ({ 1.0f });
    cab.setMix (0.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    fillSine (buffer, 220.0f, 0.5f);
    juce::AudioBuffer<float> reference (2, 512);
    reference.makeCopyOf (buffer);

    cab.process (buffer);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            REQUIRE (buffer.getSample (ch, i)
                     == Catch::Approx (reference.getSample (ch, i)).margin (1e-5));
}

TEST_CASE ("Cab con IR delta es identidad", "[dsp][cab]")
{
    flang::dsp::CabinetIR cab;
    cab.prepare (kSr, 2, 512);
    cab.setImpulseResponse ({ 1.0f });   // convolucion con delta = identidad
    cab.setMix (1.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    fillSine (buffer, 220.0f, 0.5f);
    juce::AudioBuffer<float> reference (2, 512);
    reference.makeCopyOf (buffer);

    cab.process (buffer);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            REQUIRE (buffer.getSample (ch, i)
                     == Catch::Approx (reference.getSample (ch, i)).margin (1e-5));
}

TEST_CASE ("Cab suaviza los agudos", "[dsp][cab]")
{
    auto rmsFor = [] (float freq)
    {
        flang::dsp::CabinetIR cab;
        cab.prepare (kSr, 2, 4096);
        cab.setImpulseResponse ({ 0.5f, 0.5f });   // promedio movil -> lowpass
        cab.setMix (1.0f);

        juce::AudioBuffer<float> buffer (2, 4096);
        fillSine (buffer, freq, 0.5f);
        cab.process (buffer);
        return rms (buffer, 1024);
    };

    REQUIRE (rmsFor (20000.0f) < rmsFor (1000.0f) * 0.6f);
}
