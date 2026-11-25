# P001 — Isolierte 1.0.0-Vorstufen-Parität

Stand 2026-11-25; main vor diesem Test-/Dokumentationscommit: b35f3e0a8014bd4aedc6be9e11f981ff25a6034e. Engineering-Commit 53b10bd4a81fe6c5d62e110b932aef7155172ad6 unverändert; START-HERE vollständig erneut gelesen und kompletter Lesebeleg gültig. Originalquelle siehe P001-DJENTY-1.0.0-REFERENZMESSUNG.md. Keine Produktcodeänderung.

## Quellvergleich

- Preamp5150.h: Original enthält optionalen runAhead-Testhook (standardmäßig aus) und setzt Spannungen aller vier Schaltungspfade je Sample. Dystorator entfernt den Hook, speichert die aktuellen V1/V2-Werte und übergibt sie erst beim tatsächlichen runPath-Aufruf. Schaltungsdefinition, Gewichtung und Kanalübergang unverändert. Der genaue aktuelle Header wurde aus Original und vollständig gelesenen Änderungen lokal rekonstruiert: normalisierte Git-Blob-SHA1 **2f85f7373676fce94a57707c57bed85e73d0d9c7** (21.084 Bytes), stimmt exakt mit GitHub überein.
- PotTapers.h: gleiche Vorwärtskennlinien/Parameter; lediglich taperAInverse und algorithm-Include entfernt. Kein Grund für neue Klangkennlinie. Abhängigkeits-/Aufruferprüfung noch offen.
- Simd.h: explizit erzwingbare Scalar/SSE2-Testpfade, MSVC AVX noinline und AVX helper Attribute; CPUID/OSXSAVE-Prüfung beibehalten. Komplette DKCircuit-/PowerAmp-Nutzung und Backend-Parität noch offen.
- CabMatch.h: unveränderte Original-Mono-Funktion und zusätzliche gemeinsame Stereo-Gain-Funktion mit Mittelwert der gewichteten L/R-Leistung. Wahrung der Polarität/Amplitude plausibel, aber Grenzfälle, FFT-Skalierung nach Sample-Rate und tatsächliche JUCE-IR-Verarbeitung noch separat zu prüfen.
- AmpSim.h/AmpModes.h/CMakeLists.txt vollständig gelesen: 9 vorbereitete Amp-Kerne (3 Quality x 3 Lanes), davon regulär 1 oder 2 aktiv, während Umschaltung mehrere. Pedale weiterhin vorhanden. Standalone und externe ASIO-SDK-Abhängigkeit noch im Build. Diese sind entgegen vereinbartem Umfang in P002 gezielt zu entfernen; gekoppelte Schaltung und Stereo erhalten. Quality-Umschaltung beendet alten Pfad erst am Blockende, daher Partitionierungs-/CPU-Stressprüfung notwendig. Noch keine Änderung.

## Ausgeführte isolierte Prüfung

Headless GCC 14.2.0 Linux x86_64, -std=c++17 -O3 -DNDEBUG; zwei unabhängig kompilierte Varianten desselben Testprogramms tests/p001/rail_parity.cpp:
1. unveränderte 1.0.0 Originalquelle
2. 1.0.0, ausschließlich Preamp5150.h ersetzt durch den SHA-verifizierten aktuellen Dystorator-Header (alle anderen Dateien Original).
3. Negative Kontrolle: Variante 2, nur testlokal V2-Aktualisierung in setRails entfernt, keine Produktänderung.

Pro Variante 88.2/176.4/384 kHz interne Rate, 0.75 s gemeinsamer DC settle nach prepare; 0.22 s pro Rate. STOCK; Red/Blue/Clean, Crunch-Wechsel mitten in Fade, Rückwechsel, Gain/Tone/Master, Bright, Stille am Ende. Sechs double-Werte je Sample: Speaker-Volt, Vorstufenausgang, V1/V2, beide Vorstufenströme. Eigenständiger Python/NumPy-Vergleich der kompletten binären Ausgaben, Release-aktive finite- und I/O-Checks.

**PASS — ausschließlich isolierter Header-Austausch:** 142.692 Records / 856.152 double-Werte (6.849.216 Bytes) bitidentisch, beide SHA256 7cc7c8d0ca7b87bb9d9c758744676ce3d9322c48a6ae879282dfcead4ce5852b; Speaker-Peak 58.0843237253 V. Negative Kontrolle finite, **142.692 von 142.692 Speaker-Samples unterschiedlich**, max. Delta 4.8834313282 V; SHA256 5a407d5a07a202ef7e1436fc58dd7950c10e97386dee84131d6e0c290fd8e18e. Die Gegenprobe erkennt die fehlerhafte Spannungsversorgung anhand realer modellierter Ausgangswerte, nicht nur eines State-Flags.

Reproduktion: unverändertes Originalarchiv aus P001 prüfen/extrahieren; separat kopieren und ausschließlich Preamp5150.h aus obigem main-SHA einsetzen (LF). Je Variante `g++ -std=c++17 -O3 -DNDEBUG -I <Variante>/Source tests/p001/rail_parity.cpp -o rail-parity`; anschließend `./rail-parity <Datei>` und beide Binärdateien mit cmp vergleichen. Negative Variante: ausschließlich `railNow1 = v1; railNow2 = v2;` durch `railNow1 = v1;` ersetzen. Beide positiven Dateien müssen gleich sein; Negativdatei muss bei Speaker-Samples abweichen. Testquelle Git-Blob fbea9934b6045a5eba01083f4a062d80f720cbf4. Nicht als regulären CTest registriert: Originalquelle muss unabhängig bereitgestellt werden.

## Nächster Schritt

PowerAmp5150 und DKCircuit vollständig gegen 1.0.0 vergleichen, gezielt isolierte und kombinierte PCM-Parität mit alter/neuer Solver-/SIMD-Pipeline ausführen. Processor/Editor/APVTS/Tests vollständig untersuchen, anschließend P002 Pedal-/Tube-Set-Selector-/Standalone-Bereinigung in kohärentem Change. Keine Spekulation oder alte 0.5.6 als Oracle. Container-Netzzugriff zu GitHub derzeit DNS-Fehler; Connector-Lesezugriff verfügbar. Vorstufenheader konnte durch exakte Blob-Prüfung lokal reproduziert werden; noch kein vollständiger lokaler Checkout des aktuellen Dystorator.

**NOT RUN:** vollständiger aktueller Dystorator gegen 1.0.0, echte Gitarren-DI, CPU-Vergleich, vollständiger Windows-VST3, native ASIO/DAW und unabhängiger Audit. Kein Release, keine CI ausgelöst.
