# 125A Dystorator – v1.0.0 upstream audit follow-up / compact UI + IR candidate

**Date:** 2026-10-27. **Product:** unreleased 125A Dystorator Windows x64 VST3. **Source baseline:** `dev/compact-stereo-on-ir-fix-20261026` at `44a184599abeaaa928f6751ce10a71b574ea499c` (v0.5.6 version metadata); `main` is historical import. **Upstream reference:** `SteveAsAConstant/djenty-donkey` release `v1.0.0` (published 2026-10-26). No published 125A FULL/Gumroad release/tag exists for this development repo. This branch must **NOT** be merged/released solely because its files exist.

## Review: What was already inherited

- `Source/dsp/PortSolve.h` and `Source/dsp/TriodeNetwork.h` are byte-identical to upstream v1.0.0. The conductive-port Newton reduction was imported in `398204e26d84c3f40bcd1c0af55dfa8e97893ca7` along with SIMD circuit matrices.
- `Source/dsp/DKCircuit.h`, `Source/dsp/Simd.h`, `Source/dsp/PowerAmp5150.h` are optimized but locally modified; **not** source-identical to upstream. Upstream's 40% CPU claim is NOT a new Dystorator CPU gain. Earlier Dystorator measurements showed ~38.55% HIGH gain against the pre-optimization baseline, followed by negligible further gain.
- Upstream v1.0.0 includes a 1176 LN, WSOLA transpose acceleration, IR loudness matching, new delay reset and additional tests. Dystorator's current compact processor explicitly excludes compressor, transpose, EQ, phaser, delay, reverb and loop gate from the audio path. Their latest optimizations do not therefore explain compact AMP CPU load. Do not reintroduce those modules solely because upstream has updated them.
- The Dystorator additionally has independent nonlinear L/R amp paths, quality (ECO/STANDARD/HIGH), and stereo IR ownership and low/high cut implementations. The original upstream single-amp benchmark is not directly comparable to the two-lane product.

## Changes on this isolated branch

### 1. Compact functional editor

Files `Source/PluginEditor.cpp` and `.h`:
- Remove the seven obsolete sections representing effects without an active processor path (loop 3-band gate, transpose, compressor, 10-band EQ, phaser, delay and reverb). Preserve their legacy APVTS parameter IDs for old states: **no DSP/state/automation schema changes**.
- Expose currently active `ampMode` choice (MONO/STEREO), `quality` choice (ECO/STANDARD/HIGH), and `lowCutOn/lowCutHz/highCutOn/highCutHz`, with normal JUCE parameter attachments.
- Retain INPUT, active INPUT GATE, DRIVE/TS9, amp, cabinet IR and OUTPUT. Compact three-row layout, default 1380×730, min 1120×640.
- Remove visibly wrong DJENTY DONKEY / DONKEY HEAD names. Current header uses text **DYSTORATOR**, **not** a faux 125A logo; the 125A master SVG and final metallic design must be integrated only in the dedicated branding phase.
- Exact VST3 manufacturer IDs, CMake name, installers, presets, file metadata and old project migration remain unchanged in this candidate and are **NOT** declared branding-clean.
- HIGH remains manually selectable for offline bounce; there is **no automatic host-offline detection or quality flip**.

### 2. Upstream-inspired IR loudness, stereo preserving

Files `Source/dsp/CabMatch.h` (derived from upstream v1.0.0, under existing AGPLv3), `Source/CabinetIR.h`:
- On control/prepare side only, calculate the upstream K-weighted/spectral reference gain to match the existing built-in 5-biquad cabinet. For stereo files, use **the mean of weighted powers** and **one shared gain**, not L+R summing or per-side normalization.
- Gain is applied to the existing bounded, fully decoded immutable IR before calling JUCE convolution with `Normalise::no`. Keep source rate and channel routing, 50-ms crossfade, asynchronous reclamation, rollback upon invalid load, PCM path, block splitting for large FFT, and callback allocation boundary.
- `Normalise::yes` intentionally replaced: this **changes loaded IR output level** by design, but does not change frequency response. This is a sonic contract change; real-guitar DI level matching and owner sonic acceptance are mandatory before integration.
- No compressor, effects, stereo amp, solver or quality factor modified as part of the IR operation.

## Tests and adequacy

New `tests/ir_presets/matched_ir_oracle.h`; changes in `ir_presets_test.cpp`, `compact_stereo_regressions.h`, `large_ir_heap.cpp` and `CMakeLists.txt`:
- Existing independent JUCE reference audio now decodes a separate IR copy, applies the intended K-weighted gain and compares actual PCM (including stereo and sample-rate conversion); previous raw `Normalise::yes` reference would now be invalid.
- New metamorphic property: quarter-amplitude IR must get 4× compensating gain; anti-phase stereo channels must share the same gain; empty/silent impulse must not blow up.
- New JUCE editor tree test checks exactly 8 live panels and exercises real ECO/HIGH, MONO/STEREO and LOW/HIGH-CUT toggle attachments via mouse-command/button events, then verifies APVTS parameter state.
- Existing invalid/truncated/NaN IR, atomic scene, preset/save/restore, concurrent load/audio, 16k-frame block and callback C/process-heap tests are retained and must run on the exact new binary.

| Contract | Evidence before candidate | New candidate status |
|---|---|---|
| 47/47 Steinberg, 40/40 125A Tester, VST3 host | Prior Windows source `8a442d9` (not this candidate) | **NOT RUN** |
| Current editor bindings / removing dead panels | Source mismatch previously confirmed | **Windows native editor/control test PASS 2/2 including IR integration** |
| IR loudness ≈ built-in cab, stereo ratio/polarity, anti-phase | Upstream v1 methodology; candidate code/tests | **IMPLEMENTED; listening and Windows integration NOT RUN** |
| IR PCM across 44.1/48/96k, mono/stereo, variable blocks | Prior reference expected Normalise::yes | **Windows targeted native IR integration PASS** |
| IR invalid files / rollback / long-IR callback heap | Historical Windows PASS | **Native integration PASS; long-IR callback heap PASS in run #40424003598** |
| Project recall / automation / GUI open-close-reopen | Historical Windows functional PASS on previous source | **NEW CANDIDATE NOT RUN** |
| ECO/STEREO in Studio One on owner's i9-11900K | Owner synthetic host has overruns | **ASIO NOT RUN – realtime NOT APPROVED** |
| Branding / metadata / master logo | Work deliberately out of scope | **NOT DONE** |

## Verifizierter Windows-QA-Status des Kandidaten

**Die gezielte Windows-QA ist erfolgreich beendet.** Die beiden Windows-Actions-Läufe nutzen für `Source/`, `tests/` und `CMakeLists.txt` exakt denselben geprüften Quellstand; alle Zwischencommits danach änderten nur die GitHub-Workflow-Datei. Die Identitätsprüfung im zweiten und dritten Lauf war PASS.

- **Native echte JUCE-Integration und Editorbindungen:** [Windows Run #40422575513](https://github.com/challanger2000/125A-Dystorator/actions/runs/40422575513), Code-Commit `b4856bdf6aab67d794284b39fd45ccfb5578df71`: **2/2 CTests PASS**. `ir_presets_integration` 189,58 s, `compact_editor_bindings` 0,21 s. Der Gesamtlauf war FAIL nur wegen einer nachgeschalteten CMake-Argument-Splittung des Großblock-Tests.
- **Erster Reparaturlauf:** [#40423553877](https://github.com/challanger2000/125A-Dystorator/actions/runs/40423553877) CMake-Großblock-Konfiguration PASS, Compile **FAIL** (`detours.h` nicht gefunden); CMake hatte beim eingeschränkten Großblock-Test das `step3_detours` Ziel nicht erzeugt. **Keine ausführbare Heap-Messung**, kein Produktfehler nachgewiesen.
- **Abschließender gezielter Lauf:** [#40424003598](https://github.com/challanger2000/125A-Dystorator/actions/runs/40424003598), Code unverändert, Workflow-Commit `c00d2618e7ade4a93e9ba1aca4e80f742841b611`: **SUCCESS**. Positiver Test für bisherige native Evidenz/Quellidentität PASS, **1/1 `large_ir_process_heap` PASS** (Windows x64 Release/Detours; einschließlich 128/256/8192/8193/16384 und Mono/Stereo in der Testmatrix), **Windows-x64-Release-VST3 erfolgreich gebaut** und SHA256 erfasst. Actions-Artefakt `dystorator-compact-ir-windows-c00d2618e7ade4a93e9ba1aca4e80f742841b611` (GitHub-Actions-Retention 14 Tage). Der korrekte Detours-Target-Pin `IR_BUILD_VST3_HOST=ON` ist nur eine Workflow-Konfigurationsänderung.

Diese grünen gezielten Tests ersetzen **nicht** den vollständigen 125A/Steinberg Validator, die Prüfung jedes realen GUI-Bedienelements im Host, vollständige Preset-Migrationsabnahme, Eigentümer-i9-ASIO-ECO-Stereo, DI-Hörvergleich oder finale umbenannte/brandete VST3-ID. **PR #3 bleibt Draft, NO MERGE / NO FINAL.**

## Required next gate

1. Run the native `ir_presets_test` integration and `compact_editor_bindings` on Windows Release; run matrix `large_ir_heap` with genuine Detours heap observer.
2. Verify actual packaged VST3: validator, 125A Tester, processor tests, editor open/close/scale, parameter edits, project save/recall/automation and preset IR path/binary.
3. Compare loaded IR vs built-in on actual stereo guitar riffs and leads at matched settings; measure BS.1770-weighted loudness, relative gains, spectra, polarity/phase, l/r separation, and transitions. Verify new candidate is preferable; no arbitrary louder-is-better conclusion.
4. Rerun CPU ECO/STEREO only if actual solver, processor or convolver callback workload changes; note extra control-side IR preparation cost separately.
5. Keep current `44a1845` dev branch and active `perf/owner-i9-cpu-evidence-20261026` diagnostic PR unmodified. **No merge, no v1.0, no final release** until full source/binary/DAW/sonic gates pass.
