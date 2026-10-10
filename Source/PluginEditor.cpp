#include "PluginEditor.h"

//==============================================================================
namespace
{
    constexpr int kHeaderHeight  = 60;
    constexpr int kKnobRows      = 3;
    constexpr int kChoicesHeight = 56;
    constexpr int kPresetHeight  = 28;
    constexpr int kSectionHeight = 16;
    constexpr int kMaxKnobSize   = 72;
}

//==============================================================================
FlangAudioProcessorEditor::FlangAudioProcessorEditor (FlangAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      presets (p.apvts)
{
    // --- PREAMP ---
    setupKnob (knobs[0],  "Input",  FlangAudioProcessor::inputGainID,
               "Input gain into the preamp (dB).");
    setupKnob (knobs[1],  "Bass",   FlangAudioProcessor::bassID,
               "Low shelf EQ (dB).");
    setupKnob (knobs[2],  "Mid",    FlangAudioProcessor::midID,
               "Mid peak EQ (dB).");
    setupKnob (knobs[3],  "Treble", FlangAudioProcessor::trebleID,
               "High shelf EQ (dB).");

    // --- DRIVE ---
    setupKnob (knobs[4],  "Drive",  FlangAudioProcessor::driveID,
               "Amount of overdrive / distortion.");
    setupKnob (knobs[5],  "Tone",   FlangAudioProcessor::driveToneID,
               "Post-drive lowpass: left = dark, right = bright.");
    setupKnob (knobs[6],  "Mix",    FlangAudioProcessor::driveMixID,
               "Clean <-> driven blend (0 = clean amp).");
    setupKnob (knobs[7],  "Output", FlangAudioProcessor::outputGainID,
               "Output level (dB).");

    // --- MOD ---
    setupKnob (knobs[8],  "Rate",     FlangAudioProcessor::modRateID,
               "Modulation speed (Hz).");
    setupKnob (knobs[9],  "Depth",    FlangAudioProcessor::modDepthID,
               "Modulation amount.");
    setupKnob (knobs[10], "Feedback", FlangAudioProcessor::modFeedbackID,
               "Resonance of the delay line (mostly for Flanger).");
    setupKnob (knobs[11], "ModMix",   FlangAudioProcessor::modMixID,
               "Modulation blend (0 = dry).");
    setupKnob (knobs[12], "Spread",   FlangAudioProcessor::modSpreadID,
               "Stereo LFO phase offset: 0 = mono, 1 = 180 degrees.");

    // --- Selectores ---
    setupChoice (choices[0], "Type",       FlangAudioProcessor::driveTypeID,
                 "Distortion shape: Soft (tanh), Hard (clip), Fold (foldback).");
    setupChoice (choices[1], "Oversample", FlangAudioProcessor::oversampleID,
                 "Anti-aliasing oversampling factor (higher = cleaner).");
    setupChoice (choices[2], "Mode",       FlangAudioProcessor::modModeID,
                 "Chorus (subtle, wide) or Flanger (short delay + feedback, jet).");

    // Toggle de Mod.
    addAndMakeVisible (modToggle);
    modToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::lightgrey);
    modToggle.setTooltip ("Enable the modulation (chorus / flanger).");
    modAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, FlangAudioProcessor::modOnID, modToggle);

    // Titulos de seccion.
    const juce::StringArray sectionNames { "PREAMP", "DRIVE", "MOD" };
    for (size_t i = 0; i < sectionLabels.size(); ++i)
    {
        auto& label = sectionLabels[i];
        label.setText (sectionNames[(int) i], juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setColour (juce::Label::textColourId, juce::Colours::orange.withAlpha (0.85f));
        label.setFont (juce::Font (juce::FontOptions (12.0f)));
        addAndMakeVisible (label);
    }

    // Barra de presets + ayuda.
    addAndMakeVisible (presetBox);
    presetBox.setTooltip ("Load a preset (factory or user).");
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedItemIndex();
        if (index >= 0)
            presets.load (index);
    };

    presetName.setText ("User Preset", false);
    presetName.setSelectAllWhenFocused (true);
    presetName.setTooltip ("Name for the preset you save.");
    addAndMakeVisible (presetName);

    saveButton.setTooltip ("Save the current settings as a user preset.");
    saveButton.onClick = [this] { saveCurrentPreset(); };
    addAndMakeVisible (saveButton);

    resetButton.setTooltip ("Reset every control to its default.");
    resetButton.onClick = [this]
    {
        presets.resetAll();
        presetBox.setSelectedItemIndex (0, juce::dontSendNotification);
    };
    addAndMakeVisible (resetButton);

    helpButton.setTooltip ("What does each control do?");
    helpButton.onClick = [this] { showHelp(); };
    addAndMakeVisible (helpButton);

    refreshPresetList();

    setSize (640, 560);
}

//==============================================================================
void FlangAudioProcessorEditor::setupKnob (Knob& knob,
                                           const juce::String& name,
                                           const char* paramID,
                                           const juce::String& tooltip)
{
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
    knob.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                     juce::MathConstants<float>::pi * 2.75f,
                                     true);
    knob.slider.setTooltip (tooltip);
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
                                             const char* paramID,
                                             const juce::String& tooltip)
{
    addAndMakeVisible (choice.box);
    choice.box.setTooltip (tooltip);

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

void FlangAudioProcessorEditor::showHelp()
{
    const juce::String text =
        "PREAMP\n"
        "  Input - gain into the amp\n"
        "  Bass / Mid / Treble - 3-band EQ (dB)\n\n"
        "DRIVE\n"
        "  Drive - amount of distortion\n"
        "  Tone - post-drive brightness\n"
        "  Mix - clean <-> driven (0 = clean amp)\n"
        "  Output - output level\n\n"
        "MOD\n"
        "  Mod - enable chorus / flanger\n"
        "  Mode - Chorus (subtle, wide) or Flanger (short delay + feedback, jet)\n"
        "  Rate - modulation speed\n"
        "  Depth - modulation amount\n"
        "  Feedback - resonance (mostly for Flanger)\n"
        "  Mod Mix - modulation blend\n"
        "  Spread - stereo width of the modulation\n\n"
        "Type / Oversample - distortion shape and anti-aliasing quality";

    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                            "Flang con Dulce de Leche - controls",
                                            text);
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

    // Barra de presets + ayuda (abajo de todo).
    auto presetRow = area.removeFromBottom (kPresetHeight);
    helpButton.setBounds (presetRow.removeFromRight (28));
    presetRow.removeFromRight (6);
    resetButton.setBounds (presetRow.removeFromRight (64));
    presetRow.removeFromRight (6);
    saveButton.setBounds (presetRow.removeFromRight (64));
    presetRow.removeFromRight (6);
    presetName.setBounds (presetRow.removeFromRight (150));
    presetRow.removeFromRight (6);
    presetBox.setBounds (presetRow);

    // Fila de selectores: Type | Oversample | Mode | Mod.
    auto choiceRow = area.removeFromBottom (kChoicesHeight);
    const int cellW = choiceRow.getWidth() / 4;

    auto layoutChoice = [&] (Choice& choice)
    {
        auto cell = choiceRow.removeFromLeft (cellW).reduced (10, 0);
        choice.label.setBounds (cell.removeFromTop (18));
        choice.box.setBounds (cell.withSizeKeepingCentre (cell.getWidth(), 26));
    };

    layoutChoice (choices[0]);
    layoutChoice (choices[1]);
    layoutChoice (choices[2]);

    auto modCell = choiceRow.reduced (10, 0);
    modToggle.setBounds (modCell.withSizeKeepingCentre (modCell.getWidth(), 26));

    // Perillas: 3 filas de 4 / 4 / 5.
    const std::array<int, kKnobRows> perRow { 4, 4, 5 };
    const int rowHeight = area.getHeight() / kKnobRows;
    int index = 0;

    for (int row = 0; row < kKnobRows; ++row)
    {
        auto rowArea = area.removeFromTop (rowHeight);
        sectionLabels[(size_t) row].setBounds (rowArea.removeFromTop (kSectionHeight).reduced (6, 0));

        const int cellWidth = rowArea.getWidth() / perRow[(size_t) row];

        for (int col = 0; col < perRow[(size_t) row]; ++col)
        {
            auto cell = rowArea.removeFromLeft (cellWidth);
            auto& knob = knobs[(size_t) index++];

            knob.label.setBounds (cell.removeFromTop (18));

            const int size = juce::jmin (cell.getWidth(), cell.getHeight(), kMaxKnobSize);
            knob.slider.setBounds (cell.withSizeKeepingCentre (size, size));
        }
    }
}
