#include "PluginEditor.h"

//==============================================================================
namespace
{
    constexpr int kHeaderHeight = 60;
    constexpr int kKnobRows     = 2;
    constexpr int kChoicesHeight = 56;
    constexpr int kMaxKnobSize  = 80;
}

//==============================================================================
FlangAudioProcessorEditor::FlangAudioProcessorEditor (FlangAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p)
{
    setupKnob (knobs[0], "Input",  FlangAudioProcessor::inputGainID);
    setupKnob (knobs[1], "Drive",  FlangAudioProcessor::driveID);
    setupKnob (knobs[2], "Tone",   FlangAudioProcessor::driveToneID);
    setupKnob (knobs[3], "Mix",    FlangAudioProcessor::driveMixID);
    setupKnob (knobs[4], "Bass",   FlangAudioProcessor::bassID);
    setupKnob (knobs[5], "Mid",    FlangAudioProcessor::midID);
    setupKnob (knobs[6], "Treble", FlangAudioProcessor::trebleID);
    setupKnob (knobs[7], "Output", FlangAudioProcessor::outputGainID);

    setupChoice (choices[0], "Type",       FlangAudioProcessor::driveTypeID);
    setupChoice (choices[1], "Oversample", FlangAudioProcessor::oversampleID);

    setSize (620, 400);
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

void FlangAudioProcessorEditor::setupChoice (Choice& choice,
                                             const juce::String& name,
                                             const char* paramID)
{
    addAndMakeVisible (choice.box);

    choice.label.setText (name, juce::dontSendNotification);
    choice.label.setJustificationType (juce::Justification::centred);
    choice.label.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (choice.label);

    choice.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.apvts, paramID, choice.box);
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

    auto choiceRow = area.removeFromBottom (kChoicesHeight);
    const int perChoice = choiceRow.getWidth() / (int) choices.size();

    for (auto& choice : choices)
    {
        auto cell = choiceRow.removeFromLeft (perChoice).reduced (20, 0);
        choice.label.setBounds (cell.removeFromTop (18));
        choice.box.setBounds (cell.withSizeKeepingCentre (cell.getWidth(), 26));
    }

    const int perRow   = (int) knobs.size() / kKnobRows;
    const int rowHeight = area.getHeight() / kKnobRows;

    for (int row = 0; row < kKnobRows; ++row)
    {
        auto rowArea = area.removeFromTop (rowHeight);
        const int cellWidth = rowArea.getWidth() / perRow;

        for (int col = 0; col < perRow; ++col)
        {
            auto cell = rowArea.removeFromLeft (cellWidth);
            auto& knob = knobs[(size_t) (row * perRow + col)];

            knob.label.setBounds (cell.removeFromTop (18));

            const int size = juce::jmin (cell.getWidth(), cell.getHeight(), kMaxKnobSize);
            knob.slider.setBounds (cell.withSizeKeepingCentre (size, size));
        }
    }
}
