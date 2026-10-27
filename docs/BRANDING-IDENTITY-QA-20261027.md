# 125A Dystorator — isolated product identity / branding QA

Date: 2026-10-27 (Europe/Berlin). Product status **UNRELEASED DEV CANDIDATE 0.1.0**, not V1 FULL/FINAL. Branch `branding/dystorator-identity-20261027`; based on compact IR/GUI candidate draft PR #3 (product source at `1016a2980ec94aafcab22927436cf171a170405b`). Do not merge to main or publish by default.

## User-approved product identity

| Field | Candidate |
|---|---|
| Display / VST3 product | **125A Dystorator** |
| Company / VST3 vendor | **125A Systems** |
| JUCE CMake target | `Dystorator` |
| JUCE manufacturer / plugin four-char code | `A125` / `Dyst` |
| Version | `0.1.0` unreleased development |
| Windows bundled VST3 | `125A Dystorator.vst3` |
| Standalone | `125A Dystorator.exe` |
| Installer | `125A-Dystorator-Setup-0.1.0.exe` with distinct Inno AppId |
| User presets | `Documents/125A Systems/Dystorator/Presets` |
| User preset XML | `DystoratorPreset` |
| License | GNU AGPLv3, original authorship/source notice preserved |

Original factory / preset and host parameter IDs remain unchanged. A deliberately new VST3 class identity and XML root **do not automatically migrate** host projects or preset XML created under an earlier experimental identity. The original experimental fork was never released as a 125A product. A conversion tool requires separately verified scope and should not re-embed original branding strings in the shipped DLL without approval.

## What is and is not removed

The old external plugin brand was replaced in shipped CMake product metadata, installer, VST3/EXE path, installer/package/build scripts, user preset path/tag, and standalone app class. Superseded original-named installer files removed. The source README describes the actual compact signal chain, not inactive removed effects.

Source/product text audit on 27 October: **40** active files (all `Source/`, current `installer/`, top-level CMake, build.bat, README) examined for legacy name strings. Only legal attribution in README and third-party notice remains; no branding string in active DSP/CMake/installer script/GUI.

**Legal exception is intentional and mandatory:** Original developer and AGPLv3 provenance may not be misrepresented or erased. The shipped notice uses the 125A Dystorator heading but explicitly attributes the original project. Historical engineering evidence, test logs and prior GitHub branches are not retroactively rewritten.

The 125A-branding master remains `challanger2000/125A-Branding/125A_Logo_Master_FINAL.svg`. Its exact geometry is binding, and is **NOT YET embedded in this functional JUCE editor**. The final hardware-look GUI is a separate design and visual acceptance stage. Do not claim this branding candidate already contains the authoritative logo.

## Required actual proof

1. Rebuild Windows x64 Release VST3 from this independent identity; verify moduleinfo/vendor, plugin/class UID distinct, binary SHA, zero unwanted original product strings, product passes loading and editor create/destroy.
2. Native preset-XML negative/rollback/host roundtrip, stereo-cab PCM and active-control GUI tests. Existing audio DSP, large-block heap and CPU hot path should remain identical; compare source-tree changes.
3. Independent Steinberg Validator and 125A Tester **on the new module**, not inherited from the pre-rebrand candidate.
4. Package/installer must be built and tested on Windows; user-visible installer/start menu and uninstall/manual routes checked. Until then packaging is source-only.
5. Real owner i9 / Studio One / ASIO ECO-STEREO performance and representative guitar DI listening for new IR loudness are required before V1; synthetic CI and plugin QA cannot replace either.

## Evidence

- Earlier compact/UI/IR actual Windows build #40424003598: SUCCESS; independent full plugin tester #40425110420: v0.2.9 **40/40 PASS, 0 warnings/errors**, Steinberg **47/47 PASS** (on the original test identity only, NOT the newly rebranded DLL).
- Branded Windows run #40426906480 on product source `fc62d4a940cf4319d39e28ee722085ab068d7c27`: Native GUI/IR integration **PASS** and Windows x64 Release VST3/binary legacy-string scan **PASS** by live job steps. Independent tester stage **IN PROGRESS / NOT VERIFIED** at the time of this report; inspect official result before recording it as PASS.
- Changes after `fc62d4` are limited to README/legal attribution and this documentation, not CMake, Source or tests; earlier binaries stay tied to the exact source commit.

**QA release conclusion:** BRAND IDENTITY IMPLEMENTED; FINAL NOT APPROVED. Missing owner listening, host-ASIO realtime, master logo GUI, installed Windows package, final independent whole-release audit.
