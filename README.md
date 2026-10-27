# 125A Dystorator

**Windows x64 VST3 / standalone – development candidate 0.1.0.**
**Manufacturer:** 125A Systems. **Status:** Unreleased; not a FINAL or V1 package.

Compact high-gain guitar amplifier simulator with two independent nonlinear
left/right amp signal paths for stereo virtual guitars. The component-level
tube model and drive simulations originate from the AGPLv3 open-source
amp simulator developed by SteveAsAConstant (see Attribution and License).
The 125A variant adds its own stereo amp routing, selectable processing quality,
independent stereo cabinet IR handling and post-cabinet low/high cut filters.

## Active signal path

Input gain → input gate → Precision Drive or TS9-style boost → selected
amp circuit (CLEAN / BLUE / RED; stereo paths when selected) →
builtin five-filter cabinet or loaded mono/stereo impulse response →
low-cut and high-cut → output gain.

The editor and VST3 host parameter list contain only live controls.
Unused transpose, compressor, loop gate, 10-band EQ, phaser, delay and reverb
parameters have been removed from this **new, unreleased VST3 identity**.
Earlier pre-release 125A state/preset XML may still be loaded: recognized
obsolete control entries are discarded, while all currently supported
parameters, cabinet IR and knob-curve upgrades remain intact.

Built-in 125A factory presets are Default, Tight Rhythm, Modern Rhythm,
Lead Drive, Classic Crunch and Open Clean. None promise inactive effects,
such as pitch shifting, delay or reverb.

## Quality and CPU

Select ECO for lower-CPU realtime tracking; STANDARD is an intermediate mode.
Select HIGH manually before offline bounce. Automatic HIGH switching in offline
host mode is **not implemented**. ECO/STEREO ASIO performance is not yet
qualified on the owner i9-11900K. The plugin may be CPU intensive; no minimum
CPU or maximal instance count is guaranteed from GitHub-hosted CPU tests.

The current IR-level matching follows an amp-spectrum-weighted,
K-weighted matching method inspired by the original v1.0.0 code,
adapted to preserve L/R polarity and level ratios with one common gain.
This changes loaded IR loudness compared to earlier unbranded test builds.
Real-guitar level-matched listening and acceptance are outstanding.

## Candidate install paths

- Windows VST3: `C:\Program Files\Common Files\VST3\125A Dystorator.vst3`
- User presets: `Documents\125A Systems\Dystorator\Presets`
- Preset XML root: `DystoratorPreset`
- Optional standalone: `125A Dystorator.exe`

The product has a **new VST3 identity and preset namespace**, separate from
earlier unbranded experimental builds. Do not assume projects or presets from
those builds migrate automatically. Parameter IDs remain stable within this
125A development line, but old plugin-class IDs are not used by the new product.

## Development

Requirements: Windows 10/11 x64, Visual Studio 2022 Build Tools with desktop
C++, CMake 3.22+, JUCE 8.0.4. Source build:

```bat
build.bat
```

Output: `%USERPROFILE%\vab-build\Dystorator_artefacts\Release\`.

Build/test verification is pinned to exact source commits and Windows VST3
artifacts; inspect `docs/V1-UPSTREAM-COMPACT-IR-REPAIR-20261027.md` and the
GitHub Actions workflow reports. A successful build or validator does not
imply Studio One realtime qualification or final release approval.

## Attribution and license

Copyright in original contributions remains with their respective authors.
The amplifier circuit, solver, drive and some cabinet-level matching code
are derived from the open-source project
[SteveAsAConstant/djenty-donkey](https://github.com/SteveAsAConstant/djenty-donkey)
and are adapted here under the **GNU AGPLv3**. Preserve original notices and
make corresponding source available. See `LICENSE` and
`installer/THIRD-PARTY-NOTICES.txt`, including the licenses applicable to
JUCE and the Steinberg SDK.

Names of amplifier/pedal designs and manufacturers are used only to describe
circuit inspirations; no endorsement or affiliation is implied.

The official 125A master logo geometry is held in
`challanger2000/125A-Branding/125A_Logo_Master_FINAL.svg`. The final 125A
hardware appearance, master-logo embedding, end-user screenshots and release
branding require a separate visual review.
