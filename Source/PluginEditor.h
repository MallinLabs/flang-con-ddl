#pragma once

#include <array>

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

//==============================================================================
/**
    GUI de Fase 1: Input / Bass / Mid / Treble / Output, atados al APVTS.
    Sigue siendo minimalista a proposito; la GUI "linda" es Fase 6.
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

    void setupKnob (Knob& knob, const juce::String& name, const char* paramID);

    FlangAudioProcessor& processorRef;
    std::array<Knob, 5> knobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlangAudioProcessorEditor)
};
