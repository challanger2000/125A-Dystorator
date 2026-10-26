#include "../step3/WindowsHeapProbe.h"
#include "../step3/WindowsCrashDiagnostics.h"
#include "CabinetIR.h"
#include "matched_ir_oracle.h"
#include <iostream>
#include <juce_audio_utils/juce_audio_utils.h>
#if defined(STEP3_WRAP_C_HEAP)
namespace cHeap
{
inline thread_local bool armed = false;
inline thread_local unsigned alloc = 0, freed = 0;
} // namespace cHeap
extern "C" void *__real_malloc (size_t);
extern "C" void *__real_calloc (size_t, size_t);
extern "C" void *__real_realloc (void *, size_t);
extern "C" void __real_free (void *);
extern "C" void *__wrap_malloc (size_t n)
{
    if (cHeap::armed)
        ++cHeap::alloc;
    return __real_malloc (n);
}
extern "C" void *__wrap_calloc (size_t n, size_t m)
{
    if (cHeap::armed)
        ++cHeap::alloc;
    return __real_calloc (n, m);
}
extern "C" void *__wrap_realloc (void *p, size_t n)
{
    if (cHeap::armed)
        ++cHeap::alloc;
    return __real_realloc (p, n);
}
extern "C" void __wrap_free (void *p)
{
    if (cHeap::armed && p)
        ++cHeap::freed;
    __real_free (p);
}
#endif
void irTestCaptureCabinetInput (const float *, int) {}
void irTestConvolutionCall (const void *, int) {}
int main ()
{
    step3Crash::install ();
    std::cout << "LARGE_IR_STAGE install heap" << std::endl;
#if defined(_WIN32)
    step3Heap::Installation hooks (true);
    if (!step3Heap::Installation::selfTest ())
        return 2;
#endif
    juce::ScopedJuceInitialiser_GUI init;
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getNonexistentChildFile ("125a-large-ir", "", false);
    dir.createDirectory ();
    int failures = 0;
    for (int channels : {1, 2})
    {
        const auto file = dir.getChildFile (channels == 1 ? "mono.wav" : "stereo.wav");
        juce::WavAudioFormat wav;
        auto stream = file.createOutputStream ();
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (stream.release (), 48000, (unsigned)channels, 24, {}, 0));
        juce::AudioBuffer<float> taps (channels, 64);
        taps.clear ();
        taps.setSample (0, 0, .7f);
        taps.setSample (0, 17, -.2f);
        if (channels == 2)
        {
            taps.setSample (1, 0, .4f);
            taps.setSample (1, 31, .3f);
        }
        if (!writer || !writer->writeFromAudioSampleBuffer (taps, 0, 64))
            return 2;
        writer.reset ();
        for (int frames : {128, 256, 8192, 8193, 16384})
        {
            CabinetIR cab;
            if (!cab.load (file))
                return 2;
            const juce::dsp::ProcessSpec spec{48000., (juce::uint32)frames, 2};
            cab.prepare (spec);
            juce::dsp::Convolution reference;
            loadMatchedReference (reference, file, 2);
            reference.prepare (spec);
            juce::AudioBuffer<float> audio (2, frames), expected (2, frames);
            double error = 0, energy = 0;
            unsigned allocations = 0, releases = 0;
            bool finite = true;
            for (int k = 0; k < 6; ++k)
            {
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < frames; ++i)
                        audio.setSample (c, i, (float)(.1 * std::sin ((k * frames + i) * (c ? .037 : .019))));
                expected.makeCopyOf (audio, true);
                auto block = juce::dsp::AudioBlock<float> (audio);
#if defined(STEP3_WRAP_C_HEAP)
                cHeap::alloc = cHeap::freed = 0;
                cHeap::armed = true;
#endif
                {
                    step3Heap::Scope probe;
                    cab.processStereo (block, [] (float *, int, int) {});
                }
#if defined(STEP3_WRAP_C_HEAP)
                cHeap::armed = false;
                allocations += cHeap::alloc;
                releases += cHeap::freed;
#elif defined(_WIN32) && defined(STEP3_DETOURS_AVAILABLE)
                allocations += (unsigned)step3Heap::allocations;
                releases += (unsigned)step3Heap::releases;
#else
#error A real C/process heap observer is required for this test
#endif
                auto expectedBlock = juce::dsp::AudioBlock<float> (expected);
                juce::dsp::ProcessContextReplacing<float> context (expectedBlock);
                reference.process (context);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < frames; ++i)
                    {
                        const double actual = audio.getSample (c, i), wanted = expected.getSample (c, i);
                        finite = finite && std::isfinite (actual) && std::isfinite (wanted);
                        error = std::max (error, std::abs (actual - wanted));
                        energy += wanted * wanted;
                    }
            }
            const bool ok = allocations == 0 && releases == 0 && finite && error < 1e-6 && energy > 1e-8;
            failures += !ok;
            std::cout << (ok ? "PASS" : "FAIL") << " IR_HEAP channels=" << channels << " block=" << frames
                      << " allocations=" << allocations << " frees=" << releases << " pcm_max=" << error
                      << " reference_energy=" << energy << std::endl;
        }
    }
    dir.deleteRecursively ();
    return failures ? 1 : 0;
}
