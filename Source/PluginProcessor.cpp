#include <cmath>

#include <juce_audio_formats/juce_audio_formats.h>

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
namespace
{
    // Soft clip de salida: lineal por debajo de 0.7, satura suave arriba.
    // Evita el recorte duro del host ("cruje") al empujar perillas.
    inline float softClip (float x) noexcept
    {
        constexpr float t = 0.7f;
        if (x >  t) return  t + (1.0f - t) * std::tanh ((x - t) / (1.0f - t));
        if (x < -t) return -t + (1.0f - t) * std::tanh ((x + t) / (1.0f - t));
        return x;
    }

    // Divisiones musicales del eco (nombre + multiplicador de pulsos).
    const juce::StringArray kDivisionNames { "1/1", "1/2", "1/4", "1/4.", "1/8",
                                             "1/8.", "1/8T", "1/16", "1/16." };
    const double kDivisionBeats[] { 4.0, 2.0, 1.0, 1.5, 0.5, 0.75, 1.0 / 3.0, 0.25, 0.375 };
    constexpr int kNumDivisions = (int) (sizeof (kDivisionBeats) / sizeof (kDivisionBeats[0]));
}

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
        juce::StringArray { "Soft", "Hard", "Fold", "Tube" }, 3));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driveToneID, 1 }, "Tone",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 1.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { driveMixID, 1 }, "Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { warmthID, 1 }, "Warmth",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.5f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { cabMixID, 1 }, "Cab",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.0f));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { oversampleID, 1 }, "Oversample",
        juce::StringArray { "2x", "4x", "8x" }, 1));

    // Mod (chorus / flanger).
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { modOnID, 1 }, "Mod", false));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { modModeID, 1 }, "Mode",
        juce::StringArray { "Chorus", "Flanger" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { modRateID, 1 }, "Rate",
        juce::NormalisableRange<float> { 0.05f, 5.0f, 0.001f, 0.4f }, 0.6f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { modDepthID, 1 }, "Depth",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.5f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { modFeedbackID, 1 }, "Feedback",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { modMixID, 1 }, "Mod Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.5f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { modSpreadID, 1 }, "Spread",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 1.0f));

    // Space: reverb + echo.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { spaceRoutingID, 1 }, "Order",
        juce::StringArray { "Reverb > Echo", "Echo > Reverb", "Parallel" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { reverbAmountID, 1 }, "Reverb",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { reverbDecayID, 1 }, "Decay",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.5f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { delayAmountID, 1 }, "Delay",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f }, 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { delayTimeID, 1 }, "Time",
        juce::NormalisableRange<float> { 20.0f, 1000.0f, 1.0f, 0.4f }, 350.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { delayFeedbackID, 1 }, "Repeats",
        juce::NormalisableRange<float> { 0.0f, 0.9f, 0.001f }, 0.35f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { delaySyncID, 1 }, "Sync", false));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { delayDivisionID, 1 }, "Division", kDivisionNames, 4));

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
    cab.prepare    (sampleRate, numChannels, samplesPerBlock);
    mod.prepare    (sampleRate, numChannels, samplesPerBlock);
    space.prepare  (sampleRate, numChannels, samplesPerBlock);
    tuner.prepare  (sampleRate);

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

    // Afinador: escucha la ENTRADA cruda (antes del drive/eq/cab).
    tuner.process (buffer.getReadPointer (0), numSamples);
    tunerFreq.store (tuner.getFrequency());

    // Preamp: input gain + EQ.
    preamp.setInputGainDb (param (inputGainID));
    preamp.setBassDb      (param (bassID));
    preamp.setMidDb       (param (midID));
    preamp.setTrebleDb    (param (trebleID));
    preamp.process (buffer);

    // Drive: waveshaper + oversampling.
    drive.setShape (static_cast<flang::dsp::DriveShape> ((int) param (driveTypeID)));
    drive.setDriveAmount (param (driveID));
    drive.setWarmth (param (warmthID));
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

    // Cab: simulador de parlante (convolucion).
    cab.setMix (param (cabMixID));
    cab.process (buffer);

    // Mod: chorus o flanger (mismo motor, distinto regimen de parametros).
    // Apagado => mix 0 (dry puro).
    const bool flanger = param (modModeID) > 0.5f;

    mod.setRate (param (modRateID));
    mod.setDepth (param (modDepthID));
    mod.setSpread (param (modSpreadID));
    mod.setCentreDelayMs (flanger ? 2.0f : 15.0f);
    mod.setFeedback (param (modFeedbackID) * (flanger ? 0.9f : 0.2f));
    mod.setMix (param (modOnID) > 0.5f ? param (modMixID) : 0.0f);
    mod.process (buffer);

    // Space: reverb + echo con ruteo conmutable.
    space.setReverbAmount (param (reverbAmountID));
    space.setReverbDecay (param (reverbDecayID));
    space.setDelayAmount (param (delayAmountID));
    space.setDelayTimeMs (param (delayTimeID));
    space.setDelayFeedback (param (delayFeedbackID));
    space.setRouting (static_cast<flang::dsp::SpaceRouting> ((int) param (spaceRoutingID)));

    // Tempo-sync: si esta activo, el tiempo del eco sigue al BPM del host.
    if (param (delaySyncID) > 0.5f)
    {
        double bpm = 120.0;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto b = pos->getBpm())
                    bpm = *b;

        const int div = juce::jlimit (0, kNumDivisions - 1, (int) param (delayDivisionID));
        space.setDelayTimeMs ((float) (kDivisionBeats[div] * 60.0 / juce::jmax (20.0, bpm) * 1000.0));
    }

    space.process (buffer);

    // Output gain + soft clip de seguridad.
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (param (outputGainID)));
    outputGain.applyGain (buffer, numSamples);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            d[i] = softClip (d[i]);
    }

    // Medidor: nivel de pico del bloque.
    levelL.store (buffer.getMagnitude (0, 0, numSamples));
    levelR.store (buffer.getNumChannels() > 1
                      ? buffer.getMagnitude (1, 0, numSamples)
                      : levelL.load());
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

    // Recordar la IR de parlante cargada (si la sesion la tenia).
    const auto irPath = apvts.state.getProperty ("cabIrPath").toString();
    if (irPath.isNotEmpty())
    {
        const juce::File f (irPath);
        if (f.existsAsFile())
            loadCabIR (f);
    }
}

//==============================================================================
void FlangAudioProcessor::loadCabIR (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
        return;

    const int numSamples = (int) juce::jmin ((juce::int64) flang::dsp::CabinetIR::kMaxIrLength,
                                             reader->lengthInSamples);
    if (numSamples <= 0)
        return;

    juce::AudioBuffer<float> ir ((int) juce::jmax (1u, reader->numChannels), numSamples);
    reader->read (&ir, 0, numSamples, 0, true, true);

    // Sumar a mono (las IR de parlante suelen ser mono).
    std::vector<float> mono ((size_t) numSamples, 0.0f);
    for (int ch = 0; ch < ir.getNumChannels(); ++ch)
        for (int i = 0; i < numSamples; ++i)
            mono[(size_t) i] += ir.getSample (ch, i);

    for (auto& v : mono)
        v /= (float) ir.getNumChannels();

    cab.setImpulseResponse (mono);
    cabIrName = file.getFileNameWithoutExtension();
    apvts.state.setProperty ("cabIrPath", file.getFullPathName(), nullptr);
    sendChangeMessage();
}

//==============================================================================
// Fabrica del plugin: el host (VST3 / Standalone) llama a esto para instanciarlo.
// Con CMake hay que definirla a mano; el Projucer la generaba automaticamente.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FlangAudioProcessor();
}
