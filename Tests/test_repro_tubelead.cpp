// Reproduccion offline del bug reportado: "con el preset Tube Lead el sonido
// se crunchea". Corre la cadena real (Preamp -> Drive -> Cab -> Mod -> Space ->
// soft clip) con los valores exactos del preset y mide, por etapa: NaN/Inf,
// pico, discontinuidades (|y[n]-y[n-1]| grande), ganancia en dB y alias.

#include <cmath>
#include <cstdio>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "dsp/CabinetIR.h"
#include "dsp/Drive.h"
#include "dsp/ModEngine.h"
#include "dsp/Preamp.h"
#include "dsp/SpaceFX.h"

namespace
{
struct Stats
{
    int   nan = 0, inf = 0, jumps = 0, hot = 0, knee = 0;
    float peak = 0.0f, prev = 0.0f, maxJump = 0.0f, sumSq = 0.0f;
    long  n = 0;
};

void accumulate (Stats& s, const float* d, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        const float x = d[i];
        if (std::isnan (x)) { ++s.nan; continue; }
        if (std::isinf (x)) { ++s.inf; continue; }

        s.peak = std::fmax (s.peak, std::fabs (x));
        if (std::fabs (x) >= 0.95f) ++s.hot;
        if (std::fabs (x) > 0.7f) ++s.knee;
        s.sumSq += x * x;

        if (s.n > 0)
        {
            const float j = std::fabs (x - s.prev);
            s.maxJump = std::fmax (s.maxJump, j);
            if (j > 0.5f) ++s.jumps;
        }

        s.prev = x;
        ++s.n;
    }
}

float rms (const Stats& s) { return s.n ? std::sqrt (s.sumSq / (float) s.n) : 0.0f; }

void report (const char* name, const Stats& s)
{
    std::printf ("  %-10s nan=%-3d pico=%-7.3f rms=%-7.4f jumps=%-4d maxJump=%.3f\n",
                 name, s.nan, s.peak, rms (s), s.jumps, s.maxJump);
}

// Energia fuera de f0 y sus armonicos (alias), Goertzel, salteando el transitorio.
float aliasRatio (const std::vector<float>& x, double sr, float f0)
{
    const size_t start = (size_t) (sr * 1.0);
    const size_t n = juce::jmin ((size_t) 16384, x.size() - start);
    auto mag = [&] (float f)
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * f / sr;
        const double c = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;
        for (size_t i = 0; i < n; ++i) { const double s0 = (double) x[start + i] + c * s1 - s2; s2 = s1; s1 = s0; }
        return std::sqrt (std::fabs (s1 * s1 + s2 * s2 - c * s1 * s2)) / (double) n;
    };

    double total = 0.0, harmonic = 0.0;
    for (float f = 20.0f; f < (float) (sr * 0.5); f += 25.0f)
    {
        const double m = mag (f);
        total += m * m;
    }
    for (int h = 1; h <= 30 && f0 * h < (float) (sr * 0.5); ++h)
        for (float df : { 0.0f, -25.0f, 25.0f })
        {
            const double m = mag (f0 * h + df);
            harmonic += m * m;
        }

    return (float) (std::fmax (0.0, total - harmonic) / std::fmax (1e-20, total));
}

struct Chain
{
    flang::dsp::Preamp    preamp;
    flang::dsp::Drive     drive;
    flang::dsp::CabinetIR cab;
    flang::dsp::ModEngine mod;
    flang::dsp::SpaceFX   space;

    void prepare (double sr, int ch, int maxBlock)
    {
        preamp.prepare (sr, ch, maxBlock);
        drive.prepare  (sr, ch, maxBlock);
        cab.prepare    (sr, ch, maxBlock);
        mod.prepare    (sr, ch, maxBlock);
        space.prepare  (sr, ch, maxBlock);
    }

    // Preset "Tube Lead" tal cual esta en PresetManager.h.
    void applyTubeLead()
    {
        drive.setShape (flang::dsp::DriveShape::Tube);
        drive.setDriveAmount (0.85f);
        drive.setWarmth (0.5f);
        drive.setTone (0.5f);
        drive.setMix (1.0f);
        drive.setOversamplingMode (2);

        cab.setMix (1.0f);
        mod.setMix (0.0f);

        space.setReverbAmount (0.35f);
        space.setReverbDecay (0.6f);
        space.setDelayAmount (0.35f);
        space.setDelayTimeMs (380.0f);
        space.setDelayFeedback (0.4f);
        space.setRouting (flang::dsp::SpaceRouting::EchoIntoReverb);
    }
};

// Soft clip de salida del processor (copiado de PluginProcessor.cpp).
inline float softClip (float x) noexcept
{
    constexpr float t = 0.7f;
    if (x >  t) return  t + (1.0f - t) * std::tanh ((x - t) / (1.0f - t));
    if (x < -t) return -t + (1.0f - t) * std::tanh ((x + t) / (1.0f - t));
    return x;
}
} // namespace

TEST_CASE ("repro: ganancia de cada etapa con Tube Lead", "[repro]")
{
    constexpr double sr = 48000.0;
    constexpr int ch = 2, block = 128;
    const int total = (int) (sr * 3);

    Chain c;
    c.prepare (sr, ch, block);
    c.applyTubeLead();

    juce::AudioBuffer<float> buffer (ch, block);
    Stats sIn, sPre, sDrive, sCab, sMod, sSpace, sOut;
    std::vector<float> out;
    out.reserve ((size_t) total);

    double phase = 0.0;
    for (int produced = 0; produced < total; )
    {
        const int n = juce::jmin (block, total - produced);
        for (int i = 0; i < n; ++i)
        {
            const double t = phase + (double) i / sr;
            float x  = 0.30f * (float) std::sin (juce::MathConstants<double>::twoPi * 82.41 * t);
            x += 0.18f * (float) std::sin (juce::MathConstants<double>::twoPi * 164.8 * t);
            x += 0.10f * (float) std::sin (juce::MathConstants<double>::twoPi * 246.9 * t);
            x += 0.06f * (float) std::sin (juce::MathConstants<double>::twoPi * 329.6 * t);
            buffer.setSample (0, i, x);
            buffer.setSample (1, i, x * 0.95f);
        }
        phase += (double) n / sr;

        accumulate (sIn, buffer.getReadPointer (0), n);
        c.preamp.process (buffer);
        accumulate (sPre, buffer.getReadPointer (0), n);
        c.drive.process (buffer);
        accumulate (sDrive, buffer.getReadPointer (0), n);
        c.cab.process (buffer);
        accumulate (sCab, buffer.getReadPointer (0), n);
        c.mod.process (buffer);
        accumulate (sMod, buffer.getReadPointer (0), n);
        c.space.process (buffer);
        accumulate (sSpace, buffer.getReadPointer (0), n);

        auto* d = buffer.getWritePointer (0);
        for (int i = 0; i < n; ++i) d[i] = softClip (d[i]);
        accumulate (sOut, buffer.getReadPointer (0), n);

        for (int i = 0; i < n; ++i) out.push_back (buffer.getSample (0, i));
        produced += n;
    }

    std::printf ("\n=== Tube Lead, 48k, bloque 128 ===\n");
    report ("entrada",  sIn);
    report ("preamp",   sPre);
    report ("drive",    sDrive);
    report ("cab",      sCab);
    report ("mod",      sMod);
    report ("space",    sSpace);
    report ("softclip", sOut);

    const float gDrive = 20.0f * std::log10 (rms (sDrive) / juce::jmax (1e-9f, rms (sPre)));
    const float gCab   = 20.0f * std::log10 (rms (sCab)   / juce::jmax (1e-9f, rms (sDrive)));
    const float gSpace = 20.0f * std::log10 (rms (sSpace) / juce::jmax (1e-9f, rms (sCab)));
    std::printf ("  ganancia drive=%+.1f dB  cab=%+.1f dB  space=%+.1f dB\n", gDrive, gCab, gSpace);
    std::printf ("  alias no armonico en salida: %.2f%%\n", 100.0f * aliasRatio (out, sr, 82.41f));

    int clipped = 0;
    for (float v : out) if (std::fabs (v) > 0.95f) ++clipped;
    std::printf ("  muestras a >=0.95 en salida: %.2f%%\n", 100.0f * (float) clipped / (float) out.size());

    // Ganancia DC de la IR interna: cuanto sube el nivel con cabMix=1.
    {
        flang::dsp::CabinetIR cabOnly;
        cabOnly.prepare (sr, 1, block);
        cabOnly.setMix (1.0f);
        juce::AudioBuffer<float> dc (1, block);
        double sum = 0.0;
        int count = 0;
        for (int b = 0; b < 40; ++b)   // 40 bloques de DC puro = 1.0
        {
            for (int i = 0; i < block; ++i) dc.setSample (0, i, 1.0f);
            cabOnly.process (dc);
            if (b > 20) { sum += dc.getSample (0, 0); ++count; }
        }
        std::printf ("  IR interna: respuesta a DC (ganancia) = %+.1f dB\n",
                     20.0f * std::log10 (juce::jmax (1e-9, sum / juce::jmax (1, count))));
    }
}

//==============================================================================
// Aislamiento: que etapa rompe cuando el host manda un bloque mas grande que
// el declarado en prepareToPlay. Cada caso se corre en su propio proceso
// (un SIGSEGV mata el binario entero), por eso un TAG por etapa.
namespace
{
template <typename Stage, typename Setup>
void runBlockMismatch (Stage& s, int preparedBlock, int block, int blocks, Setup setup)
{
    s.prepare (48000.0, 2, preparedBlock);
    setup (s);

    juce::AudioBuffer<float> buf (2, block);
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < block; ++i)
        {
            const float x = 0.3f * std::sin (0.01f * (float) (b * block + i));
            buf.setSample (0, i, x);
            buf.setSample (1, i, x);
        }
        s.process (buf);
    }
}
} // namespace

TEST_CASE ("bloque 512 mayor al preparado 128: drive", "[repro-block-drive]")
{
    flang::dsp::Drive d;
    runBlockMismatch (d, 128, 512, 8, [] (flang::dsp::Drive& x)
    {
        x.setShape (flang::dsp::DriveShape::Tube);
        x.setDriveAmount (0.85f); x.setMix (1.0f); x.setOversamplingMode (2);
    });
    std::printf ("  drive: sobrevivio %d bloques de 512 tras preparar 128\n", 8);
}

TEST_CASE ("bloque 512 mayor al preparado 128: cab", "[repro-block-cab]")
{
    flang::dsp::CabinetIR c;
    runBlockMismatch (c, 128, 512, 8, [] (flang::dsp::CabinetIR& x) { x.setMix (1.0f); });
    std::printf ("  cab: sobrevivio\n");
}

TEST_CASE ("bloque 512 mayor al preparado 128: mod", "[repro-block-mod]")
{
    flang::dsp::ModEngine m;
    runBlockMismatch (m, 128, 512, 8, [] (flang::dsp::ModEngine& x) { x.setMix (0.5f); });
    std::printf ("  mod: sobrevivio\n");
}

TEST_CASE ("bloque 512 mayor al preparado 128: space", "[repro-block-space]")
{
    flang::dsp::SpaceFX s;
    runBlockMismatch (s, 128, 512, 8, [] (flang::dsp::SpaceFX& x)
    {
        x.setReverbAmount (0.35f); x.setDelayAmount (0.35f); x.setDelayFeedback (0.4f);
    });
    std::printf ("  space: sobrevivio\n");
}

//==============================================================================
// Cuanto del "crunch" es la cab: barrido de cabMix y drive midiendo saturacion
// en el soft clip de salida.
TEST_CASE ("repro: saturacion de salida segun cabMix y drive", "[repro-sweep]")
{
    constexpr double sr = 48000.0;
    constexpr int ch = 2, block = 128;
    const int total = (int) (sr * 2);

    std::printf ("\n  cabMix  drive  picoDrive  picoCab  rmsOut  >=0.95   >0.70 (rodilla)\n");

    for (float cabMix : { 0.0f, 0.5f, 0.9f, 1.0f })
    {
        for (float drv : { 0.70f, 0.85f })
        {
            Chain c;
            c.prepare (sr, ch, block);
            c.applyTubeLead();
            c.drive.setDriveAmount (drv);
            c.cab.setMix (cabMix);

            juce::AudioBuffer<float> buffer (ch, block);
            Stats sDrive, sCab, sOut;
            double phase = 0.0;

            for (int produced = 0; produced < total; )
            {
                const int n = juce::jmin (block, total - produced);
                for (int i = 0; i < n; ++i)
                {
                    const double t = phase + (double) i / sr;
                    float x  = 0.30f * (float) std::sin (juce::MathConstants<double>::twoPi * 82.41 * t);
                    x += 0.18f * (float) std::sin (juce::MathConstants<double>::twoPi * 164.8 * t);
                    x += 0.10f * (float) std::sin (juce::MathConstants<double>::twoPi * 246.9 * t);
                    buffer.setSample (0, i, x);
                    buffer.setSample (1, i, x);
                }
                phase += (double) n / sr;

                c.preamp.process (buffer);
                c.drive.process (buffer);
                accumulate (sDrive, buffer.getReadPointer (0), n);
                c.cab.process (buffer);
                accumulate (sCab, buffer.getReadPointer (0), n);
                c.mod.process (buffer);
                c.space.process (buffer);

                auto* d = buffer.getWritePointer (0);
                for (int i = 0; i < n; ++i) d[i] = softClip (d[i]);
                accumulate (sOut, buffer.getReadPointer (0), n);
                produced += n;
            }

            std::printf ("  %-6.2f  %-5.2f  %-10.3f %-8.3f %-7.4f  %5.2f%%   %5.2f%%\n",
                         cabMix, drv, sDrive.peak, sCab.peak, rms (sOut),
                         100.0f * (float) sOut.hot / (float) juce::jmax (1L, sOut.n),
                         100.0f * (float) sOut.knee / (float) juce::jmax (1L, sOut.n));
        }
    }
}