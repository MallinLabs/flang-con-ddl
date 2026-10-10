#include "PluginEditor.h"

//==============================================================================
namespace
{
    constexpr int kPlateInset    = 24;
    constexpr int kPlatePad      = 14;
    constexpr int kHeaderHeight  = 74;
    constexpr int kKnobRows      = 5;
    constexpr int kChoicesHeight = 52;
    constexpr int kPresetHeight  = 28;
    constexpr int kSectionHeight = 15;
    constexpr int kMaxKnobSize   = 46;

    using flang::ui::colour::engraved;
    using flang::ui::colour::cream;
    using flang::ui::colour::brassMid;
    using flang::ui::colour::brassLight;
}

//==============================================================================
FlangAudioProcessorEditor::FlangAudioProcessorEditor (FlangAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      routingSwitch (p.apvts, FlangAudioProcessor::spaceRoutingID),
      presets (p.apvts)
{
    setLookAndFeel (&lnf);
    setResizable (true, false);
    setResizeLimits (620, 640, 1200, 1200);

    // Textura de tolex (ruido procedural cacheado).
    noise = juce::Image (juce::Image::ARGB, 48, 48, true);
    {
        juce::Random rng (0x5eed1234);
        for (int yy = 0; yy < noise.getHeight(); ++yy)
            for (int xx = 0; xx < noise.getWidth(); ++xx)
                noise.setPixelAt (xx, yy, juce::Colours::black.withAlpha (rng.nextFloat() * 0.5f));
    }

    // --- PREAMP ---
    setupKnob (knobs[0],  "Input",  FlangAudioProcessor::inputGainID, "Input gain into the preamp (dB).");
    setupKnob (knobs[1],  "Bass",   FlangAudioProcessor::bassID, "Low shelf EQ (dB).");
    setupKnob (knobs[2],  "Mid",    FlangAudioProcessor::midID,  "Mid peak EQ (dB).");
    setupKnob (knobs[3],  "Treble", FlangAudioProcessor::trebleID, "High shelf EQ (dB).");

    // --- DRIVE ---
    setupKnob (knobs[4],  "Drive",  FlangAudioProcessor::driveID, "Amount of overdrive / distortion.");
    setupKnob (knobs[5],  "Warmth", FlangAudioProcessor::warmthID,
               "Tube warmth: even harmonics (asymmetry).");
    setupKnob (knobs[6],  "Tone",   FlangAudioProcessor::driveToneID,
               "Post-drive lowpass: left = dark, right = bright.");
    setupKnob (knobs[7],  "Mix",    FlangAudioProcessor::driveMixID, "Clean <-> driven (0 = clean amp).");
    setupKnob (knobs[8],  "Output", FlangAudioProcessor::outputGainID, "Output level (dB).");

    // --- MOD ---
    setupKnob (knobs[9],  "Rate",     FlangAudioProcessor::modRateID, "Modulation speed (Hz).");
    setupKnob (knobs[10], "Depth",    FlangAudioProcessor::modDepthID, "Modulation amount.");
    setupKnob (knobs[11], "Feedback", FlangAudioProcessor::modFeedbackID,
               "Resonance of the delay line (mostly for Flanger).");
    setupKnob (knobs[12], "ModMix",   FlangAudioProcessor::modMixID, "Modulation blend (0 = dry).");
    setupKnob (knobs[13], "Spread",   FlangAudioProcessor::modSpreadID,
               "Stereo LFO phase offset: 0 = mono, 1 = 180 degrees.");

    // --- SPACE ---
    setupKnob (knobs[14], "Reverb",  FlangAudioProcessor::reverbAmountID, "Reverb amount.");
    setupKnob (knobs[15], "Decay",   FlangAudioProcessor::reverbDecayID,  "Reverb size / tail length.");
    setupKnob (knobs[16], "Delay",   FlangAudioProcessor::delayAmountID,  "Echo amount.");
    setupKnob (knobs[17], "Time",    FlangAudioProcessor::delayTimeID,    "Echo time (ms).");
    setupKnob (knobs[18], "Repeats", FlangAudioProcessor::delayFeedbackID, "Echo feedback (number of repeats).");

    // --- CAB ---
    setupKnob (knobs[19], "Cab", FlangAudioProcessor::cabMixID, "Speaker simulation blend (0 = off).");

    // --- Selectores ---
    setupChoice (choices[0], "Type",       FlangAudioProcessor::driveTypeID,
                 "Distortion shape: Tube (warm), Soft, Hard, Fold.");
    setupChoice (choices[1], "Oversample", FlangAudioProcessor::oversampleID,
                 "Anti-aliasing oversampling factor (higher = cleaner).");
    setupChoice (choices[2], "Mode",       FlangAudioProcessor::modModeID,
                 "Chorus (subtle, wide) or Flanger (short delay + feedback, jet).");

    // Switch fisico de orden (Reverb/Echo).
    addAndMakeVisible (routingSwitch);

    // Toggle de Mod (joya ambar).
    addAndMakeVisible (modToggle);
    modToggle.setTooltip ("Enable the modulation (chorus / flanger).");
    modAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, FlangAudioProcessor::modOnID, modToggle);

    // Titulos de seccion grabados.
    const juce::StringArray sectionNames { "PREAMP", "DRIVE", "MOD", "SPACE", "CAB" };
    for (size_t i = 0; i < sectionLabels.size(); ++i)
    {
        auto& label = sectionLabels[i];
        label.setText (sectionNames[(int) i], juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredLeft);
        label.setColour (juce::Label::textColourId, engraved);
        label.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        addAndMakeVisible (label);
    }

    // --- Footer: presets + IR + ayuda ---
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
    presetName.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff2a1b0f));
    presetName.setColour (juce::TextEditor::textColourId, cream);
    presetName.setColour (juce::TextEditor::outlineColourId, brassMid.withAlpha (0.6f));
    presetName.setColour (juce::TextEditor::focusedOutlineColourId, cream);
    addAndMakeVisible (presetName);

    auto styleMetalButton = [] (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::textColourOffId, engraved);
        b.setColour (juce::TextButton::textColourOnId, engraved);
    };

    loadIRButton.setTooltip ("Load a speaker impulse response (.wav / .aif).");
    loadIRButton.onClick = [this] { chooseIR(); };
    styleMetalButton (loadIRButton);
    addAndMakeVisible (loadIRButton);

    saveButton.setTooltip ("Save the current settings as a user preset.");
    saveButton.onClick = [this] { saveCurrentPreset(); };
    styleMetalButton (saveButton);
    addAndMakeVisible (saveButton);

    resetButton.setTooltip ("Reset every control to its default.");
    resetButton.onClick = [this]
    {
        presets.resetAll();
        presetBox.setSelectedItemIndex (0, juce::dontSendNotification);
    };
    styleMetalButton (resetButton);
    addAndMakeVisible (resetButton);

    helpButton.setTooltip ("What does each control do?");
    helpButton.onClick = [this] { showHelp(); };
    styleMetalButton (helpButton);
    addAndMakeVisible (helpButton);

    refreshPresetList();

    setSize (700, 700);
}

FlangAudioProcessorEditor::~FlangAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void FlangAudioProcessorEditor::setupKnob (Knob& knob,
                                           const juce::String& name,
                                           const char* paramID,
                                           const juce::String& tooltip)
{
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    knob.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                     juce::MathConstants<float>::pi * 2.75f,
                                     true);
    knob.slider.setTooltip (tooltip);
    knob.slider.setColour (juce::Slider::textBoxTextColourId, engraved);
    knob.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    knob.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (knob.slider);

    knob.label.setText (name, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setColour (juce::Label::textColourId, engraved);
    knob.label.setFont (juce::Font (juce::FontOptions (11.0f)));
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
    choice.box.setColour (juce::ComboBox::textColourId, cream);
    choice.box.setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    choice.box.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);

    choice.label.setText (name, juce::dontSendNotification);
    choice.label.setJustificationType (juce::Justification::centred);
    choice.label.setColour (juce::Label::textColourId, engraved);
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

void FlangAudioProcessorEditor::chooseIR()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Load a speaker impulse response...",
                                                       juce::File{}, "*.wav;*.aif;*.aiff");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();
                                  if (file.existsAsFile())
                                      processorRef.loadCabIR (file);
                              });
}

void FlangAudioProcessorEditor::showHelp()
{
    const juce::String text =
        "PREAMP\n"
        "  Input - gain into the amp\n"
        "  Bass / Mid / Treble - 3-band EQ (dB)\n\n"
        "DRIVE\n"
        "  Drive - amount of distortion\n"
        "  Warmth - tube even-harmonic warmth (asymmetry)\n"
        "  Tone - post-drive brightness\n"
        "  Mix - clean <-> driven (0 = clean amp)\n"
        "  Output - output level\n\n"
        "MOD\n"
        "  Mod - enable chorus / flanger\n"
        "  Mode - Chorus (subtle, wide) or Flanger (jet)\n"
        "  Rate / Depth - modulation speed and amount\n"
        "  Feedback - resonance (mostly for Flanger)\n"
        "  Mod Mix - modulation blend\n"
        "  Spread - stereo width of the modulation\n\n"
        "SPACE\n"
        "  Reverb / Decay - reverb amount and size\n"
        "  Delay / Time / Repeats - echo amount, time (ms) and feedback\n"
        "  Order switch - Reverb > Echo, Echo > Reverb, or Parallel\n\n"
        "CAB\n"
        "  Cab - speaker simulation blend (Load IR to use your own .wav/.aif)\n\n"
        "Type / Oversample - distortion shape and anti-aliasing quality";

    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                            "Flang con Dulce de Leche - controls",
                                            text);
}

//==============================================================================
void FlangAudioProcessorEditor::drawScrew (juce::Graphics& g, juce::Point<float> centre) const
{
    juce::Rectangle<float> r (centre.x - 5.0f, centre.y - 5.0f, 10.0f, 10.0f);

    juce::ColourGradient grad (juce::Colour (0xff6b5330), centre.x, r.getY(),
                               juce::Colour (0xff3a2a16), centre.x, r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (r);

    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (r, 1.0f);
    g.drawLine (centre.x - 3.5f, centre.y, centre.x + 3.5f, centre.y, 1.2f);
}

//==============================================================================
void FlangAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace flang::ui::colour;

    // Tolex: degradado + textura.
    juce::ColourGradient tolex (tolexTop, 0.0f, 0.0f, tolexBottom, 0.0f, (float) getHeight(), false);
    g.setGradientFill (tolex);
    g.fillAll();
    g.setTiledImageFill (noise, 0, 0, 0.08f);
    g.fillAll();

    auto outer = getLocalBounds().toFloat().reduced ((float) kPlateInset);
    auto header = outer.removeFromTop ((float) kHeaderHeight);
    auto plate = outer;

    // Panel de laton (solo los controles; la cabina marron queda arriba y alrededor).
    juce::ColourGradient brass (brassLight, plate.getCentreX(), plate.getY(),
                                brassDark, plate.getCentreX(), plate.getBottom(), false);
    g.setGradientFill (brass);
    g.fillRoundedRectangle (plate, 10.0f);

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (plate, 10.0f, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (plate.reduced (2.5f), 8.0f, 1.0f);

    drawScrew (g, { plate.getX() + 16.0f,     plate.getY() + 16.0f });
    drawScrew (g, { plate.getRight() - 16.0f, plate.getY() + 16.0f });
    drawScrew (g, { plate.getX() + 16.0f,     plate.getBottom() - 16.0f });
    drawScrew (g, { plate.getRight() - 16.0f, plate.getBottom() - 16.0f });

    // Nameplate sobre la cabina marron.
    g.setColour (brassLight);
    g.setFont (juce::Font (juce::FontOptions ("Snell Roundhand", 44.0f, juce::Font::plain)));
    g.drawText ("Flang", header.removeFromTop (48.0f), juce::Justification::centred, false);

    g.setColour (cream.withAlpha (0.85f));
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawText ("con Dulce de Leche", header, juce::Justification::centred, false);
}

void FlangAudioProcessorEditor::resized()
{
    auto outer = getLocalBounds().reduced (kPlateInset);
    outer.removeFromTop (kHeaderHeight);
    auto content = outer.reduced (kPlatePad);

    // Footer: preset | nombre | Save | Reset | ?
    auto presetRow = content.removeFromBottom (kPresetHeight);
    helpButton.setBounds (presetRow.removeFromRight (28));
    presetRow.removeFromRight (6);
    resetButton.setBounds (presetRow.removeFromRight (60));
    presetRow.removeFromRight (6);
    saveButton.setBounds (presetRow.removeFromRight (60));
    presetRow.removeFromRight (6);
    presetName.setBounds (presetRow.removeFromRight (130));
    presetRow.removeFromRight (6);
    presetBox.setBounds (presetRow);

    // Fila de selectores: Type | Oversample | Mode | Order | Mod.
    auto choiceRow = content.removeFromBottom (kChoicesHeight);
    const int cellW = choiceRow.getWidth() / 5;

    auto layoutChoice = [&] (Choice& choice)
    {
        auto cell = choiceRow.removeFromLeft (cellW).reduced (8, 0);
        choice.label.setBounds (cell.removeFromTop (16));
        choice.box.setBounds (cell.withSizeKeepingCentre (cell.getWidth(), 24));
    };

    layoutChoice (choices[0]);
    layoutChoice (choices[1]);
    layoutChoice (choices[2]);

    routingSwitch.setBounds (choiceRow.removeFromLeft (cellW).reduced (8, 6));

    auto modCell = choiceRow.reduced (8, 0);
    modToggle.setBounds (modCell.withSizeKeepingCentre (modCell.getWidth(), 26));

    // Perillas: 5 filas de 4 / 5 / 5 / 5 / 1 (+ boton Load IR en la fila CAB).
    const std::array<int, kKnobRows> perRow { 4, 5, 5, 5, 1 };
    const int rowHeight = content.getHeight() / kKnobRows;
    int index = 0;

    for (int row = 0; row < kKnobRows; ++row)
    {
        auto rowArea = content.removeFromTop (rowHeight);
        sectionLabels[(size_t) row].setBounds (rowArea.removeFromTop (kSectionHeight).reduced (6, 0));

        const int cellWidth = rowArea.getWidth() / perRow[(size_t) row];

        for (int col = 0; col < perRow[(size_t) row]; ++col)
        {
            auto cell = rowArea.removeFromLeft (cellWidth);
            auto& knob = knobs[(size_t) index++];

            knob.label.setBounds (cell.removeFromTop (16));

            const int size = juce::jmin (cell.getWidth(), cell.getHeight(), kMaxKnobSize);
            knob.slider.setBounds (cell.withSizeKeepingCentre (size, size));
        }

        if (row == kKnobRows - 1)
        {
            auto cell = rowArea.removeFromLeft (cellWidth).reduced (cellWidth / 6, 0);
            loadIRButton.setBounds (cell.withSizeKeepingCentre (cell.getWidth(), 26));
        }
    }
}
