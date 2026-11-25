# P001 — Original Djenty Donkey 1.0.0 DSP-Referenz (25.11.2026)

## Provenienz

Dystorator main vor diesem Dokumentationscommit: c4bc612fd3761f90171a6d93bb597b7c2dcc2537. Engineering-Pflichtlektüre für den vereinbarten Umfang vollständig dokumentiert, keine Produktcodeänderung. Unveränderte Djenty Donkey 1.0.0 Quell- und Windows-Archive sowie Owner-Referenzpaket erneut SHA256- und ZIP-CRC-geprüft, alle Hashes wie docs/P001-REFERENZ-UND-PRODUKTUMFANG.md. Kein Rückgriff auf 0.5.6 als DSP-Oracle.

## Ausgeführte Original-Tests

Privater Linux x86_64 Container, Intel Xeon Platinum 8573C, GCC 14.2.0. Unverändertes Source-ZIP in temporären Arbeitsordner extrahiert, C++17 entsprechend upstream CMake, **kein JUCE/Windows-Plugin-Build**. Befehle:

```sh
g++ -std=c++17 -O3 -DNDEBUG -I Source tests/dsp_test.cpp -o dsp_test
./dsp_test fault
g++ -std=c++17 -O3 -DNDEBUG -I Source tests/golden.cpp -o golden
./golden write djenty-donkey-1.0.0-golden.bin
```

- Compile DSP-Test: exit 0.
- Original fault suite: exit 0, 19/19 ausgegebene Fälle PASS (NaN/Inf an Red/Blue preamp/power amp, erzwungene Newton-Fehler, retry/hold, korrupte Lösung, mehrmsige Fehler, 1 kV spike und 100 V sine). Clean-Referenz Red peak 54.3 V; extremer spike 54.3 V, 100 V sine 58.7 V, jeweils 0 gemeldete Newton failures. Mehrmsige Fehlerszenarien zeigen längere Abweichungen (28.78/64.03/83.07 ms); dies entspricht den eigenen upstream-Kriterien, **nicht** einem universellen <1-ms-Recovery-PASS. Test verwendet synthetische Eingangsdaten und eigene upstream-Orakel. Der Exitcode deckt die fault-Funktion ab; andere Testmodi wurden nicht ausgeführt.
- dsp_test ausführbare Linux-Datei SHA256 229835dfa19ae2f9b5f947f88d41ba302eab201150d2ee9f0fee0e0074ec10eb; fault.log SHA256 8a53352932b3eed64628bc9a4385d6e5deb4442bebcd01ac3b002522d11e5591.
- Golden-Generator: kompiliert, Originalprogramm meldete 19 gerenderte Szenen. Dateiformat unabhängig mit Python/NumPy überprüft: genau 19 Szenen, 7.920.000 float64 Samples, alle endlich und je Szene mindestens ein Nichtnullsample, exakte erwartete Szenenlängen, kein Rest am Dateiende. Golden-Datei 63.360.152 Bytes, SHA256 15f23215112c2a3f9a3a4a405d43230e48d169ddf6b3d0a08fda0dc79c287ad2. **Kein erneuter golden check durchgeführt.** Der Toolaufruf hatte ein Zeitlimit erreicht; Datei und Abschlussmeldung wurden danach direkt verifiziert. Keine Behauptung eines verifizierten Prozess-Exitcodes für golden write.
- Die 19 Szenen umfassen 10 Amp-Konfigurationen/Automation, 3 Kompressor-, 2 einzelne Pedal- und 4 Transpose-Szenen. Amp/Drive 384 kHz interner Rate, Kompressor 96 kHz, Transpose 48 kHz; **kein fertiger VST3-Stereo-/Cabinet-/Oversampling-Host-Render**. Nicht alle Szenen sind im späteren Dystorator erforderlich. Die erhaltenen Szenen der entfernten Module dienen separat als historische Originalreferenz.

## Referenzpaket

Im Chat/Arbeitsdatei erstellt: **125A-Dystorator-P001-DSP-Referenz.zip**, 58.717.731 Bytes, SHA256 018a23196671ca60e9af9f050e13024bc4f4580fa4b23aa5bcc7fd0fe3b7c46e. 33 ZIP-Einträge, CRC PASS; Moduldateien und Golden-Datei unabhängig gegen Manifest und Originalbytes nachgeprüft. Enthält Original-Quellarchiv, die exakten 1.0.0 PrecisionDrive/TS9/1176-Modulquellen samt direkt benötigter DSP-Header, Schaltungsnotizen, SPICE-Netzlisten, Referenztests und Original-Notices; zusätzlich Golden-Binärdaten und SHA256/PCM-Messmanifest. Kein eigenständiger Pedal-/1176-VST3 gebaut. Das Paket ist **nicht auf GitHub hochgeladen**; aus Chat/Library bzw. Originalarchiv abrufen und vor Nutzung Hash prüfen. Originalsource-README bezeichnet GNU AGPLv3; im gelieferten Source-ZIP fehlt jedoch die dort verlinkte LICENSE-Datei. Dystorator-Repo hat LICENSE (Blob 0ad25db4bd1d86c452db3f9602ccdbe172438f52); vor Veröffentlichung vollständige korrekte Lizenz/Attribution und abhängige Komponenten prüfen.

## Nächste Arbeit (P001/P002)

Vergleiche die neun abweichenden DSP-Header gegen die exakte 1.0.0, insbesondere PowerAmp5150/DKCircuit/Preamp5150/PotTapers/Simd; keine unkritische Übernahme alter Optimierungen. Pinne alle tatsächlich benutzten Originaldateien und erzeuge geeignete unabhängige vergleichbare Amp-only-Referenztests vor Produktcodeänderungen. Vorhandenen Dystorator-CMake, Parameter, Processor, DSP und relevante Tests prüfen, inklusive Stereo/Recall/IR-Verträge. P002 entfernt ungewollte Drive-/sonstige Pfade; keine Änderung der originalen Schaltung ohne quantitative Begründung. Die alten in tests/baseline und tests/step3 vorhandenen Quellen bleiben historische Gegenproben und sind nicht die maßgebliche 1.0.0 Referenz.

Windows-/VST3-/DAW-/echte Gitarren-DI-/RT-/ASIO-/unabhängige Vergleichs- und Audit-Nachweise **NOT RUN**. Kein FINAL/RELEASE, keine CI ausgelöst.
