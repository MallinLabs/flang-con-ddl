#pragma once

#include <cmath>

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace flang::ui
{

//==============================================================================
/** Display del afinador: nota + desviacion en cents (+ verde si esta afinado). */
class TunerDisplay : public juce::Component,
                     private juce::Timer
{
public:
    explicit TunerDisplay (FlangAudioProcessor& p) : proc (p)
    {
        startTimerHz (15);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        g.setColour (juce::Colour (0xff1a0f07));
        g.fillRoundedRectangle (b, 3.0f);

        const float freq = proc.getTunerFrequency();
        const bool active = freq > 20.0f;

        juce::String name = "--";
        float cents = 0.0f;

        if (active)
        {
            const float noteNum = 69.0f + 12.0f * std::log2 (freq / 440.0f);
            const int nearest = juce::roundToInt (noteNum);
            cents = (noteNum - (float) nearest) * 100.0f;

            static const char* names[] { "C", "C#", "D", "D#", "E", "F",
                                         "F#", "G", "G#", "A", "A#", "B" };
            name = juce::String (names[((nearest % 12) + 12) % 12]) + juce::String (nearest / 12 - 1);
        }

        const bool inTune = active && std::abs (cents) < 5.0f;
        const auto col = ! active ? juce::Colours::grey
                                  : (inTune ? juce::Colour (0xff5fd06a) : juce::Colour (0xffe0a83a));

        auto area = b.reduced (6.0f);

        g.setColour (col);
        g.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
        g.drawText (name, area.removeFromTop (area.getHeight() * 0.62f),
                    juce::Justification::centred, false);

        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (active ? juce::String (cents, 0) + " cents" : "tuner",
                    area, juce::Justification::centred, false);
    }

private:
    void timerCallback() override { repaint(); }

    FlangAudioProcessor& proc;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunerDisplay)
};

} // namespace flang::ui
