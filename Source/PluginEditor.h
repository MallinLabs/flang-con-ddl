#pragma once

#include <array>

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "PresetManager.h"

//==============================================================================
/**
    GUI: 12 perillas (preamp / drive / mod), 2 selectores (Type / Oversample),
    un toggle de Mod, y una barra de presets (combo + nombre + Save + Reset).
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

    void setupKnob (Knob& knob, const juce::String& name, const char* paramID);
    void setupChoice (Choice& choice, const juce::String& name, const char* paramID);
    void refreshPresetList();
    void saveCurrentPreset();

    FlangAudioProcessor& processorRef;
    std::array<Knob, 12> knobs;
    std::array<Choice, 2> choices;

    juce::ToggleButton modToggle { "Mod" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> modAttachment;

    juce::ComboBox presetBox;
    juce::TextEditor presetName;
    juce::TextButton saveButton { "Save" };
    juce::TextButton resetButton { "Reset" };
    flang::PresetManager presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlangAudioProcessorEditor)
};
