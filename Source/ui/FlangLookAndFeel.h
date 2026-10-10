#pragma once

#include <cmath>

#include <juce_gui_basics/juce_gui_basics.h>

namespace flang::ui
{

//==============================================================================
/** Paleta "dulce de leche": tolex marron oscuro, panel de laton, perillas marfil. */
namespace colour
{
    inline const juce::Colour tolexTop    { 0xff2c1c11 };
    inline const juce::Colour tolexBottom { 0xff1b0f08 };
    inline const juce::Colour brassLight  { 0xffe3c063 };
    inline const juce::Colour brassMid    { 0xffc79a3a };
    inline const juce::Colour brassDark   { 0xff8f6b26 };
    inline const juce::Colour ivory       { 0xffefe3c2 };
    inline const juce::Colour ivoryDark   { 0xffc6b892 };
    inline const juce::Colour pointer     { 0xff33240f };
    inline const juce::Colour jewel       { 0xffffc86b };
    inline const juce::Colour jewelDark   { 0xff8a4a12 };
    inline const juce::Colour engraved    { 0xff33240f };
    inline const juce::Colour cream       { 0xffeee0bd };
    inline const juce::Colour inset       { 0xff2a1b0f };
}

//==============================================================================
/** LookAndFeel del ampli: perillas marfil, combos/insets oscuros, botones laton. */
class FlangLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FlangLookAndFeel() = default;

    //==========================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
        auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        auto centre = bounds.getCentre();
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Marcas alrededor (como un ampli).
        const int ticks = 11;
        for (int i = 0; i < ticks; ++i)
        {
            const float a = rotaryStartAngle + (float) i / (float) (ticks - 1)
                                                 * (rotaryEndAngle - rotaryStartAngle);
            const juce::Point<float> p1 (centre.x + std::sin (a) * (radius + 2.5f),
                                         centre.y - std::cos (a) * (radius + 2.5f));
            const juce::Point<float> p2 (centre.x + std::sin (a) * (radius + 5.5f),
                                         centre.y - std::cos (a) * (radius + 5.5f));
            g.setColour (colour::engraved.withAlpha (0.55f));
            g.drawLine ({ p1, p2 }, 1.0f);
        }

        // Sombra.
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (bounds.translated (0.0f, 2.0f));

        // Cuerpo marfil.
        juce::ColourGradient grad (colour::ivory, centre.x, bounds.getY(),
                                   colour::ivoryDark, centre.x, bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillEllipse (bounds);

        g.setColour (colour::pointer.withAlpha (0.7f));
        g.drawEllipse (bounds, 1.5f);

        // Puntero.
        juce::Path pointer;
        const float pw = juce::jmax (2.0f, radius * 0.14f);
        pointer.addRoundedRectangle (-pw * 0.5f, -radius * 0.95f, pw, radius * 0.62f, pw * 0.5f);
        g.setColour (colour::pointer);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));

        // Brillo.
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawEllipse (bounds.reduced (2.0f), 1.0f);
    }

    //==========================================================================
    void drawComboBox (juce::Graphics& g, int width, int height, bool,
                       int, int, int, int, juce::ComboBox&) override
    {
        auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);

        g.setColour (colour::inset);
        g.fillRoundedRectangle (bounds, 3.0f);

        g.setColour (colour::brassMid);
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

        juce::Path arrow;
        const float cx = (float) width - 12.0f;
        const float cy = (float) height * 0.5f;
        arrow.addTriangle (cx - 4.0f, cy - 2.5f, cx + 4.0f, cy - 2.5f, cx, cy + 3.0f);
        g.setColour (colour::cream);
        g.fillPath (arrow);
    }

    //==========================================================================
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour&, bool highlighted, bool down) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);

        auto base = colour::brassMid;
        if (down)             base = base.darker (0.25f);
        else if (highlighted) base = base.brighter (0.12f);

        juce::ColourGradient grad (base.brighter (0.18f), bounds.getCentreX(), bounds.getY(),
                                   base.darker (0.22f), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds, 4.0f);

        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawRoundedRectangle (bounds.reduced (1.5f), 3.0f, 1.0f);
    }

    //==========================================================================
    // Toggle como joya ambar + texto.
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool, bool) override
    {
        auto bounds = button.getLocalBounds().toFloat();

        auto jewelArea = bounds.removeFromLeft (juce::jmin (bounds.getHeight(), 26.0f)).reduced (4.0f);
        const bool on = button.getToggleState();

        if (on)
        {
            g.setColour (colour::jewel.withAlpha (0.35f));
            g.fillEllipse (jewelArea.expanded (4.0f));
        }

        juce::ColourGradient jg (on ? colour::jewel.brighter (0.4f) : juce::Colour (0xff5a4632),
                                 jewelArea.getCentreX(), jewelArea.getY(),
                                 on ? colour::jewelDark : juce::Colour (0xff2f2417),
                                 jewelArea.getCentreX(), jewelArea.getBottom(), false);
        g.setGradientFill (jg);
        g.fillEllipse (jewelArea);

        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (jewelArea, 1.0f);

        g.setColour (colour::engraved);
        g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
        g.drawText (button.getButtonText(), bounds.reduced (6.0f, 0.0f),
                    juce::Justification::centredLeft);
    }

    //==========================================================================
    juce::Font getLabelFont (juce::Label&) override
    {
        return juce::Font (juce::FontOptions (13.0f));
    }
};

} // namespace flang::ui
