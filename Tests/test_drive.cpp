#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "dsp/Drive.h"

//==============================================================================
namespace
{
    constexpr double kSr = 48000.0;

    // Magnitud de una frecuencia puntual (Goertzel).
    float goertzel (const float* x, int n, float freq)
    {
        const double w = juce::MathConstants<double>::twoPi * freq / kSr;
        const double c = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const double s0 = (double) x[i] + c * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        return (float) std::sqrt (s1 * s1 + s2 * s2 - c * s1 * s2);
    }

    void fillSine (juce::AudioBuffer<float>& buffer, float freq, float amp)
    {
        auto* d = buffer.getWritePointer (0);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            d[i] = amp * std::sin (2.0f * juce::MathConstants<float>::pi * freq
                                   * (float) i / (float) kSr);
    }
}

//==============================================================================
TEST_CASE ("Drive con mix=0 es limpio (bit-exacto)", "[dsp][drive]")
{
    flang::dsp::Drive drive;
    drive.prepare (kSr, 1, 512);
    drive.setOversamplingMode (2);      // 4x
    drive.setDriveAmount (1.0f);
    drive.setMix (0.0f);                // dry puro

    juce::AudioBuffer<float> buffer (1, 512);
    fillSine (buffer, 220.0f, 0.5f);

    juce::AudioBuffer<float> reference (1, 512);
    reference.copyFrom (0, 0, buffer, 0, 0, 512);

    drive.process (buffer);

    for (int i = 0; i < 512; ++i)
        REQUIRE (buffer.getSample (0, i)
                 == Catch::Approx (reference.getSample (0, i)).margin (1e-6));
}

TEST_CASE ("Drive comprime picos", "[dsp][drive]")
{
    flang::dsp::Drive drive;
    drive.prepare (kSr, 1, 4096);
    drive.setOversamplingMode (2);
    drive.setShape (flang::dsp::DriveShape::Soft);
    drive.setDriveAmount (1.0f);
    drive.setTone (1.0f);
    drive.setMix (1.0f);                // todo wet

    juce::AudioBuffer<float> buffer (1, 4096);
    fillSine (buffer, 220.0f, 1.0f);

    drive.process (buffer);

    float peak = 0.0f;
    for (int i = 2048; i < 4096; ++i)
        peak = juce::jmax (peak, std::abs (buffer.getSample (0, i)));

    REQUIRE (peak < 0.5f);      // comprimido por debajo de la entrada (1.0)
    REQUIRE (peak > 0.05f);     // pero no desaparece
}

TEST_CASE ("Oversampling reduce el aliasing", "[dsp][drive]")
{
    constexpr float kFundamental = 15000.0f;  // 3er armonico (45k) aliasa a 3k
    constexpr float kAliasFreq   = 3000.0f;
    constexpr int   kLen         = 16384;
    constexpr int   kSkip        = 4096;

    auto measureAlias = [&] (int oversamplingMode)
    {
        flang::dsp::Drive drive;
        drive.prepare (kSr, 1, kLen);
        drive.setOversamplingMode (oversamplingMode);
        drive.setShape (flang::dsp::DriveShape::Hard);
        drive.setDriveAmount (1.0f);
        drive.setTone (1.0f);
        drive.setMix (1.0f);

        juce::AudioBuffer<float> buffer (1, kLen);
        fillSine (buffer, kFundamental, 0.9f);
        drive.process (buffer);

        return goertzel (buffer.getReadPointer (0) + kSkip, kLen - kSkip, kAliasFreq);
    };

    const float aliasOff = measureAlias (0);   // 1x
    const float alias8x  = measureAlias (3);   // 8x

    REQUIRE (aliasOff > 0.0f);
    REQUIRE (alias8x < aliasOff);
    REQUIRE (alias8x < aliasOff * 0.5f);
}

TEST_CASE ("Drive nunca produce NaN/Inf con entrada extrema", "[dsp][drive]")
{
    flang::dsp::Drive drive;
    drive.prepare (kSr, 2, 256);
    drive.setOversamplingMode (3);
    drive.setShape (flang::dsp::DriveShape::Fold);
    drive.setDriveAmount (1.0f);
    drive.setMix (1.0f);

    juce::AudioBuffer<float> buffer (2, 256);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < 256; ++i)
            d[i] = (ch == 0) ? 100.0f : -100.0f;
    }

    drive.process (buffer);

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 256; ++i)
            REQUIRE (std::isfinite (buffer.getSample (ch, i)));
}

TEST_CASE ("Tube genera armonicos pares (calidez)", "[dsp][drive]")
{
    auto secondHarmonicRatio = [] (flang::dsp::DriveShape shape, float warmth)
    {
        flang::dsp::Drive drive;
        constexpr int len = 32768;
        drive.prepare (kSr, 1, len);
        drive.setOversamplingMode (2);
        drive.setShape (shape);
        drive.setWarmth (warmth);
        drive.setDriveAmount (0.7f);
        drive.setTone (1.0f);
        drive.setMix (1.0f);

        juce::AudioBuffer<float> buffer (1, len);
        auto* d = buffer.getWritePointer (0);
        for (int i = 0; i < len; ++i)
            d[i] = std::sin (2.0f * juce::MathConstants<float>::pi * 400.0f * (float) i / (float) kSr);

        drive.process (buffer);

        const int skip = 8192;
        const float fundamental = goertzel (buffer.getReadPointer (0) + skip, len - skip, 400.0f);
        const float second      = goertzel (buffer.getReadPointer (0) + skip, len - skip, 800.0f);
        return second / juce::jmax (1.0e-6f, fundamental);
    };

    const float tube = secondHarmonicRatio (flang::dsp::DriveShape::Tube, 1.0f);
    const float soft = secondHarmonicRatio (flang::dsp::DriveShape::Soft, 0.0f);

    REQUIRE (tube > 0.05f);        // el tubo genera 2do armonico (calidez)
    REQUIRE (tube > soft * 3.0f);  // mucho mas que el shaping simetrico
}
