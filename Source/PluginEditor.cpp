#include "PluginEditor.h"

//==============================================================================
namespace
{
    constexpr int kHeaderHeight = 60;
    constexpr int kMaxKnobSize  = 92;
}

//==============================================================================
FlangAudioProcessorEditor::FlangAudioProcessorEditor (FlangAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p)
{
    setupKnob (knobs[0], "Input",  FlangAudioProcessor::inputGainID);
    setupKnob (knobs[1], "Bass",   FlangAudioProcessor::bassID);
    setupKnob (knobs[2], "Mid",    FlangAudioProcessor::midID);
    setupKnob (knobs[3], "Treble", FlangAudioProcessor::trebleID);
    setupKnob (knobs[4], "Output", FlangAudioProcessor::outputGainID);

    setSize (560, 240);
}

//==============================================================================
void FlangAudioProcessorEditor::setupKnob (Knob& knob,
                                           const juce::String& name,
                                           const char* paramID)
{
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
    knob.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                     juce::MathConstants<float>::pi * 2.75f,
                                     true);
    addAndMakeVisible (knob.slider);

    knob.label.setText (name, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (knob.label);

    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, paramID, knob.slider);
}

//==============================================================================
void FlangAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1b1f));

    auto header = getLocalBounds().removeFromTop (kHeaderHeight);

    g.setColour (juce::Colours::orange);
    g.setFont (22.0f);
    g.drawFittedText ("FLANG", header.withTrimmedBottom (18), juce::Justification::centred, 1);

    g.setColour (juce::Colours::grey);
    g.setFont (12.0f);
    g.drawFittedText ("con Dulce de Leche", header.removeFromBottom (18),
                      juce::Justification::centred, 1);
}

void FlangAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    area.removeFromTop (kHeaderHeight - 12);

    const int cellWidth = area.getWidth() / (int) knobs.size();

    for (auto& knob : knobs)
    {
        auto cell = area.removeFromLeft (cellWidth);
        knob.label.setBounds (cell.removeFromTop (18));

        const int size = juce::jmin (cell.getWidth(), cell.getHeight(), kMaxKnobSize);
        knob.slider.setBounds (cell.withSizeKeepingCentre (size, size));
    }
}
