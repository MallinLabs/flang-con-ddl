#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "FlangLookAndFeel.h"

namespace flang::ui
{

//==============================================================================
/** Switch fisico de 3 posiciones para el orden Reverb/Echo.
    Click cicla: Reverb>Echo -> Echo>Reverb -> Parallel. */
class RoutingSwitch : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    RoutingSwitch (juce::AudioProcessorValueTreeState& apvts, const char* paramID)
        : param (apvts.getParameter (paramID)),
          attachment (*param, [this] (float v)
          {
              // El callback puede llegar normalizado (0..1) o denormalizado (indice).
              normalised = (v <= 1.0f) ? v : (v / 2.0f);
              repaint();
          })
    {
        attachment.sendInitialUpdate();
        setTooltip ("Order: Reverb > Echo, Echo > Reverb, or Parallel.");
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        const int current = juce::jlimit (0, 2, juce::roundToInt (normalised * 2.0f));
        const int next = (current + 1) % 3;
        param->setValueNotifyingHost ((float) next / 2.0f); // 0, 0.5, 1 normalizado
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour (colour::inset);
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (colour::brassMid);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);

        const juce::StringArray labels { "R>E", "E>R", "||" };
        const int active = juce::jlimit (0, 2, juce::roundToInt (normalised * 2.0f));
        const float third = bounds.getWidth() / 3.0f;
        const float leverH = bounds.getHeight() * 0.42f;

        for (int i = 0; i < 3; ++i)
        {
            auto cell = juce::Rectangle<float> (bounds.getX() + (float) i * third,
                                                bounds.getY(), third, bounds.getHeight());

            auto textArea = cell.removeFromBottom (14.0f);
            g.setColour (i == active ? colour::cream : colour::cream.withAlpha (0.45f));
            g.setFont (juce::Font (juce::FontOptions (12.0f, i == active ? juce::Font::bold
                                                                         : juce::Font::plain)));
            g.drawText (labels[i], textArea, juce::Justification::centred, false);
        }

        auto lever = juce::Rectangle<float> (bounds.getX() + (float) active * third + third * 0.25f,
                                             bounds.getY() + 4.0f, third * 0.5f, leverH);
        juce::ColourGradient lg (colour::ivory, lever.getCentreX(), lever.getY(),
                                 colour::ivoryDark, lever.getCentreX(), lever.getBottom(), false);
        g.setGradientFill (lg);
        g.fillRoundedRectangle (lever, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (lever, 3.0f, 1.0f);
    }

private:
    juce::RangedAudioParameter* param = nullptr;
    float normalised = 0.0f;
    juce::ParameterAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoutingSwitch)
};

} // namespace flang::ui
