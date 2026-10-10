#pragma once

#include <array>

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "PresetManager.h"

//==============================================================================
/**
    GUI organizada en 3 secciones (PREAMP / DRIVE / MOD) para domar la cantidad
    de perillas, con tooltips y un boton de ayuda (?).
*/
class FlangAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit FlangAudioProcessorEditor (FlangAudioProcessor&);
    ~FlangAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Choice
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    void setupKnob (Knob& knob, const juce::String& name, const char* paramID,
                    const juce::String& tooltip);
    void setupChoice (Choice& choice, const juce::String& name, const char* paramID,
                      const juce::String& tooltip);
    void refreshPresetList();
    void saveCurrentPreset();
    void showHelp();

    FlangAudioProcessor& processorRef;
    std::array<Knob, 13> knobs;
    std::array<Choice, 3> choices;
    std::array<juce::Label, 3> sectionLabels;

    juce::ToggleButton modToggle { "Mod" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> modAttachment;

    juce::ComboBox presetBox;
    juce::TextEditor presetName;
    juce::TextButton saveButton { "Save" };
    juce::TextButton resetButton { "Reset" };
    juce::TextButton helpButton { "?" };
    flang::PresetManager presets;

    juce::TooltipWindow tooltipWindow { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlangAudioProcessorEditor)
};
