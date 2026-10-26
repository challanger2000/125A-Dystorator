#pragma once
// Independent JUCE impulse-response oracle: file decoding + documented
// K-weighted gain, with source IR channels and original sample rate preserved.
// Used by native integration tests; NOT part of audio callback or release code.
static void loadMatchedReference (juce::dsp::Convolution& target, const juce::File& file,
                                  int hostChannels)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (! reader || reader->lengthInSamples < 1 || reader->lengthInSamples > CabinetIR::maxSamples
        || reader->numChannels < 1 || reader->numChannels > 2)
        throw std::runtime_error ("IR reference cannot decode source fixture");
    juce::AudioBuffer<float> copy ((int) reader->numChannels, (int) reader->lengthInSamples);
    if (! reader->read (copy.getArrayOfWritePointers(), copy.getNumChannels(), 0, copy.getNumSamples()))
        throw std::runtime_error ("IR reference read failure");
    const bool stereo = copy.getNumChannels() == 2 && hostChannels == 2;
    const double gain = stereo
        ? vamp::cabMatchGainStereo (copy.getReadPointer (0), copy.getReadPointer (1),
                                    copy.getNumSamples(), reader->sampleRate)
        : vamp::cabMatchGain (copy.getReadPointer (0), copy.getNumSamples(), reader->sampleRate);
    if (! std::isfinite (gain) || gain <= 0.0)
        throw std::runtime_error ("IR reference has invalid gain");
    copy.applyGain ((float) gain);
    target.loadImpulseResponse (std::move (copy), reader->sampleRate,
                                stereo ? juce::dsp::Convolution::Stereo::yes
                                       : juce::dsp::Convolution::Stereo::no,
                                juce::dsp::Convolution::Trim::yes, juce::dsp::Convolution::Normalise::no);
}

// These properties distinguish the level-matching rule from a disabled match
// and from destructive mono summing of an anti-phase stereo IR.
static bool cabMatchGainInvariants()
{
    float full[256] {}, quarter[256] {}, inverse[256] {}, zero[256] {};
    full[0] = 1.0f; full[19] = 0.4f;
    for (int i = 0; i < 256; ++i)
    {
        quarter[i] = full[i] * 0.25f;
        inverse[i] = -full[i];
    }
    const double g = vamp::cabMatchGain (full, 256, 48000.0);
    const double q = vamp::cabMatchGain (quarter, 256, 48000.0);
    const double anti = vamp::cabMatchGainStereo (full, inverse, 256, 48000.0);
    return std::isfinite (g) && g > 0 && std::isfinite (q)
        && std::abs (q / g - 4.0) < 1e-5
        && std::isfinite (anti) && std::abs (anti / g - 1.0) < 1e-5
        && vamp::cabMatchGain (zero, 256, 48000.0) == 1.0;
}
