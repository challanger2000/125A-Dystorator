#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace ids
{
inline constexpr const char* inGain      = "inGain";
inline constexpr const char* inGateOn    = "inGateOn";
inline constexpr const char* inGateThresh = "inGateThresh";
inline constexpr const char* inGateRelease = "inGateRelease";
inline constexpr const char* boostOn     = "boostOn";
inline constexpr const char* boostDrive  = "boostDrive";
inline constexpr const char* boostTone   = "boostTone";
inline constexpr const char* boostLevel  = "boostLevel";
inline constexpr const char* boostAttack = "boostAttack";
inline constexpr const char* boostModel  = "boostModel";
inline constexpr const char* channel     = "channel";
inline constexpr const char* ampBright   = "ampBright";
inline constexpr const char* ampCrunch   = "ampCrunch";
inline constexpr const char* ampNoise    = "ampNoise";
inline constexpr const char* tubeSet     = "tubeSet";
inline constexpr const char* ampGain     = "ampGain";
inline constexpr const char* bass        = "bass";
inline constexpr const char* mid         = "mid";
inline constexpr const char* treble      = "treble";
inline constexpr const char* presence    = "presence";
inline constexpr const char* resonance   = "resonance";
inline constexpr const char* master      = "master";
inline constexpr const char* ampVolume   = "ampVolume";
inline constexpr const char* cabOn       = "cabOn";
inline constexpr const char* lowCutOn    = "lowCutOn";
inline constexpr const char* lowCutHz    = "lowCutHz";
inline constexpr const char* highCutOn   = "highCutOn";
inline constexpr const char* highCutHz   = "highCutHz";
inline constexpr const char* outGain     = "outGain";
inline constexpr const char* ampMode     = "ampMode";
inline constexpr const char* quality     = "quality";

} // namespace ids

namespace vamp
{
inline std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& id, const juce::String& name,
                                                             float min, float max, float def,
                                                             const juce::String& suffix = {}, int decimals = 1,
                                                             float skewCentre = 0.0f)
{
    juce::NormalisableRange<float> range (min, max);
    if (skewCentre > min && skewCentre < max)
        range.setSkewForCentre (skewCentre);

    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, range, def,
        juce::AudioParameterFloatAttributes()
            .withLabel (suffix)
            .withStringFromValueFunction ([decimals, suffix] (float v, int) { return juce::String (v, decimals) + suffix; }));
}

inline std::unique_ptr<juce::AudioParameterBool> makeBool (const juce::String& id, const juce::String& name, bool def)
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def);
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (makeFloat (ids::inGain, "Input", -24.0f, 24.0f, 0.0f, " dB"));

    layout.add (makeBool  (ids::inGateOn, "Input Gate On", false));
    layout.add (makeFloat (ids::inGateThresh, "Input Gate Threshold", -90.0f, 0.0f, -50.0f, " dB"));
    layout.add (makeFloat (ids::inGateRelease, "Input Gate Release", 2.0f, 200.0f, 20.0f, " ms", 0, 20.0f));

    layout.add (makeBool  (ids::boostOn, "Boost On", false));
    layout.add (makeFloat (ids::boostDrive, "Boost Drive", 0.0f, 10.0f, 0.0f));
    layout.add (makeFloat (ids::boostTone, "Drive Bright", 0.0f, 10.0f, 5.0f));
    layout.add (makeFloat (ids::boostLevel, "Drive Volume", 0.0f, 10.0f, 7.0f));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { ids::boostAttack, 1 }, "Drive Attack", 1, 6, 4));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::boostModel, 1 }, "Drive Model",
                                                              juce::StringArray { "Precision Drive", "TS9" }, 0));

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::channel, 1 }, "Channel",
                                                              juce::StringArray { "Clean", "Blue", "Red" }, 2));
    layout.add (makeBool (ids::ampBright, "Bright", true)); // the rhythm channel's BRIGHT pushbutton (CLEAN, BLUE)
    layout.add (makeBool (ids::ampCrunch, "Crunch", true)); // the rhythm channel's CRUNCH pushbutton (BLUE)
    layout.add (makeBool (ids::ampNoise, "Amp Noise", false)); // circuit noise: resistors' thermal, tubes' shot / flicker
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::tubeSet, 1 }, "Tube Set",
                                                              juce::StringArray { "Stock", "Set A", "Set B", "Set C", "Set D", "Set E" }, 0));
    layout.add (makeFloat (ids::ampGain, "Gain", 0.0f, 10.0f, 6.9f));
    layout.add (makeFloat (ids::bass, "Bass", 0.0f, 10.0f, 5.0f));
    layout.add (makeFloat (ids::mid, "Mid", 0.0f, 10.0f, 5.0f));
    layout.add (makeFloat (ids::treble, "Treble", 0.0f, 10.0f, 6.0f));
    layout.add (makeFloat (ids::presence, "Presence", 0.0f, 10.0f, 5.0f));
    layout.add (makeFloat (ids::resonance, "Resonance", 0.0f, 10.0f, 5.0f));
    layout.add (makeFloat (ids::master, "Master", 0.0f, 10.0f, 4.6f));
    layout.add (makeFloat (ids::ampVolume, "Amp Volume", -40.0f, 12.0f, 0.0f, " dB"));

    layout.add (makeBool (ids::cabOn, "Cab On", true));
    layout.add (makeBool  (ids::lowCutOn, "Low Cut On", false));
    layout.add (makeFloat (ids::lowCutHz, "Low Cut", 20.0f, 300.0f, 85.0f, " Hz", 0, 100.0f));
    layout.add (makeBool  (ids::highCutOn, "High Cut On", false));
    layout.add (makeFloat (ids::highCutHz, "High Cut", 3000.0f, 20000.0f, 10000.0f, " Hz", 0, 7000.0f));

    layout.add (makeFloat (ids::outGain, "Output", -24.0f, 24.0f, 0.0f, " dB"));
    // Defaults preserve the confirmed stereo/HIGH project sound. Missing legacy
    // fields are restored with these actual defaults by the existing migration.
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::ampMode, 1 },
        "AMP MODE", juce::StringArray { "MONO", "STEREO" }, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ids::quality, 1 },
        "QUALITY", juce::StringArray { "ECO", "STANDARD", "HIGH" }, 2));

    return layout;
}
} // namespace vamp
