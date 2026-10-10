#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/Drive.h"
#include "dsp/ModEngine.h"
#include "dsp/Preamp.h"
#include "params/ParameterIDs.h"

//==============================================================================
/**
    Flang con Dulce de Leche — AudioProcessor.

    Adapter delgado entre el host y el nucleo DSP (Source/dsp/). El DSP no sabe
    nada del host: eso es lo que mantiene la logica pura y testeable.

    Cadena actual: preamp (input gain + EQ) -> drive (waveshaper + oversampling)
    -> output gain.
*/
class FlangAudioProcessor : public juce::AudioProcessor
{
public:
    //==========================================================================
    FlangAudioProcessor();
    ~FlangAudioProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==========================================================================
    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    //==========================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // IDs estables de parametros. Nunca renombrar: rompe la automatizacion guardada.
    // Viven en params/ParameterIDs.h para compartirlos con el PresetManager.
    static constexpr const char* inputGainID  = flang::params::inputGain;
    static constexpr const char* bassID       = flang::params::bass;
    static constexpr const char* midID        = flang::params::mid;
    static constexpr const char* trebleID     = flang::params::treble;
    static constexpr const char* driveID      = flang::params::drive;
    static constexpr const char* driveTypeID  = flang::params::driveType;
    static constexpr const char* driveToneID  = flang::params::driveTone;
    static constexpr const char* driveMixID   = flang::params::driveMix;
    static constexpr const char* oversampleID = flang::params::oversample;
    static constexpr const char* modOnID      = flang::params::modOn;
    static constexpr const char* modRateID    = flang::params::modRate;
    static constexpr const char* modDepthID   = flang::params::modDepth;
    static constexpr const char* modMixID     = flang::params::modMix;
    static constexpr const char* modSpreadID  = flang::params::modSpread;
    static constexpr const char* outputGainID = flang::params::outputGain;

    juce::AudioProcessorValueTreeState apvts;

private:
    //==========================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    float param (const char* id) const noexcept
    {
        return apvts.getRawParameterValue (id)->load();
    }

    flang::dsp::Preamp    preamp;
    flang::dsp::Drive     drive;
    flang::dsp::ModEngine mod;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlangAudioProcessor)
};
