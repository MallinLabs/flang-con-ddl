#pragma once

#include <juce_dsp/juce_dsp.h>

namespace flang::dsp
{

//==============================================================================
/**
    EQ de 3 bandas estilo ampli: low shelf + peak + high shelf.

    Nucleo puro: no depende del host ni del AudioProcessor. Se puede instanciar y
    testear offline (ver Tests/).
*/
class ToneStack
{
public:
    void prepare (double newSampleRate, int numChannels, int maxBlockSize)
    {
        sampleRate = newSampleRate;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate       = newSampleRate;
        spec.maximumBlockSize = (juce::uint32) juce::jmax (1, maxBlockSize);
        spec.numChannels      = (juce::uint32) juce::jmax (1, numChannels);

        bass.prepare (spec);
        mid.prepare (spec);
        treble.prepare (spec);

        reset();
        updateCoefficients();
        dirty = false;
    }

    void reset()
    {
        bass.reset();
        mid.reset();
        treble.reset();
    }

    void setBassDb   (float db) noexcept { if (! juce::exactlyEqual (db, bassDb))   { bassDb   = db; dirty = true; } }
    void setMidDb    (float db) noexcept { if (! juce::exactlyEqual (db, midDb))    { midDb    = db; dirty = true; } }
    void setTrebleDb (float db) noexcept { if (! juce::exactlyEqual (db, trebleDb)) { trebleDb = db; dirty = true; } }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        if (dirty)
        {
            updateCoefficients();
            dirty = false;
        }

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> context (block);

        bass.process (context);
        mid.process (context);
        treble.process (context);
    }

private:
    //==========================================================================
    using Band = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                juce::dsp::IIR::Coefficients<float>>;

    void updateCoefficients()
    {
        constexpr float q = 0.70710678f;

        // Nota: makeX() reserva un objeto de coeficientes (asignacion chica).
        // Por eso solo se recalcula cuando un parametro cambia (flag `dirty`),
        // nunca por bloque. La version lock-free es Fase avanzada.
        *bass.state   = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (
                            sampleRate, 200.0f, q, juce::Decibels::decibelsToGain (bassDb));
        *mid.state    = *juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                            sampleRate, 800.0f, 1.0f, juce::Decibels::decibelsToGain (midDb));
        *treble.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                            sampleRate, 3500.0f, q, juce::Decibels::decibelsToGain (trebleDb));
    }

    double sampleRate = 44100.0;
    float bassDb = 0.0f, midDb = 0.0f, trebleDb = 0.0f;
    bool dirty = true;

    Band bass, mid, treble;
};

} // namespace flang::dsp
