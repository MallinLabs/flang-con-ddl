#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
FlangAudioProcessor::FlangAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

FlangAudioProcessor::~FlangAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
FlangAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const juce::NormalisableRange<float> eqRange   { -12.0f, 12.0f, 0.1f };
    const juce::NormalisableRange<float> gainRange { -24.0f, 24.0f, 0.1f };

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { inputGainID, 1 }, "Input", gainRange, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { bassID, 1 }, "Bass", eqRange, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { midID, 1 }, "Mid", eqRange, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { trebleID, 1 }, "Treble", eqRange, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driveID, 1 }, "Drive",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.0f));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { driveTypeID, 1 }, "Type",
        juce::StringArray { "Soft", "Hard", "Fold" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driveToneID, 1 }, "Tone",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 1.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driveMixID, 1 }, "Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.0f));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { oversampleID, 1 }, "Oversample",
        juce::StringArray { "2x", "4x", "8x" }, 1));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { outputGainID, 1 }, "Output",
        juce::NormalisableRange<float> { -24.0f, 12.0f, 0.1f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return layout;
}

//==============================================================================
void FlangAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());

    preamp.prepare (sampleRate, numChannels, samplesPerBlock);
    drive.prepare  (sampleRate, numChannels, samplesPerBlock);

    outputGain.reset (sampleRate, 0.02);   // 20 ms: anti-zipper
    outputGain.setCurrentAndTargetValue (1.0f);

    // Publicar la latencia del oversampling para que el host compense.
    setLatencySamples (drive.getLatencySamples());
}

void FlangAudioProcessor::releaseResources()
{
}

void FlangAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    // Algunos hosts mandan bloques de 0 samples: hay que sobrevivir.
    if (numSamples == 0)
        return;

    // Preamp: input gain + EQ.
    preamp.setInputGainDb (param (inputGainID));
    preamp.setBassDb      (param (bassID));
    preamp.setMidDb       (param (midID));
    preamp.setTrebleDb    (param (trebleID));
    preamp.process (buffer);

    // Drive: waveshaper + oversampling.
    drive.setShape (static_cast<flang::dsp::DriveShape> ((int) param (driveTypeID)));
    drive.setDriveAmount (param (driveID));
    drive.setTone (param (driveToneID));
    drive.setMix (param (driveMixID));

    // El choice 0/1/2 (2x/4x/8x) mapea al modo interno 1/2/3.
    const int oversamplingMode = 1 + (int) param (oversampleID);
    if (oversamplingMode != drive.getOversamplingMode())
    {
        drive.setOversamplingMode (oversamplingMode);
        setLatencySamples (drive.getLatencySamples());
    }

    drive.process (buffer);

    // Output gain.
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (param (outputGainID)));
    outputGain.applyGain (buffer, numSamples);
}

//==============================================================================
juce::AudioProcessorEditor* FlangAudioProcessor::createEditor()
{
    return new FlangAudioProcessorEditor (*this);
}

//==============================================================================
void FlangAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FlangAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// Fabrica del plugin: el host (VST3 / Standalone) llama a esto para instanciarlo.
// Con CMake hay que definirla a mano; el Projucer la generaba automaticamente.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FlangAudioProcessor();
}
