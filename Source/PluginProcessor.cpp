#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

#if defined (VAMP_IR_TEST_CAPTURE)
// Observational integration-test tap; absent from shipping builds.
extern void irTestCaptureCabinetInput (const float*, int);
#endif

VibeAmpProcessor::VibeAmpProcessor (juce::File presetFolder)
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::mono(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", vamp::createParameterLayout()),
      presets (apvts, [this] (const juce::File& f)
      {
          if (f == juce::File()) return clearIR();
          return loadIR (f);
      }, [this] { return getIRPath(); }, presetFolder, &sceneMutex)
{
    auto get = [this] (const juce::String& id)
    {
        auto* p = apvts.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };

    pInGain = get (ids::inGain);
    pGateOn = get (ids::gateOn); pGateThresh = get (ids::gateThresh); pGateRelease = get (ids::gateRelease);
    pGateLow = get (ids::gateLow); pGateMid = get (ids::gateMid); pGateHigh = get (ids::gateHigh);
    pTranspose = get (ids::transpose); pTransposeMode = get (ids::transposeMode);
    pInGateOn = get (ids::inGateOn); pInGateThresh = get (ids::inGateThresh); pInGateRelease = get (ids::inGateRelease);
    pAmpVolume = get (ids::ampVolume);
    pCompOn = get (ids::compOn); pCompThresh = get (ids::compThresh); pCompRatio = get (ids::compRatio);
    pCompAttack = get (ids::compAttack); pCompRelease = get (ids::compRelease); pCompMakeup = get (ids::compMakeup);
    pBoostOn = get (ids::boostOn); pBoostDrive = get (ids::boostDrive); pBoostTone = get (ids::boostTone);
    pBoostLevel = get (ids::boostLevel); pBoostAttack = get (ids::boostAttack); pBoostModel = get (ids::boostModel);
    pChannel = get (ids::channel); pAmpBright = get (ids::ampBright); pAmpCrunch = get (ids::ampCrunch); pAmpNoise = get (ids::ampNoise); pTubeSet = get (ids::tubeSet); pAmpGain = get (ids::ampGain); pBass = get (ids::bass); pMid = get (ids::mid);
    pTreble = get (ids::treble); pPresence = get (ids::presence); pResonance = get (ids::resonance);
    pMaster = get (ids::master);
    pCabOn = get (ids::cabOn);
    pLowCutOn = get (ids::lowCutOn); pLowCutHz = get (ids::lowCutHz);
    pHighCutOn = get (ids::highCutOn); pHighCutHz = get (ids::highCutHz);
    pEqOn = get (ids::eqOn);
    for (int i = 0; i < 10; ++i)
        pEq[i] = get (ids::eqBand (i));
    pPhaserOn = get (ids::phaserOn); pPhRate = get (ids::phRate); pPhDepth = get (ids::phDepth);
    pPhFeedback = get (ids::phFeedback); pPhMix = get (ids::phMix);
    pDelayOn = get (ids::delayOn); pDlTime = get (ids::dlTime); pDlFeedback = get (ids::dlFeedback);
    pDlMix = get (ids::dlMix); pDlPingPong = get (ids::dlPingPong);
    pReverbOn = get (ids::reverbOn); pRvSize = get (ids::rvSize); pRvDamping = get (ids::rvDamping);
    pRvWidth = get (ids::rvWidth); pRvMix = get (ids::rvMix);
    pOutGain = get (ids::outGain);
    pAmpMode = get (ids::ampMode); pQuality = get (ids::quality);
}

bool VibeAmpProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const auto ok = [] (const juce::AudioChannelSet& s)
    { return s == juce::AudioChannelSet::mono() || s == juce::AudioChannelSet::stereo(); };
    return ok (in) && ok (out);
}

void VibeAmpProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateHz = sampleRate;
    maxBlock = juce::jmax (1, samplesPerBlock);
    stereoBuf.setSize (2, maxBlock);
    dryBuf.setSize (2, maxBlock);
    cabinetDry.setSize (2, maxBlock);

    const juce::dsp::ProcessSpec stereoSpec { sampleRate, (juce::uint32) maxBlock, 2 };
    inputGain.prepare (stereoSpec);
    inputGain.setRampDurationSeconds (0.05);
    outputGain.prepare (stereoSpec);
    outputGain.setRampDurationSeconds (0.05);

    for (int channel = 0; channel < 2; ++channel)
    {
        using T = vamp::Biquad::Type;
        auto& f = fallbackCab[channel];
        f[0].set (T::highPass, sampleRate, 80.0, 0.7);
        f[1].set (T::peak, sampleRate, 120.0, 1.2, 3.0);
        f[2].set (T::peak, sampleRate, 400.0, 1.0, -3.0);
        f[3].set (T::peak, sampleRate, 2500.0, 1.5, 3.0);
        f[4].set (T::lowPass, sampleRate, 5000.0, 0.9);
        for (auto& biquad : f) biquad.reset();
        lastInput[channel] = 0.0f;
    }

    // CabinetIR's immutable prepared engines remain under the audited B01
    // control/audio ownership contract, now prepared for two output channels.
    cabinet.prepare (stereoSpec);
    cuts.prepare (sampleRate);
    inGateOn = false;
    ampBank.prepare (sampleRate, maxBlock, getTotalNumInputChannels(),
                     juce::roundToInt (pAmpMode->load()), juce::roundToInt (pQuality->load()));
    effectiveFactor = ampBank.getActiveFactor(); effectiveLanes = ampBank.getActiveLanes(); ampTransition = false;
    const int latency = ampBank.getLatencySamples();
    dryLatency = latency;
    dryDelay.setSize (2, latency + 1);
    dryDelay.clear();
    dryPosition = 0;
    bypassMix.reset (sampleRate, 0.005);
    bypassMix.setCurrentAndTargetValue (0.0f);
    cabinetMix.reset (sampleRate, 0.005);
    cabinetMix.setCurrentAndTargetValue (pCabOn->load() > 0.5f ? 1.0f : 0.0f);
    targetLatency = latency;
    setLatencySamples (latency);
}

void VibeAmpProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    processAudio (buffer, false);
}

void VibeAmpProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    processAudio (buffer, true);
}

void VibeAmpProcessor::processAudio (juce::AudioBuffer<float>& buffer, bool bypassed)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    inputGain.setGainDecibels (pInGain->load());
    const bool enabled = pInGateOn->load() > 0.5f;
    inGateOn = enabled;
    inGateThresh = pInGateThresh->load();
    inGateRelease = pInGateRelease->load();

    ampSettings.channel = juce::roundToInt (pChannel->load());
    ampSettings.bright = pAmpBright->load() > 0.5f;
    ampSettings.crunch = pAmpCrunch->load() > 0.5f;
    ampSettings.noise = pAmpNoise->load() > 0.5f;
    ampSettings.tubeSet = juce::roundToInt (pTubeSet->load());
    ampSettings.gain = pAmpGain->load() * 0.1f;
    ampSettings.bass = pBass->load() * 0.1f;
    ampSettings.mid = pMid->load() * 0.1f;
    ampSettings.treble = pTreble->load() * 0.1f;
    ampSettings.presence = pPresence->load() * 0.1f;
    ampSettings.resonance = pResonance->load() * 0.1f;
    ampSettings.master = pMaster->load() * 0.1f;
    ampSettings.volumeDb = pAmpVolume->load();
    ampSettings.boostOn = pBoostOn->load() > 0.5f;
    ampSettings.boostDrive = pBoostDrive->load() * 0.1f;
    ampSettings.boostBright = pBoostTone->load() * 0.1f;
    ampSettings.boostLevel = pBoostLevel->load() * 0.1f;
    ampSettings.boostAttack = juce::roundToInt (pBoostAttack->load()) - 1;
    ampSettings.boostModel = juce::roundToInt (pBoostModel->load());

    cabOn = pCabOn->load() > 0.5f;
    cabinetMix.setTargetValue (cabOn ? 1.0f : 0.0f);
    bypassMix.setTargetValue (bypassed ? 1.0f : 0.0f);
    cuts.setCutoffs (pLowCutHz->load(), pHighCutHz->load());
    cuts.setEnabled (pLowCutOn->load() > 0.5f, pHighCutOn->load() > 0.5f);
    outputGain.setGainDecibels (pOutGain->load());

    // Obsolete legacy parameter IDs stay available to project recall.
    // Compressor, Transpose, EQ, Phaser, Delay, Reverb and loop gate have
    // NO active audio path in this compact product.
    for (int start = 0; start < numSamples; start += maxBlock)
        processChunk (buffer, start, juce::jmin (maxBlock, numSamples - start), numIn, numOut);
}

void VibeAmpProcessor::processChunk (juce::AudioBuffer<float>& buffer, int start, int len, int numIn, int numOut)
{
    // Each stereo input has its own circuit. Mono inputs process ONE amp
    // and split into L/R only immediately before the stereo cabinet stage.
    const int ampLanes = numIn >= 2 ? 2 : 1;
    for (int channel = 0; channel < ampLanes; ++channel)
    {
        if (numIn > channel)
            stereoBuf.copyFrom (channel, 0, buffer, channel, start, len);
        else
            stereoBuf.clear (channel, 0, len);

        float* data = stereoBuf.getWritePointer (channel);
        for (int i = 0; i < len; ++i)
            if (std::isfinite (data[i]))
                lastInput[channel] = data[i];
            else
                data[i] = lastInput[channel];
    }
    if (ampLanes == 1)
        stereoBuf.copyFrom (1, 0, stereoBuf, 0, 0, len);

    // Continuously advance the pre-gain dry delay, including while wet. Both
    // output layouts share one frame clock; no state/allocation on bypass entry.
    for (int i = 0; i < len; ++i)
    {
        for (int channel = 0; channel < 2; ++channel)
        {
            const float input = stereoBuf.getSample (channel, i);
            dryDelay.setSample (channel, dryPosition, input);
            const int read = (dryPosition + 1) % (dryLatency + 1);
            dryBuf.setSample (channel, i, dryDelay.getSample (channel, read));
        }
        dryPosition = (dryPosition + 1) % (dryLatency + 1);
    }

    auto stereoBlock = juce::dsp::AudioBlock<float> (stereoBuf).getSubBlock (0, (size_t) len);
    juce::dsp::ProcessContextReplacing<float> stereoCtx (stereoBlock);
    inputGain.process (stereoCtx);
    updatePeak (inputPeak, juce::jmax (stereoBuf.getMagnitude (0, 0, len),
                                       stereoBuf.getMagnitude (1, 0, len)));

    ampBank.process (stereoBlock, ampSettings, numIn, juce::roundToInt (pAmpMode->load()),
                     juce::roundToInt (pQuality->load()), inGateOn, inGateThresh, inGateRelease);
    effectiveFactor = ampBank.getActiveFactor(); effectiveLanes = ampBank.getActiveLanes();
    ampTransition = ampBank.transitioning();

#if defined (VAMP_IR_TEST_CAPTURE)
    irTestCaptureCabinetInput (stereoBuf.getReadPointer (0), len);
#endif

    for (int channel = 0; channel < 2; ++channel)
        cabinetDry.copyFrom (channel, 0, stereoBuf, channel, 0, len);
    // Keep convolution history and pending IR transitions warm while CAB is off.
    cabinet.processStereo (stereoBlock, [this] (float* data, int count, int channel) noexcept
        {
            for (int i = 0; i < count; ++i)
            {
                float value = data[i];
                for (auto& f : fallbackCab[channel]) value = f.process (value);
                data[i] = value;
            }
        });
    for (int i = 0; i < len; ++i)
    {
        const float wet = cabinetMix.getNextValue();
        for (int channel = 0; channel < 2; ++channel)
        {
            const float dry = cabinetDry.getSample (channel, i);
            if (wet == 0.0f) stereoBuf.setSample (channel, i, dry);
            else if (wet != 1.0f)
                stereoBuf.setSample (channel, i, dry + wet * (stereoBuf.getSample (channel, i) - dry));
        }
    }

    // 2-pole Butterworth cuts with common coefficients and separate L/R
    // states. On/off and cutoff modulation are smoothed by GuitarCuts.
    float* left = stereoBuf.getWritePointer (0);
    float* right = stereoBuf.getWritePointer (1);
    for (int i = 0; i < len; ++i)
        cuts.processStereo (left[i], right[i]);

    outputGain.process (stereoCtx);
    for (int i = 0; i < len; ++i)
    {
        const float bypass = bypassMix.getNextValue();
        if (bypass == 0.0f) continue;
        for (int channel = 0; channel < 2; ++channel)
        {
            const float dry = dryBuf.getSample (channel, i);
            const float wet = stereoBuf.getSample (channel, i);
            stereoBuf.setSample (channel, i, bypass == 1.0f ? dry : wet + bypass * (dry - wet));
        }
    }
    updatePeak (outputPeak, juce::jmax (stereoBuf.getMagnitude (0, 0, len),
                                        stereoBuf.getMagnitude (1, 0, len)));

    if (numOut >= 2)
    {
        buffer.copyFrom (0, start, stereoBuf, 0, 0, len);
        buffer.copyFrom (1, start, stereoBuf, 1, 0, len);
    }
    else if (numOut == 1)
    {
        buffer.copyFrom (0, start, stereoBuf, 0, 0, len);
        buffer.addFrom (0, start, stereoBuf, 1, 0, len);
        buffer.applyGain (0, start, len, 0.5f);
    }
    for (int channel = juce::jlimit (0, 2, numOut); channel < buffer.getNumChannels(); ++channel)
        buffer.clear (channel, start, len);

    for (auto& band : gateBandGain) band.store (1.0f); // legacy meter only
}

// ---------------------------------------------------------------------------
// IR handling
// ---------------------------------------------------------------------------
bool VibeAmpProcessor::loadIR (const juce::File& file)
{
    std::lock_guard<std::recursive_mutex> lock (sceneMutex);
    return cabinet.load (file);
}

bool VibeAmpProcessor::clearIR()
{
    std::lock_guard<std::recursive_mutex> lock (sceneMutex);
    return cabinet.clear();
}

void VibeAmpProcessor::stepIR (int direction)
{
    std::lock_guard<std::recursive_mutex> lock (sceneMutex);
    const auto current = getIRPath();
    if (current.isEmpty() || ! juce::File::isAbsolutePath (current))
        return;

    const juce::File cur (current);
    auto files = cur.getParentDirectory().findChildFiles (juce::File::findFiles, false, "*.wav;*.aif;*.aiff");
    if (files.isEmpty())
        return;

    files.sort();
    int idx = files.indexOf (cur);
    if (idx < 0) idx = 0;
    idx = (idx + direction + files.size()) % files.size();
    loadIR (files[idx]);
}

juce::String VibeAmpProcessor::getIRPath() const
{
    return cabinet.getPath();
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
void VibeAmpProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    std::lock_guard<std::recursive_mutex> lock (sceneMutex);
    auto state = apvts.copyState();
    state.setProperty ("irPath", getIRPath(), nullptr);
    state.setProperty ("presetName", presets.getCurrentName(), nullptr);
    state.setProperty (kKnobCurvesAttr, kKnobCurvesVersion, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VibeAmpProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::lock_guard<std::recursive_mutex> lock (sceneMutex);
    if (data == nullptr || sizeInBytes <= 0 || sizeInBytes > 1024 * 1024) return;
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (! xml || ! validatePresetState (*xml, apvts, apvts.state.getType().toString())) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    // Legacy partial states receive defined defaults, not values left over from
    // the previous project. The existing root/tag/parameter IDs remain unchanged.
    for (auto* parameter : getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (parameter))
        {
            bool found = false;
            for (auto child : tree)
                found = found || child.getProperty ("id").toString() == p->paramID;
            if (! found)
            {
                juce::ValueTree child ("PARAM");
                child.setProperty ("id", p->paramID, nullptr);
                child.setProperty ("value", p->convertFrom0to1 (p->getDefaultValue()), nullptr);
                tree.addChild (child, -1, nullptr);
            }
        }
    const auto path = tree.getProperty ("irPath").toString();
    if (path.isNotEmpty())
    {
        if (! juce::File::isAbsolutePath (path) || ! loadIR (juce::File (path))) return;
    }
    else if (! clearIR())
        return;
    // Validation and IR preparation have succeeded. Rejects above mutate none
    // of the previously accepted parameters, preset name or cabinet selection.
    apvts.replaceState (tree);
    presets.setCurrentName (tree.getProperty ("presetName", juce::String()).toString());
}

juce::AudioProcessorEditor* VibeAmpProcessor::createEditor()
{
    return new VibeAmpEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VibeAmpProcessor();
}
