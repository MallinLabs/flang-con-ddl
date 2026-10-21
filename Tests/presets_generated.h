// Generado desde Source/PresetManager.h (makeFactoryPresets). No editar a mano.
#pragma once

#include <vector>
#include <utility>
#include <string>

namespace flang::generated {
struct PresetParam { const char* id; float value; };
struct Preset { const char* name; std::vector<PresetParam> params; };

inline const std::vector<Preset>& presets()
{
    static const std::vector<Preset> table {        
        { "Clean JC", { { "drive", 0.0f }, { "driveMix", 0.0f } } },
        { "Studio DI", { { "drive", 0.15f }, { "driveMix", 0.25f }, { "driveType", 3.0f }, { "warmth", 0.6f }, { "driveTone", 0.8f } } },
        { "Amped Clean", { { "drive", 0.1f }, { "driveMix", 0.2f }, { "driveType", 3.0f }, { "warmth", 0.6f }, { "cabMix", 1.0f } } },
        { "JC Chorus", { { "modOn", 1.0f }, { "modMode", 0.0f }, { "modRate", 0.5f }, { "modDepth", 0.5f }, { "modMix", 0.5f }, { "modSpread", 1.0f } } },
        { "Surf Clean", { { "bass", 3.0f }, { "treble", 6.0f }, { "drive", 0.1f }, { "driveMix", 0.15f }, { "driveType", 3.0f }, { "cabMix", 0.85f }, { "modOn", 1.0f }, { "modMode", 0.0f }, { "modRate", 0.8f }, { "modDepth", 0.4f }, { "modMix", 0.4f }, { "modSpread", 1.0f }, { "reverbAmount", 0.5f }, { "reverbDecay", 0.7f } } },
        { "Wet Shimmer", { { "bass", 2.0f }, { "treble", 5.0f }, { "drive", 0.15f }, { "driveMix", 0.25f }, { "driveType", 3.0f }, { "warmth", 0.6f }, { "cabMix", 0.8f }, { "modOn", 1.0f }, { "modMode", 0.0f }, { "modRate", 0.35f }, { "modDepth", 0.6f }, { "modMix", 0.5f }, { "modSpread", 1.0f }, { "reverbAmount", 0.55f }, { "reverbDecay", 0.8f }, { "delayAmount", 0.4f }, { "delayTime", 420.0f }, { "delayFeedback", 0.4f }, { "spaceRouting", 0.0f } } },
        { "Blues Breakup", { { "drive", 0.35f }, { "driveMix", 0.55f }, { "driveType", 3.0f }, { "warmth", 0.7f }, { "driveTone", 0.65f }, { "cabMix", 1.0f }, { "reverbAmount", 0.25f }, { "reverbDecay", 0.45f } } },
        { "Crunch", { { "drive", 0.45f }, { "driveMix", 0.75f }, { "driveType", 3.0f }, { "warmth", 0.6f }, { "driveTone", 0.7f }, { "cabMix", 0.9f } } },
        { "Amped Crunch", { { "drive", 0.45f }, { "driveMix", 0.75f }, { "driveType", 3.0f }, { "warmth", 0.6f }, { "cabMix", 1.0f } } },
        { "Tube Lead", { { "drive", 0.85f }, { "driveMix", 1.0f }, { "driveType", 3.0f }, { "warmth", 0.5f }, { "driveTone", 0.5f }, { "cabMix", 1.0f }, { "delayAmount", 0.35f }, { "delayTime", 380.0f }, { "delayFeedback", 0.4f }, { "reverbAmount", 0.35f }, { "reverbDecay", 0.6f }, { "spaceRouting", 1.0f } } },
        { "Dulce Lead", { { "mid", 2.0f }, { "drive", 0.7f }, { "driveMix", 0.95f }, { "driveType", 3.0f }, { "warmth", 0.7f }, { "driveTone", 0.55f }, { "cabMix", 0.9f }, { "reverbAmount", 0.4f }, { "reverbDecay", 0.6f }, { "delayAmount", 0.45f }, { "delayTime", 380.0f }, { "delayFeedback", 0.45f }, { "spaceRouting", 1.0f } } },
        { "Fuzz", { { "drive", 1.0f }, { "driveMix", 1.0f }, { "driveType", 2.0f }, { "driveTone", 0.4f } } },
        { "Scooped", { { "bass", 6.0f }, { "mid", -6.0f }, { "treble", 4.0f }, { "drive", 0.3f }, { "driveMix", 0.6f }, { "driveType", 3.0f }, { "cabMix", 0.9f } } },
        { "Doom", { { "bass", 4.0f }, { "mid", -4.0f }, { "drive", 1.0f }, { "driveMix", 1.0f }, { "driveType", 2.0f }, { "driveTone", 0.3f }, { "cabMix", 1.0f }, { "modOn", 1.0f }, { "modMode", 1.0f }, { "modRate", 0.2f }, { "modDepth", 0.6f }, { "modFeedback", 0.5f }, { "modMix", 0.5f } } },
        { "Jet Flanger", { { "drive", 0.6f }, { "driveMix", 0.8f }, { "driveType", 3.0f }, { "warmth", 0.5f }, { "cabMix", 0.9f }, { "modOn", 1.0f }, { "modMode", 1.0f }, { "modRate", 0.3f }, { "modDepth", 0.8f }, { "modFeedback", 0.7f }, { "modMix", 0.6f }, { "modSpread", 1.0f } } },
        { "Slow Swirl", { { "drive", 0.2f }, { "driveMix", 0.3f }, { "driveType", 3.0f }, { "cabMix", 0.9f }, { "modOn", 1.0f }, { "modMode", 0.0f }, { "modRate", 0.15f }, { "modDepth", 0.9f }, { "modFeedback", 0.3f }, { "modMix", 0.6f }, { "modSpread", 1.0f } } },
        { "Ambient Clean", { { "drive", 0.2f }, { "driveMix", 0.3f }, { "driveType", 3.0f }, { "warmth", 0.6f }, { "cabMix", 0.85f }, { "modOn", 1.0f }, { "modMode", 0.0f }, { "modRate", 0.4f }, { "modDepth", 0.5f }, { "modMix", 0.4f }, { "reverbAmount", 0.5f }, { "reverbDecay", 0.75f }, { "delayAmount", 0.35f }, { "delayTime", 500.0f }, { "delayFeedback", 0.4f }, { "spaceRouting", 2.0f } } },
        { "Cathedral Lead", { { "drive", 0.5f }, { "driveMix", 0.75f }, { "driveType", 3.0f }, { "warmth", 0.6f }, { "cabMix", 0.9f }, { "reverbAmount", 0.6f }, { "reverbDecay", 0.85f }, { "delayAmount", 0.5f }, { "delayTime", 460.0f }, { "delayFeedback", 0.5f }, { "spaceRouting", 1.0f } } },
        { "Stadium", { { "drive", 0.55f }, { "driveMix", 0.8f }, { "driveType", 3.0f }, { "warmth", 0.6f }, { "cabMix", 1.0f }, { "modOn", 1.0f }, { "modMode", 0.0f }, { "modRate", 0.4f }, { "modDepth", 0.4f }, { "modMix", 0.35f }, { "modSpread", 1.0f }, { "reverbAmount", 0.55f }, { "reverbDecay", 0.8f }, { "delayAmount", 0.4f }, { "delayTime", 420.0f }, { "delayFeedback", 0.45f }, { "spaceRouting", 0.0f } } },
        { "Dub Echo", { { "bass", 3.0f }, { "treble", -3.0f }, { "drive", 0.25f }, { "driveMix", 0.4f }, { "driveType", 3.0f }, { "warmth", 0.6f }, { "cabMix", 0.9f }, { "delayAmount", 0.6f }, { "delayTime", 550.0f }, { "delayFeedback", 0.72f }, { "reverbAmount", 0.3f }, { "reverbDecay", 0.7f }, { "spaceRouting", 1.0f } } },
    };
    return table;
}

} // namespace flang::generated
