#pragma once

namespace flang::params
{
    // IDs estables de parametros. Vive aca para compartirlos entre el processor
    // y el PresetManager sin duplicar literales.
    inline constexpr const char* inputGain  = "inputGain";
    inline constexpr const char* bass       = "bass";
    inline constexpr const char* mid        = "mid";
    inline constexpr const char* treble     = "treble";
    inline constexpr const char* drive      = "drive";
    inline constexpr const char* driveType  = "driveType";
    inline constexpr const char* driveTone  = "driveTone";
    inline constexpr const char* driveMix   = "driveMix";
    inline constexpr const char* warmth     = "warmth";
    inline constexpr const char* oversample = "oversample";
    inline constexpr const char* cabMix     = "cabMix";

    inline constexpr const char* modOn       = "modOn";
    inline constexpr const char* modMode     = "modMode";
    inline constexpr const char* modRate     = "modRate";
    inline constexpr const char* modDepth    = "modDepth";
    inline constexpr const char* modFeedback = "modFeedback";
    inline constexpr const char* modMix      = "modMix";
    inline constexpr const char* modSpread   = "modSpread";

    inline constexpr const char* spaceRouting  = "spaceRouting";
    inline constexpr const char* reverbAmount  = "reverbAmount";
    inline constexpr const char* reverbDecay   = "reverbDecay";
    inline constexpr const char* delayAmount   = "delayAmount";
    inline constexpr const char* delayTime     = "delayTime";
    inline constexpr const char* delayFeedback = "delayFeedback";
    inline constexpr const char* delaySync     = "delaySync";
    inline constexpr const char* delayDivision = "delayDivision";

    inline constexpr const char* outputGain = "output";
}
