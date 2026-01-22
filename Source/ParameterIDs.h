/*
  ==============================================================================
    BeccaToneAmp - Parameter IDs
    Becca's signature guitar tone in a box

    Define all parameter identifiers as constexpr strings.
    These IDs must match exactly between:
    - C++ (APVTS parameter creation)
    - C++ (relay creation)
    - TypeScript (getSliderState(), etc.)
  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ParamIDs
{
    // ===========================================================================
    // Amp Parameters (main amp knobs)
    // ===========================================================================
    inline constexpr const char* gain       = "gain";        // Drive/warmth
    inline constexpr const char* bass       = "bass";        // Low frequencies (lows)
    inline constexpr const char* mid        = "mid";         // Mid frequencies (mids)
    inline constexpr const char* treble     = "treble";      // High frequencies (highs)
    inline constexpr const char* presence   = "presence";    // High-mid clarity
    inline constexpr const char* level      = "level";       // Output level
    inline constexpr const char* volume     = "volume";      // Master volume
    inline constexpr const char* compression = "compression"; // Built-in compression

    // ===========================================================================
    // Chorus Pedal
    // ===========================================================================
    inline constexpr const char* chorusEnabled = "chorus_enabled";
    inline constexpr const char* chorusRate    = "chorus_rate";
    inline constexpr const char* chorusDepth   = "chorus_depth";
    inline constexpr const char* chorusMix     = "chorus_mix";

    // ===========================================================================
    // Tremolo Pedal
    // ===========================================================================
    inline constexpr const char* tremoloEnabled = "tremolo_enabled";
    inline constexpr const char* tremoloRate    = "tremolo_rate";
    inline constexpr const char* tremoloDepth   = "tremolo_depth";

    // ===========================================================================
    // Delay Pedal
    // ===========================================================================
    inline constexpr const char* delayEnabled  = "delay_enabled";
    inline constexpr const char* delayTime     = "delay_time";
    inline constexpr const char* delayFeedback = "delay_feedback";
    inline constexpr const char* delayMix      = "delay_mix";

    // ===========================================================================
    // Reverb Pedal
    // ===========================================================================
    inline constexpr const char* reverbEnabled = "reverb_enabled";
    inline constexpr const char* reverbSize    = "reverb_size";
    inline constexpr const char* reverbDamping = "reverb_damping";
    inline constexpr const char* reverbMix     = "reverb_mix";

    // ===========================================================================
    // Distortion/Fuzz Pedal
    // ===========================================================================
    inline constexpr const char* distortionEnabled = "distortion_enabled";
    inline constexpr const char* distortionDrive   = "distortion_drive";
    inline constexpr const char* distortionTone    = "distortion_tone";
    inline constexpr const char* distortionLevel   = "distortion_level";

    // ===========================================================================
    // Global
    // ===========================================================================
    inline constexpr const char* bypass = "bypass";
    inline constexpr const char* preset = "preset";

    // ===========================================================================
    // Advanced Controls (Expert Mode)
    // ===========================================================================
    inline constexpr const char* expertMode    = "expert_mode";
    inline constexpr const char* micDistance   = "mic_distance";   // Room: mic placement
    inline constexpr const char* cabStyle      = "cab_style";      // Cabinet style

    // 8-band EQ (Advanced)
    inline constexpr const char* eq60Hz   = "eq_60hz";
    inline constexpr const char* eq120Hz  = "eq_120hz";
    inline constexpr const char* eq250Hz  = "eq_250hz";
    inline constexpr const char* eq500Hz  = "eq_500hz";
    inline constexpr const char* eq1kHz   = "eq_1khz";
    inline constexpr const char* eq2kHz   = "eq_2khz";
    inline constexpr const char* eq4kHz   = "eq_4khz";
    inline constexpr const char* eq8kHz   = "eq_8khz";
}

// Preset names
namespace PresetNames
{
    inline const juce::StringArray presets {
        "Becca Signature Tone",
        "Pristine Clean",
        "Super Clean",
        "Overdriven / Fuzzy",
        "80s Dream",
        "Funk Groove",
        "Indie Grit"
    };
}

// Cabinet style options
namespace CabStyles
{
    inline const juce::StringArray styles {
        "Vintage",
        "Modern",
        "Tight"
    };
}
