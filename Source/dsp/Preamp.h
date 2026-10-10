#pragma once

#include <juce_dsp/juce_dsp.h>

#include "ToneStack.h"

namespace flang::dsp
{

//==============================================================================
/**
    Fase 1: preamp = ganancia de entrada suavizada + EQ de 3 bandas.
    Todavia NO hay saturacion (eso es Fase 2).
*/
class Preamp
{
public:
    void prepare (double sampleRate, int numChannels, int maxBlockSize)
    {
        tone.prepare (sampleRate, numChannels, maxBlockSize);

        inputGain.reset (sampleRate, 0.02);
        inputGain.setCurrentAndTargetValue (1.0f);
    }

    void reset() { tone.reset(); }

    void setInputGainDb (float db) noexcept { inputGain.setTargetValue (juce::Decibels::decibelsToGain (db)); }
    void setBassDb      (float db) noexcept { tone.setBassDb (db); }
    void setMidDb       (float db) noexcept { tone.setMidDb (db); }
    void setTrebleDb    (float db) noexcept { tone.setTrebleDb (db); }

    void process (juce::AudioBuffer<float>& buffer) noexcept
    {
        inputGain.applyGain (buffer, buffer.getNumSamples());
        tone.process (buffer);
    }

private:
    ToneStack tone;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputGain { 1.0f };
};

} // namespace flang::dsp
