#pragma once

#include <algorithm>

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace flang::ui
{

//==============================================================================
/** Medidor de nivel stereo (pico por bloque, con caida suave). */
class LevelMeter : public juce::Component,
                   private juce::Timer
{
public:
    explicit LevelMeter (FlangAudioProcessor& p) : proc (p)
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        g.setColour (juce::Colour (0xff1a0f07));
        g.fillRoundedRectangle (b, 2.0f);

        const float halfW = b.getWidth() * 0.5f;
        const float levels[2] { dispL, dispR };

        for (int ch = 0; ch < 2; ++ch)
        {
            auto cell = b.withWidth (halfW)
                            .withX (b.getX() + (float) ch * halfW)
                            .reduced (1.5f);

            const float v = juce::jlimit (0.0f, 1.0f, levels[ch]);
            auto bar = cell.removeFromBottom (cell.getHeight() * v);

            g.setColour (v > 0.92f ? juce::Colours::red : juce::Colour (0xffe0a83a));
            g.fillRect (bar);
        }
    }

private:
    void timerCallback() override
    {
        dispL = juce::jmax (proc.getLevel (0), dispL * 0.82f);
        dispR = juce::jmax (proc.getLevel (1), dispR * 0.82f);
        repaint();
    }

    FlangAudioProcessor& proc;
    float dispL = 0.0f, dispR = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
};

} // namespace flang::ui
