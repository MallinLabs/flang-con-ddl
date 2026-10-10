#pragma once

#include <algorithm>
#include <utility>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "params/ParameterIDs.h"

namespace flang
{

//==============================================================================
/**
    Manejo de presets: factory (en codigo) + usuario (archivos XML en
    ~/Library/Application Support/MallinLabs/Flang con Dulce de Leche/Presets).

    No es DSP: es estado del plugin. Por eso vive en Source/ y no en Source/dsp/.
*/
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& stateToUse)
        : state (stateToUse), factory (makeFactoryPresets())
    {
    }

    //==========================================================================
    juce::StringArray getPresetNames() const
    {
        juce::StringArray names;

        for (const auto& preset : factory)
            names.add (preset.name);

        for (const auto& file : getUserFiles())
            names.add (file.getFileNameWithoutExtension());

        return names;
    }

    void load (int index)
    {
        const auto names = getPresetNames();
        if (! juce::isPositiveAndBelow (index, names.size()))
            return;

        if (index < (int) factory.size())
            applyFactory (factory[(size_t) index]);
        else
        {
            const auto userIndex = (size_t) (index - (int) factory.size());
            loadFile (getUserFiles()[userIndex]);
        }
    }

    juce::File save (const juce::String& name)
    {
        auto dir = getUserPresetDirectory();
        dir.createDirectory();

        auto file = dir.getChildFile (juce::File::createLegalFileName (name) + ".xml");

        if (auto xml = state.copyState().createXml())
            file.replaceWithText (xml->toString());

        return file;
    }

    /** Vuelve todos los parametros a su default. */
    void resetAll()
    {
        for (auto* parameter : state.processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                ranged->setValueNotifyingHost (ranged->getDefaultValue());
    }

    static juce::File getUserPresetDirectory()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Application Support")
                   .getChildFile ("MallinLabs")
                   .getChildFile ("Flang con Dulce de Leche")
                   .getChildFile ("Presets");
    }

private:
    //==========================================================================
    struct FactoryPreset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values; // id -> valor real
    };

    static std::vector<FactoryPreset> makeFactoryPresets()
    {
        namespace p = flang::params;

        return {
            { "Clean JC",   { { p::drive, 0.0f },  { p::driveMix, 0.0f } } },
            { "Crunch",     { { p::drive, 0.4f },  { p::driveMix, 0.6f },
                              { p::driveType, 0.0f }, { p::driveTone, 0.7f } } },
            { "Distortion", { { p::drive, 0.8f },  { p::driveMix, 1.0f },
                              { p::driveType, 1.0f }, { p::driveTone, 0.5f } } },
            { "Fuzz",       { { p::drive, 1.0f },  { p::driveMix, 1.0f },
                              { p::driveType, 2.0f }, { p::driveTone, 0.4f } } },
            { "Scooped",    { { p::bass, 6.0f },   { p::mid, -6.0f }, { p::treble, 4.0f },
                              { p::drive, 0.3f },  { p::driveMix, 0.5f } } },
        };
    }

    void applyFactory (const FactoryPreset& preset)
    {
        resetAll();

        for (const auto& [id, value] : preset.values)
            if (auto* param = state.getParameter (id))
                param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (value));
    }

    void loadFile (const juce::File& file)
    {
        if (auto xml = juce::XmlDocument::parse (file))
            if (xml->hasTagName (state.state.getType()))
                state.replaceState (juce::ValueTree::fromXml (*xml));
    }

    std::vector<juce::File> getUserFiles() const
    {
        std::vector<juce::File> files;

        auto dir = getUserPresetDirectory();
        if (dir.isDirectory())
        {
            juce::Array<juce::File> found;
            dir.findChildFiles (found, juce::File::findFiles, false, "*.xml");
            files.assign (found.begin(), found.end());
        }

        std::sort (files.begin(), files.end(),
                   [] (const juce::File& a, const juce::File& b)
                   { return a.getFileName() < b.getFileName(); });

        return files;
    }

    juce::AudioProcessorValueTreeState& state;
    std::vector<FactoryPreset> factory;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};

} // namespace flang
