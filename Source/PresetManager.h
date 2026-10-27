#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "dsp/Preamp5150.h"
#include <mutex>
#include <set>
#include <sstream>
#include <locale>

// Knob scales that changed, so that older states and preset files keep their sound:
//   1 -> 2 (v0.4): Gain / Master recalibrated against a real amp (see Preamp5150.h); states without the marker are v0.3
//   2 -> 3 (v0.5): the Precision Drive's Attack as on the real pedal: six positions, 1 = 470n (fullest) ... 6 = 33n;
//                  before, eight from the Dwarven Hammer clone in reverse (1 = 33n ... 8 = 470n): each old position goes
//                  to the same capacitor, 82n and 220n (not on the real pedal) to the nearest one (68n, 330n)
inline constexpr int kKnobCurvesVersion = 3;
inline constexpr const char* kKnobCurvesAttr = "knobCurves";

inline void upgradeKnobCurves (juce::XmlElement& xml)
{
    const int version = xml.getIntAttribute (kKnobCurvesAttr, 1);
    if (version >= kKnobCurvesVersion)
        return;
    if (version < 2)
    {
        bool lead = true; // the Gain curve differs per channel (lead pre-gain low end, see Preamp5150.h)
        for (auto* p : xml.getChildWithTagNameIterator ("PARAM"))
            if (p->getStringAttribute ("id") == ids::channel)
                lead = juce::roundToInt (p->getDoubleAttribute ("value")) == vamp::Preamp5150::Red;
        for (auto* p : xml.getChildWithTagNameIterator ("PARAM"))
        {
            const auto id = p->getStringAttribute ("id");
            const double v = p->getDoubleAttribute ("value") * 0.1;
            if (id == ids::ampGain) p->setAttribute ("value", 10.0 * vamp::gainKnobFromV03 (v, lead));
            else if (id == ids::master) p->setAttribute ("value", 10.0 * vamp::masterKnobFromV03 (v));
        }
    }
    if (version < 3)
    {
        // old 1..8 = 33n 47n 68n 82n 100n 220n 330n 470n -> new 1..6 = 470n 330n 100n 68n 47n 33n
        static constexpr int newAttack[8] = { 6, 5, 4, 4, 3, 2, 2, 1 };
        for (auto* p : xml.getChildWithTagNameIterator ("PARAM"))
            if (p->getStringAttribute ("id") == ids::boostAttack)
                p->setAttribute ("value", newAttack[juce::jlimit (1, 8, juce::roundToInt (p->getDoubleAttribute ("value"))) - 1]);
    }
    xml.setAttribute (kKnobCurvesAttr, kKnobCurvesVersion);
}

// Pre-release compatibility: remove the no-longer-existent controls from
// earlier 125A Dystorator state/preset XML before strict validation.
// They are not JUCE parameters, not automatable and have no DSP/UI path.
inline bool isRetiredCompactParameter (const juce::String& id)
{
    static constexpr const char* removed[] = {
        "gateOn", "gateThresh", "gateRelease", "gateLow", "gateMid", "gateHigh",
        "transpose", "transposeMode", "compOn", "compThresh", "compRatio",
        "compAttack", "compRelease", "compMakeup", "eqOn",
        "phaserOn", "phRate", "phDepth", "phFeedback", "phMix",
        "delayOn", "dlTime", "dlFeedback", "dlMix", "dlPingPong",
        "reverbOn", "rvSize", "rvDamping", "rvWidth", "rvMix",
        "eq0", "eq1", "eq2", "eq3", "eq4", "eq5", "eq6", "eq7", "eq8", "eq9"
    };
    for (const auto* oldId : removed)
        if (id == oldId) return true;
    return false;
}
inline void stripRetiredCompactParameters (juce::XmlElement& xml)
{
    for (auto* element = xml.getFirstChildElement(); element != nullptr;)
    {
        auto* next = element->getNextElement();
        if (element->hasTagName ("PARAM")
            && isRetiredCompactParameter (element->getStringAttribute ("id")))
            xml.removeChildElement (element, true);
        element = next;
    }
}

// Validate before any live parameter/IR mutation. Older partial presets remain
// supported: missing parameters use their actual declared defaults.
inline bool validatePresetState (juce::XmlElement& xml,
                                 juce::AudioProcessorValueTreeState& state,
                                 const juce::String& expectedTag)
{
    if (! xml.hasTagName (expectedTag)) return false;
    stripRetiredCompactParameters (xml);
    const int curves = xml.getIntAttribute (kKnobCurvesAttr, 1);
    if (curves < 1 || curves > kKnobCurvesVersion
        || (xml.hasAttribute (kKnobCurvesAttr) && xml.getStringAttribute (kKnobCurvesAttr) != juce::String (curves))) return false;
    if (xml.hasAttribute ("version"))
    {
        const auto v = xml.getStringAttribute ("version").upToFirstOccurrenceOf ("-", false, false);
        const auto parts = juce::StringArray::fromTokens (v, ".", "");
        if (parts.size() != 3) return false;
        for (const auto& part : parts)
            if (part.isEmpty() || part.length() > 4 || ! part.containsOnly ("0123456789")) return false;
        const auto supported = juce::StringArray::fromTokens (
            juce::String (JucePlugin_VersionString).upToFirstOccurrenceOf ("-", false, false), ".", "");
        if (supported.size() != 3) return false;
        for (int i = 0; i < 3; ++i)
        {
            if (parts[i].getIntValue() > supported[i].getIntValue()) return false;
            if (parts[i].getIntValue() < supported[i].getIntValue()) break;
        }
    }
    std::set<juce::String> seen;
    for (auto* item : xml.getChildIterator())
    {
        if (! item->hasTagName ("PARAM") || item->getNumChildElements() != 0
            || ! item->hasAttribute ("id") || ! item->hasAttribute ("value")) return false;
        const auto id = item->getStringAttribute ("id");
        auto* parameter = state.getParameter (id);
        if (! parameter || ! seen.insert (id).second) return false;
        const auto text = item->getStringAttribute ("value").trim().toStdString();
        std::istringstream in (text);
        in.imbue (std::locale::classic());
        double value;
        in >> std::noskipws >> value;
        if (! in || ! in.eof() || ! std::isfinite (value)) return false;
        const auto range = parameter->getNormalisableRange();
        const double maximum = curves < 3 && id == ids::boostAttack ? 8.0 : range.end;
        if (value < range.start || value > maximum) return false;
        if (range.interval > 0 && std::abs ((value - range.start) / range.interval
                                          - std::round ((value - range.start) / range.interval)) > 1e-5)
            return false;
    }
    if (seen.empty()) return false;
    upgradeKnobCurves (xml);
    return true;
}

// Factory presets (built into the plugin) + user presets (XML files in
// Documents/125A Systems/Dystorator/Presets, easy to share). Call everything from the message thread.
class PresetManager
{
public:
    using Values = std::vector<std::pair<const char*, float>>;
    struct Factory { const char* name; Values values; };

    PresetManager (juce::AudioProcessorValueTreeState& state,
                   std::function<bool (const juce::File&)> loadIRFn,
                   std::function<juce::String()> getIRPathFn,
                   juce::File folderOverride = {},
                   std::recursive_mutex* sharedMutex = nullptr)
        : apvts (state), loadIR (std::move (loadIRFn)), getIRPath (std::move (getIRPathFn)),
          userFolder (folderOverride == juce::File() ? getUserFolder() : folderOverride),
          mutex (sharedMutex ? *sharedMutex : ownedMutex)
    {
        refresh();
    }

    static juce::File getUserFolder()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                       .getChildFile ("125A Systems").getChildFile ("Dystorator").getChildFile ("Presets");
        dir.createDirectory();
        return dir;
    }

    void refresh()
    {
        std::lock_guard<std::recursive_mutex> lock (mutex);
        userFiles = userFolder.findChildFiles (juce::File::findFiles, false, "*.xml");
        userFiles.sort();
    }

    int getNumFactory() const { return (int) factory().size(); }
    int getNumPresets() const { std::lock_guard<std::recursive_mutex> lock (mutex); return getNumFactory() + userFiles.size(); }
    bool isUser (int index) const { return index >= getNumFactory(); }

    juce::String getName (int index) const
    {
        std::lock_guard<std::recursive_mutex> lock (mutex);
        if (index < 0 || index >= getNumPresets()) return {};
        return isUser (index) ? userFiles[index - getNumFactory()].getFileNameWithoutExtension()
                              : juce::String (factory()[(size_t) index].name);
    }

    int indexOf (const juce::String& name) const
    {
        std::lock_guard<std::recursive_mutex> lock (mutex);
        for (int i = 0; i < getNumPresets(); ++i)
            if (getName (i) == name) return i;
        return -1;
    }

    juce::String getCurrentName() const { std::lock_guard<std::recursive_mutex> lock (mutex); return currentName; }
    void setCurrentName (const juce::String& n) { std::lock_guard<std::recursive_mutex> lock (mutex); currentName = n; }
    juce::String getLastError() const { std::lock_guard<std::recursive_mutex> lock (mutex); return lastError; }

    bool load (int index)
    {
        std::lock_guard<std::recursive_mutex> lock (mutex);
        if (index < 0 || index >= getNumPresets()) return false;
        std::unique_ptr<juce::XmlElement> xml;
        juce::File ir;
        if (isUser (index))
        {
            const auto file = userFiles[index - getNumFactory()];
            if (file.getSize() <= 0 || file.getSize() > 1024 * 1024)
                return reject ("Preset file is missing, empty or too large");
            xml = juce::XmlDocument::parse (file);
            if (! xml || ! validatePresetState (*xml, apvts, "DystoratorPreset"))
                return reject ("Invalid or unsupported preset");
            const auto path = xml->getStringAttribute ("irPath");
            if (path.isNotEmpty())
            {
                if (! juce::File::isAbsolutePath (path)) return reject ("IR path must be absolute");
                ir = juce::File (path);
            }
        }
        // Empty File is an explicit built-in request. A missing/non-decodable
        // nonempty path rejects the ENTIRE scene before parameters/name change.
        if (! loadIR (ir)) return reject ("Cannot load preset IR; previous scene retained");
        resetAllToDefault();
        if (xml)
            for (auto* item : xml->getChildWithTagNameIterator ("PARAM"))
                setParam (item->getStringAttribute ("id"), (float) item->getDoubleAttribute ("value"));
        else
            for (auto& [id, value] : factory()[(size_t) index].values)
                setParam (id, value);
        currentName = getName (index);
        lastError.clear();
        return true;
    }

    bool save (const juce::String& rawName)
    {
        std::lock_guard<std::recursive_mutex> lock (mutex);
        const auto name = juce::File::createLegalFileName (rawName.trim());
        if (name.isEmpty()) return false;

        auto state = apvts.copyState();
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        if (xml == nullptr) return false;
        xml->setTagName ("DystoratorPreset");
        xml->setAttribute ("name", name);
        xml->setAttribute ("version", JucePlugin_VersionString);
        xml->setAttribute (kKnobCurvesAttr, kKnobCurvesVersion);
        xml->setAttribute ("irPath", getIRPath());

        juce::TemporaryFile temporary (userFolder.getChildFile (name + ".xml"));
        const bool ok = xml->writeTo (temporary.getFile()) && temporary.overwriteTargetFileWithTemporary();
        refresh();
        if (ok) currentName = name;
        return ok;
    }

    bool remove (int index)
    {
        std::lock_guard<std::recursive_mutex> lock (mutex);
        if (! isUser (index) || index >= getNumPresets()) return false;
        const bool ok = userFiles[index - getNumFactory()].deleteFile();
        refresh();
        return ok;
    }

private:
    void setParam (const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    }

    void resetAllToDefault()
    {
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                rp->beginChangeGesture();
                rp->setValueNotifyingHost (rp->getDefaultValue());
                rp->endChangeGesture();
            }
    }

    // Starting points tuned from the circuit's behaviour - adjust by ear.
    static const std::vector<Factory>& factory()
    {
        static const std::vector<Factory> presets = {
            { "Default", {} },
            { "Tight Rhythm", {
                { ids::channel, 2 }, { ids::ampGain, 6.1f }, { ids::bass, 4.5f },
                { ids::mid, 6.0f }, { ids::treble, 6.5f }, { ids::presence, 5.5f },
                { ids::resonance, 4.0f }, { ids::master, 5.1f },
                { ids::boostOn, 1 }, { ids::boostDrive, 0.0f }, { ids::boostLevel, 9.0f },
                { ids::boostTone, 6.0f }, { ids::boostAttack, 5 },
                { ids::inGateOn, 1 }, { ids::inGateThresh, -50.0f },
                { ids::inGateRelease, 15.0f } } },
            { "Modern Rhythm", {
                { ids::channel, 2 }, { ids::ampGain, 6.1f }, { ids::bass, 4.0f },
                { ids::mid, 6.0f }, { ids::treble, 6.5f }, { ids::presence, 5.5f },
                { ids::resonance, 3.5f }, { ids::master, 5.1f },
                { ids::boostOn, 1 }, { ids::boostDrive, 0.0f }, { ids::boostLevel, 9.0f },
                { ids::boostTone, 6.0f }, { ids::boostAttack, 6 },
                { ids::inGateOn, 1 }, { ids::inGateThresh, -50.0f },
                { ids::inGateRelease, 15.0f } } },
            { "Lead Drive", {
                { ids::channel, 2 }, { ids::ampGain, 7.7f }, { ids::bass, 5.0f },
                { ids::mid, 6.5f }, { ids::treble, 6.0f }, { ids::presence, 6.0f },
                { ids::resonance, 5.0f }, { ids::master, 5.7f },
                { ids::boostOn, 1 }, { ids::boostDrive, 2.0f },
                { ids::boostLevel, 7.0f }, { ids::boostAttack, 4 } } },
            { "Classic Crunch", {
                { ids::channel, 1 }, { ids::ampGain, 6.9f }, { ids::bass, 5.5f },
                { ids::mid, 5.5f }, { ids::treble, 6.0f }, { ids::presence, 5.0f },
                { ids::resonance, 5.0f }, { ids::master, 4.6f } } },
            { "Open Clean", {
                { ids::channel, 0 }, { ids::ampGain, 5.3f }, { ids::bass, 5.0f },
                { ids::mid, 5.0f }, { ids::treble, 6.5f },
                { ids::presence, 4.0f }, { ids::resonance, 3.0f },
                { ids::master, 5.7f } } },
        };
        return presets;
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::function<bool (const juce::File&)> loadIR;
    std::function<juce::String()> getIRPath;
    juce::File userFolder;
    juce::Array<juce::File> userFiles;
    juce::String currentName { "Default" };
    juce::String lastError;
    mutable std::recursive_mutex ownedMutex;
    std::recursive_mutex& mutex;
    bool reject (const juce::String& text) { lastError = text; return false; }
};
