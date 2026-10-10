#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "dsp/Preamp.h"

//==============================================================================
namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 512;
    constexpr float  kFreqHz     = 440.0f;
}

TEST_CASE ("Preamp neutro es identidad", "[dsp][preamp]")
{
    flang::dsp::Preamp preamp;
    preamp.prepare (kSampleRate, 1, kBlockSize);
    preamp.setInputGainDb (0.0f);
    preamp.setBassDb (0.0f);
    preamp.setMidDb (0.0f);
    preamp.setTrebleDb (0.0f);

    juce::AudioBuffer<float> buffer (1, kBlockSize);
    auto* d = buffer.getWritePointer (0);
    for (int i = 0; i < kBlockSize; ++i)
        d[i] = std::sin (2.0f * juce::MathConstants<float>::pi * kFreqHz
                         * (float) i / (float) kSampleRate);

    juce::AudioBuffer<float> reference (1, kBlockSize);
    reference.copyFrom (0, 0, buffer, 0, 0, kBlockSize);

    preamp.process (buffer);

    for (int i = 0; i < kBlockSize; ++i)
        REQUIRE (buffer.getSample (0, i)
                 == Catch::Approx (reference.getSample (0, i)).margin (1e-4));
}

TEST_CASE ("Preamp input gain escala la senal", "[dsp][preamp]")
{
    flang::dsp::Preamp preamp;
    preamp.prepare (kSampleRate, 1, kBlockSize);
    preamp.setInputGainDb (6.0f);   // ~2x

    juce::AudioBuffer<float> buffer (1, kBlockSize);
    auto* d = buffer.getWritePointer (0);
    for (int i = 0; i < kBlockSize; ++i)
        d[i] = (i % 2 == 0) ? 0.5f : -0.5f;   // cuadrada de amplitud 0.5

    preamp.process (buffer);

    // Tras el rampado de 20 ms (que aca sobra en 512 samples) el estado deberia
    // acercarse a 2x; verificamos que no quedo identidad.
    REQUIRE (std::abs (buffer.getSample (0, kBlockSize - 1)) > 0.5f);
}
