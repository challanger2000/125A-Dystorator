#pragma once
#include <numeric>

// These checks run through the actual JUCE AudioProcessor, not a routing mock.
static bool layoutFor (VibeAmpProcessor &p, int inputs, int outputs)
{
    auto layout = p.getBusesLayout ();
    layout.inputBuses.set (0,
                           inputs == 1 ? juce::AudioChannelSet::mono () : juce::AudioChannelSet::stereo ());
    layout.outputBuses.set (0,
                            outputs == 1 ? juce::AudioChannelSet::mono () : juce::AudioChannelSet::stereo ());
    return p.setBusesLayout (layout);
}

static void compactSettings (VibeAmpProcessor &p)
{
    param (p, ids::ampNoise, 0);
    param (p, ids::inGateOn, 0);
    param (p, ids::cabOn, 0);
    param (p, ids::lowCutOn, 0);
    param (p, ids::highCutOn, 0);
    param (p, ids::boostOn, 0);
    param (p, ids::channel, 2);
    param (p, ids::ampGain, 5);
    param (p, ids::bass, 5);
    param (p, ids::mid, 5);
    param (p, ids::treble, 6);
    param (p, ids::master, 3);
    param (p, ids::presence, 5);
    param (p, ids::resonance, 5);
    param (p, ids::ampVolume, 0);
    param (p, ids::inGain, 0);
    param (p, ids::outGain, 0);
}

static float fixtureSample (int frame, int channel, double fs)
{
    const double t = frame / fs;
    return (float)(.12 * std::sin (2 * vamp::kPi * (channel ? 277 : 110) * t + .3) +
                   .025 * std::sin (2 * vamp::kPi * (channel ? 1723 : 3731) * t));
}

static void compactBypass (const juce::File &dir)
{
    for (int inputs : {1, 2})
        for (int outputs : {1, 2})
        {
            VibeAmpProcessor p (dir), reference (dir);
            check (layoutFor (p, inputs, outputs) && layoutFor (reference, inputs, outputs),
                   "bypass bus setup");
            compactSettings (p);
            compactSettings (reference);
            p.prepareToPlay (48000, 64);
            reference.prepareToPlay (48000, 64);
            const int latency = p.getLatencySamples ();
            juce::AudioBuffer<float> a (2, 64), b (2, 64);
            juce::MidiBuffer midi;
            double dryError = 0, wetError = 0, energy = 0;
            for (int k = 0; k < 36; ++k)
            {
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 64; ++i)
                        a.setSample (c, i, fixtureSample (k * 64 + i, c < inputs ? c : 0, 48000));
                b.makeCopyOf (a, true);
                if (k >= 8 && k < 25)
                    p.processBlockBypassed (a, midi);
                else
                    p.processBlock (a, midi);
                reference.processBlock (b, midi);
                if (k >= 16 && k < 25)
                    for (int c = 0; c < outputs; ++c)
                        for (int i = 0; i < 64; ++i)
                        {
                            const int pos = k * 64 + i - latency;
                            const float l = fixtureSample (pos, 0, 48000);
                            const float r = fixtureSample (pos, inputs == 2 ? 1 : 0, 48000);
                            const float expected = outputs == 1 ? .5f * (l + r) : (c == 0 ? l : r);
                            dryError = std::max (dryError, (double)std::abs (expected - a.getSample (c, i)));
                        }
                if (k >= 33)
                    for (int c = 0; c < outputs; ++c)
                        for (int i = 0; i < 64; ++i)
                        {
                            wetError = std::max (wetError,
                                                 (double)std::abs (a.getSample (c, i) - b.getSample (c, i)));
                            energy += (double)b.getSample (c, i) * b.getSample (c, i);
                        }
            }
            std::cout << "BYPASS " << inputs << "->" << outputs << " latency=" << latency
                      << " dry_max=" << dryError << " warm_wet_max=" << wetError << " energy=" << energy
                      << '\n';
            check (dryError < 1e-6, "host bypass is dry with reported latency and intended bus mapping");
            check (wetError < 1e-6 && energy > 1e-9, "host bypass advances the complete wet path");
        }
}

static void compactCoreMatrix (const juce::File &dir)
{
    for (double fs : {44100., 48000., 96000.})
    {
        VibeAmpProcessor p (dir);
        layoutFor (p, 2, 2);
        compactSettings (p);
        p.prepareToPlay (fs, 512);
        vamp::AmpSim left, right;
        left.prepare (fs, 512);
        right.prepare (fs, 512);
        juce::dsp::Gain<float> inputGainReference, outputGainReference;
        for (auto *gain : {&inputGainReference, &outputGainReference})
        {
            gain->prepare ({fs, 512, 2});
            gain->setRampDurationSeconds (.05);
            gain->setGainDecibels (0);
        }
        vamp::AmpSim::Settings settings;
        settings.gain = .5f;
        settings.bass = .5f;
        settings.mid = .5f;
        settings.treble = .6f;
        settings.presence = settings.resonance = .5f;
        settings.master = .3f;
        juce::MidiBuffer midi;
        double error = 0, energy = 0;
        bool finite = true;
        int frame = 0;
        // Non-divisors, zero length, and a block larger than prepare's maximum
        // exercise the actual chunk boundaries and oversampling buffers.
        for (int n : {0, 1, 17, 32, 64, 128, 256, 512, 777, 64, 512})
        {
            if (n == 256)
            {
                settings.channel = 1;
                settings.gain = .8f;
                settings.boostOn = true;
                param (p, ids::channel, 1);
                param (p, ids::ampGain, 8);
                param (p, ids::boostOn, 1);
            }
            if (n == 777)
            {
                settings.channel = 0;
                settings.boostModel = 1;
                settings.tubeSet = 3;
                param (p, ids::channel, 0);
                param (p, ids::boostModel, 1);
                param (p, ids::tubeSet, 3);
            }
            juce::AudioBuffer<float> actual (2, n), expected (2, n);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                    actual.setSample (c, i, fixtureSample (frame + i, c, fs));
            expected.makeCopyOf (actual, true);
            p.processBlock (actual, midi);
            // Same chunking contract, frozen unmodified AmpSim arithmetic.
            for (int start = 0; start < n; start += 512)
            {
                const int count = std::min (512, n - start);
                auto referenceBlock =
                    juce::dsp::AudioBlock<float> (expected).getSubBlock ((size_t)start, (size_t)count);
                juce::dsp::ProcessContextReplacing<float> gainContext (referenceBlock);
                inputGainReference.process (gainContext);
                left.process (expected.getWritePointer (0, start), count, settings);
                right.process (expected.getWritePointer (1, start), count, settings);
                outputGainReference.process (gainContext);
            }
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    const float x = actual.getSample (c, i), y = expected.getSample (c, i);
                    finite = finite && std::isfinite (x);
                    error = std::max (error, (double)std::abs (x - y));
                    energy += (double)y * y;
                }
            frame += n;
        }
        std::cout << "CORE fs=" << fs << " max_delta=" << error << " energy=" << energy
                  << " latency=" << p.getLatencySamples () << '\n';
        check (finite && error < 1e-6 && energy > 1e-8,
               "both actual amp lanes match unchanged AmpSim including oversampling and block boundaries");
        check (p.getLatencySamples () == left.getLatencySamples (),
               "processor latency equals unchanged oversampling latency");
    }
}

static juce::File stereoWav (const juce::File &dir, int channels = 2)
{
    auto file = dir.getChildFile ("ir-" + juce::String (channels) + "ch.wav");
    juce::WavAudioFormat format;
    auto out = file.createOutputStream ();
    std::unique_ptr<juce::AudioFormatWriter> writer (
        format.createWriterFor (out.release (), 48000, channels, 32, {}, 0));
    if (!writer)
        throw std::runtime_error ("stereo fixture writer");
    juce::AudioBuffer<float> ir (channels, 257);
    ir.clear ();
    for (int c = 0; c < channels; ++c)
    {
        ir.setSample (c, 0, c == 1 ? -.6f : .8f);
        ir.setSample (c, 51 + c * 43, .2f);
    }
    if (!writer->writeFromAudioSampleBuffer (ir, 0, 257))
        throw std::runtime_error ("stereo fixture write");
    return file;
}

static void compactCabinetMatrix (const juce::File &dir)
{
    for (int irChannels : {1, 2})
        for (double fs : {44100., 48000., 96000.})
        {
            auto file = stereoWav (dir, irChannels);
            CabinetIR tested;
            check (tested.load (file), "mono/stereo IR accepted before activation");
            tested.prepare ({fs, 512, 2});
            juce::dsp::Convolution reference;
            reference.loadImpulseResponse (file, juce::dsp::Convolution::Stereo::yes,
                                           juce::dsp::Convolution::Trim::yes, 0,
                                           juce::dsp::Convolution::Normalise::yes);
            reference.prepare ({fs, 512, 2});
            double error = 0, energy = 0;
            int pos = 0;
            for (int n : {1, 17, 32, 64, 128, 256, 512, 64, 512})
            {
                juce::AudioBuffer<float> a (2, n), b (2, n);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < n; ++i)
                        a.setSample (c, i, fixtureSample (pos + i, c, fs));
                b.makeCopyOf (a, true);
                tested.processStereo (juce::dsp::AudioBlock<float> (a), [] (float *, int, int) noexcept {});
                auto refBlock = juce::dsp::AudioBlock<float> (b);
                juce::dsp::ProcessContextReplacing<float> ctx{refBlock};
                reference.process (ctx);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < n; ++i)
                    {
                        error = std::max (error, (double)std::abs (a.getSample (c, i) - b.getSample (c, i)));
                        energy += (double)b.getSample (c, i) * b.getSample (c, i);
                    }
                pos += n;
            }
            std::cout << "IR channels=" << irChannels << " fs=" << fs << " max_delta=" << error
                      << " energy=" << energy << '\n';
            check (error < 1e-6 && energy > 1e-8,
                   "both IR channels/resampling match original JUCE file oracle at variable blocks");
            const auto accepted = tested.getPath ();
            const auto invalid = stereoWav (dir, 3);
            check (!tested.load (invalid) && tested.getPath () == accepted,
                   "unsupported multichannel IR rejects without replacing stereo scene");
        }
}

static void compactStateAndAutomation (const juce::File &dir)
{
    VibeAmpProcessor a (dir), b (dir);
    layoutFor (a, 2, 2);
    layoutFor (b, 2, 2);
    compactSettings (a);
    const auto ir = stereoWav (dir);
    a.loadIR (ir);
    param (a, ids::cabOn, 1);
    param (a, ids::lowCutOn, 1);
    param (a, ids::highCutOn, 1);
    param (a, ids::lowCutHz, 150);
    param (a, ids::highCutHz, 6000);
    a.prepareToPlay (48000, 128);
    check (a.presets.save ("compact-stereo"), "stereo/cuts scene saved");
    const auto state = hostState (a);
    b.setStateInformation (state.getData (), (int)state.getSize ());
    b.prepareToPlay (48000, 128);
    juce::AudioBuffer<float> x (2, 128), y (2, 128);
    juce::MidiBuffer midi;
    double error = 0, energy = 0;
    bool finite = true;
    unsigned alloc = 0, freed = 0;
    const std::vector<const char *> inert = {
        ids::compOn,      ids::compThresh, ids::compRatio,     ids::compAttack, ids::compRelease,
        ids::compMakeup,  ids::transpose,  ids::transposeMode, ids::gateOn,     ids::gateThresh,
        ids::gateRelease, ids::gateLow,    ids::gateMid,       ids::gateHigh,   ids::eqOn,
        ids::phaserOn,    ids::phRate,     ids::phDepth,       ids::phFeedback, ids::phMix,
        ids::delayOn,     ids::dlTime,     ids::dlFeedback,    ids::dlMix,      ids::dlPingPong,
        ids::reverbOn,    ids::rvSize,     ids::rvDamping,     ids::rvWidth,    ids::rvMix};
    for (int k = 0; k < 100; ++k)
    {
        // Only A receives all retired-FX automation. Both receive identical
        // live cut/channel/drive automation, including reversals mid-smoothing.
        for (auto *id : inert)
            a.apvts.getParameter (id)->setValueNotifyingHost ((k % 2) ? 1.f : 0.f);
        for (int i = 0; i < 10; ++i)
            a.apvts.getParameter (ids::eqBand (i))->setValueNotifyingHost ((k % 2) ? 1.f : 0.f);
        for (auto *p : {&a, &b})
        {
            param (*p, ids::lowCutHz, 20 + (k * 37) % 281);
            param (*p, ids::highCutHz, 3000 + (k * 347) % 17001);
            param (*p, ids::lowCutOn, k % 3 != 0);
            param (*p, ids::highCutOn, k % 5 != 0);
            if (k % 20 == 0)
            {
                param (*p, ids::channel, (k / 20) % 3);
                param (*p, ids::ampGain, 2 + (k / 20) % 7);
            }
        }
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 128; ++i)
                x.setSample (c, i, fixtureSample (k * 128 + i, c, 48000));
        y.makeCopyOf (x, true);
        heapProbe::allocations = heapProbe::deletions = 0;
        heapProbe::enabled = true;
        a.processBlock (x, midi);
        heapProbe::enabled = false;
        alloc += heapProbe::allocations;
        freed += heapProbe::deletions;
        b.processBlock (y, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 128; ++i)
            {
                finite = finite && std::isfinite (x.getSample (c, i));
                error = std::max (error, (double)std::abs (x.getSample (c, i) - y.getSample (c, i)));
                energy += (double)y.getSample (c, i) * y.getSample (c, i);
            }
    }
    std::cout << "STATE/FX automation max_delta=" << error << " energy=" << energy << " new=" << alloc
              << " delete=" << freed << '\n';
    check (finite && error < 1e-6 && energy > 1e-8,
           "stereo IR/state and cuts automation identical despite every retired FX parameter changing");
    check (alloc == 0 && freed == 0, "full processor automation callback does no observed C++ new/delete");
    double cabinetWarmError = 0;
    for (int k = 100; k < 150; ++k)
    {
        if (k == 100)
            param (a, ids::cabOn, 0);
        if (k == 135)
            param (a, ids::cabOn, 1);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 128; ++i)
                x.setSample (c, i, fixtureSample (k * 128 + i, c, 48000));
        y.makeCopyOf (x, true);
        a.processBlock (x, midi);
        b.processBlock (y, midi);
        if (k >= 140)
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 128; ++i)
                    cabinetWarmError = std::max (cabinetWarmError,
                                                 (double)std::abs (x.getSample (c, i) - y.getSample (c, i)));
    }
    std::cout << "CAB bypass warm_max=" << cabinetWarmError << '\n';
    check (cabinetWarmError < 1e-6, "CAB off/on keeps stereo convolution history warm through reactivation");
    b.presets.refresh ();
    check (b.presets.load (b.presets.indexOf ("compact-stereo")), "compact stereo preset reload succeeds");
    check (b.getIRPath () == ir.getFullPathName () && *b.apvts.getRawParameterValue (ids::lowCutOn) == 1.f &&
               std::abs (*b.apvts.getRawParameterValue (ids::highCutHz) - 6000.f) < .01f,
           "compact preset restores IR and all new cut controls");
    // Old states must reset new controls to OFF, rather than retaining an edit.
    auto old = juce::AudioProcessor::getXmlFromBinary (state.getData (), (int)state.getSize ());
    for (auto *child = old->getFirstChildElement (); child != nullptr;)
    {
        auto *next = child->getNextElement ();
        if (child->getStringAttribute ("id").startsWith ("lowCut") ||
            child->getStringAttribute ("id").startsWith ("highCut"))
            old->removeChildElement (child, true);
        child = next;
    }
    juce::MemoryBlock legacy;
    juce::AudioProcessor::copyXmlToBinary (*old, legacy);
    b.setStateInformation (legacy.getData (), (int)legacy.getSize ());
    check (*b.apvts.getRawParameterValue (ids::lowCutOn) == 0.f &&
               *b.apvts.getRawParameterValue (ids::highCutOn) == 0.f,
           "legacy host state supplies neutral defaults for new cuts");
}

static void compactRoutingAndLifecycle (const juce::File &dir)
{
    VibeAmpProcessor stereo (dir), monoOut (dir), monoIn (dir);
    layoutFor (stereo, 2, 2);
    layoutFor (monoOut, 2, 1);
    layoutFor (monoIn, 1, 2);
    for (auto *p : {&stereo, &monoOut, &monoIn})
        compactSettings (*p);
    // Repeated setup and differing last-block sizes exercise the live buffers,
    // gains, independent circuit histories, and cabinet reconfiguration.
    for (double fs : {48000., 44100., 96000.})
    {
        for (auto *p : {&stereo, &monoOut, &monoIn})
        {
            p->releaseResources ();
            p->prepareToPlay (fs, 128);
        }
        juce::AudioBuffer<float> a (2, 257), b (2, 257), c (2, 257);
        juce::MidiBuffer midi;
        double downmixError = 0, monoSplitError = 0, energy = 0;
        bool finite = true;
        for (int k = 0; k < 12; ++k)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 257; ++i)
                    a.setSample (ch, i, fixtureSample (k * 257 + i, ch, fs));
            b.makeCopyOf (a, true);
            c.makeCopyOf (a, true);
            if (k == 3)
            {
                a.setSample (1, 7, std::numeric_limits<float>::quiet_NaN ());
                b.setSample (1, 7, std::numeric_limits<float>::quiet_NaN ());
            }
            if (k == 4)
            {
                a.setSample (0, 5, std::numeric_limits<float>::infinity ());
                b.setSample (0, 5, std::numeric_limits<float>::infinity ());
                c.setSample (0, 5, std::numeric_limits<float>::infinity ());
            }
            if (k == 5)
            {
                a.setSample (0, 9, 1e-39f);
                b.setSample (0, 9, 1e-39f);
                c.setSample (0, 9, 1e-39f);
            }
            stereo.processBlock (a, midi);
            monoOut.processBlock (b, midi);
            monoIn.processBlock (c, midi);
            for (int i = 0; i < 257; ++i)
            {
                downmixError = std::max (
                    downmixError,
                    (double)std::abs (b.getSample (0, i) - .5f * (a.getSample (0, i) + a.getSample (1, i))));
                monoSplitError =
                    std::max (monoSplitError, (double)std::abs (c.getSample (0, i) - c.getSample (1, i)));
                energy += (double)a.getSample (0, i) * a.getSample (0, i);
                finite = finite && std::isfinite (a.getSample (0, i)) && std::isfinite (a.getSample (1, i)) &&
                         std::isfinite (b.getSample (0, i)) && std::isfinite (c.getSample (0, i));
            }
        }
        std::cout << "ROUTING fs=" << fs << " downmix_max=" << downmixError
                  << " mono_split_max=" << monoSplitError << " energy=" << energy << '\n';
        check (finite && energy > 1e-8 && downmixError < 1e-6 && monoSplitError < 1e-6,
               "wet stereo-to-mono average and mono-to-stereo split survive reprepare, oversized blocks and "
               "hostile samples");
    }
}

static void compactStereoHandoff (const juce::File &dir)
{
    const auto stereo = stereoWav (dir, 2), mono = stereoWav (dir, 1);
    CabinetIR cabinet;
    cabinet.load (stereo);
    cabinet.prepare ({48000, 128, 2});
    juce::AudioBuffer<float> audio (2, 128);
    std::atomic<bool> go{false}, ok{true};
    std::thread producer (
        [&]
        {
            while (!go.load ())
                std::this_thread::yield ();
            for (int k = 0; k < 40; ++k)
            {
                ok = ok.load () && (k % 3 == 0 ? cabinet.clear () : cabinet.load (k % 2 ? mono : stereo));
                juce::Thread::sleep (3);
            }
            ok = ok.load () && cabinet.load (stereo);
        });
    go = true;
    bool finite = true;
    double energy = 0;
    unsigned allocations = 0, deletions = 0;
    for (int k = 0; k < 240; ++k)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 128; ++i)
                audio.setSample (c, i, fixtureSample (k * 128 + i, c, 48000));
        heapProbe::allocations = heapProbe::deletions = 0;
        heapProbe::enabled = true;
        cabinet.processStereo (juce::dsp::AudioBlock<float> (audio), [] (float *, int, int) noexcept {});
        heapProbe::enabled = false;
        allocations += heapProbe::allocations;
        deletions += heapProbe::deletions;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 128; ++i)
            {
                finite = finite && std::isfinite (audio.getSample (c, i));
                energy += (double)audio.getSample (c, i) * audio.getSample (c, i);
            }
        juce::Thread::sleep (1);
    }
    producer.join ();
    // Final scene must eventually be audible even with no more producer calls.
    for (int k = 0; k < 180; ++k)
    {
        audio.clear ();
        cabinet.processStereo (juce::dsp::AudioBlock<float> (audio), [] (float *, int, int) noexcept {});
        juce::Thread::sleep (1);
    }
    audio.clear ();
    audio.setSample (0, 0, .2f);
    audio.setSample (1, 0, .2f);
    cabinet.processStereo (juce::dsp::AudioBlock<float> (audio), [] (float *, int, int) noexcept {});
    check (ok && finite && energy > 1e-8 && audio.getSample (0, 0) > .01f && audio.getSample (1, 0) < -.01f,
           "rapid concurrent mono/stereo IR and clear settles to last coherent stereo scene without a later "
           "control call");
    check (allocations == 0 && deletions == 0,
           "stereo IR crossfade/handoff has no observed audio C++ new/delete");
}

static void compactMalformedStereo (const juce::File &dir)
{
    CabinetIR cabinet;
    const auto valid = stereoWav (dir, 2);
    cabinet.load (valid);
    cabinet.prepare ({48000, 64, 2});
    juce::MemoryBlock bytes;
    valid.loadFileAsData (bytes);
    bytes.setSize (bytes.getSize () - 100);
    auto *raw = static_cast<unsigned char *> (bytes.getData ());
    const auto size = bytes.getSize () - 8;
    for (int i = 0; i < 4; ++i)
        raw[4 + i] = (unsigned char)(size >> (8 * i));
    const auto damaged = dir.getChildFile ("inner-truncated-stereo.wav");
    damaged.replaceWithData (bytes.getData (), bytes.getSize ());
    check (!cabinet.load (damaged) && cabinet.getPath () == valid.getFullPathName (),
           "stereo WAV inner data beyond EOF rejects even when RIFF size matches snapshot");
    check (cabinet.load (valid), "restore valid stereo fixture before independent NaN test");
    juce::MemoryOutputStream nan;
    nan.write ("RIFF", 4);
    nan.writeInt (44);
    nan.write ("WAVEfmt ", 8);
    nan.writeInt (16);
    nan.writeShort (3);
    nan.writeShort (2);
    nan.writeInt (48000);
    nan.writeInt (384000);
    nan.writeShort (8);
    nan.writeShort (32);
    nan.write ("data", 4);
    nan.writeInt (8);
    nan.writeFloat (.5f);
    nan.writeInt (0x7fc00000);
    const auto nonfinite = dir.getChildFile ("right-nan.wav");
    nonfinite.replaceWithData (nan.getData (), nan.getDataSize ());
    check (!cabinet.load (nonfinite) && cabinet.getPath () == valid.getFullPathName (),
           "NaN confined to right IR channel rejects whole stereo scene");
    const auto aiff = dir.getChildFile ("stereo.aiff");
    juce::AiffAudioFormat format;
    auto stream = aiff.createOutputStream ();
    std::unique_ptr<juce::AudioFormatWriter> writer (
        format.createWriterFor (stream.release (), 48000, 2, 24, {}, 0));
    if (!writer)
        throw std::runtime_error ("AIFF fixture writer");
    juce::AudioBuffer<float> response (2, 64);
    response.clear ();
    response.setSample (0, 0, .8f);
    response.setSample (1, 0, -.4f);
    writer->writeFromAudioSampleBuffer (response, 0, 64);
    writer.reset ();
    check (cabinet.load (aiff), "valid stereo AIFF passes bounded container and decoder validation");
    cabinet.prepare ({48000, 64, 2});
    juce::AudioBuffer<float> audio (2, 64);
    audio.clear ();
    audio.setSample (0, 0, .2f);
    audio.setSample (1, 0, .2f);
    cabinet.processStereo (juce::dsp::AudioBlock<float> (audio), [] (float *, int, int) noexcept {});
    check (audio.getSample (0, 0) > .01f && audio.getSample (1, 0) < -.005f,
           "actual stereo AIFF convolution preserves both channel responses");
    aiff.loadFileAsData (bytes);
    bytes.setSize (bytes.getSize () - 10);
    raw = static_cast<unsigned char *> (bytes.getData ());
    const auto formSize = bytes.getSize () - 8;
    for (int i = 0; i < 4; ++i)
        raw[4 + i] = (unsigned char)(formSize >> (8 * (3 - i)));
    const auto badAiff = dir.getChildFile ("inner-truncated.aiff");
    badAiff.replaceWithData (bytes.getData (), bytes.getSize ());
    check (!cabinet.load (badAiff) && cabinet.getPath () == aiff.getFullPathName (),
           "AIFF inner sound data beyond snapshot rejects without replacing valid stereo asset");
}

static void compactRegressions (const juce::File &dir)
{
    compactCoreMatrix (dir);
    compactCabinetMatrix (dir);
    compactStateAndAutomation (dir);
    compactMalformedStereo (dir);
    compactRoutingAndLifecycle (dir);
    compactStereoHandoff (dir);
    compactBypass (dir);
}

static void compactCPU (const juce::File &dir)
{
    std::vector<double> timer (10000);
    for (auto &t : timer)
    {
        auto a = std::chrono::steady_clock::now ();
        auto b = std::chrono::steady_clock::now ();
        t = std::chrono::duration<double, std::micro> (b - a).count ();
    }
    std::sort (timer.begin (), timer.end ());
    std::cout << "TIMER p99_us=" << timer[9899] << '\n';
    for (double fs : {44100., 48000.})
        for (int inputs : {1, 2})
        {
            VibeAmpProcessor p (dir);
            layoutFor (p, inputs, 2);
            compactSettings (p);
            // Full unchanged 8x amp + built-in cabinet + active cuts.
            param (p, ids::cabOn, 1);
            param (p, ids::lowCutOn, 1);
            param (p, ids::highCutOn, 1);
            p.prepareToPlay (fs, 512);
            for (int frames : {32, 64, 128, 256, 512})
                for (bool paced : {false, true})
                {
                    juce::AudioBuffer<float> audio (2, frames);
                    juce::MidiBuffer midi;
                    std::vector<double> times;
                    times.reserve (160);
                    int overruns = 0, late = 0;
                    double energy = 0;
                    const double deadline = 1e6 * frames / fs;
                    auto next = std::chrono::steady_clock::now ();
                    for (int k = 0; k < 184; ++k)
                    {
                        for (int c = 0; c < 2; ++c)
                            for (int i = 0; i < frames; ++i)
                                audio.setSample (c, i,
                                                 fixtureSample (k * frames + i, c < inputs ? c : 0, fs));
                        if (paced && k >= 24)
                            std::this_thread::sleep_until (next);
                        const auto begin = std::chrono::steady_clock::now ();
                        p.processBlock (audio, midi);
                        const auto end = std::chrono::steady_clock::now ();
                        if (k >= 24)
                        {
                            const double t = std::chrono::duration<double, std::micro> (end - begin).count ();
                            times.push_back (t);
                            overruns += t > deadline;
                            if (paced)
                                late += end > next + std::chrono::duration<double> (frames / fs);
                            for (int c = 0; c < 2; ++c)
                                for (int i = 0; i < frames; ++i)
                                    energy += (double)audio.getSample (c, i) * audio.getSample (c, i);
                        }
                        if (k < 24)
                            next = end;
                        else
                            next += std::chrono::duration_cast<std::chrono::steady_clock::duration> (
                                std::chrono::duration<double> (frames / fs));
                    }
                    const auto mean = std::accumulate (times.begin (), times.end (), 0.0) / times.size ();
                    std::sort (times.begin (), times.end ());
                    std::cout << "CPU fs=" << fs << " lanes=" << inputs << " frames=" << frames
                              << " mode=" << (paced ? "paced" : "offline") << " mean_us=" << mean
                              << " p95_us=" << times[151] << " p99_us=" << times[158]
                              << " max_us=" << times.back () << " deadline_us=" << deadline
                              << " utilization_percent=" << 100 * mean / deadline << " overruns=" << overruns
                              << " schedule_misses=" << late << " energy=" << energy
                              << " status=" << (overruns == 0 && (!paced || late == 0) ? "PASS" : "FAIL")
                              << std::endl;
                    check (energy > 1e-8 && std::isfinite (energy),
                           "CPU workload has actual finite non-silent amp output");
                    check (overruns == 0 && (!paced || late == 0),
                           "CPU block work and paced completion meet the actual audio deadline");
                }
        }
}
