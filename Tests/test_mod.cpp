#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "dsp/ModEngine.h"

//==============================================================================
namespace
{
    constexpr double kSr = 48000.0;

    juce::AudioBuffer<float> makeMonoInput (int numSamples, float freq)
    {
        juce::AudioBuffer<float> buffer (2, numSamples);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                d[i] = std::sin (2.0f * juce::MathConstants<float>::pi * freq
                                 * (float) i / (float) kSr);
        }
        return buffer;
    }
}

//==============================================================================
TEST_CASE ("ModEngine con mix=0 es identidad", "[dsp][mod]")
{
    flang::dsp::ModEngine mod;
    mod.prepare (kSr, 2, 512);
    mod.setMix (0.0f);

    auto buffer = makeMonoInput (512, 220.0f);
    juce::AudioBuffer<float> reference (2, 512);
    reference.makeCopyOf (buffer);

    mod.process (buffer);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            REQUIRE (buffer.getSample (ch, i)
                     == Catch::Approx (reference.getSample (ch, i)).margin (1e-6));
}

TEST_CASE ("Spread genera ancho estereo", "[dsp][mod]")
{
    auto process = [] (float spread)
    {
        flang::dsp::ModEngine mod;
        mod.prepare (kSr, 2, 8192);
        mod.setRate (1.0f);
        mod.setDepth (1.0f);
        mod.setMix (1.0f);
        mod.setSpread (spread);

        auto buffer = makeMonoInput (8192, 220.0f);
        mod.process (buffer);
        return buffer;
    };

    auto maxChannelDiff = [] (const juce::AudioBuffer<float>& b)
    {
        float diff = 0.0f;
        for (int i = 4096; i < b.getNumSamples(); ++i)
            diff = juce::jmax (diff, std::abs (b.getSample (0, i) - b.getSample (1, i)));
        return diff;
    };

    const auto withSpread   = process (1.0f);
    const auto withoutSpread = process (0.0f);

    REQUIRE (maxChannelDiff (withSpread) > 0.01f);      // con desfase L/R -> canales distintos
    REQUIRE (maxChannelDiff (withoutSpread) < 1e-6f);   // sin desfase -> canales identicos
}

TEST_CASE ("Flanger: el delay modulado no toca el piso", "[dsp][mod]")
{
    flang::dsp::ModEngine mod;
    mod.prepare (kSr, 2, 512);
    mod.setCentreDelayMs (2.0f);   // flanger
    mod.setDepth (1.0f);
    mod.setRate (2.0f);
    mod.setSpread (0.0f);
    mod.setFeedback (0.0f);
    mod.setMix (0.0f);

    for (int b = 0; b < 60; ++b)
    {
        auto buffer = makeMonoInput (512, 300.0f);
        mod.process (buffer);
    }

    // Con el fix, el delay va ~0.2..3.8 ms. Con el clamp viejo, tocaba 0.1.
    REQUIRE (mod.getMinDelayMs() > 0.15f);
    REQUIRE (mod.getMaxDelayMs() < 4.0f);
}

TEST_CASE ("ModEngine nunca produce NaN/Inf", "[dsp][mod]")
{
    flang::dsp::ModEngine mod;
    mod.prepare (kSr, 2, 256);
    mod.setRate (5.0f);
    mod.setDepth (1.0f);
    mod.setMix (1.0f);
    mod.setSpread (1.0f);
    mod.setFeedback (0.9f);

    juce::AudioBuffer<float> buffer (2, 256);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < 256; ++i)
            d[i] = (ch == 0) ? 50.0f : -50.0f;
    }

    mod.process (buffer);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 256; ++i)
            REQUIRE (std::isfinite (buffer.getSample (ch, i)));
}
