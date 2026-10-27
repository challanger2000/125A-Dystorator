#include "PluginEditor.h"
#include "Parameters.h"

using namespace ui;

// ===========================================================================
// IRPanel
// ===========================================================================
IRPanel::IRPanel (VibeAmpProcessor& p) : proc (p)
{
    lastDirectory = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    loadButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Select a cabinet IR", lastDirectory, "*.wav;*.aif;*.aiff");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile())
                                  {
                                      lastDirectory = f.getParentDirectory();
                                      proc.loadIR (f);
                                  }
                              });
    };
    prevButton.onClick = [this] { proc.stepIR (-1); };
    nextButton.onClick = [this] { proc.stepIR (1); };
    builtinButton.onClick = [this] { proc.clearIR(); };

    for (auto* b : { &loadButton, &prevButton, &nextButton, &builtinButton })
        addAndMakeVisible (b);

    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (font (13.0f, true));
    nameLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff101014));
    addAndMakeVisible (nameLabel);

    timerCallback();
    startTimerHz (5);
}

void IRPanel::timerCallback()
{
    const auto path = proc.getIRPath();
    const auto text = path.isEmpty() ? juce::String ("Built-in cab (drop .wav here)")
                                     : juce::File (path).getFileNameWithoutExtension();
    if (nameLabel.getText() != text)
        nameLabel.setText (text, juce::dontSendNotification);
}

void IRPanel::resized()
{
    auto b = getLocalBounds().reduced (4);
    nameLabel.setBounds (b.removeFromTop (juce::jmin (32, b.getHeight() / 3)));
    b.removeFromTop (8);
    auto row = b.removeFromTop (juce::jmin (30, b.getHeight() / 2));
    prevButton.setBounds (row.removeFromLeft (30));
    nextButton.setBounds (row.removeFromRight (30));
    loadButton.setBounds (row.reduced (4, 0));
    b.removeFromTop (6);
    builtinButton.setBounds (b.removeFromTop (juce::jmin (26, b.getHeight())).reduced (34, 0));
}

void IRPanel::paint (juce::Graphics& g)
{
    if (dragOver)
    {
        g.setColour (colours::yellow.withAlpha (0.25f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    }
}

bool IRPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif") || f.endsWithIgnoreCase (".aiff"))
            return true;
    return false;
}

void IRPanel::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    repaint();
    for (auto& f : files)
    {
        const juce::File file (f);
        if (file.existsAsFile())
        {
            lastDirectory = file.getParentDirectory();
            proc.loadIR (file);
            return;
        }
    }
}

// ===========================================================================
// PresetBar
// ===========================================================================
PresetBar::PresetBar (VibeAmpProcessor& p) : proc (p)
{
    box.setTextWhenNothingSelected ("(no preset)");
    box.onChange = [this]
    {
        const int index = box.getSelectedId() - 1;
        if (index >= 0 && proc.presets.getName (index) != proc.presets.getCurrentName())
            proc.presets.load (index);
    };
    prevButton.onClick = [this] { step (-1); };
    nextButton.onClick = [this] { step (1); };
    saveButton.onClick = [this] { savePreset(); };
    deleteButton.onClick = [this] { deletePreset(); };
    folderButton.onClick = [] { PresetManager::getUserFolder().startAsProcess(); };
    folderButton.setTooltip ("Open the preset folder (share the .xml files with friends)");

    for (auto* c : std::initializer_list<juce::Component*> { &box, &prevButton, &nextButton, &saveButton, &deleteButton, &folderButton })
        addAndMakeVisible (c);
    refreshList();
}

void PresetBar::refreshList()
{
    auto& pm = proc.presets;
    pm.refresh();
    box.clear (juce::dontSendNotification);
    box.addSectionHeading ("Factory");
    for (int i = 0; i < pm.getNumFactory(); ++i)
        box.addItem (pm.getName (i), i + 1);
    if (pm.getNumPresets() > pm.getNumFactory())
    {
        box.addSectionHeading ("User");
        for (int i = pm.getNumFactory(); i < pm.getNumPresets(); ++i)
            box.addItem (pm.getName (i), i + 1);
    }
    const int current = pm.indexOf (pm.getCurrentName());
    if (current >= 0) box.setSelectedId (current + 1, juce::dontSendNotification);
    deleteButton.setEnabled (current >= 0 && pm.isUser (current));
}

void PresetBar::step (int direction)
{
    auto& pm = proc.presets;
    const int n = pm.getNumPresets();
    if (n == 0) return;
    const int current = juce::jmax (0, pm.indexOf (pm.getCurrentName()));
    pm.load ((current + direction + n) % n);
    refreshList();
}

void PresetBar::savePreset()
{
    auto& pm = proc.presets;
    const int current = pm.indexOf (pm.getCurrentName());
    const auto suggestion = current >= 0 && pm.isUser (current) ? pm.getCurrentName() : juce::String ("My Preset");

    auto* aw = new juce::AlertWindow ("Save preset", "Preset name (saving with an existing name overwrites it):",
                                      juce::MessageBoxIconType::NoIcon, this);
    aw->addTextEditor ("name", suggestion);
    aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int result)
    {
        if (result == 1)
        {
            proc.presets.save (aw->getTextEditorContents ("name"));
            refreshList();
        }
    }), true);
}

void PresetBar::deletePreset()
{
    auto& pm = proc.presets;
    const int current = pm.indexOf (pm.getCurrentName());
    if (current < 0 || ! pm.isUser (current)) return;

    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Delete preset",
                                        "Delete \"" + pm.getCurrentName() + "\"?", "Delete", "Cancel", this,
                                        juce::ModalCallbackFunction::create ([this, current] (int result)
    {
        if (result == 1)
        {
            proc.presets.remove (current);
            proc.presets.setCurrentName ({});
            refreshList();
        }
    }));
}

void PresetBar::resized()
{
    auto b = getLocalBounds();
    folderButton.setBounds (b.removeFromRight (70).reduced (2, 0));
    deleteButton.setBounds (b.removeFromRight (70).reduced (2, 0));
    saveButton.setBounds (b.removeFromRight (64).reduced (2, 0));
    b.removeFromRight (6);
    nextButton.setBounds (b.removeFromRight (34).reduced (2, 0));
    prevButton.setBounds (b.removeFromLeft (34).reduced (2, 0));
    box.setBounds (b.reduced (2, 0));
}

// ===========================================================================
// Editor
// ===========================================================================
VibeAmpEditor::VibeAmpEditor (VibeAmpProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    // ---- row 1: pre-amp ----
    inSec = std::make_unique<Section> ("INPUT", colours::text);
    inSec->add<Knob> (s, ids::inGain, "Level", colours::text);
    inSec->add<LevelMeter> ([this] { return proc.popInputPeak(); }, true);

    inGateSec = std::make_unique<Section> ("INPUT GATE", colours::teal, &s, ids::inGateOn);
    inGateSec->add<Knob> (s, ids::inGateThresh, "Thresh", colours::teal);
    inGateSec->add<Knob> (s, ids::inGateRelease, "Release", colours::teal);

    boostSec = std::make_unique<Section> ("PRECISION DRIVE", colours::clean, &s, ids::boostOn);
    auto* boostModelSel = boostSec->add<ChoiceButtons> (s, ids::boostModel, "Model", juce::StringArray { "PRECISION", "TS9" }, colours::clean);
    boostModelSel->setTooltip ("PRECISION: Horizon Devices Precision Drive (tight, Attack sets the low end).  "
                               "TS9: Ibanez Tube Screamer (mid hump, smoother).");
    auto* boostLevelKnob = boostSec->add<Knob> (s, ids::boostLevel, "Volume", colours::clean);
    boostSec->add<Knob> (s, ids::boostDrive, "Drive", colours::clean);
    auto* boostToneKnob = boostSec->add<Knob> (s, ids::boostTone, "Bright", colours::clean);
    auto* boostAttackKnob = boostSec->add<Knob> (s, ids::boostAttack, "Attack", colours::clean);
    // the two pedals share the knobs: the names follow the selected one, the TS9 has no Attack
    boostModelSel->setOnChange ([this, boostLevelKnob, boostToneKnob, boostAttackKnob] (int model)
    {
        const bool ts9 = model == 1;
        boostSec->setTitle (ts9 ? "TS9 TUBE SCREAMER" : "PRECISION DRIVE");
        boostLevelKnob->setLabel (ts9 ? "Level" : "Volume");
        boostToneKnob->setLabel (ts9 ? "Tone" : "Bright");
        boostAttackKnob->setActive (! ts9);
    });

    // ---- row 2: amp ----
    ampSec = std::make_unique<Section> ("AMP / HIGH GAIN", colours::red);
    ampSec->add<ChannelSelector> (s, ids::channel, [this] (int ch) { setAmpChannelColour (ch); });
    ampSec->add<ToggleStack> (s, std::vector<ToggleStack::Item> {
        { ids::ampBright, "BRIGHT", "The rhythm channel's BRIGHT switch (C10 across R21, as on the amp): CLEAN and BLUE." },
        { ids::ampCrunch, "CRUNCH", "The rhythm channel's CRUNCH switch (relay K4, as on the amp): on, BLUE is the crunch "
                                    "rhythm; off, the 6505's rhythm channel without crunch (all five stages, less gain)." },
        { ids::ampNoise, "NOISE", "The preamp's own noise, from physics: thermal noise of its resistors, shot and flicker "
                                  "noise of its tubes. Off: a noiseless amp." } }, colours::red,
        ToggleStack::Item { ids::tubeSet, "TUBES", "Stock: the datasheet (nominal) tubes. Set A-E: particular real tubes, each "
                            "12AX7 and 6L6 within production spreads (gain, bias and the output pairs' balance differ "
                            "a little, as from one amp to the next)." });
    ampSec->add<Knob> (s, ids::ampGain, "Gain", colours::red);
    ampSec->add<Knob> (s, ids::bass, "Bass", colours::red);
    ampSec->add<Knob> (s, ids::mid, "Middle", colours::red);
    ampSec->add<Knob> (s, ids::treble, "Treble", colours::red);
    ampSec->add<Knob> (s, ids::presence, "Presence", colours::red);
    ampSec->add<Knob> (s, ids::resonance, "Resonance", colours::red);
    ampSec->add<Knob> (s, ids::master, "Master", colours::red);
    ampSec->add<Knob> (s, ids::ampVolume, "Volume", colours::red);
    setAmpChannelColour (juce::roundToInt (s.getRawParameterValue (ids::channel)->load()));

    // Only controls that actually reach the compact processor audio path are visible.
    // Legacy APVTS parameter IDs remain in Parameters.h for old-state compatibility.
    cabSec = std::make_unique<Section> ("CABINET IR", colours::orange, &s, ids::cabOn);
    cabSec->add<IRPanel> (proc);

    cutsSec = std::make_unique<Section> ("POST-CAB CUTS", colours::orange);
    cutsSec->add<Toggle> (s, ids::lowCutOn, "LOW ON", colours::orange);
    cutsSec->add<Knob> (s, ids::lowCutHz, "Low Hz", colours::orange);
    cutsSec->add<Toggle> (s, ids::highCutOn, "HIGH ON", colours::orange);
    cutsSec->add<Knob> (s, ids::highCutHz, "High Hz", colours::orange);

    modeSec = std::make_unique<Section> ("AMP MODE / QUALITY", colours::blue);
    modeSec->add<ChoiceButtons> (s, ids::ampMode, "Input path",
                                juce::StringArray { "MONO", "STEREO" }, colours::blue);
    modeSec->add<ChoiceButtons> (s, ids::quality, "Quality",
                                juce::StringArray { "ECO", "STANDARD", "HIGH" }, colours::blue)
        ->setTooltip ("ECO is intended for realtime tracking. Select HIGH before offline bounce. "
                      "HIGH selection is not automatic.");

    outSec = std::make_unique<Section> ("OUTPUT", colours::text);
    outSec->add<Knob> (s, ids::outGain, "Level", colours::text);
    outSec->add<LevelMeter> ([this] { return proc.popOutputPeak(); }, false);

    addAndMakeVisible (presetBar);
    for (auto* sec : { inSec.get(), inGateSec.get(), boostSec.get(), outSec.get(),
                       ampSec.get(), cabSec.get(), cutsSec.get(), modeSec.get() })
        addAndMakeVisible (sec);

    setResizable (true, true);
    setResizeLimits (1120, 640, 2400, 1500);
    setSize (1380, 730);
}

VibeAmpEditor::~VibeAmpEditor()
{
    setLookAndFeel (nullptr);
}

void VibeAmpEditor::setAmpChannelColour (int channel)
{
    if (ampSec == nullptr)
        return;
    const juce::Colour cols[] = { colours::clean, colours::blue, colours::red };
    ampSec->setAccent (cols[juce::jlimit (0, 2, channel)]);
}

void VibeAmpEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    auto header = getLocalBounds().reduced (16, 8).removeFromTop (40);
    g.setColour (juce::Colours::white);
    g.setFont (font (26.0f, true));
    g.drawText ("DYSTORATOR", header, juce::Justification::centredLeft);

    g.setColour (colours::text.withAlpha (0.5f));
    g.setFont (font (12.0f));
    g.drawText ("v" JucePlugin_VersionString, header.withTrimmedLeft (215), juce::Justification::centredLeft);

    const auto accent = ampSec ? ampSec->getAccent() : colours::red;
    g.setColour (accent);
    g.fillRect (16, 50, getWidth() - 32, 2);
}

namespace
{
void layoutRow (juce::Rectangle<int> r, std::initializer_list<std::pair<juce::Component*, float>> items)
{
    constexpr int gap = 8;
    float total = 0.0f;
    for (auto& it : items) total += it.second;
    const int avail = r.getWidth() - gap * ((int) items.size() - 1);

    for (auto& it : items)
    {
        it.first->setBounds (r.removeFromLeft (juce::roundToInt ((float) avail * it.second / total)));
        r.removeFromLeft (gap);
    }
}
} // namespace

void VibeAmpEditor::resized()
{
    auto b = getLocalBounds().reduced (16, 10);
    presetBar.setBounds (b.removeFromTop (50).removeFromRight (640).reduced (0, 6));

    constexpr int gap = 8;
    const int available = b.getHeight() - gap * 2;
    auto row1 = b.removeFromTop (juce::roundToInt (available * 0.22f));
    b.removeFromTop (gap);
    auto row2 = b.removeFromTop (juce::roundToInt (available * 0.42f));
    b.removeFromTop (gap);
    auto row3 = b;

    layoutRow (row1, { { inSec.get(), 2.0f }, { inGateSec.get(), 2.5f },
                        { boostSec.get(), 6.2f }, { outSec.get(), 2.0f } });
    ampSec->setBounds (row2);
    layoutRow (row3, { { cabSec.get(), 4.2f }, { cutsSec.get(), 4.1f },
                        { modeSec.get(), 3.7f } });
}
