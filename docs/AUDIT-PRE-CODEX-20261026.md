# 125A-Dystorator – PRE-CODEX BASELINE AUDIT
**Datum:** 2026-10-26 (Europe/Berlin)  
**Status:** Bestandsaufnahme, keine Plugin-Änderungen; **kein FINAL-/Windows-/Studio-One-PASS**.

## 1. Identität und reproduzierbare Grundlage

- Ziel: `challanger2000/125A-Dystorator` (`main`, Commit `81cc90dfd076f1510c02bf6ccc8318575dc4aa45`).
- Quelle: `SteveAsAConstant/djenty-donkey`, Commit `e363027d7a125018bb1cb56acc14958bc9baf9ff`, Version `0.5.6-beta`.
- Der importierte Git-Baum `f8d392bec9972bc70483fbe21bd4859e105c644f` entspricht **exakt** dem Git-Baum der Quelle (73 identische Blob-SHAs, einschließlich binärer PNG-Datei). Es handelt sich um eine unveränderte Upstream-Kopie, nicht um einen bereits optimierten 125A-Build.
- Plugin-Metadaten in `CMakeLists.txt`: weiterhin `Djenty Donkey`, JUCE 8.0.4, `VST3` + `Standalone`, AGPLv3; ursprüngliche Plugin-/Herstellercodes. Repository-Name `125A-Dystorator` ändert **nicht automatisch** den im Host sichtbaren Plugin-Namen oder die VST3-Identität.
- `main` bleibt durch diesen Audit unberührt. Audit-Dokument ausschließlich auf `audit/pre-codex-20261026`.

## 2. Was der Entwickler selbst dokumentiert

- Am Prüfzeitpunkt keine öffentlichen GitHub-Issues (`issues?state=all` lieferte `[]`). **Keine gemeldeten Issues bedeutet nicht fehlerfrei.**
- Neuester Upstream-Release: `v0.5.6-beta`, veröffentlicht 2026-10-26 um 00:03 deutscher Zeit.
- Folge tiefgreifender Reparaturen in v0.5.2–0.5.6: Presence-Kennlinie/Schaltung und Netzteil, Mitten/6L6-Ausgangswiderstand, Resonance-Taper und Bässe, Schirmgitterstrom und Newton-Solver-Fehlerbehandlung, abschließend Höhenverhalten der Lautsprecherlast.
- Die Anpassung des V30-Voice-Coil-Eddy-Current-Wertes von 15 auf 6 Ohm ist vom Entwickler **empirisch an Captures angepasst**, nicht durch direkte V30-Impedanzmessung verifiziert.
- Upstream dokumentiert explizit nicht modellierte passive Bauteiltoleranzen, Endstufen-Eigenrauschen und Netzbrummen sowie ausstehende Roadmap-Arbeiten (Eco-Mode, Preset-Browser).
- Die umfangreichen LTspice- und NAM-Vergleiche sind wertvolle Evidenz **des Entwicklers**; die zugrunde liegenden externen realen NAM-Captures sind nicht sämtlich Bestandteil dieses Repositories. Ihre Messbehauptungen sind hier nicht unabhängig bestätigt.

## 3. Priorisierte Befunde

### A01 – P0 | Stereoeingang wird stillschweigend auf links reduziert – BESTÄTIGT
`Source/PluginProcessor.cpp:45–51,207–212,287–292`

`isBusesLayoutSupported()` akzeptiert Stereo-Eingang und Stereo-Ausgang. `processChunk()` kopiert jedoch bei jedem gültigen Eingang **ausschließlich Kanal 0** nach `monoBuf` und verarbeitet nur diesen durch Gate, Pitch, Compressor, Amp, Cab und EQ. Danach wird Mono auf L/R dupliziert und erst anschließend mit Stereo-FX versehen. Rechter Eingang hat keinen Einfluss auf das Ergebnis. Kritischer Funktions-/Bus-Vertragsfehler für Stereo-Spuren. **Nicht** einfach zweiten Kanal addieren: das zerstört Kanaltrennung. Vor Entwurf echte Mono-/Stereo-Betriebsarten und CPU-Kosten vereinbaren; bei zwei unabhängigen Amp-Pfaden ist rund doppelte Kernlast grundsätzlich möglich.

### A02 – P1 | CPU-/Realtime-Kosten – ANWENDERBEOBACHTUNG + TEILMESSUNG
`Source/dsp/AmpSim.h:38–80,135–162`, `tests/dsp_test.cpp`

- Laut Anwender ca. **60 % Anzeige in Studio One** für eine Mono-Instanz; genaue ASIO-Puffer-/Treiberbedingungen nicht dokumentiert.
- Bei 44,1/48 kHz arbeitet der vollständige Amp-Kern intern mit **8× Oversampling** (352,8/384 kHz), inklusive rechenintensiver gekoppelter Röhren- und Netzteilsimulation.
- Unabhängig kompilierter `dsp_test cpu` unter Linux/GCC meldete bei internem **4×-Testpfad**, 48-kHz-Hostannahme und 64er Blöcken: RED-Preamp 38,58 % eines Kerns, Endstufe 27,71 %, Drive+RED+Power 76,69 % eines Kerns; höchste gemeldete Einzelblockspitze 384,3 %. Diese Zahlen sind **nicht** der Windows-VST3- oder 8×-Produktivwert; einzelne Peaks können Start-/Messartefakte enthalten und benötigen saubere paced Wiederholung. Sie zeigen dennoch klar die Hauptlast.
- Verpflichtender nächster Schritt: instrumentiertes Windows-VST3-End-to-End-Profiling mit CPU-Mittel/p95/p99/max, Pufferdeadline, Overruns, Warm-up, für 44,1/48/96 kHz, 32–1024 Frames, alle Kanäle, Boost, Cab, Gate, HQ-Transpose. Klanggenaue Frozen-Reference vor jeder DSP-Änderung.

### A03 – P1 | LIVE-Transpose-Latenz wird gegenüber Host nicht gemeldet – BESTÄTIGT
`Source/dsp/PitchShifter.h:353–376`; `Source/PluginProcessor.cpp:130–136`

`PitchShifter::getReportedLatency()` meldet für LIVE auch bei aktivem Pitch-Shift **0 Samples** (plus unabhängig davon die Amp-Latenz). Der mitgelieferte `dsp_test transpose` misst bei −2 Halbtönen einen Onset rund **6,2 ms später**, während für LIVE explizit `0 samples` berichtet werden. Der tatsächliche LIVE-Delay ist zustands-/signalabhängig, weshalb eine exakte feste PDC nicht trivial ist. PDC-Korrektheit, Dry/Wet-Alignment und Spur-Parallelität ausdrücklich spezifizieren; keine bloße Konstante ohne Messung eintragen. HQ meldet 2052 Samples bei 48 kHz, gemessener Onset ca. 46,9 ms; Bewertung nur mit geeigneter Delay-/Korrelationstestmethode.

### A04 – P1 | Presetwechsel kann alte IR beibehalten – BESTÄTIGTER CODEPFAD
`Source/PresetManager.h:95–118`; `Source/PluginProcessor.cpp:321–340,379–396`

`PresetManager::load()` lädt die IR bei User-Presets **nur, wenn eine vorhandene absolute Datei angegeben ist**. Andernfalls folgt kein `clearIR()`. Factory-Presets setzen zwar Parameter auf Defaults, rufen aber ebenfalls kein `clearIR()` auf. Resultat: Wer vorher IR A geladen hat, kann nach Preset B oder Factory-Preset klanglich weiter IR A hören. Die Host-Projekt-Restore-Funktion behandelt fehlenden Pfad dagegen ausdrücklich mit `clearIR()`. Mit einer reproduzierbaren echten JUCE/VST3-Integration testen und konsistent beheben, inklusive Rückkehr zur Built-in-Cab und fehlendem IR-Pfad.

### A05 – P1 | Release-Evidenz/Bauketten-Lücken – BESTÄTIGTE BESTANDSAUFNAHME
- Keine `.github/workflows` in den übernommenen 73 Quelldateien; Windows-CI, Steinberg Validator, Pluginval, Studio-One-Hosttest und installierter VST3 wurden **hier nicht ausgeführt**.
- `tests/amp_path_test.cpp` verlangt JUCE; für diesen Audit standen weder ein Windows-System noch ein produktiver JUCE-VST3-Host zur Verfügung.
- Der unabhängige `tone_regress` arbeitet auf nativen DSP-Modulen mit 192 kHz, während der aktuelle 44,1/48-kHz-Pluginpfad 8× arbeitet. Tests für genau den ausgelieferten 8×-VST3-End-to-End-Pfad fehlen als unabhängig geprüfte Evidenz.
- Ein grüner C++-Test ersetzt keine Windows-/VST3-QA.

### A06 – P2 | NaN bei 0-V-Schirmgitterspannung – REPRODUZIERT
`Source/dsp/PowerAmp5150.h:33–65`

`KorenPentode::screenTerms(0.0)` bildet `1.0 / 0.0`, und die anschließende Plattenstromberechnung erzeugt bei `Vg2k=0` NaN-Resultate (z. B. `Vpk=300, Vg1k=0`). In einem separat kompilierten C++-Repro traten Nicht-Endlichkeitswerte auf. **Nicht erwiesen**, dass dieser exakt-null-Zustand im normalen geschlossenen Endstufensolver erreichbar ist. Safeguard, physikalisch gültige Grenzfälle, Überspannung und bewusst erzwungene Nicht-Konvergenz gezielt testen; kein voreiliger „Studio One crasht“-Schluss.

### A07 – P2 | Großflächiges und schwer zu erweiterndes GUI – BESTÄTIGT
`Source/PluginEditor.cpp:318–320,336–389`

Editor initial 1480×840, Resize-Minimum 1200×700, maximale Größe 2400×1500. Vier gleichzeitige horizontale Modulreihen mit insgesamt 13 Sektionen, zum Teil sehr vielen Reglern. Ein echtes kompaktes, tab-/seitenbasiertes Layout fehlt. Das aktuelle Layout ist bei 1920×1080 sowie Windows-/Host-DPI und Zoom separat auf Lesbarkeit zu testen. Professionelle visuelle Gestaltung **erst nach vollständiger Parameter- und Signalflussinventur**, unter Verwendung der Quellen `125A-Branding` und `125A-Knob-Designer`, ohne DSP-/Parameter-IDs zu verändern. Noch **keine** GUI-Implementierung freigegeben.

### A08 – P2 | Pedal-Doppelberechnung als messbare Optimierungsoption
`Source/dsp/AmpSim.h:150–156`

Bei aktiviertem Boost werden Precision Drive **und** TS9 **bei jedem oversampelten Sample berechnet**, auch wenn die Mischung dauerhaft auf 0 oder 1 steht. Das erhält beide Zustände für klickfreie Umschaltung, verursacht aber Mehrarbeit. Nur nach Profiling optimieren; bei Einsparungen Kanalumschaltung, Crossfade und Zustandsfortschreibung nachweisen. Ohne A/B-/Timingnachweis **keine Freigabe**.

### A09 – P2 | Hohe Initialisierungskosten möglich – CODEBESTAND, DAUER NICHT GEMESSEN
`Source/dsp/AmpSim.h:38–80`

`prepare()` führt drei Settle-Paare zu 0,1 s plus 0,15 s aus: insgesamt **0,75 s synthetisch simulierter Audiozeit** bei interner OS-Rate. Bei 44,1 kHz sind das 264.600 vorab gerechnete Samples, zusätzlich zur Vorinitialisierung der DSP-Objekte. Das ist **keine nachgewiesene 0,75-s-Wartezeit in Echtzeit**: tatsächliche Wandzeit messen, insbesondere beim Neuöffnen, Samplerate-Wechsel und mehrfachen Instanzen. Verlagerung/Caching nur bei identischem Einschwingzustand.

### A10 – P2 | IR-Speicherung ist pfadbasiert
`Source/PluginProcessor.cpp:369–395`, `Source/PresetManager.h:126–134`

Projekt/Presets speichern `irPath` als absoluten Dateipfad, **nicht** die IR-Bytes. Bei Verschieben auf einen anderen Rechner kann die IR fehlen. Projekt-Recall, Asset-Missing-UI und definierte Fallback-Klangquelle benötigen Tests. Achtung: Zustand bei fehlender IR ist nicht dieselbe Frage wie Presetwechsel A04.

### A11 – P2 | Modaler Dialog nach Editor-Lebensende – UNVERIFIZIERTER RISIKOPFAD
`Source/PluginEditor.cpp:151–188`

Speichern-/Löschen-Dialoge registrieren asynchrone Callbacks mit roher `[this]`-Capturing-Referenz auf den Editor. Bei Host-Schließen des Editors während offenem Dialog muss geprüft werden, ob JUCE die Callback-Lebensdauer sichert; andernfalls Use-after-free-Risiko. Keine bestätigte Reproduktion. Erst hostnaher Lifecycle-Test, dann SafePointer-/Modaltechnik prüfen.

### A12 – P2 | Testadäquanz: mehrere Tests sind Diagnostik, kein FAIL-Gate
`tests/dsp_test.cpp:1069–1092`, `tests/tone_regress.cpp:353–376`

`dsp_test ingate`, `transpose` und `cpu` geben Messwerte aus und enden bedingungslos mit Exit 0. Sie beweisen daher **nicht**, dass gemessene Latenz, Last oder Gate-Eigenschaften vertraglich in Grenzen liegen. `tone_regress` enthält dagegen echte Fail-Zählung und Exit 1. Die neuen Tests müssen bei bewusst injizierten Defekten nachweislich rot werden.

### A13 – P2 | Kommerzielles Produkt-/Lizenz- und Identitäts-Gate
`LICENSE`, `CMakeLists.txt`, `installer/THIRD-PARTY-NOTICES.txt`

AGPLv3-Quellcode und JUCE-/ASIO-Abhängigkeiten sind zu beachten. Der Fork bleibt als abgeleitetes Projekt AGPLv3, soweit keine gesonderten Rechte vorliegen; eigene 125A-Marken-/Produktidentität erst in geplanter Migration, mit Host-Projekt-Recall-/IDs-Prüfung. VST3-Manufacturer-/Plugin-Codes bei einer eigenständigen Distribution systematisch behandeln. Kein geschlossenes proprietäres Release bloß durch Umbenennen. Rechtlicher/Packaging-Check eigenständig vor Release.

## 4. Tatsächlich ausgeführte unabhängige Tests

**Umgebung:** Linux-Container, GCC g++ `-O2 -std=c++17`, native unabhängige C++-Testziele aus dem importierten ZIP, kein Windows/JUCE-VST3.

| Test | Status | Eindeutiger Befund |
|---|---|---|
| `g++ ... tests/dsp_test.cpp` | PASS | nativer Test kompiliert |
| `g++ ... tests/tone_regress.cpp` | PASS | nativer Regressionstest kompiliert |
| `dsp_test ingate` | EXECUTED | meldet −300 dB zwischen Chugs, praktisch 0 dB Änderung während Chugs; keine assertive Exit-Grenze |
| `dsp_test transpose` | EXECUTED | Pitchwerte meist nah an Soll, LIVE Onset +6,2 ms bei gemeldet 0 Samples |
| `dsp_test cpu` | EXECUTED | 4×-Kernlast s. A02; Diagnose, kein Realtime-PASS |
| `tone_regress "drive "` | PASS | 8/8 Fälle, 0 Fail; **kein** kompletter Pluginklang-Regressionstest |
| `dsp_test fault` | NOT FULLY VERIFIED | Teilresultate mit PASS sichtbar, Gesamtlauf nicht vollständig/mit Exit 0 abgeschlossen |
| separates `KorenPentode`-Grenzwertprogramm | FAIL-REPRO | bei Vg2k = 0.0 NaN |
| JUCE `amp_path_test` / 8× | NOT RUN | JUCE/Windows-Pfad hier nicht verifiziert |
| offizielle VST3-Validator-Suite | NOT RUN | |
| Studio One inkl. Projekt-Recall | NOT RUN | |
| vollständiges Audio-A/B gegen realen 6505 | NOT RUN | keine vollständigen Referenz-Captures vorhanden |

Diese Teilbefunde dürfen **nicht** zu einem grünen Gesamt-Audit zusammengefasst werden.

## 5. Anforderungen an eine vollständige Prüfmatrix vor Implementierung

| Vertrag | Positivtest | Negativ-/Grenztest | Aktueller Status |
|---|---|---|---|
| Stereo-Eingang/Ausgang | L- und R-unabhängige Testsignale hörbar korrekt | R-only-Input darf nie stillschweigend wegfallen | **FAIL im Code** |
| Mono-Input/Stereo-Out | unveränderter Monoklang, Effektbreite | kein Phantomkanal, keine doppelte Kernlast | NOT RUN |
| CPU/Deadline | 44,1/48/96 kHz; 32/64/128/256/512/1024 Frames | p99/max/Overruns/Cold-Start/Mehrfachinstanzen | PARTIAL |
| Amp-Klang | Frozen-DI, Pegel-/Frequenz-/Transienten-/Harmoniksweep | extreme Gain-/Röhren-/Channel-/Boost-Automation | PARTIAL |
| Solver | Grenzwerte endlich, zustandsstabil | NaN/Inf/Null-Schirmspannung, Fallback, erneute Konvergenz | PARTIAL; Grenzfall FAIL |
| Transpose/PDC | LIVE und HQ Audio + gemessene Latenz | schnelle Pitch-/Mode-Wechsel, Parallelbus/Loop | PARTIAL |
| Gate, EQ und FX | echte Audiofunktion & Neutralität bei Bypass | Automationssprünge, extreme Parameter, Tail | PARTIAL |
| IR-Dateien | WAV/AIFF, Drag-Drop, Prev/Next, built-in | defekte/große/fehlende IR, Wechsel im Betrieb, Persistenz | NOT RUN |
| Presets & Host-Recall | Save, Reload, Wiederherstellung desselben Klangs | fehlende IR, ungültiges XML, Factory-/User-Wechsel | Code-Presetpfad FAIL |
| GUI/Windows | 100/125/130/150 % Zoom, Controller-Bindings | Close/Reopen, Dialog bei Editorende, DPI, Minimalkomponenten | NOT RUN |
| Packaging/Identity | VST3-Installation/Standalone, Rechte/Lizenzen | Update/Deinstallation, getrennte Produkt-ID | NOT RUN |

## 6. Architektur- und Produktentscheidungen **vor** Codex-Coding

1. **Baseline einfrieren**: Upstream v0.5.6 exakt referenzieren, gemessene unveränderte Audioreferenz unter 44,1/48 kHz, festen DI-Fixtures und allen Kanälen herstellen.
2. **Mono/Stereo-Vertrag definieren**: echtes Stereo als zwei unabhängige DSP-Ketten oder explizite kostengünstigere Architektur; L/R-Trennung sowie Ping-Pong-FX und mono-kompatible Sessions berücksichtigen. Zwei komplette Amp-Solver kosten grundsätzlich etwa 2× Kernarbeit: „Stereo“ beseitigt keine CPU-Kosten.
3. **CPU zuerst messen, dann ändern**: Profile für V1/V2-... Solver, Oversampling, beide Pedale, Kontrollmatrizen, Schaltungslast, AudioThread-Perzentile. Keine Klang-Verschlechterung für bloße FLOP-Einsparung.
4. **Host-PDC und Preset-/IR-Recall spezifizieren**, bevor Parameter oder Zustandsformat geändert werden. Alle bestehenden Parameter-IDs bei kompatibler Weiterentwicklung bewahren.
5. **Crash-/Numerik-Pfade abschließen**: Nullen, extreme Spannungen, NaN/Inf, Solver-Convergence, Thread-Safety, IR-Load und Editor-Lifecycle.
6. **GUI später gestalten**: komplette Parameterinventur und Funktionshierarchie, Hardware-Look, kompakt, Zoom und echte 125A-Branding-Master; zunächst keine generierten „fertigen“ Plugin-Screenshots als Implementierungsbeweis.
7. **Windows-QA etablieren**: reproduzierbarer CMake-/VST3-Build, Steinberg Validator, geeignete Funktions-Assertions, echte Studio-One-Nachweise und Audio-A/B.
8. **AGPL-/JUCE-/ASIO-Abhängigkeiten und separate Plugin-Identität** vor einer Distribution dokumentieren.

## 7. Vorgaben für den nächsten Codex-Auftrag

- Zuerst **ausschließlich** bestehenden Code nachprüfen und eigene Anforderungen-zu-Test-Matrix erstellen, **nicht** unmittelbar alles umbauen.
- Verbindliche Referenz: `challanger2000/125A-Engineering/START-HERE.md` sowie hier dokumentierte SHA-Grundlage. Aktuelle Upstream-Version erneut vergleichen; keine stillschweigende Versionsmigration.
- Änderungen nur auf intentionalem Branch, nicht auf `main`; Original `main` und Klangreferenzen unangetastet lassen.
- Priorität A01, A03, A04 und A06: jeweils belegtes Repro, Root Cause, Minimaländerung, echte Regression. Leistungsoptimierung A02 separat mit Messungen und Host-Qualitätsbeweis.
- **Keine Erfolgsbehauptung** aus bloßem grünen GitHub Actions-Build. Windows-VST3/Studio One ausdrücklich als PASS/FAIL/NOT RUN führen.
- GitHub Actions budgetbewusst: lokale statische Analyse und native DSP-Tests vor kostenpflichtigen Windows-Builds, gebündelte sinnvolle Verifikationsläufe.

**Gesamturteil:** technisch bemerkenswerter, noch sehr junger Beta-Code; mehrere echte Funktions-/Zustandsmängel und erheblicher Optimierungsbedarf. **NICHT RELEASE-READY.** Dieser Bericht ist die unabhängige *statische Bestandsaufnahme mit begrenzten nativen Testläufen*, kein behauptetes vollständiges Release-Audit.
