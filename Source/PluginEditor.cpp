#include "PluginEditor.h"

//==============================================================================
namespace
{
    constexpr int kHeaderHeight  = 60;
    constexpr int kKnobRows      = 3;
    constexpr int kChoicesHeight = 56;
    constexpr int kPresetHeight  = 28;
    constexpr int kMaxKnobSize   = 76;
}

//==============================================================================
FlangAudioProcessorEditor::FlangAudioProcessorEditor (FlangAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      presets (p.apvts)
{
    // Preamp.
    setupKnob (knobs[0],  "Input",  FlangAudioProcessor::inputGainID);
    setupKnob (knobs[1],  "Bass",   FlangAudioProcessor::bassID);
    setupKnob (knobs[2],  "Mid",    FlangAudioProcessor::midID);
    setupKnob (knobs[3],  "Treble", FlangAudioProcessor::trebleID);
    // Drive.
    setupKnob (knobs[4],  "Drive",  FlangAudioProcessor::driveID);
    setupKnob (knobs[5],  "Tone",   FlangAudioProcessor::driveToneID);
    setupKnob (knobs[6],  "Mix",    FlangAudioProcessor::driveMixID);
    setupKnob (knobs[7],  "Output", FlangAudioProcessor::outputGainID);
    // Mod (chorus).
    setupKnob (knobs[8],  "Rate",   FlangAudioProcessor::modRateID);
    setupKnob (knobs[9],  "Depth",  FlangAudioProcessor::modDepthID);
    setupKnob (knobs[10], "ModMix", FlangAudioProcessor::modMixID);
    setupKnob (knobs[11], "Spread", FlangAudioProcessor::modSpreadID);

    setupChoice (choices[0], "Type",       FlangAudioProcessor::driveTypeID);
    setupChoice (choices[1], "Oversample", FlangAudioProcessor::oversampleID);

    // Toggle de Mod.
    addAndMakeVisible (modToggle);
    modToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    modAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, FlangAudioProcessor::modOnID, modToggle);

    // Barra de presets.
    addAndMakeVisible (presetBox);
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedItemIndex();
        if (index >= 0)
            presets.load (index);
    };

    presetName.setText ("User Preset", false);
    presetName.setSelectAllWhenFocused (true);
    addAndMakeVisible (presetName);

    saveButton.onClick = [this] { saveCurrentPreset(); };
    addAndMakeVisible (saveButton);

    resetButton.onClick = [this]
    {
        presets.resetAll();
        presetBox.setSelectedItemIndex (0, juce::dontSendNotification);
    };
    addAndMakeVisible (resetButton);

    refreshPresetList();

    setSize (620, 540);
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

void FlangAudioProcessorEditor::refreshPresetList()
{
    presetBox.clear (juce::dontSendNotification);

    const auto names = presets.getPresetNames();
    for (int i = 0; i < names.size(); ++i)
        presetBox.addItem (names[i], i + 1);

    if (names.size() > 0)
        presetBox.setSelectedItemIndex (0, juce::dontSendNotification);
}

void FlangAudioProcessorEditor::saveCurrentPreset()
{
    auto name = presetName.getText().trim();
    if (name.isEmpty())
        name = "Preset";

    presets.save (name);
    refreshPresetList();

    if (presetBox.getNumItems() > 0)
        presetBox.setSelectedItemIndex (presetBox.getNumItems() - 1, juce::dontSendNotification);
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

    // Barra de presets (abajo de todo).
    auto presetRow = area.removeFromBottom (kPresetHeight);
    resetButton.setBounds (presetRow.removeFromRight (72));
    presetRow.removeFromRight (6);
    saveButton.setBounds (presetRow.removeFromRight (72));
    presetRow.removeFromRight (6);
    presetName.setBounds (presetRow.removeFromRight (150));
    presetRow.removeFromRight (6);
    presetBox.setBounds (presetRow);

    // Fila de selectores: Type | Oversample | Mod.
    auto choiceRow = area.removeFromBottom (kChoicesHeight);
    const int cellW = choiceRow.getWidth() / 3;

    auto typeCell = choiceRow.removeFromLeft (cellW).reduced (16, 0);
    choices[0].label.setBounds (typeCell.removeFromTop (18));
    choices[0].box.setBounds (typeCell.withSizeKeepingCentre (typeCell.getWidth(), 26));

    auto osCell = choiceRow.removeFromLeft (cellW).reduced (16, 0);
    choices[1].label.setBounds (osCell.removeFromTop (18));
    choices[1].box.setBounds (osCell.withSizeKeepingCentre (osCell.getWidth(), 26));

    auto modCell = choiceRow.reduced (16, 0);
    modToggle.setBounds (modCell.withSizeKeepingCentre (modCell.getWidth(), 26));

    // Perillas (3 filas x 4).
    const int perRow    = (int) knobs.size() / kKnobRows;
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
