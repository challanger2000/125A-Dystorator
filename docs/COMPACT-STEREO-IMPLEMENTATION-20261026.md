# 125A High-Gain Amp – Compact Stereo Implementation Status

**Date:** 2026-10-26. This document records the historical WIP at
`30988fcae3785e1efb5e491277948416d1c97935`.
Current continuation, fixes, executable evidence and remaining gates:
[Stereo completion QA](STEREO-COMPLETION-QA-20261026.md).

**Historical status at that source commit: WIP / NOT JUCE-VERIFIED / NOT WINDOWS-VERIFIED / NOT RELEASE-READY.**

## Source provenance
- Repair baseline: fix/ir-presets-block1-20261026 at 1009fa6906746f46516107baf059bf12d473a04f (B01/A04/B03/B04).
- Independent audit: docs/CODEX-INDEPENDENT-AUDIT-20261026.md on repair branch.
- New development: dev/compact-stereo-on-ir-fix-20261026. Main and repair branch are not merged or changed.
- Original nonlinear 5150/6505 model, PowerAmp5150, Preamp5150, AmpSim and oversampling source files unchanged. Their native regression claims still apply ONLY to the old baseline until this new full path is actually exercised.

## Implemented in source, NOT accepted as working VST3
1. Two independent AmpSim instances with isolated mutable circuit and oversampling state, one shared set of controls. 2-input bus processes both L/R; 1-input bus runs one AmpSim and duplicates only before cabinet. No deliberate L+R summing except requested mono output downmix.
2. Two independent input gates, input/output gain, stereo peak meter. Removed the former compressor, pitch shifting, 3-band loop gate, graphic EQ, phaser, delay and reverb from the **active** process chain. Their legacy parameters and source headers remain for host-state compatibility; the old GUI still displays them and will not be a shipping interface. Legacy presets that used deleted FX are **not sonically backwards-compatible**.
3. CabinetIR was extended from the repaired asynchronous snapshot/thread-owned engine to decode one or two IR channels, preserve stereo channel polarity/content and process a two-channel JUCE convolution with a single atomic scene/fade. Built-in 5-filter cabinet has independent L/R state.
4. Stereo-safe 2-pole Butterworth Low Cut (20–300 Hz) and High Cut (3–20 kHz) with per-frame coefficient/bypass smoothing after cabinet; new parameter IDs lowCutOn/lowCutHz/highCutOn/highCutHz, both cuts OFF by default to avoid silently altering old sessions. Source/dsp/GuitarCuts.h is separately unit tested.
5. Retained all 65 original parameter IDs; new compact product has 69 IDs during migration. Not a final host-compatibility guarantee. Original CMake plugin identity is intentionally unchanged *during development* and must be assigned a distinct 125A identity at packaging, with migration consequences documented.
6. Host tail reports conservative 8 s while the removed feedback/reverb chain is inactive; true remaining convolution tail/host-truncation contract still needs a measured QA gate.

## Verification today
- Standalone native C++17 GuitarCuts test: **PASS (0 failures)** on 44.1/48/96-kHz frequencies, bypass, L/R separation, antiphase, fast parameter changes. Its compiled binary was rerun after the new source was adopted by exact blob SHA; this does not constitute a JUCE build.
- Added unexecuted integration assertions to tests/ir_presets/ir_presets_test.cpp: actual two-input buses, L-only/R-only audible path with independent silent control, absence of cross-bleed, 2-channel opposite-sign WAV impulse, and actual post-cab cut PCM reference captured from the real processor.
- The previous repair branch's 67/67 or 68/68 results were on **different source**: do not transfer PASS to this development HEAD.
- New branch JUCE build, stereo/IR integration tests, sanitizer, ASan/UBSan, Windows VST3, official validator, Studio One, GUI control mapping, CPU/deadline/paced audio and listening tests: **NOT RUN**. No GitHub Actions triggered.

## Immediate verification required BEFORE GUI or CPU optimization
1. On this exact branch HEAD, build and run tests/ir_presets with JUCE 8.0.4; run CTest with and without ownership trace, then full ThreadSanitizer. Record test count, exact compile/runner logs and code revision. Check the new stereo tests and the previous B01/A04/B03/B04 negatives; tests must fail on a deliberately discarded-right-input mutation.
2. Run the native guitar_cuts_test via root CMake/CTest; verify original 74-case tone_regress and 19 fault cases are still green, but separately test the new processor.
3. If compile or test fails, fix root cause on THIS dev branch; do not change stable repair/main branches and do not mask failures or loosen thresholds without analysis. Inspect real JUCE 8.0.4 API before patching.
4. Add true stereo-to-mono and mono-to-stereo, 44.1/48/96k and 32/64/128/256/512 buffers, IR rapid load/clear and invalid dual-channel files, state save/reload, bypass, automation and editor lifecycle.
5. One coherent Windows x64 VST3 build after locally green headless tests; official validator and Studio One audio R-only/L-only/antiphase tests, real CPU mean/p95/p99/max and deadlines for 1/2 amp chains.
6. Only after these gates: compact 125A Systems GUI design and implementation from authoritative 125A-Branding/Knob-Designer assets. Remove deprecated active UI controls and eventually dead FX code only after host-state migration plan and dependency check.

## Important architecture caveats and product decisions
- Stereo IR means L and R channel responses are preserved. Mono IR used on both lanes. Need empirical JUCE engine test to certify that mapping.
- Amp noise generators are independent; equality tests need noise OFF.
- Two complete oversampled nonlinear circuits may be very expensive; CPU must be measured, not guessed. Never silently reduce original 8x oversampling or solver quality without audio A/B, alias and RT gates.
- New Cab safety max 64 MiB and source duration around 5.46 seconds (cross-rate budget) preserved. Absolute IR path portability remains unresolved.
- Original DJENTY name and AGPLv3 remain in repository. A commercial 125A name does not remove AGPLv3 source/distribution obligations. No release until identity/license fully evaluated.
- The original GUI still contains obsolete controls. This branch is only a DSP-/processor WIP, not a user-facing compact GUI or a stable build.

**Mandatory delivery when follow-up work finishes:** Push to this actual GitHub branch, check remote HEAD and files, cite exact full commit SHA, include PASS/FAIL/NOT RUN evidence and do not call it finished if merely committed locally. No merge.
