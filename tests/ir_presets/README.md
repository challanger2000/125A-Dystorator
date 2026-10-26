# IR/preset JUCE integration regression

Tests the real `VibeAmpProcessor`, APVTS, `PresetManager`, audio-format decoders
and JUCE 8.0.4 Convolution. The unchanged editor is linked but never opened.
All WAV/XML fixtures are generated in a unique temporary folder; no user preset
folder, real cabinet captures or audio hardware is used. Assertions remain active
in Release (`check`, nonzero failure exit); a zero-check result is not accepted.

## Build and run

Use CMake >= 3.24, a C++17 compiler, Python 3 (for the ownership trace) and JUCE's
normal platform development dependencies. No paid Actions build is necessary.

```sh
cmake -S tests/ir_presets -B build/ir-presets -DCMAKE_BUILD_TYPE=Release
cmake --build build/ir-presets --config Release --parallel 3
ctest --test-dir build/ir-presets -C Release --output-on-failure
```

The default fetch is JUCE **8.0.4**. For an existing checkout, pass
`-DJUCE_SOURCE_DIR=/absolute/path/to/JUCE-8.0.4`. This standalone test project does
not change the shipping plugin identity/version/build targets or fetch ASIO.
Windows/DAW/VST3-wrapper testing is a separate gate, not implied by this test.

### Ownership trace (default ON)

`trace_convolution.py` creates **build-local copies** of two JUCE implementation
files and adds observation calls at the actual Convolution load/process/destruct
APIs. It does not modify the JUCE checkout or replace convolution processing.
A test-only `VAMP_IR_TEST_CAPTURE` input tap also observes the real post-amp
Cabinet input for the external-IR and built-in routing oracles. It only copies
into a preallocated test buffer, changes no processing result, and is absent
from shipping builds. This avoids assuming that two independent amp renders
are bit-identical when testing a downstream linear cabinet.

The assertions intentionally enforce our stronger ownership rule: after an
instance is rendered/published, it receives no more loads. This is not a claim
that every sequential post-process load is illegal under JUCE. It makes this
specific repair architecture's thread contract deterministic to test. No crash
or actual data race is inferred just from the baseline ownership failure.
The trace uses a test mutex; never use traced timings as product RT evidence.

`9a1ce3e` is the pre-fix test commit (only an isolated preset-directory injection
seam was added to the product). Its 9 checks produce **5 failures**, exit 1,
covering A04 factory/user, B03, B04 and B01 ownership. See the committed QA logs.

### Untraced / sanitizer runs

```sh
cmake -S tests/ir_presets -B build/ir-presets -DIR_TRACE_CONVOLUTION=OFF
cmake --build build/ir-presets --config Release --parallel 3
ctest --test-dir build/ir-presets -C Release --output-on-failure

# GCC/Clang; use a separate build directory or reconfigure the existing one.
cmake -S tests/ir_presets -B build/ir-presets-tsan \
  -DCMAKE_BUILD_TYPE=Release -DIR_TRACE_CONVOLUTION=OFF -DIR_SANITIZER=thread
cmake --build build/ir-presets-tsan --parallel 3
TSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/ir-presets-tsan --output-on-failure
```

The sanitizer option instruments **all** linked JUCE modules as well as the test
and product sources. Partial instrumentation misses JUCE's detached-thread exit
synchronisation and produced an inconclusive teardown diagnostic during setup;
it is retained in the evidence and is not suppressed. Fully instrumented results
are reported separately. `IR_SANITIZER=address,undefined` is supported for future
checks but was not executed for this repair.

The C++ heap probe counts thread-local `new`/`delete` in the **CabinetIR** audio
path, including fades and swaps, while another thread prepares IRs. It does not
intercept direct C allocation, OS calls, aligned allocation or every third-party
synchronisation primitive. Source inspection and TSan complement it; it does not
constitute a whole-plugin realtime certification. Timing output is diagnostic,
not a scheduling/ASIO acceptance gate.

## Contracts and remaining boundaries

- Rejects must preserve parameters, name, cabinet selection and convolution audio
  state, not just return false. Includes missing/deleted assets, malformed XML,
  unknown/duplicate/non-finite/out-of-range/fractional parameters, future versions,
  truncated WAV, IEEE-float NaN IR and failed preset save.
- Missing nonempty IR references reject the **whole scene**. An empty IR path
  explicitly selects built-in. No silent fallback to a different sound.
- A validated IR is prepared before publication; the last accepted request wins.
  Crossfades finish before a later candidate is installed; reclamation must also
  progress after the producer stops making requests.
- Normalisation, trimming and first-channel mono semantics match the old JUCE
  path in the impulse comparison. A real amp/cab signal compares saved-preset and
  host-state recall with a continuously processed reference, after the 50 ms IR
  transition. Host restore before and after prepare are covered across these
  cases; this is an in-process host-API harness, not a DAW integration test.
- Supported input safety budgets: file <=64 MiB, decoded mono <=4,194,304 samples,
  sample rate 8–768 kHz; prepared/resampled IR also <=4,194,304 samples.
  Already before activation, every candidate is checked against 768 kHz to
  keep ordinary later sample-rate changes within the same budget: maximum
  source duration is therefore about 5.46 seconds (before trimming). Preset and
  host-state blobs <=1 MiB. Oversized candidates are rejected, not truncated.
- IRs remain path-based in saved states. Embedding, file identity hashes and the
  other audit findings are outside this repair block.
