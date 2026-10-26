#include "HeapProbe.h"
#include "Parameters.h"
#include "PluginProcessor.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <juce_audio_formats/juce_audio_formats.h>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
static std::mutex traceMutex;
static std::set<const void *> renderedConvolvers;
static int ownershipViolations = 0;
void irTestConvolutionCall (const void *object, int event)
{
    const bool probeWasEnabled = heapProbe::enabled;
    heapProbe::enabled = false;
    std::lock_guard<std::mutex> lock (traceMutex);
    if (event == 0)
        renderedConvolvers.insert (object);
    if (event == 1 && renderedConvolvers.count (object))
        ++ownershipViolations;
    if (event == 2)
        renderedConvolvers.erase (object);
    heapProbe::enabled = probeWasEnabled;
}

static thread_local float *cabinetInputCapture = nullptr;
void irTestCaptureCabinetInput (const float *data, int count)
{
    if (cabinetInputCapture != nullptr)
        std::copy_n (data, count, cabinetInputCapture);
}

static int checks = 0, failures = 0;
static void check (bool ok, const char *text)
{
    ++checks;
    if (!ok)
        ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << text << std::endl;
}
static void param (VibeAmpProcessor &p, const char *id, float v)
{
    auto *x = p.apvts.getParameter (id);
    x->setValueNotifyingHost (x->convertTo0to1 (v));
}
static juce::File wav (const juce::File &folder, const char *name, bool inverted = false,
                       int sampleCount = 256)
{
    auto f = folder.getChildFile (name);
    juce::WavAudioFormat format;
    auto out = f.createOutputStream ();
    auto writer = std::unique_ptr<juce::AudioFormatWriter> (
        format.createWriterFor (out.release (), 48000, 1, 32, {}, 0));
    if (!writer)
        throw std::runtime_error ("fixture writer");
    juce::AudioBuffer<float> data (1, sampleCount);
    data.clear ();
    data.setSample (0, 0, inverted ? -1.f : 1.f);
    data.setSample (0, 24, inverted ? -.5f : .5f);
    if (!writer->writeFromAudioSampleBuffer (data, 0, sampleCount))
        throw std::runtime_error ("fixture write");
    return f;
}
static void block (VibeAmpProcessor &p)
{
    juce::AudioBuffer<float> b (2, 64);
    b.clear ();
    for (int i = 0; i < 64; ++i)
        b.setSample (0, i, .05f * std::sin (i * .15f));
    juce::MidiBuffer m;
    p.processBlock (b, m);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 64; ++i)
            if (!std::isfinite (b.getSample (c, i)))
                throw std::runtime_error ("nonfinite audio");
}

static juce::MemoryBlock hostState (VibeAmpProcessor &p)
{
    juce::MemoryBlock state;
    p.getStateInformation (state);
    return state;
}
static void malformedScenes (VibeAmpProcessor &p, const juce::File &dir, const juce::File &a)
{
    p.loadIR (a);
    param (p, ids::ampGain, 2);
    p.presets.setCurrentName ("keep-me");
    auto original = hostState (p);
    const std::vector<std::pair<const char *, const char *>> invalid = {
        {"foreign", "<unrelated/>"},
        {"empty", "<DjentyDonkeyPreset/>"},
        {"truncated", "<DjentyDonkeyPreset><PARAM"},
        {"future",
         "<DjentyDonkeyPreset knobCurves='99'><PARAM id='ampGain' value='3'/></DjentyDonkeyPreset>"},
        {"version",
         "<DjentyDonkeyPreset version='1.0.0'><PARAM id='ampGain' value='3'/></DjentyDonkeyPreset>"},
        {"nan", "<DjentyDonkeyPreset><PARAM id='ampGain' value='nan'/></DjentyDonkeyPreset>"},
        {"range", "<DjentyDonkeyPreset><PARAM id='ampGain' value='300'/></DjentyDonkeyPreset>"},
        {"junk", "<DjentyDonkeyPreset><PARAM id='ampGain' value='2oops'/></DjentyDonkeyPreset>"},
        {"unknown", "<DjentyDonkeyPreset><PARAM id='unknown' value='2'/></DjentyDonkeyPreset>"},
        {"duplicate", "<DjentyDonkeyPreset><PARAM id='ampGain' value='2'/><PARAM id='ampGain' "
                      "value='3'/></DjentyDonkeyPreset>"},
        {"fractional", "<DjentyDonkeyPreset><PARAM id='channel' value='0.5'/></DjentyDonkeyPreset>"},
        {"relative",
         "<DjentyDonkeyPreset irPath='relative.wav'><PARAM id='ampGain' value='3'/></DjentyDonkeyPreset>"}};
    for (const auto &item : invalid)
    {
        auto file = dir.getChildFile (juce::String ("reject-") + item.first + ".xml");
        file.replaceWithText (item.second);
        p.presets.refresh ();
        check (!p.presets.load (p.presets.indexOf (file.getFileNameWithoutExtension ())), item.first);
        check (hostState (p) == original, "rejected preset preserves whole serialized scene");
    }
    // Valid XML referring to a missing/invalid asset must not partially apply gain.
    for (const char *path : {"missing.wav", "bad.wav"})
    {
        auto xml = juce::AudioProcessor::getXmlFromBinary (original.getData (), (int)original.getSize ());
        xml->setTagName ("DjentyDonkeyPreset");
        xml->setAttribute ("irPath", dir.getChildFile (path).getFullPathName ());
        for (auto *child : xml->getChildWithTagNameIterator ("PARAM"))
            if (child->getStringAttribute ("id") == ids::ampGain)
                child->setAttribute ("value", 9);
        const auto f = dir.getChildFile ("reject-asset.xml");
        xml->writeTo (f);
        p.presets.refresh ();
        check (!p.presets.load (p.presets.indexOf ("reject-asset")), "unusable preset IR rejects");
        check (hostState (p) == original, "unusable preset IR preserves all parameters/name/path");
        xml->setTagName ("PARAMS");
        juce::MemoryBlock damaged;
        juce::AudioProcessor::copyXmlToBinary (*xml, damaged);
        p.setStateInformation (damaged.getData (), (int)damaged.getSize ());
        check (hostState (p) == original, "unusable host IR preserves previous scene");
    }
    auto missing = dir.getChildFile ("deleted-preset.xml");
    missing.replaceWithText (invalid[0].second);
    p.presets.refresh ();
    auto index = p.presets.indexOf ("deleted-preset");
    missing.deleteFile ();
    check (!p.presets.load (index) && hostState (p) == original,
           "preset disappears after directory scan: rollback");
    p.setStateInformation (original.getData (), (int)original.getSize () / 2);
    check (hostState (p) == original, "truncated host binary rollback");
    p.setStateInformation (nullptr, 0);
    check (hostState (p) == original, "null host state rollback");
    const auto blocked = dir.getChildFile ("blocked.xml");
    blocked.createDirectory ();
    const auto sentinel = blocked.getChildFile ("keep.txt");
    sentinel.replaceWithText ("untouched");
    check (!p.presets.save ("blocked"), "failed save reports failure");
    check (hostState (p) == original && sentinel.loadFileAsString () == "untouched",
           "failed save preserves scene and existing destination");
    // Existing partial legacy presets are intentionally supported, with defaults.
    auto legacy = dir.getChildFile ("legacy.xml");
    legacy.replaceWithText (
        "<DjentyDonkeyPreset knobCurves='2'><PARAM id='boostAttack' value='8'/></DjentyDonkeyPreset>");
    p.presets.refresh ();
    check (p.presets.load (p.presets.indexOf ("legacy")), "legacy partial preset accepted");
    check (*p.apvts.getRawParameterValue (ids::boostAttack) == 1.f && p.getIRPath ().isEmpty (),
           "legacy Attack migration and built-in recall");
}

static void cabinetAudio (const juce::File &dir, const juce::File &a, const juce::File &b)
{
    CabinetIR cabinet;
    check (cabinet.load (a), "IR accepted before prepare");
    cabinet.prepare ({48000, 64, 1});
    juce::dsp::Convolution reference;
    reference.loadImpulseResponse (a, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::yes,
                                   0, juce::dsp::Convolution::Normalise::yes);
    reference.prepare ({48000, 64, 1});
    juce::AudioBuffer<float> x (1, 64), y (1, 64);
    auto builtin = [] (float *, int) noexcept {}; // identity stand-in, not an amp mock
    double error = 0;
    for (int blockIndex = 0; blockIndex < 12; ++blockIndex)
    {
        x.clear ();
        if (blockIndex == 0)
            x.setSample (0, 0, 1);
        y.makeCopyOf (x, true);
        cabinet.process (juce::dsp::AudioBlock<float> (x), builtin);
        juce::dsp::AudioBlock<float> refBlock (y);
        juce::dsp::ProcessContextReplacing<float> ctx (refBlock);
        reference.process (ctx);
        for (int i = 0; i < 64; ++i)
            error = std::max (error, (double)std::abs (x.getSample (0, i) - y.getSample (0, i)));
    }
    check (error < 1e-6, "decoded prepared IR matches original JUCE file-load audio");
    // Leave a real echo pending across the block boundary before any reject.
    // A reset-to-empty-history bug must fail even if it retains the same IR path.
    x.clear ();
    x.setSample (0, 63, 1);
    y.makeCopyOf (x, true);
    cabinet.process (juce::dsp::AudioBlock<float> (x), builtin);
    {
        juce::dsp::AudioBlock<float> seedBlock (y);
        juce::dsp::ProcessContextReplacing<float> seedContext (seedBlock);
        reference.process (seedContext);
    }
    for (const char *file : {"bad.wav", "missing.wav"})
        check (!cabinet.load (dir.getChildFile (file)), "IR error is returned to caller");
    const auto longIR = wav (dir, "over-budget.wav", false, 48000 * 6);
    CabinetIR beforeActivation;
    beforeActivation.load (a);
    check (!beforeActivation.load (longIR) && beforeActivation.getPath () == a.getFullPathName (),
           "IR resampling budget rejected before activation, previous asset retained");
    beforeActivation.prepare ({192000, 64, 1});
    // A valid header with missing payload is often zero-padded by decoders.
    auto truncated = dir.getChildFile ("truncated.wav");
    juce::MemoryBlock data;
    a.loadFileAsData (data);
    truncated.replaceWithData (data.getData (), data.getSize () / 2);
    check (!cabinet.load (truncated), "truncated WAV payload rejected");
    juce::MemoryOutputStream nanWav;
    nanWav.write ("RIFF", 4);
    nanWav.writeInt (40);
    nanWav.write ("WAVEfmt ", 8);
    nanWav.writeInt (16);
    nanWav.writeShort (3);
    nanWav.writeShort (1);
    nanWav.writeInt (48000);
    nanWav.writeInt (192000);
    nanWav.writeShort (4);
    nanWav.writeShort (32);
    nanWav.write ("data", 4);
    nanWav.writeInt (4);
    nanWav.writeInt (0x7fc00000);
    auto nonfinite = dir.getChildFile ("nan.wav");
    nonfinite.replaceWithData (nanWav.getData (), nanWav.getDataSize ());
    check (!cabinet.load (nonfinite), "IEEE-float NaN IR rejected");
    check (cabinet.getPath () == a.getFullPathName (), "all IR rejects leave old cabinet selected");
    // Failure leaves the existing convolution STATE untouched, not just the path.
    x.clear ();
    y.makeCopyOf (x, true);
    cabinet.process (juce::dsp::AudioBlock<float> (x), builtin);
    juce::dsp::AudioBlock<float> refBlock (y);
    juce::dsp::ProcessContextReplacing<float> ctx (refBlock);
    reference.process (ctx);
    error = 0;
    for (int i = 0; i < 64; ++i)
        error = std::max (error, (double)std::abs (x.getSample (0, i) - y.getSample (0, i)));
    std::cout << "IR rollback tail: max_error=" << error << " reference_peak=" << y.getMagnitude (0, 64)
              << std::endl;
    check (error < 1e-6 && y.getMagnitude (0, 64) > 1e-4,
           "failed loads preserve a non-silent pending convolution tail");
    // Queue more than one change during a fade. The last accepted request wins,
    // even after the producer stops sending updates (reclamation must progress).
    cabinet.load (b);
    x.clear ();
    cabinet.process (juce::dsp::AudioBlock<float> (x), builtin);
    cabinet.load (a);
    cabinet.clear ();
    for (int k = 0; k < 200; ++k)
    {
        x.clear ();
        cabinet.process (juce::dsp::AudioBlock<float> (x), builtin);
        juce::Thread::sleep (1);
    }
    x.clear ();
    x.setSample (0, 0, 1);
    cabinet.process (juce::dsp::AudioBlock<float> (x), builtin);
    check (std::abs (x.getSample (0, 0) - 1.f) < 1e-6 && x.getMagnitude (0, 1, 63) < 1e-6,
           "rapid updates settle to last built-in request on audio");
    auto cached = wav (dir, "cached.wav");
    cabinet.load (cached);
    cached.deleteFile ();
    cabinet.prepare ({96000, 17, 1}); // retained decoded snapshot, no deferred file reopening
    check (cabinet.getPath () == cached.getFullPathName (),
           "sample-rate reprepare retains accepted decoded asset");
    juce::AudioBuffer<float> odd (1, 17);
    odd.clear ();
    odd.setSample (0, 0, 1);
    cabinet.process (juce::dsp::AudioBlock<float> (odd), builtin);
    check (odd.getMagnitude (0, 17) > 0, "reprepared IR actually produces audio after source deletion");
}

static void realtimeHandoff (const juce::File &a, const juce::File &b)
{
    CabinetIR cabinet;
    cabinet.load (a);
    cabinet.prepare ({48000, 64, 1});
    juce::AudioBuffer<float> x (1, 64);
    std::atomic<bool> start{false}, finished{false};
    std::thread producer (
        [&]
        {
            while (!start.load ())
                juce::Thread::sleep (1);
            for (int k = 0; k < 30; ++k)
            {
                cabinet.load (k % 2 ? a : b);
                if (k % 3 == 0)
                    cabinet.clear ();
            }
            finished = true;
        });
    std::vector<double> timings (1200);
    auto builtin = [] (float *, int) noexcept {};
    heapProbe::allocations = heapProbe::deletions = 0;
    start = true;
    for (int k = 0; k < 1200; ++k)
    {
        x.clear ();
        x.setSample (0, 0, .1f);
        const auto before = std::chrono::steady_clock::now ();
        heapProbe::enabled = true;
        cabinet.process (juce::dsp::AudioBlock<float> (x), builtin);
        heapProbe::enabled = false;
        timings[(size_t)k] =
            std::chrono::duration<double, std::micro> (std::chrono::steady_clock::now () - before).count ();
        juce::Thread::sleep (1); // paced synthetic driver, not an ASIO deadline claim
    }
    producer.join ();
    check (finished, "IR control preparation completes concurrently with paced processing");
    check (heapProbe::allocations == 0 && heapProbe::deletions == 0,
           "IR process/fade/handoff does no C++ new/delete");
    std::sort (timings.begin (), timings.end ());
    std::cout << "IR-only callback microseconds: p95=" << timings[1140] << " p99=" << timings[1188]
              << " max=" << timings.back ()
              << "; deadline at 48k/64=1333.33 us (VM, no hard realtime claim)\n";
}

static void sceneAudio (const juce::File &dir, const juce::File &ir)
{
    VibeAmpProcessor original (dir), recalled (dir), presetCopy (dir);
    param (original, ids::channel, 0);
    param (original, ids::gateOn, 0);
    param (original, ids::ampGain, 3);
    original.loadIR (ir);
    original.prepareToPlay (48000, 64);
    check (original.presets.save ("audio-recall"), "save complete audible scene");
    const auto saved = hostState (original);
    // Exercise host restore AFTER activation, and preset restore BEFORE activation.
    recalled.prepareToPlay (48000, 64);
    recalled.setStateInformation (saved.getData (), (int)saved.getSize ());
    presetCopy.presets.refresh ();
    check (presetCopy.presets.load (presetCopy.presets.indexOf ("audio-recall")),
           "reload saved preset in another processor");
    presetCopy.prepareToPlay (48000, 64);
    juce::AudioBuffer<float> a (2, 64), b (2, 64), c (2, 64);
    juce::MidiBuffer midi;
    double hostError = 0, presetError = 0, energy = 0;
    for (int k = 0; k < 120; ++k)
    {
        a.clear ();
        for (int i = 0; i < 64; ++i)
            a.setSample (0, i, .1f * std::sin ((k * 64 + i) * .03f));
        b.makeCopyOf (a, true);
        c.makeCopyOf (a, true);
        original.processBlock (a, midi);
        recalled.processBlock (b, midi);
        presetCopy.processBlock (c, midi);
        if (k > 80)
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < 64; ++i)
                {
                    const float x = a.getSample (channel, i);
                    energy += x * x;
                    hostError = std::max (hostError, (double)std::abs (x - b.getSample (channel, i)));
                    presetError = std::max (presetError, (double)std::abs (x - c.getSample (channel, i)));
                }
    }
    std::cout << "Scene recall: host_max_error=" << hostError << " preset_max_error=" << presetError
              << " reference_energy=" << energy << std::endl;
    check (energy > 1e-10, "recall comparison exercises non-silent amp/cab audio");
    check (hostError < 1e-6, "host recall preserves actual stereo output after IR transition");
    check (presetError < 1e-6, "preset reload preserves actual stereo output");
}

// Observe the exact post-amp input; compare the two real cabinet routes with
// independent original-JUCE and five-filter oracles, without a second amp.
static void builtinReturnAudio (const juce::File &dir, const juce::File &ir)
{
    VibeAmpProcessor switched (dir);
    switched.loadIR (ir);
    switched.prepareToPlay (48000, 64);
    juce::dsp::Convolution originalIR;
    originalIR.loadImpulseResponse (ir, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::yes,
                                    0, juce::dsp::Convolution::Normalise::yes);
    originalIR.prepare ({48000, 64, 1});
    juce::AudioBuffer<float> audio (2, 64), captured (1, 64), expectedIR (1, 64);
    juce::MidiBuffer midi;
    double error = 0, energy = 0, effect = 0;
    for (int k = 0; k < 120; ++k)
    {
        audio.clear ();
        for (int i = 0; i < 64; ++i)
            audio.setSample (0, i, .1f * std::sin ((k * 64 + i) * .03f));
        cabinetInputCapture = captured.getWritePointer (0);
        switched.processBlock (audio, midi);
        cabinetInputCapture = nullptr;
        expectedIR.makeCopyOf (captured, true);
        juce::dsp::AudioBlock<float> refBlock (expectedIR);
        juce::dsp::ProcessContextReplacing<float> ctx (refBlock);
        originalIR.process (ctx);
        if (k > 80)
            for (int i = 0; i < 64; ++i)
            {
                const float expected = expectedIR.getSample (0, i);
                energy += expected * expected;
                effect = std::max (effect, (double)std::abs (expected - captured.getSample (0, i)));
                for (int c = 0; c < 2; ++c)
                    error = std::max (error, (double)std::abs (audio.getSample (c, i) - expected));
            }
    }
    std::cout << "Processor external IR: max_error=" << error << " effect=" << effect << std::endl;
    check (error < 1e-6 && energy > 1e-10 && effect > 1e-5,
           "actual processor external IR matches original JUCE file-load oracle");

    switched.presets.load (0);
    vamp::Biquad curve[5];
    using T = vamp::Biquad::Type;
    curve[0].set (T::highPass, 48000, 80, 0.7);
    curve[1].set (T::peak, 48000, 120, 1.2, 3);
    curve[2].set (T::peak, 48000, 400, 1.0, -3);
    curve[3].set (T::peak, 48000, 2500, 1.5, 3);
    curve[4].set (T::lowPass, 48000, 5000, 0.9);
    error = energy = effect = 0;
    for (int k = 0; k < 120; ++k)
    {
        audio.clear ();
        for (int i = 0; i < 64; ++i)
            audio.setSample (0, i, .1f * std::sin ((k * 64 + i) * .03f));
        cabinetInputCapture = captured.getWritePointer (0);
        switched.processBlock (audio, midi);
        cabinetInputCapture = nullptr;
        for (int i = 0; i < 64; ++i)
        {
            float expected = captured.getSample (0, i);
            for (auto &filter : curve)
                expected = filter.process (expected);
            if (k > 80)
            {
                energy += expected * expected;
                effect = std::max (effect, (double)std::abs (expected - captured.getSample (0, i)));
                for (int c = 0; c < 2; ++c)
                    error = std::max (error, (double)std::abs (audio.getSample (c, i) - expected));
            }
        }
    }
    std::cout << "Built-in return: max_error=" << error << " curve_effect=" << effect
              << " reference_energy=" << energy << std::endl;
    check (error < 1e-6 && energy > 1e-10 && effect > 1e-5,
           "factory return routes actual processor audio through audited built-in curve");
}

int main ()
{
    juce::ScopedJuceInitialiser_GUI init;
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getNonexistentChildFile ("125a-ir-tests", "", false);
    dir.createDirectory ();
    try
    {
        const auto a = wav (dir, "a.wav"), b = wav (dir, "b.wav", true), bad = dir.getChildFile ("bad.wav"),
                   missing = dir.getChildFile ("missing.wav");
        bad.replaceWithText ("not an audio file");
        VibeAmpProcessor p (dir);
        p.prepareToPlay (48000, 64);
        param (p, ids::channel, 0);
        param (p, ids::gateOn, 0);
        p.loadIR (a);
        block (p);
        p.presets.load (0);
        check (p.getIRPath ().isEmpty (), "A04 factory restores built-in");
        p.loadIR (a);
        param (p, ids::ampGain, 2);
        const auto previous = p.apvts.copyState ().toXmlString ();
        const auto name = p.presets.getCurrentName ();
        auto wrong = dir.getChildFile ("foreign.xml");
        wrong.replaceWithText ("<unrelated/>");
        p.presets.refresh ();
        p.presets.load (p.presets.indexOf ("foreign"));
        check (p.apvts.copyState ().toXmlString () == previous && p.presets.getCurrentName () == name,
               "B03 foreign XML rejects without parameter/name mutation");
        p.loadIR (a);
        p.loadIR (bad);
        check (p.getIRPath () == a.getFullPathName (), "B04 invalid WAV preserves accepted IR path");
        p.loadIR (a);
        p.loadIR (missing);
        check (p.getIRPath () == a.getFullPathName (), "missing IR preserves accepted IR path");
        p.loadIR (a);
        p.presets.save ("with-ir");
        p.loadIR (b);
        p.presets.load (p.presets.indexOf ("with-ir"));
        check (p.getIRPath () == a.getFullPathName (), "preset save/load restores IR A");
        p.clearIR ();
        p.presets.save ("builtin");
        p.loadIR (a);
        p.presets.load (p.presets.indexOf ("builtin"));
        check (p.getIRPath ().isEmpty (), "A04 user built-in preset clears IR A");
        p.loadIR (a);
        juce::MemoryBlock state;
        p.getStateInformation (state);
        p.loadIR (b);
        p.setStateInformation (state.getData (), (int)state.getSize ());
        check (p.getIRPath () == a.getFullPathName (), "host state restores IR A");
        malformedScenes (p, dir, a);
        cabinetAudio (dir, a, b);
        sceneAudio (dir, a);
        builtinReturnAudio (dir, a);
        realtimeHandoff (a, b);
        std::atomic<bool> done{false}, finite{true};
        std::thread audio (
            [&]
            {
                try
                {
                    for (int i = 0; i < 1000; ++i)
                        block (p);
                }
                catch (...)
                {
                    finite = false;
                }
                done = true;
            });
        bool saves = true;
        for (int i = 0; i < 20; ++i)
        {
            p.loadIR (i % 2 ? a : b);
            if (i % 3 == 0)
                p.clearIR ();
            saves = p.presets.save ("during-audio") && saves;
            saves = p.presets.load (p.presets.indexOf ("during-audio")) && saves;
            const auto scene = hostState (p);
            p.setStateInformation (scene.getData (), (int)scene.getSize ());
        }
        audio.join ();
        check (done && finite, "concurrent load/process completes with finite audio");
        check (saves, "preset save/reload and host restore succeed during audio processing");
#ifdef IR_TRACE_CONVOLUTION
        check (ownershipViolations == 0, "B01 no load command touches a published/rendered convolver");
#endif
    }
    catch (const std::exception &e)
    {
        check (false, e.what ());
    }
    dir.deleteRecursively ();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
