#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/AmpSim.h"
#include "dsp/GuitarCuts.h"
#include "dsp/Filters.h"
#include "dsp/MultibandGate.h"
#include "dsp/PitchShifter.h"
#include "dsp/StereoDelay.h"
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
    const juce::String getProgramName (int) override { return {}; }
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

    // Peak levels since the last call (linear), for the UI meters
    float popInputPeak()  { return inputPeak.exchange (0.0f); }
    float popOutputPeak() { return outputPeak.exchange (0.0f); }

    // Noise gate band gains (0..1: low, mid, high) for the UI
    float getGateBandGain (int band) const { return gateBandGain[band].load(); }

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;

private:
    void processAudio (juce::AudioBuffer<float>&, bool bypassed);
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int len, int numIn, int numOut);
    void handleAsyncUpdate() override { setLatencySamples (targetLatency.load()); }

    // cached parameter pointers
    std::atomic<float>* pInGain {}; std::atomic<float>* pGateOn {}; std::atomic<float>* pGateThresh {};
    std::atomic<float>* pGateRelease {}; std::atomic<float>* pTranspose {};
    std::atomic<float>* pGateLow {}; std::atomic<float>* pGateMid {}; std::atomic<float>* pGateHigh {};
    std::atomic<float>* pTransposeMode {};
    std::atomic<float>* pInGateOn {}; std::atomic<float>* pInGateThresh {}; std::atomic<float>* pInGateRelease {};
    std::atomic<float>* pAmpVolume {};
    std::atomic<float>* pCompOn {}; std::atomic<float>* pCompThresh {}; std::atomic<float>* pCompRatio {};
    std::atomic<float>* pCompAttack {}; std::atomic<float>* pCompRelease {}; std::atomic<float>* pCompMakeup {};
    std::atomic<float>* pBoostOn {}; std::atomic<float>* pBoostDrive {}; std::atomic<float>* pBoostTone {};
    std::atomic<float>* pBoostLevel {}; std::atomic<float>* pBoostAttack {}; std::atomic<float>* pBoostModel {};
    std::atomic<float>* pChannel {}; std::atomic<float>* pAmpBright {}; std::atomic<float>* pAmpCrunch {}; std::atomic<float>* pAmpNoise {}; std::atomic<float>* pTubeSet {}; std::atomic<float>* pAmpGain {}; std::atomic<float>* pBass {};
    std::atomic<float>* pMid {}; std::atomic<float>* pTreble {}; std::atomic<float>* pPresence {};
    std::atomic<float>* pResonance {}; std::atomic<float>* pMaster {};
    std::atomic<float>* pCabOn {}; std::atomic<float>* pLowCutOn {}; std::atomic<float>* pLowCutHz {};
    std::atomic<float>* pHighCutOn {}; std::atomic<float>* pHighCutHz {}; std::atomic<float>* pEqOn {}; std::atomic<float>* pEq[10] {};
    std::atomic<float>* pPhaserOn {}; std::atomic<float>* pPhRate {}; std::atomic<float>* pPhDepth {};
    std::atomic<float>* pPhFeedback {}; std::atomic<float>* pPhMix {};
    std::atomic<float>* pDelayOn {}; std::atomic<float>* pDlTime {}; std::atomic<float>* pDlFeedback {};
    std::atomic<float>* pDlMix {}; std::atomic<float>* pDlPingPong {};
    std::atomic<float>* pReverbOn {}; std::atomic<float>* pRvSize {}; std::atomic<float>* pRvDamping {};
    std::atomic<float>* pRvWidth {}; std::atomic<float>* pRvMix {};
    std::atomic<float>* pOutGain {};

    double sampleRateHz = 44100.0;
    int maxBlock = 512;
    juce::AudioBuffer<float> stereoBuf;
    juce::AudioBuffer<float> dryBuf, cabinetDry, dryDelay;
    int dryPosition = 0, dryLatency = 0;
    juce::SmoothedValue<float> bypassMix, cabinetMix;

    // Compact stereo amplifier. Each lane has its own nonlinear circuit state.
    juce::dsp::Gain<float> inputGain, outputGain;
    vamp::InputGate inputGate[2];
    bool inGateOn = false;
    float inGateThresh = -50.0f, inGateRelease = 20.0f;
    vamp::AmpSim amp[2];
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
    std::atomic<float> gateBandGain[3] { { 1.0f }, { 1.0f }, { 1.0f } };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VibeAmpProcessor)
};
