# P001 — Verbindliche Entwicklungsgrundlage (25.11.2026)

## Referenzen
DSP-Referenz ausschließlich Djenty Donkey 1.0.0 (nicht 0.5.6); unveränderte Originale:
- DjentyDonkey-source-1.0.0.zip, SHA256 5913d40d1680ab02473813e05420df620bfbeb70daadd51d6f9fb7f44a9b0887
- DjentyDonkey-1.0.0-Windows.zip, SHA256 9a04a89a7b736b07022373e4b1158b88fdee856bfbd301154a16b68f3020098a
- Dystorator-approved-reference.png, SHA256 ece0b4efe9cb4dd7380c95fd3b296b30f05383f4cef9b1235e5d963c7432c97d
- Chat-Referenzpaket 125A-Dystorator-P001-Referenzen.zip, SHA256 12fe74fb5e668691a23a0c1703d7a9139cff4f202b7f40109a55113911deddfd.
ZIP-CRC-Prüfung bestanden. Original-Binärarchive noch nicht in GitHub gespeichert.

Bisheriger Dystorator: a0138e3c5807bb513c38eebd29faadaa32fcd479. Headervergleich mit 1.0.0 nach CRLF/LF-Normalisierung: 13 identisch (Amp5150, FFT, Filters, FullPreamp5150, LinearNetwork, MultibandGate, PortSolve, PrecisionDrive, Supply5150, TriodeNetwork, TriodeStage, TubeNoise, TubeSet); 9 abweichend (AmpSim, CabMatch, DKCircuit, PitchShifter, PotTapers, PowerAmp5150, Preamp5150, Simd, StereoDelay); 3 fehlen (Comp1176, SemiNetwork, Urei1176). AmpModes und GuitarCuts sind 125A-spezifisch. Abweichungen einzeln fachlich prüfen, keine Übernahme allein aufgrund der Historie.

## Verbindliche Owner-Entscheidungen
Eigenständiges 125A-Produkt, kein GitHub-Fork. C++/JUCE, Windows x64 VST3, AGPLv3 und Original-Attribution erhalten. Komponentenbasierte 5150/6505-Emulation (kein NAM). Echtes unabhängiges Stereo als Default, Mono optional. Input/Output Gain und Meter. Noise Gate On/Off, Threshold, Release; normales Gate gegen input-keyed 3-Band-Gate prüfen. Clean/Blue/Red, Bright/Crunch, Gain, Bass/Middle/Treble, Master/Presence/Resonance. Nur STOCK-Röhrensatz. Röhrenrauschen optional, default OFF. Internes Cabinet, eigener IR-Loader, Cabinet-Bypass, Low/High Cut einzeln abschaltbar. ECO/STANDARD/HIGH; optional automatische HIGH-Qualität bei Offline-Render, korrekte Latenz. Eigene Werkspresets, keine Djenty-Presetkompatibilität nötig. Vollständiger DAW-Recall, Automation, Mehrfachinstanzen, stufenlose GUI-Skalierung. Ausschließlich hochgeladene metallische GUI-Vorlage, DRIVE-Sektion entfällt. Keine alternative Optik. Kein Precision Drive, TS9, 1176, Transpose, EQ, Phaser, Delay, Reverb, weitere Röhrensätze, NAM-Loader oder Standalone im Dystorator. Pedal/1176-Module für separate spätere Projekte sichern.

## Vorgehen
P001 Referenzen sichern und Unterschiede prüfen; P002 Funktionsumfang bereinigen; P003 Schaltung messen und optimieren, Klang gegen 1.0.0 bewahren, Stereo-Realtime-Deadline nachweisen; P004 Owner-GUI; P005 vollständiger Audit und Windows-QA, Studio-Pro-Praxistest zuletzt. Engineering-Pflichtlektüre vor Implementierung vollständig lesen. Keine Freigabe ohne tatsächliche Nachweise. Dokumentation allein ist keine Implementierung oder QA. Keine unnötigen CI-Builds.
