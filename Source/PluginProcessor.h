#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/AmpSim.h"
#include "dsp/AmpModes.h"
#include "dsp/GuitarCuts.h"
#include "dsp/Filters.h"
#include "PresetManager.h"
#include "CabinetIR.h"

class VibeAmpProcessor : public juce::AudioProcessor,
                         private juce::AsyncUpdater
{
private:
    mutable std::recursive_mutex sceneMutex; // non-audio scene transactions only
public:
    explicit VibeAmpProcessor (juce::File presetFolder = {});
    ~VibeAmpProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int index) override { return index == 0 ? "Default" : juce::String {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Cabinet IR control API: message/host control threads only, never processBlock.
    // Success means decoded/validated (and prepared if already active).
    // Audio adopts the prepared engine at a block boundary.
    bool loadIR (const juce::File& file);
    juce::String getIRLoadError() const { return cabinet.getError(); }
    bool clearIR();
    void stepIR (int direction); // next / previous IR in the same folder
    juce::String getIRPath() const;
    int getEffectiveOversamplingFactor() const noexcept { return effectiveFactor.load(); }
    int getEffectiveAmpLanes() const noexcept { return effectiveLanes.load(); }
    bool isAmpTransitioning() const noexcept { return ampTransition.load(); }

    // Peak levels since the last call (linear), for the UI meters
    float popInputPeak()  { return inputPeak.exchange (0.0f); }
    float popOutputPeak() { return outputPeak.exchange (0.0f); }

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;

private:
    void processAudio (juce::AudioBuffer<float>&, bool bypassed);
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int len, int numIn, int numOut);
    void handleAsyncUpdate() override { setLatencySamples (targetLatency.load()); }

    // Every cached parameter has a live, audible compact signal path.
    std::atomic<float>* pInGain {};
    std::atomic<float>* pInGateOn {}, *pInGateThresh {}, *pInGateRelease {};
    std::atomic<float>* pAmpVolume {};
    std::atomic<float>* pBoostOn {}, *pBoostDrive {}, *pBoostTone {}, *pBoostLevel {};
    std::atomic<float>* pBoostAttack {}, *pBoostModel {};
    std::atomic<float>* pChannel {}, *pAmpBright {}, *pAmpCrunch {}, *pAmpNoise {};
    std::atomic<float>* pTubeSet {}, *pAmpGain {}, *pBass {}, *pMid {}, *pTreble {};
    std::atomic<float>* pPresence {}, *pResonance {}, *pMaster {};
    std::atomic<float>* pCabOn {}, *pLowCutOn {}, *pLowCutHz {};
    std::atomic<float>* pHighCutOn {}, *pHighCutHz {};
    std::atomic<float>* pOutGain {}, *pAmpMode {}, *pQuality {};

    double sampleRateHz = 44100.0;
    int maxBlock = 512;
    juce::AudioBuffer<float> stereoBuf;
    juce::AudioBuffer<float> dryBuf, cabinetDry, dryDelay;
    int dryPosition = 0, dryLatency = 0;
    juce::SmoothedValue<float> bypassMix, cabinetMix;

    // Compact stereo amplifier. Each lane has its own nonlinear circuit state.
    juce::dsp::Gain<float> inputGain, outputGain;
    bool inGateOn = false;
    float inGateThresh = -50.0f, inGateRelease = 20.0f;
    vamp::AmpModes ampBank;
    std::atomic<int> effectiveFactor {8}, effectiveLanes {1};
    std::atomic<bool> ampTransition {false};
    vamp::AmpSim::Settings ampSettings;
    CabinetIR cabinet;
    vamp::Biquad fallbackCab[2][5];
    vamp::GuitarCuts cuts;
    bool cabOn = true;
    float lastInput[2] { 0.0f, 0.0f };

    static void updatePeak (std::atomic<float>& target, float value)
    {
        float prev = target.load();
        while (value > prev && ! target.compare_exchange_weak (prev, value)) {}
    }

    std::atomic<int> targetLatency { 0 };
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VibeAmpProcessor)
};
