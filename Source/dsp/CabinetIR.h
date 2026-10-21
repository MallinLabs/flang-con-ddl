#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <juce_dsp/juce_dsp.h>

namespace flang::dsp
{

//==============================================================================
/**
    Simulador de parlante por convolucion (FIR directo).

    Usa una IR mono corta (incorporada, sintetica, o cargada por el usuario) y la
    aplica a cada canal. Nucleo puro: no depende del host ni de formatos de audio
    (la lectura de archivos la hace el processor).
*/
class CabinetIR
{
public:
    CabinetIR() = default;

    static constexpr int kMaxIrLength = 4096;

    //==========================================================================
    void prepare (double newSampleRate, int numChannels, int maxBlockSize)
    {
        sampleRate = newSampleRate;
        channels   = juce::jmax (1, numChannels);
        maxBlock   = juce::jmax (1, maxBlockSize);

        history.assign ((size_t) channels, std::vector<float> ((size_t) kMaxIrLength, 0.0f));
        writePos.assign ((size_t) channels, 0);
        dryBuffer.setSize (channels, maxBlock);

        // No pisar una IR custom si ya fue cargada (p.ej. al recordar una sesion).
        if (std::atomic_load (&impulse) == nullptr)
            setImpulseResponse (makeBuiltInIR (newSampleRate));

        reset();
    }

    void reset()
    {
        for (auto& h : history)
            std::fill (h.begin(), h.end(), 0.0f);

        std::fill (writePos.begin(), writePos.end(), 0);
    }

    //==========================================================================
    void setMix (float m) noexcept { mix = juce::jlimit (0.0f, 1.0f, m); }

    void setImpulseResponse (const std::vector<float>& newIr)
    {
        if (newIr.empty())
            return;

        const int len = juce::jmin ((int) newIr.size(), kMaxIrLength);
        std::vector<float> normalized (newIr.begin(), newIr.begin() + len);
        normalizeGain (normalized);

        std::atomic_store (&impulse, std::make_shared<const std::vector<float>> (std::move (normalized)));
    }

    //==========================================================================
    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh      = juce::jmin (channels, buffer.getNumChannels());

        if (numSamples == 0 || numCh == 0)
            return;

        auto ir = std::atomic_load (&impulse);
        if (ir == nullptr || ir->empty())
            return;

        // Trozos de maxBlock: el host puede mandar un bloque mas grande que el
        // declarado y dryBuffer esta dimensionado a maxBlock (antes: OOB).
        for (int offset = 0; offset < numSamples; offset += maxBlock)
            processChunk (buffer, offset, juce::jmin (maxBlock, numSamples - offset), numCh, *ir);
    }

private:
    void processChunk (juce::AudioBuffer<float>& buffer, int offset, int numSamples, int numCh,
                       const std::vector<float>& ir) noexcept
    {
        for (int ch = 0; ch < numCh; ++ch)
            dryBuffer.copyFrom (ch, 0, buffer, ch, offset, numSamples);

        for (int ch = 0; ch < numCh; ++ch)
            processChannel (ch, buffer.getWritePointer (ch) + offset, numSamples, ir);

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch) + offset;
            const auto* dry = dryBuffer.getReadPointer (ch);

            for (int i = 0; i < numSamples; ++i)
                d[i] = dry[i] + mix * (d[i] - dry[i]);
        }
    }

private:
    //==========================================================================
    void processChannel (int ch, float* data, int numSamples, const std::vector<float>& ir) noexcept
    {
        auto& hist = history[(size_t) ch];
        int pos = writePos[(size_t) ch];
        const int len = (int) ir.size();

        for (int i = 0; i < numSamples; ++i)
        {
            hist[(size_t) pos] = data[i];

            float acc = 0.0f;
            int idx = pos;

            for (int k = 0; k < len; ++k)
            {
                acc += ir[(size_t) k] * hist[(size_t) idx];
                if (--idx < 0)
                    idx += kMaxIrLength;
            }

            data[i] = acc;

            if (++pos >= kMaxIrLength)
                pos = 0;
        }

        writePos[(size_t) ch] = pos;
    }

    // Normaliza la IR a ganancia unitaria en frecuencia (max |H(f)| = 1): ninguna
    // frecuencia queda amplificada. Antes se normalizaba por energia (norma L2),
    // lo que dejaba la cab con ~8 dB de ganancia de pico y con cabMix al 100%
    // empujaba la salida al soft clip (el preset sonaba "estallado").
    void normalizeGain (std::vector<float>& ir) const
    {
        const int n = (int) ir.size();
        if (n == 0)
            return;

        const double fLo = 20.0;
        const double fHi = juce::jmin (20000.0, sampleRate * 0.49);

        double peak = 0.0;
        constexpr int kPoints = 128;

        for (int p = 0; p < kPoints; ++p)
        {
            const double f = fLo * std::pow (fHi / fLo, (double) p / (double) (kPoints - 1));
            const double w = juce::MathConstants<double>::twoPi * f / sampleRate;

            double re = 0.0, im = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double v = (double) ir[(size_t) i];
                re += v * std::cos (w * i);
                im -= v * std::sin (w * i);
            }

            peak = juce::jmax (peak, std::sqrt (re * re + im * im));
        }

        if (peak > 1.0e-9)
            for (auto& v : ir)
                v = (float) ((double) v / peak);
    }

    static std::vector<float> makeBuiltInIR (double sr)
    {
        const int len = 600;
        std::vector<float> ir ((size_t) len, 0.0f);
        juce::Random rng (0x5ca6);

        const float aHp = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 90.0f / (float) sr);
        const float aLp = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 5000.0f / (float) sr);
        float hp = 0.0f, lp1 = 0.0f, lp2 = 0.0f;

        for (int i = 0; i < len; ++i)
        {
            const float x = rng.nextFloat() * 2.0f - 1.0f;

            hp += aHp * (x - hp);
            float y = x - hp;                       // high-pass ~90 Hz (cierra los graves)
            lp1 += aLp * (y - lp1);
            lp2 += aLp * (lp1 - lp2);               // low-pass ~5 kHz (tapa el fizz)
            ir[(size_t) i] = lp2;
        }

        const float tau = 0.003f * (float) sr;
        for (int i = 0; i < len; ++i)
            ir[(size_t) i] *= std::exp (-(float) i / tau);

        // La normalizacion la hace setImpulseResponse (ganancia unitaria).
        return ir;
    }

    //==========================================================================
    double sampleRate = 48000.0;
    int channels = 2;
    int maxBlock = 512;

    float mix = 0.0f;
    std::shared_ptr<const std::vector<float>> impulse;
    juce::AudioBuffer<float> dryBuffer;
    std::vector<std::vector<float>> history;
    std::vector<int> writePos;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CabinetIR)
};

} // namespace flang::dsp
