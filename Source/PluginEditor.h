#pragma once

#include <array>

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

//==============================================================================
/**
    GUI de Fase 2: 8 perillas (Input/Drive/Tone/Mix/Bass/Mid/Treble/Output)
    + 2 selectores (Type/Oversample). Minimalista a proposito; la GUI "linda"
    es Fase 6.
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

    FlangAudioProcessor& processorRef;
    std::array<Knob, 8> knobs;
    std::array<Choice, 2> choices;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlangAudioProcessorEditor)
};
