#pragma once

#include <array>

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "PresetManager.h"
#include "ui/FlangLookAndFeel.h"
#include "ui/LevelMeter.h"
#include "ui/RoutingSwitch.h"

//==============================================================================
/**
    GUI con estetica de amplificador: cabina marron (tolex dulce de leche) con el
    nameplate, y panel de laton con los controles. 5 secciones:
    PREAMP / DRIVE / MOD / SPACE / CAB.
*/
class FlangAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::ChangeListener
{
public:
    explicit FlangAudioProcessorEditor (FlangAudioProcessor&);
    ~FlangAudioProcessorEditor() override;

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
    void chooseIR();
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void drawScrew (juce::Graphics& g, juce::Point<float> centre) const;

    FlangAudioProcessor& processorRef;
    flang::ui::FlangLookAndFeel lnf;
    juce::Image noise;

    std::array<Knob, 20> knobs;
    std::array<Choice, 4> choices;
    std::array<juce::Label, 5> sectionLabels;

    juce::ToggleButton modToggle { "Mod" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> modAttachment;
    flang::ui::RoutingSwitch routingSwitch;

    juce::ToggleButton syncToggle { "Sync" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;

    flang::ui::LevelMeter meter;

    juce::ComboBox presetBox;
    juce::TextEditor presetName;
    juce::Label irLabel;
    juce::TextButton loadIRButton { "Load IR" };
    juce::TextButton saveButton { "Save" };
    juce::TextButton resetButton { "Reset" };
    juce::TextButton helpButton { "?" };
    flang::PresetManager presets;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::TooltipWindow tooltipWindow { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlangAudioProcessorEditor)
};
