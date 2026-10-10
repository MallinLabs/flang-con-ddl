#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include <juce_dsp/juce_dsp.h>

namespace flang::dsp
{

//==============================================================================
/**
    Afinador monofonico por YIN (diferencia + normalizacion cumulativa).

    Pensado para guitarra/bajo: detecta la fundamental y expone frecuencia +
    confianza. Nucleo puro (no depende del host). La conversion a nota/cents la
    hace quien lo use.
*/
class Tuner
{
public:
    Tuner() = default;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        window.assign ((size_t) kWindow, 0.0f);
        lin.assign ((size_t) kWindow, 0.0f);
        yin.assign ((size_t) kHalf + 2, 0.0f);
        writePos = 0;
        sinceAnalysis = 0;
        frequency = 0.0f;
        confidence = 0.0f;
    }

    void reset()
    {
        std::fill (window.begin(), window.end(), 0.0f);
        writePos = sinceAnalysis = 0;
        frequency = confidence = 0.0f;
    }

    void process (const float* data, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            window[(size_t) writePos] = data[i];
            if (++writePos >= kWindow)
                writePos = 0;
        }

        sinceAnalysis += numSamples;
        if (sinceAnalysis >= kHop)
        {
            sinceAnalysis = 0;
            analyse();
        }
    }

    float getFrequency() const noexcept  { return frequency; }
    float getConfidence() const noexcept { return confidence; }

private:
    //==========================================================================
    static constexpr int kWindow = 4096;   // ventana larga -> cubre bajo (~23 Hz)
    static constexpr int kHalf   = kWindow / 2;
    static constexpr int kHop    = 4096;
    static constexpr float kThreshold = 0.15f;

    void analyse() noexcept
    {
        // Linealizar el ring.
        for (int i = 0; i < kWindow; ++i)
            lin[(size_t) i] = window[(size_t) ((writePos + i) % kWindow)];

        // Gate de nivel: si hay muy poca señal, no buscar pitch.
        double energy = 0.0;
        for (int i = 0; i < kWindow; ++i)
            energy += (double) lin[(size_t) i] * lin[(size_t) i];
        if (std::sqrt (energy / kWindow) < 0.003)
        {
            frequency = 0.0f;
            confidence = 0.0f;
            return;
        }

        // Funcion de diferencia.
        for (int tau = 1; tau <= kHalf; ++tau)
        {
            float sum = 0.0f;
            for (int i = 0; i < kHalf; ++i)
            {
                const float d = lin[(size_t) i] - lin[(size_t) (i + tau)];
                sum += d * d;
            }
            yin[(size_t) tau] = sum;
        }

        // Diferencia normalizada acumulada.
        yin[0] = 1.0f;
        float running = 0.0f;
        for (int tau = 1; tau <= kHalf; ++tau)
        {
            running += yin[(size_t) tau];
            yin[(size_t) tau] = running > 0.0f ? yin[(size_t) tau] * (float) tau / running : 1.0f;
        }

        // Primer minimo local por debajo del umbral.
        int tauEst = -1;
        for (int tau = 2; tau < kHalf; ++tau)
        {
            if (yin[(size_t) tau] < kThreshold)
            {
                while (tau + 1 < kHalf && yin[(size_t) (tau + 1)] < yin[(size_t) tau])
                    ++tau;
                tauEst = tau;
                break;
            }
        }

        if (tauEst < 0)
        {
            frequency = 0.0f;
            confidence = 0.0f;
            return;
        }

        // Interpolacion parabolica.
        float betterTau = (float) tauEst;
        if (tauEst > 0 && tauEst < kHalf - 1)
        {
            const float s0 = yin[(size_t) (tauEst - 1)];
            const float s1 = yin[(size_t) tauEst];
            const float s2 = yin[(size_t) (tauEst + 1)];
            const float denom = 2.0f * (2.0f * s1 - s2 - s0);
            if (std::abs (denom) > 1.0e-9f)
                betterTau = (float) tauEst + (s2 - s0) / denom;
        }

        frequency  = (float) (sampleRate / juce::jmax (1.0f, betterTau));
        confidence = juce::jlimit (0.0f, 1.0f, 1.0f - yin[(size_t) tauEst]);
    }

    //==========================================================================
    double sampleRate = 48000.0;
    std::vector<float> window, lin, yin;
    int writePos = 0;
    int sinceAnalysis = 0;
    float frequency = 0.0f;
    float confidence = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Tuner)
};

} // namespace flang::dsp
