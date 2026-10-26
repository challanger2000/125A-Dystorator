#pragma once
#include "../step3/WindowsHeapProbe.h"

namespace step3Hosted
{
inline void parameter (juce::AudioProcessor &p, const char *name, float value)
{
    for (auto *x : p.getParameters ())
        if (x->getName (128) == name)
        {
            x->setValueNotifyingHost (value);
            return;
        }
    throw std::runtime_error (std::string ("Missing hosted parameter: ") + name);
}
struct Clock : juce::AudioPlayHead
{
    int block = 0;
    bool changing = true;
    juce::Optional<PositionInfo> getPosition () const override
    {
        PositionInfo p;
        p.setBpm (120.);
        p.setTimeInSamples ((juce::int64)block * 128);
        p.setTimeInSeconds (block * 128. / 48000.);
        p.setPpqPosition (changing && block > 60 ? (block % 16) * 128. / 24000. : block * 128. / 24000.);
        p.setIsPlaying (!changing || block % 30 < 20);
        p.setIsLooping (changing && block > 60);
        p.setLoopPoints (LoopPoints{0., 16. * 128. / 24000.});
        return p;
    }
};
inline juce::MemoryBlock sceneWithIR (juce::AudioProcessor &p, const juce::String &path,
                                      bool invalidGain = false)
{
    juce::MemoryBlock host;
    p.getStateInformation (host);
    auto xml = juce::AudioProcessor::getXmlFromBinary (host.getData (), (int)host.getSize ());
    auto *component = xml ? xml->getChildByName ("IComponent") : nullptr;
    juce::MemoryBlock bytes;
    if (!component || !bytes.fromBase64Encoding (component->getAllSubText ()))
        throw std::runtime_error ("Cannot decode actual hosted component state");
    auto scene = juce::AudioProcessor::getXmlFromBinary (bytes.getData (), (int)bytes.getSize ());
    if (!scene)
        throw std::runtime_error ("Missing actual product scene");
    scene->setAttribute ("irPath", path);
    if (invalidGain)
        for (auto *child : scene->getChildWithTagNameIterator ("PARAM"))
            if (child->getStringAttribute ("id") == "ampGain")
                child->setAttribute ("value", 2.);
    juce::AudioProcessor::copyXmlToBinary (*scene, bytes);
    component->deleteAllChildElements ();
    component->addTextElement (bytes.toBase64Encoding ());
    juce::AudioProcessor::copyXmlToBinary (*xml, host);
    return host;
}
inline bool run (juce::VST3PluginFormat &format, const juce::PluginDescription &description)
{
    bool passed = true;
    auto check = [&] (bool ok, const char *name)
    {
        std::cout << (ok ? "PASS " : "FAIL ") << name << std::endl;
        passed = passed && ok;
    };
#if defined(_WIN32) && defined(STEP3_DETOURS_AVAILABLE)
    check (step3Heap::Installation::selfTest (),
           "Windows cross-DLL heap hook detects actual allocate/reallocate/free");
#else
    std::cout << "NOT RUN Windows cross-DLL heap observation (non-Windows host)" << std::endl;
#endif
    auto make = [&] (const juce::MemoryBlock *state = nullptr)
    {
        juce::String error;
        auto p = format.createInstanceFromDescription (description, 48000, 128, error);
        if (!p)
            throw std::runtime_error (error.toStdString ());
        auto layout = p->getBusesLayout ();
        layout.inputBuses.set (0, juce::AudioChannelSet::stereo ());
        layout.outputBuses.set (0, juce::AudioChannelSet::stereo ());
        if (!p->setBusesLayout (layout))
            throw std::runtime_error ("Hosted stereo negotiation failed");
        if (state)
            p->setStateInformation (state->getData (), (int)state->getSize ());
        else
            for (auto setting : std::vector<std::pair<const char *, float>>{{"Amp Noise", 0},
                                                                            {"Input Gate On", 0},
                                                                            {"Channel", 1},
                                                                            {"Gain", .9f},
                                                                            {"Master", .3f},
                                                                            {"Boost On", 1},
                                                                            {"Boost Drive", .8f},
                                                                            {"Cab On", 1},
                                                                            {"Low Cut On", 1},
                                                                            {"High Cut On", 1},
                                                                            {"AMP MODE", 1},
                                                                            {"QUALITY", 1}})
                parameter (*p, setting.first, setting.second);
        p->prepareToPlay (48000, 128);
        return p;
    };
    auto a = make (), b = make ();
    Clock moving, continuous;
    continuous.changing = false;
    a->setPlayHead (&moving);
    b->setPlayHead (&continuous);
    juce::AudioBuffer<float> x (2, 128), y (2, 128);
    juce::MidiBuffer midi;
    std::uint64_t allocations = 0, releases = 0;
    void *firstAllocationCaller = nullptr;
    double difference = 0, energy = 0;
    const int latency = a->getLatencySamples ();
    auto input = [&] (int k)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 128; ++i)
                x.setSample (c, i,
                             (float)(.12 * std::sin (2 * juce::MathConstants<double>::pi * (c ? 277 : 110) *
                                                         (k * 128 + i) / 48000. +
                                                     .3)));
        y.makeCopyOf (x, true);
    };
    auto process = [&] (juce::AudioProcessor &p, juce::AudioBuffer<float> &buffer)
    {
        {
            step3Heap::Scope capture;
            p.processBlock (buffer, midi);
        }
        if (firstAllocationCaller == nullptr && step3Heap::allocations != 0)
            firstAllocationCaller = step3Heap::firstCaller;
        allocations += step3Heap::allocations;
        releases += step3Heap::releases;
    };
    for (int k = 0; k < 180; ++k)
    {
        moving.block = continuous.block = k;
        if (k < 30 || k == 30)
            for (auto *p : {a.get (), b.get ()})
            {
                parameter (*p, "AMP MODE", k == 30 ? 1.f : (float)(k % 2));
                parameter (*p, "QUALITY", k == 30 ? 1.f : (k % 3) * .5f);
            }
        if (k == 70 || k == 95)
            for (auto *p : {a.get (), b.get ()})
                p->getBypassParameter ()->setValueNotifyingHost (k == 70 ? 1.f : 0.f);
        input (k);
        process (*a, x);
        process (*b, y);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 128; ++i)
            {
                difference =
                    std::max (difference, (double)std::abs (x.getSample (c, i) - y.getSample (c, i)));
                energy += (double)x.getSample (c, i) * x.getSample (c, i);
                if (!std::isfinite (x.getSample (c, i)))
                    passed = false;
            }
    }
    check (difference < 1e-6 && energy > 1e-8 && a->getLatencySamples () == latency,
           "actual VST3 rapid mode/quality/bypass with stop/play/cycle preserves continuous reference PCM "
           "and latency");
    std::cout << "HOST_TRANSPORT max_delta=" << difference << " energy=" << energy << std::endl;
    a->setPlayHead (nullptr);
    b->setPlayHead (nullptr);
    a->releaseResources ();
    b->releaseResources ();
    a.reset ();
    b.reset ();

    const auto directory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getNonexistentChildFile ("125a-host-ir", "", false);
    directory.createDirectory ();
    const auto ir = directory.getChildFile ("stereo.wav");
    juce::WavAudioFormat wave;
    auto stream = ir.createOutputStream ();
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wave.createWriterFor (stream.release (), 48000, 2, 24, {}, 0));
    juce::AudioBuffer<float> impulse (2, 64);
    impulse.clear ();
    impulse.setSample (0, 0, .7f);
    impulse.setSample (1, 17, -.5f);
    if (!writer || !writer->writeFromAudioSampleBuffer (impulse, 0, 64))
        throw std::runtime_error ("IR fixture write failed");
    writer.reset ();
    a = make ();
    auto scene = sceneWithIR (*a, ir.getFullPathName ());
    a->setStateInformation (scene.getData (), (int)scene.getSize ());
    juce::MemoryBlock saved;
    a->getStateInformation (saved);
    const auto project = directory.getChildFile ("saved.hoststate");
    if (!project.replaceWithData (saved.getData (), saved.getSize ()))
        throw std::runtime_error ("Host state disk save failed");
    a->releaseResources ();
    a.reset ();
    saved.reset ();
    if (!project.loadFileAsData (saved))
        throw std::runtime_error ("Host state disk read failed");
    a = make (&saved);
    b = make (&scene);
    difference = energy = 0;
    for (int k = 0; k < 120; ++k)
    {
        input (k);
        process (*a, x);
        process (*b, y);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 128; ++i)
            {
                difference =
                    std::max (difference, (double)std::abs (x.getSample (c, i) - y.getSample (c, i)));
                energy += (double)x.getSample (c, i) * x.getSample (c, i);
            }
    }
    auto actual = hostedComponentState (*a);
    check (
        actual && actual->getStringAttribute ("irPath") == ir.getFullPathName () && difference < 1e-6 &&
            energy > 1e-8,
        "actual VST3 stereo IR host save/destroy/disk reload/fresh instance restores nondefault audible PCM");
    std::cout << "HOST_IR_RECALL max_delta=" << difference << " energy=" << energy << std::endl;
    const auto accepted = actual->toString ();
    auto missing = sceneWithIR (*a, directory.getChildFile ("missing.wav").getFullPathName (), true);
    a->setStateInformation (missing.getData (), (int)missing.getSize ());
    actual = hostedComponentState (*a);
    check (actual && actual->toString () == accepted,
           "actual VST3 missing IR recall rolls back both IR selection and changed gain");
    // Different host and FFT boundary: the default fallback FFT used to malloc
    // scratch here despite the smaller-block new/delete tests being green.
    a->releaseResources ();
    b->releaseResources ();
    a->prepareToPlay (48000, 16384);
    b->prepareToPlay (48000, 16384);
    juce::AudioBuffer<float> large (2, 16384), largeReference (2, 16384);
    double largeDelta = 0, largeEnergy = 0;
    for (int k = 0; k < 3; ++k)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 16384; ++i)
                large.setSample (c, i, (float)(.1 * std::sin ((k * 16384 + i) * (c ? .037 : .019))));
        largeReference.makeCopyOf (large, true);
        process (*a, large);
        process (*b, largeReference);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 16384; ++i)
            {
                const double actualSample = large.getSample (c, i),
                             expectedSample = largeReference.getSample (c, i);
                if (!std::isfinite (actualSample) || !std::isfinite (expectedSample))
                    passed = false;
                largeDelta = std::max (largeDelta, std::abs (actualSample - expectedSample));
                largeEnergy += actualSample * actualSample;
            }
    }
    check (largeDelta < 1e-6 && largeEnergy > 1e-8,
           "actual VST3 reprepare/IR at 16384 samples remains finite and matches fresh equivalent stream");
    std::cout << "HOST_LARGE_IR max_delta=" << largeDelta << " energy=" << largeEnergy << std::endl;
    auto clear = sceneWithIR (*a, {});
    a->setStateInformation (clear.getData (), (int)clear.getSize ());
    double changed = 0;
    for (int k = 120; k < 240; ++k)
    {
        input (k);
        process (*a, x);
        process (*b, y);
        if (k > 220)
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 128; ++i)
                {
                    const double delta = x.getSample (c, i) - y.getSample (c, i);
                    changed += delta * delta;
                }
    }
    actual = hostedComponentState (*a);
    check (
        actual && actual->getStringAttribute ("irPath").isEmpty () && changed > 1e-8,
        "actual VST3 clear IR commits built-in selection and audibly changes from continuing IR reference");
    a->releaseResources ();
    b->releaseResources ();
    a.reset ();
    b.reset ();
    directory.deleteRecursively ();
#if defined(_WIN32) && defined(STEP3_DETOURS_AVAILABLE)
    if (firstAllocationCaller != nullptr)
    {
        HMODULE module = nullptr;
        wchar_t filename[MAX_PATH]{};
        if (GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCWSTR> (firstAllocationCaller), &module))
            GetModuleFileNameW (module, filename, MAX_PATH);
        std::cout << "WINDOWS_FIRST_HEAP_CALLER module=" << juce::String (filename)
                  << " address=" << firstAllocationCaller << std::endl;
    }
    std::cout << "WINDOWS_CALLBACK_HEAP allocations=" << allocations << " frees=" << releases << std::endl;
    check (allocations == 0 && releases == 0,
           "actual VST3 callbacks perform no process-heap allocate/reallocate/free across DLL CRT boundary");
#endif
    return passed;
}
} // namespace step3Hosted

namespace step3Hosted
{
inline int cpu (juce::VST3PluginFormat &format, const juce::PluginDescription &description,
                const char *version)
{
    int failures = 0, cases = 0;
    for (double fs : {44100., 48000.})
        for (int lanes : {1, 2})
        {
            juce::String error;
            auto p = format.createInstanceFromDescription (description, fs, 512, error);
            if (!p)
                throw std::runtime_error (error.toStdString ());
            auto layout = p->getBusesLayout ();
            layout.inputBuses.set (0, juce::AudioChannelSet::stereo ());
            layout.outputBuses.set (0, juce::AudioChannelSet::stereo ());
            if (!p->setBusesLayout (layout))
                return 2;
            for (auto setting :
                 std::vector<std::pair<const char *, float>>{{"Amp Noise", 0},
                                                             {"Input Gate On", 0},
                                                             {"Channel", 1},
                                                             {"Gain", .9f},
                                                             {"Master", .3f},
                                                             {"Boost On", 1},
                                                             {"Boost Drive", .8f},
                                                             {"Boost Level", .7f},
                                                             {"Cab On", 1},
                                                             {"Low Cut On", 1},
                                                             {"High Cut On", 1},
                                                             {"AMP MODE", lanes == 1 ? 0.f : 1.f},
                                                             {"QUALITY", 1}})
                parameter (*p, setting.first, setting.second);
            p->prepareToPlay (fs, 512);
            const int latency = p->getLatencySamples ();
            for (int quality : {2, 1, 0})
            {
                parameter (*p, "QUALITY", quality * .5f);
                for (int frames : {128, 256})
                    for (bool paced : {false, true})
                    {
                        juce::AudioBuffer<float> audio (2, frames);
                        juce::MidiBuffer midi;
                        std::vector<double> times;
                        times.reserve (160);
                        const int warm = (int)std::ceil (.2 * fs / frames);
                        const double deadline = 1e6 * frames / fs;
                        int over = 0, late = 0;
                        double energy[2]{};
                        bool finite = true;
                        auto next = std::chrono::steady_clock::now ();
                        for (int k = 0; k < warm + 160; ++k)
                        {
                            for (int i = 0; i < frames; ++i)
                            {
                                const double t = (k * frames + i) / fs;
                                float l =
                                    (float)(.12 * std::sin (2 * juce::MathConstants<double>::pi * 110 * t +
                                                            .3) +
                                            .025 * std::sin (2 * juce::MathConstants<double>::pi * 3731 * t));
                                float r =
                                    (float)(.12 * std::sin (2 * juce::MathConstants<double>::pi * 277 * t +
                                                            .3) +
                                            .025 * std::sin (2 * juce::MathConstants<double>::pi * 1723 * t));
                                if (lanes == 1)
                                    l = r = .5f * (l + r);
                                audio.setSample (0, i, l);
                                audio.setSample (1, i, r);
                            }
                            if (paced && k >= warm)
                                std::this_thread::sleep_until (next);
                            const auto begin = std::chrono::steady_clock::now ();
                            p->processBlock (audio, midi);
                            const auto end = std::chrono::steady_clock::now ();
                            if (k >= warm)
                            {
                                const double us =
                                    std::chrono::duration<double, std::micro> (end - begin).count ();
                                times.push_back (us);
                                over += us > deadline;
                                if (paced)
                                    late += end > next + std::chrono::duration<double> (frames / fs);
                                for (int c = 0; c < 2; ++c)
                                    for (int i = 0; i < frames; ++i)
                                    {
                                        const float value = audio.getSample (c, i);
                                        finite = finite && std::isfinite (value);
                                        energy[c] += (double)value * value;
                                    }
                            }
                            if (k < warm)
                                next = end;
                            else
                                next += std::chrono::duration_cast<std::chrono::steady_clock::duration> (
                                    std::chrono::duration<double> (frames / fs));
                        }
                        bool stateCorrect = false;
                        if (auto scene = hostedComponentState (*p))
                        {
                            bool mode = false, q = false, gain = false, drive = false;
                            for (auto *child : scene->getChildWithTagNameIterator ("PARAM"))
                            {
                                const auto id = child->getStringAttribute ("id");
                                const double value = child->getDoubleAttribute ("value");
                                if (id == "ampMode")
                                    mode = value == (lanes == 1 ? 0 : 1);
                                if (id == "quality")
                                    q = value == quality;
                                if (id == "ampGain")
                                    gain = std::abs (value - 9.) < 1e-5;
                                if (id == "boostOn")
                                    drive = value == 1.;
                            }
                            stateCorrect = mode && q && gain && drive;
                        }
                        std::sort (times.begin (), times.end ());
                        const bool pass = finite && energy[0] > 1e-8 && energy[1] > 1e-8 && stateCorrect &&
                                          p->getLatencySamples () == latency && over == 0 &&
                                          (!paced || late == 0);
                        failures += !pass;
                        ++cases;
                        std::cout << "VST3_CPU version=" << version << " fs=" << fs << " lanes=" << lanes
                                  << " quality=" << quality << " frames=" << frames
                                  << " mode=" << (paced ? "paced" : "offline") << " mean_us="
                                  << std::accumulate (times.begin (), times.end (), 0.) / times.size ()
                                  << " p95_us=" << times[151] << " p99_us=" << times[158]
                                  << " max_us=" << times.back () << " deadline_us=" << deadline
                                  << " overruns=" << over << " schedule_misses=" << late
                                  << " left_energy=" << energy[0] << " right_energy=" << energy[1]
                                  << " component_state=" << (stateCorrect ? "PASS" : "FAIL")
                                  << " status=" << (pass ? "PASS" : "FAIL") << std::endl;
                    }
            }
            p->releaseResources ();
        }
    std::cout << cases << " VST3 CPU cases, " << failures << " deadline failures" << std::endl;
    return failures ? 1 : 0;
}
} // namespace step3Hosted
