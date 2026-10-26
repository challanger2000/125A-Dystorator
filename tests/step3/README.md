# Step 3: profiling and Windows x64 QA

Pinned product baseline: `398204e26d84c3f40bcd1c0af55dfa8e97893ca7`. Report: [STEP3-CPU-WINDOWS-QA-20261026.md](../../docs/STEP3-CPU-WINDOWS-QA-20261026.md).

`baseline/` is the 17-header amp dependency closure from that exact commit, namespace-only transformed to `vamp_step3_before`. SOURCE.txt records original SHA-256. Do not regenerate it to hide a regression.

`rails_test.cpp` compares the current Preamp with the frozen one under per-sample varying supply voltages, custom initial rails, channel/crunch fades, audio and rail-current comparisons. Root CTest registers it. `dystorator_qa_suite` depends on every executable registered through `dystorator_register_test`; build this before CTest, rather than maintaining a manual test executable list.

`prototypes/` retains REJECTED SIMD solver experiments. They are NOT product dependencies. `-DDYSTORATOR_BUILD_STEP3_PROTOTYPES=ON` additionally builds three original-order elimination checks (auto AVX, forced SSE2, scalar). Their mathematical PASS does not override their end-to-end performance regression.

## Full processor profiles / CPU

Configure `tests/ir_presets` as documented in `tests/cpu_quality/README.md`, with explicit source directory, `IR_TEST_MAIN=step3_bench.cpp`, trace/mutation OFF, Release/O3. Add `-g` to CXX flags for Linux DWARF attribution. Build a frozen before copy using:

```sh
mkdir -p build/step3-before
git archive 398204e26d84c3f40bcd1c0af55dfa8e97893ca7 Source | tar -x -C build/step3-before
```

Point before's `IR_PRODUCT_SOURCE_DIR` to that Source; after's to the working Source. Preserve the two executables. Invoke each with `before` / `after` for 48 cases (2 rates, 2 amp modes, 3 qualities, 2 blocks, offline/paced). Same RED/gain9/Drive8/level7/master3/cab/cuts fixture and strict 160-callback deadline gate as the earlier benchmark. Nonzero exit1 for missed deadlines is an observed FAIL, not infrastructure failure.

`bench profile` runs a longer warmed 2048-callback HIGH/stereo interval at 48k/256. Without FDs it measures the unprofiled interval. `record_perf.py` runs Linux perf with enable/disable acknowledgements sent by the application after warm-up:

```sh
python3 tests/step3/record_perf.py /absolute/perf /absolute/bench build/profile.data
perf report --stdio --children --sort dso,symbol --call-graph none -i build/profile.data
```

Frequency499 Hz, CPU-clock/user-only, DWARF16KiB stacks. Requires permitted perf_event_open; tool/dependency setup remains environment-specific. Preserve perf output, build ID/source hashes and on/off timings. Inclusive rows overlap; never sum Newton/matrix/tube children with their preamp/power parents. Sampling and stack collection perturb execution; use separate unprofiled CPU tables for performance acceptance. Do not profile setup/settling and then label it audio callback CPU.

Alternative actual-processor integration entry points: `step3_pcm.cpp` (27 comparisons to398204e, including boost/model changes) and `step3_alias.cpp` (64 spectral render measurements). Trace/mutation must be OFF. Null residual floors in the PCM log are numerical placeholders; maxdelta0 means equal sample values, not a measured −3000dB physical noise floor. Alias masks and 16× references have the limitations described in the report.

## Actual VST3 host

Build root VST3 and `compact_vst3_host` (`IR_BUILD_VST3_HOST=ON`). Invoke:

```text
compact_vst3_host step3-heap-selftest  # Windows observer positive control
compact_vst3_host /absolute/plugin.vst3 step3
compact_vst3_host /absolute/plugin.vst3 step3-cpu before-or-after
```

First command retains earlier non-default state/R-only/bypass/live-quality checks and adds actual stop/play/cycle context with rapid mode/quality/bypass against uninterrupted reference PCM, disk save/destroy/fresh-instance IR recall, missing-IR+gain rollback, and audible clear-to-built-in. Second measures48 end-to-end cases through the built VST3, checking actual component parameters and both channel energies. Native stereo reference tests independently protect against DC-only or muted-channel false positives.

Windows-only: configure `STEP3_DETOURS_DIR` to pinned Microsoft Detours v4.0.1. The host hooks RtlAllocateHeap/ReAllocateHeap/FreeHeap beneath plugin and host CRTs. It self-tests actual heap operations, and enables thread-local counters only around processBlock. No product hook or shipped dependency. Linux prints NOT RUN for this Windows probe. It does not claim to intercept every possible direct OS memory API or prove ASIO scheduling.

## Windows run

`.github/workflows/step3-windows-qa.yml` is scoped to the development branch and code/test/workflow paths. It pins JUCE8.0.4, SDK3.8.1build84, 125A tester source3dcf3fe and Detours4.0.1. No main change, merge, release or GUI redesign. It builds actual before/after Release DLLs, root/native and JUCE integration tests, real VST3 host/heap/transport/state tests, CPU matrices, and the current125A tester plus validator/self-tests/positive and hostile fixtures. Internal tester source/binaries are not modified or published.

Windows PowerShell driver: `run_windows_qa.ps1`; see workflow arguments. It writes each native exit/log plus JSON statuses. CPU failures remain FAIL. Tests not reached after build failure become NOT RUN. SDK/Tester self-validation failures make product interpretation inconclusive. Job evidence is committed back only to the development branch, names the tested source SHA and run URL, and cannot trigger another code build. Product bundle/logs are also a short-lived Actions artifact.

Windows CI hardware is not the owner's i9-11900K. Actual ASIO/DAW dropout capture, real guitar DI and matched listening remain separate NOT RUN gates. No FINAL approval.

## Large-IR real FFT heap boundary

`IR_TEST_MAIN=large_ir_heap.cpp` tests mono/stereo IRs at128/256/8192/8193/16384. Linux links `--wrap=malloc/calloc/realloc/free` for actual C allocations in the linked JUCE code; Windows uses the self-validated process-heap detours. Native real-FFT fallback originally made24 malloc/free pairs over6 calls at16384, despite zero C++new/delete in the older tests. Product only caps its convolver quantum at8192 and subdivides views; the amp host block/ramp and scene fade are unchanged. The oracle remains the unmodified JUCE large-block convolution, sample tolerance1e−6. Windows packaged host additionally exercises16384 with the cross-DLL hooks enabled. No DSP precision/iteration reduction.

The workflow checks out the exact triggering SHA, then attaches that local checkout to the development branch. This also works when a preceding queued job publishes a documentation-only evidence commit before checkout; it never silently tests a different source HEAD.

Windows heap hooks gate C++ TLS reads with a reserved fixed TEB TLS slot and preserve LastError. The actual test host includes unbuffered stage and fatal CRT/C++/SEH diagnostics with module/stack locations. Required heap/host functional smoke gates run before the long audio suites; a failure retains its logs and marks unreached checks NOT RUN. CPU deadline failures still retain the entire measurement matrix and remain FAIL.
