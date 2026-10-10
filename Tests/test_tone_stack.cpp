#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "dsp/ToneStack.h"

//==============================================================================
namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kNumSamples = 48000;      // 1 segundo
    constexpr float  kSineRms    = 0.70710678f; // RMS de un seno de amplitud 1

    // Procesa un seno y devuelve el RMS en estado estable (mitad final).
    float processedRms (flang::dsp::ToneStack& tone, float freqHz)
    {
        juce::AudioBuffer<float> buffer (1, kNumSamples);
        auto* d = buffer.getWritePointer (0);

        for (int i = 0; i < kNumSamples; ++i)
            d[i] = std::sin (2.0f * juce::MathConstants<float>::pi * freqHz
                             * (float) i / (float) kSampleRate);

        tone.process (buffer);

        double sum = 0.0;
        const int start = kNumSamples / 2;
        for (int i = start; i < kNumSamples; ++i)
            sum += (double) d[i] * (double) d[i];

        return (float) std::sqrt (sum / (kNumSamples - start));
    }
}

//==============================================================================
TEST_CASE ("ToneStack neutro es identidad", "[dsp][tone]")
{
    flang::dsp::ToneStack tone;
    tone.prepare (kSampleRate, 1, kNumSamples);
    tone.setBassDb (0.0f);
    tone.setMidDb (0.0f);
    tone.setTrebleDb (0.0f);

    REQUIRE (processedRms (tone, 1000.0f) == Catch::Approx (kSineRms).epsilon (0.01));
}

TEST_CASE ("Bass boost levanta graves y no toca agudos", "[dsp][tone]")
{
    flang::dsp::ToneStack tone;
    tone.prepare (kSampleRate, 1, kNumSamples);
    tone.setBassDb (12.0f);

    REQUIRE (processedRms (tone, 100.0f)  > kSineRms * 2.0f);                       // ~4x (12 dB)
    REQUIRE (processedRms (tone, 8000.0f) == Catch::Approx (kSineRms).epsilon (0.15));
}

TEST_CASE ("Treble cut baja agudos y no toca graves", "[dsp][tone]")
{
    flang::dsp::ToneStack tone;
    tone.prepare (kSampleRate, 1, kNumSamples);
    tone.setTrebleDb (-12.0f);

    REQUIRE (processedRms (tone, 8000.0f) < kSineRms * 0.5f);                       // ~0.25x
    REQUIRE (processedRms (tone, 100.0f)  == Catch::Approx (kSineRms).epsilon (0.15));
}

TEST_CASE ("Mid boost levanta la banda media", "[dsp][tone]")
{
    flang::dsp::ToneStack tone;
    tone.prepare (kSampleRate, 1, kNumSamples);
    tone.setMidDb (12.0f);

    REQUIRE (processedRms (tone, 800.0f) > kSineRms * 1.5f);
}
