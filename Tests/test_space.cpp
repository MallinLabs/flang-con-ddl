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

    // Seno de fase continua (a diferencia de fillSine, no reinicia la fase por bloque).
    std::vector<float> makeContinuousSine (int numSamples, float freq, float amp)
    {
        std::vector<float> v ((size_t) numSamples);
        for (int i = 0; i < numSamples; ++i)
            v[(size_t) i] = amp * std::sin (2.0f * juce::MathConstants<float>::pi * freq
                                            * (float) i / (float) kSr);
        return v;
    }

    void fillFromContinuous (juce::AudioBuffer<float>& buffer, const std::vector<float>& sig, int start)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (ch, i, sig[(size_t) (start + i)]);
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

TEST_CASE ("Delay Time mueve la repeticion", "[dsp][space]")
{
    auto repeatIndex = [] (float timeMs)
    {
        flang::dsp::SpaceFX space;
        constexpr int blockSize = 2048;
        space.prepare (kSr, 2, blockSize);
        space.setReverbAmount (0.0f);
        space.setDelayAmount (1.0f);
        space.setDelayTimeMs (timeMs);
        space.setDelayFeedback (0.0f);
        space.setRouting (flang::dsp::SpaceRouting::ReverbIntoEcho);

        const int total = (int) (0.8 * kSr);
        std::vector<float> out ((size_t) total, 0.0f);

        for (int start = 0; start < total; start += blockSize)
        {
            const int n = juce::jmin (blockSize, total - start);
            juce::AudioBuffer<float> buffer (2, n);
            buffer.clear();
            if (start == 0)
                buffer.setSample (0, 0, 1.0f);

            space.process (buffer);

            for (int i = 0; i < n; ++i)
                out[(size_t) (start + i)] = buffer.getSample (0, i);
        }

        int peakIndex = 0;
        float peak = 0.0f;
        for (int i = 10; i < total; ++i)   // ignorar el impulso en 0
            if (std::abs (out[(size_t) i]) > peak)
            {
                peak = std::abs (out[(size_t) i]);
                peakIndex = i;
            }
        return peakIndex;
    };

    REQUIRE (repeatIndex (100.0f) < repeatIndex (500.0f));
}

TEST_CASE ("Delay Repeats agrega repeticiones", "[dsp][space]")
{
    auto lateEnergy = [] (float feedback)
    {
        flang::dsp::SpaceFX space;
        constexpr int blockSize = 2048;
        space.prepare (kSr, 2, blockSize);
        space.setReverbAmount (0.0f);
        space.setDelayAmount (1.0f);
        space.setDelayTimeMs (100.0f);
        space.setDelayFeedback (feedback);
        space.setRouting (flang::dsp::SpaceRouting::ReverbIntoEcho);

        const int total = (int) (1.0 * kSr);
        double energy = 0.0;

        for (int start = 0; start < total; start += blockSize)
        {
            const int n = juce::jmin (blockSize, total - start);
            juce::AudioBuffer<float> buffer (2, n);
            buffer.clear();
            if (start == 0)
                buffer.setSample (0, 0, 1.0f);

            space.process (buffer);

            for (int i = 0; i < n; ++i)
                if (start + i > (int) (0.15 * kSr))   // despues de la 1ra repeticion
                    energy += std::abs (buffer.getSample (0, i));
        }
        return energy;
    };

    REQUIRE (lateEnergy (0.8f) > lateEnergy (0.0f));
}

TEST_CASE ("Paralelo: reparte el wet entre las dos ramas y no amplifica", "[dsp][space]")
{
    // El ruteo paralelo ya no es una suma lineal: los dos returns se reparten el
    // presupuesto del wet (kr + kd <= 1) y el dry se queda con el resto. Asi la
    // etapa nunca amplifica, que era lo que reventaba los presets con espacio alto.
    auto run = [] (float rv, float dl, flang::dsp::SpaceRouting routing = flang::dsp::SpaceRouting::Parallel)
    {
        flang::dsp::SpaceFX space;
        space.prepare (kSr, 2, 4096);
        space.setReverbAmount (rv);
        space.setReverbDecay (0.5f);
        space.setDelayAmount (dl);
        space.setDelayTimeMs (100.0f);
        space.setDelayFeedback (0.3f);
        space.setRouting (routing);

        juce::AudioBuffer<float> last (2, 4096);
        for (int block = 0; block < 4; ++block)
        {
            juce::AudioBuffer<float> buffer (2, 4096);
            fillSine (buffer, 300.0f, 0.3f);
            space.process (buffer);
            last.makeCopyOf (buffer);
        }
        return last;
    };

    const float inputPeak = 0.3f;

    for (auto [rv, dl] : { std::pair { 0.6f, 0.5f }, std::pair { 1.0f, 1.0f }, std::pair { 0.85f, 0.9f } })
    {
        const auto out = run (rv, dl);
        const float peak = out.getMagnitude (0, 0, out.getNumSamples());

        // Nunca por encima de la entrada (con 5% de margen por el filtrado).
        REQUIRE (peak <= inputPeak * 1.05f);

        // Los dos returns siguen audibles: la salida no es el dry pelado.
        const auto dryOnly = run (0.0f, 0.0f);
        float maxDiff = 0.0f;
        for (int i = 0; i < out.getNumSamples(); ++i)
            maxDiff = juce::jmax (maxDiff, std::abs (out.getSample (0, i) - dryOnly.getSample (0, i)));
        REQUIRE (maxDiff > 0.01f);
    }
}

TEST_CASE ("Cambiar el Delay Amount de golpe no produce clicks", "[dsp][space]")
{
    flang::dsp::SpaceFX space;
    constexpr int blockSize = 2048, blocks = 30;
    space.prepare (kSr, 2, blockSize);
    space.setReverbAmount (0.0f);
    space.setDelayTimeMs (150.0f);
    space.setDelayFeedback (0.0f);
    space.setRouting (flang::dsp::SpaceRouting::ReverbIntoEcho);
    space.setDelayAmount (0.0f);

    const auto sig = makeContinuousSine (blocks * blockSize, 300.0f, 0.3f);
    std::vector<float> out;

    for (int block = 0; block < blocks; ++block)
    {
        if (block == 15)
            space.setDelayAmount (1.0f);   // salto 0 -> 1

        juce::AudioBuffer<float> buffer (2, blockSize);
        fillFromContinuous (buffer, sig, block * blockSize);
        space.process (buffer);

        for (int i = 0; i < blockSize; ++i)
            out.push_back (buffer.getSample (0, i));
    }

    float maxJump = 0.0f; size_t mi = 0;
    for (size_t i = 1; i < out.size(); ++i)
        if (std::abs (out[i] - out[i - 1]) > maxJump) { maxJump = std::abs (out[i] - out[i - 1]); mi = i; }

    INFO ("amount change: max jump " << maxJump << " at " << mi);
    REQUIRE (maxJump < 0.1f);   // sin smoothing el wet apareceria de golpe (click ~0.3+)
}

TEST_CASE ("Cambiar el Delay Time de golpe no produce clicks", "[dsp][space]")
{
    flang::dsp::SpaceFX space;
    constexpr int blockSize = 2048, blocks = 40;
    space.prepare (kSr, 2, blockSize);
    space.setReverbAmount (0.0f);
    space.setDelayAmount (1.0f);
    space.setDelayTimeMs (100.0f);
    space.setDelayFeedback (0.0f);
    space.setRouting (flang::dsp::SpaceRouting::ReverbIntoEcho);

    const auto sig = makeContinuousSine (blocks * blockSize, 300.0f, 0.3f);
    std::vector<float> out;

    for (int block = 0; block < blocks; ++block)
    {
        if (block == 30)
            space.setDelayTimeMs (300.0f);   // cambio brusco

        juce::AudioBuffer<float> buffer (2, blockSize);
        fillFromContinuous (buffer, sig, block * blockSize);
        space.process (buffer);

        for (int i = 0; i < blockSize; ++i)
            out.push_back (buffer.getSample (0, i));
    }

    float maxJump = 0.0f; size_t mi = 0;
    for (size_t i = 1; i < out.size(); ++i)
        if (std::abs (out[i] - out[i - 1]) > maxJump) { maxJump = std::abs (out[i] - out[i - 1]); mi = i; }

    INFO ("time change: max jump " << maxJump << " at " << mi);
    REQUIRE (maxJump < 0.1f);
}
