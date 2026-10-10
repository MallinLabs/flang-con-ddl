#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "dsp/SpaceFX.h"

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
}

//==============================================================================
TEST_CASE ("SpaceFX sin efecto es identidad", "[dsp][space]")
{
    flang::dsp::SpaceFX space;
    space.prepare (kSr, 2, 512);
    space.setReverbAmount (0.0f);
    space.setDelayAmount (0.0f);
    space.setRouting (flang::dsp::SpaceRouting::ReverbIntoEcho);

    // Warmup: las ganancias suavizadas de la reverb rampan al arrancar.
    for (int block = 0; block < 8; ++block)
    {
        juce::AudioBuffer<float> warm (2, 512);
        fillSine (warm, 220.0f, 0.5f);
        space.process (warm);
    }

    juce::AudioBuffer<float> buffer (2, 512);
    fillSine (buffer, 220.0f, 0.5f);
    juce::AudioBuffer<float> reference (2, 512);
    reference.makeCopyOf (buffer);

    space.process (buffer);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            REQUIRE (buffer.getSample (ch, i)
                     == Catch::Approx (reference.getSample (ch, i)).margin (1e-4));
}

TEST_CASE ("Delay produce repeticiones", "[dsp][space]")
{
    flang::dsp::SpaceFX space;
    constexpr int blockSize = 4096;
    space.prepare (kSr, 2, blockSize);
    space.setReverbAmount (0.0f);
    space.setDelayAmount (1.0f);
    space.setRouting (flang::dsp::SpaceRouting::ReverbIntoEcho);

    constexpr int total = 6 * blockSize;   // 24576 > 16800 (350 ms)
    std::vector<float> out ((size_t) total, 0.0f);

    for (int start = 0; start < total; start += blockSize)
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.clear();

        if (start == 0)
            buffer.setSample (0, 0, 1.0f);   // impulso

        space.process (buffer);

        for (int i = 0; i < blockSize; ++i)
            out[(size_t) (start + i)] = buffer.getSample (0, i);
    }

    // Repeticion esperada a ~350 ms.
    const int repeatIndex = (int) (0.35 * kSr);
    float peak = 0.0f;
    for (int i = repeatIndex - 60; i <= repeatIndex + 60; ++i)
        if (i >= 0 && i < total)
            peak = juce::jmax (peak, std::abs (out[(size_t) i]));

    REQUIRE (peak > 0.05f);
}

TEST_CASE ("El orden Reverb/Echo cambia la salida", "[dsp][space]")
{
    auto run = [] (flang::dsp::SpaceRouting routing)
    {
        flang::dsp::SpaceFX space;
        space.prepare (kSr, 2, 8192);
        space.setReverbAmount (0.5f);
        space.setDelayAmount (0.5f);
        space.setRouting (routing);

        juce::AudioBuffer<float> last (2, 8192);
        for (int block = 0; block < 3; ++block)
        {
            juce::AudioBuffer<float> buffer (2, 8192);
            fillSine (buffer, 300.0f, 0.4f);
            space.process (buffer);
            last.makeCopyOf (buffer);
        }
        return last;
    };

    const auto serial = run (flang::dsp::SpaceRouting::ReverbIntoEcho);
    const auto other  = run (flang::dsp::SpaceRouting::EchoIntoReverb);
    const auto par    = run (flang::dsp::SpaceRouting::Parallel);

    auto maxDiff = [] (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        float diff = 0.0f;
        for (int ch = 0; ch < a.getNumChannels(); ++ch)
            for (int i = 0; i < a.getNumSamples(); ++i)
                diff = juce::jmax (diff, std::abs (a.getSample (ch, i) - b.getSample (ch, i)));
        return diff;
    };

    REQUIRE (maxDiff (serial, other) > 1e-3f);
    REQUIRE (maxDiff (serial, par)   > 1e-3f);
}
