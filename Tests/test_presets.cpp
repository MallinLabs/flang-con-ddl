// Corre los 20 presets de fabrica por la cadena real y mide, por etapa, donde
// se dispara el nivel. Es el loop que dice "que preset clipea y en que etapa".
// Los valores de los presets se generan desde Source/PresetManager.h.

#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "dsp/CabinetIR.h"
#include "dsp/Drive.h"
#include "dsp/ModEngine.h"
#include "dsp/Preamp.h"
#include "dsp/SpaceFX.h"
#include "presets_generated.h"

namespace
{
using Params = std::map<std::string, float>;

// Defaults de createParameterLayout() (PluginProcessor.cpp): lo que el preset no
// toca, queda con el default del plugin.
Params defaults()
{
    return { { "inputGain", 0.0f }, { "bass", 0.0f }, { "mid", 0.0f }, { "treble", 0.0f },
             { "drive", 0.0f },   { "driveType", 3.0f }, { "driveTone", 1.0f },
             { "driveMix", 0.0f },{ "warmth", 0.5f }, { "oversample", 1.0f },
             { "cabMix", 0.0f },
             { "modOn", 0.0f },   { "modMode", 0.0f }, { "modRate", 0.6f },
             { "modDepth", 0.5f },{ "modFeedback", 0.0f }, { "modMix", 0.5f }, { "modSpread", 1.0f },
             { "spaceRouting", 0.0f }, { "reverbAmount", 0.0f }, { "reverbDecay", 0.5f },
             { "delayAmount", 0.0f },  { "delayTime", 350.0f },  { "delayFeedback", 0.35f },
             { "delaySync", 0.0f },    { "output", 0.0f } };
}

float clampP (const Params& p, const char* id, float lo, float hi)
{
    return juce::jlimit (lo, hi, p.at (id));
}

struct StageStats
{
    float peak = 0.0f, sumSq = 0.0f;
    int   hot95 = 0, knee = 0;
    long  n = 0;
};

void measure (StageStats& s, const float* d, int n)
{
    for (int i = 0; i < n; ++i)
    {
        const float a = std::fabs (d[i]);
        s.peak = juce::jmax (s.peak, a);
        s.sumSq += d[i] * d[i];
        if (a >= 0.95f) ++s.hot95;
        if (a > 0.70f)  ++s.knee;
        ++s.n;
    }
}

inline float softClip (float x) noexcept
{
    constexpr float t = 0.7f;
    if (x >  t) return  t + (1.0f - t) * std::tanh ((x - t) / (1.0f - t));
    if (x < -t) return -t + (1.0f - t) * std::tanh ((x + t) / (1.0f - t));
    return x;
}

struct Result
{
    StageStats cab, mod, space, out;
    float rmsOut = 0.0f;
};

Result runPreset (const std::string& name, const Params& p, float inputScale = 1.0f,
                  double sr = 48000.0, int block = 64)
{
    constexpr int ch = 2;
    const int total = (int) (sr * 4.0);

    flang::dsp::Preamp    preamp;
    flang::dsp::Drive     drive;
    flang::dsp::CabinetIR cab;
    flang::dsp::ModEngine mod;
    flang::dsp::SpaceFX   space;

    preamp.prepare (sr, ch, block);
    drive.prepare  (sr, ch, block);
    cab.prepare    (sr, ch, block);
    mod.prepare    (sr, ch, block);
    space.prepare  (sr, ch, block);

    preamp.setInputGainDb (p.at ("inputGain"));
    preamp.setBassDb (p.at ("bass"));
    preamp.setMidDb (p.at ("mid"));
    preamp.setTrebleDb (p.at ("treble"));

    drive.setShape ((flang::dsp::DriveShape) (int) p.at ("driveType"));
    drive.setDriveAmount (clampP (p, "drive", 0.0f, 1.0f));
    drive.setWarmth (p.at ("warmth"));
    drive.setTone (p.at ("driveTone"));
    drive.setMix (p.at ("driveMix"));
    drive.setOversamplingMode (1 + (int) p.at ("oversample"));

    cab.setMix (p.at ("cabMix"));

    const bool flanger = p.at ("modMode") > 0.5f;
    mod.setRate (p.at ("modRate"));
    mod.setDepth (p.at ("modDepth"));
    mod.setSpread (p.at ("modSpread"));
    mod.setCentreDelayMs (flanger ? 2.0f : 15.0f);
    mod.setFeedback (p.at ("modFeedback") * (flanger ? 0.9f : 0.2f));
    mod.setMix (p.at ("modOn") > 0.5f ? p.at ("modMix") : 0.0f);

    space.setReverbAmount (p.at ("reverbAmount"));
    space.setReverbDecay (p.at ("reverbDecay"));
    space.setDelayAmount (p.at ("delayAmount"));
    space.setDelayTimeMs (p.at ("delayTime"));
    space.setDelayFeedback (p.at ("delayFeedback"));
    space.setRouting ((flang::dsp::SpaceRouting) (int) p.at ("spaceRouting"));

    juce::AudioBuffer<float> buffer (ch, block);
    Result r;
    const float outGain = juce::Decibels::decibelsToGain (p.at ("output"));

    // Pua cada 500 ms: ataque + decaimiento (los picos viven en el ataque).
    for (int produced = 0; produced < total; )
    {
        const int n = juce::jmin (block, total - produced);
        for (int i = 0; i < n; ++i)
        {
            const double t = (double) (produced + i) / sr;
            const double tPluck = std::fmod (t, 0.5);
            const float env = (float) std::exp (-tPluck * 6.0);

            float x = 0.30f * (float) std::sin (juce::MathConstants<double>::twoPi * 82.41 * t);
            x += 0.18f * (float) std::sin (juce::MathConstants<double>::twoPi * 164.8 * t);
            x += 0.10f * (float) std::sin (juce::MathConstants<double>::twoPi * 246.9 * t);
            x += 0.06f * (float) std::sin (juce::MathConstants<double>::twoPi * 329.6 * t);
            x *= env * inputScale;

            buffer.setSample (0, i, x);
            buffer.setSample (1, i, x);
        }

        preamp.process (buffer);
        drive.process (buffer);
        cab.process (buffer);
        measure (r.cab, buffer.getReadPointer (0), n);
        mod.process (buffer);
        measure (r.mod, buffer.getReadPointer (0), n);
        space.process (buffer);
        measure (r.space, buffer.getReadPointer (0), n);

        for (int c = 0; c < ch; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i) d[i] = softClip (d[i] * outGain);
        }

        measure (r.out, buffer.getReadPointer (0), n);
        produced += n;
    }

    r.rmsOut = r.out.n ? std::sqrt (r.out.sumSq / (float) r.out.n) : 0.0f;
    juce::ignoreUnused (name);
    return r;
}
} // namespace

TEST_CASE ("todos los presets: nivel por etapa y saturacion de salida", "[presets]")
{
    // Peak del DI sintetico: 0.5 = pua suave, 1.0 = DI al palo (lo que manda una
    // guitarra activa o un humbucker fuerte).
    for (float scale : { 0.5f, 0.75f, 1.0f })
    {
        std::printf ("\n=== entrada pico %.2f ===\n  %-16s %8s %8s %8s | %8s %8s\n",
                     scale, "preset", "picoCab", "picoMod", "picoSpace", ">0.70", ">=0.95");

        int clipan = 0;

        for (const auto& preset : flang::generated::presets())
        {
            Params p = defaults();
            for (const auto& pp : preset.params)
                p[pp.id] = pp.value;

            const auto r = runPreset (preset.name, p, scale);

            const float pctKnee = 100.0f * (float) r.out.knee / (float) juce::jmax (1L, r.out.n);
            const float pctHot  = 100.0f * (float) r.out.hot95 / (float) juce::jmax (1L, r.out.n);
            if (pctHot > 1.0f) ++clipan;

            std::printf ("  %-16s %8.3f %8.3f %8.3f | %7.2f%% %7.2f%%\n",
                         preset.name, r.cab.peak, r.mod.peak, r.space.peak, pctKnee, pctHot);
        }

        std::printf ("  presets que clipean (>=1%% a 0.95): %d de 20\n", clipan);
    }
}
//==============================================================================
// Cuanto amplifica la etapa SPACE: reverb y eco suman sobre el dry, sin
// compensacion. Mide ganancia de pico (salida/entrada) por rama.
TEST_CASE ("space: ganancia de la reverb y del eco", "[space-gain]")
{
    constexpr double sr = 48000.0;
    constexpr int ch = 2, block = 64;
    const int total = (int) (sr * 4.0);   // 4 s: deja crecer la cola

    auto measureGain = [&] (float rev, float decay, float del, float fb, int routing)
    {
        flang::dsp::SpaceFX s;
        s.prepare (sr, ch, block);
        s.setReverbAmount (rev);
        s.setReverbDecay (decay);
        s.setDelayAmount (del);
        s.setDelayTimeMs (400.0f);
        s.setDelayFeedback (fb);
        s.setRouting ((flang::dsp::SpaceRouting) routing);

        juce::AudioBuffer<float> buf (ch, block);
        StageStats st;
        for (int produced = 0; produced < total; )
        {
            const int n = juce::jmin (block, total - produced);
            for (int i = 0; i < n; ++i)
            {
                const double t = (double) (produced + i) / sr;
                const float x = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * t);
                buf.setSample (0, i, x);
                buf.setSample (1, i, x);
            }
            s.process (buf);
            // el pico de la segunda mitad = estado estacionario
            if (produced > total / 2) measure (st, buf.getReadPointer (0), n);
            produced += n;
        }
        return st.peak / 0.5f;
    };

    std::printf ("\n  reverb  decay  |  ganancia (rut 0)   |  ganancia (rut 2 paralelo)\n");
    for (float rev : { 0.0f, 0.35f, 0.6f, 1.0f })
        for (float decay : { 0.5f, 0.7f, 0.85f })
            std::printf ("  %-7.2f %-6.2f |  %-19.2f |  %.2f\n",
                         rev, decay, measureGain (rev, decay, 0.0f, 0.0f, 0),
                         measureGain (rev, decay, 0.0f, 0.0f, 2));

    std::printf ("\n  delay  fb   |  ganancia (rut 1)   |  ganancia (rut 2 paralelo)\n");
    for (float del : { 0.0f, 0.35f, 0.6f, 1.0f })
        for (float fb : { 0.0f, 0.5f, 0.72f })
            std::printf ("  %-6.2f %-4.2f |  %-19.2f |  %.2f\n",
                         del, fb, measureGain (0.0f, 0.5f, del, fb, 1),
                         measureGain (0.0f, 0.5f, del, fb, 2));
}
