# Schritt 3 – CPU-Profil und Windows-x64-VST3-QA

2026-10-26, Europe/Berlin; technische Messlogs in UTC. Entwicklungsbranch `dev/compact-stereo-on-ir-fix-20261026`, bestätigte Basis `398204e26d84c3f40bcd1c0af55dfa8e97893ca7`. **Keine FINAL-Freigabe.**

## Kandidat und verbindliche Grenzen

Dieser Entwicklungsbericht enthält tatsächlich ausgeführte Linux-Prüfungen und die unten einzeln belegten Windows-Läufe einschließlich ihrer Fehler. Der zuletzt ergänzte Produktstand `40f16554adac6d4dd483ca556fab6d619933b0aa` und der Testhoststand `9226bdc738354c4a396b1e3fa2d1d1f0e392c441` warten auf ihren gezielten Windows-Nachweis; frühere PASS werden nicht automatisch auf ihn übertragen. CPU-Daten sind von abgelehnten Prototypen und Profiling-Zeiten getrennt.

Pflichteinstieg und relevante Engineering-/QA-Vorgaben wurden erneut geprüft: `125A-Engineering/START-HERE.md`, unveränderter Stand `b6df84b0a04d01e240cff745ccebf60d67750486`; README, relevante Guardrails/Standards, Release-/Realtime-/Audio-Fixture-QA, Oversampling-Research und Roadmap. „verstanden“ bestätigt. Kein genehmigtes Windows-FULL-Paket dieses Entwicklungsforks erfunden. Benutzerdefinierte Arbeitsbasis ist der oben genannte Commit.

Keine Änderung an GUI, Parametern/IDs, IR-/Preset-Verträgen, Qualitätsstufen, Oversampling-Faktoren, Röhrenkennlinien, Solver-Iterationen oder Solver-Toleranzen. `main` und der ursprüngliche IR-Reparaturbranch bleiben unverändert.

## Reales HIGH-Stereo-Profil

Linux x86_64, GCC 14.2/O3/NDEBUG mit Debugsymbolen, JUCE 8.0.4. Vollständiger echter Processor, HIGH/Stereo, RED/Gain9, Precision Drive an/Drive8/Level7, Master3, Built-in-Cab und beide Cuts an, Noise/Gate aus, 48 kHz/256 Samples. Profilfenster erst nach Prepare und Warm-up, 2048 Audio-Callbacks. Linux `perf` 6.12.107, Software-CPU-Clock, 499 Hz, DWARF-Stack 16384 Bytes. Keine Source-Instrumentierung des Produkt-DSP; kontrollierter Enable/Disable mit bestätigtem perf-Ack. Aufbau und Aufwärmung sind nicht im Profilfenster.

**MEASURED:** 12985 Samples, 0 verlorene Samples im Baseline-Lauf. Inklusive Stichprobenanteile: AmpSim etwa 99,58 %, gekoppelter Amp etwa 91,34 %, Preamp etwa 57,49 %, Poweramp etwa 33,75 %. Matrix-AVX-Einstieg etwa 13,02 % inklusive. Newton-/Port-Elimination und Röhrenauswertung liegen innerhalb von Preamp/Poweramp und dürfen nicht nochmals zum Gesamtanteil addiert werden. Der Newton-TriodeNetwork-Einstieg umfasst56,80% inklusive, seine Geräteauswertung19,33%, KorenTriode::eval15,30%, DK-AVX13,02%. Die im Bericht explizit aufgelösten Port-Solve-Symbole tragen zusammen29,43% Self-Samples bei (Berichtsschwelle/Inline-Auflösung begrenzen die Vollständigkeit); dies ist kein zusätzlicher Anteil zum Preamp/Poweramp. Oversampling, Cabinet, Cuts und Host-/Processor-Umgebung liegen im verbleibenden kleinen Anteil; der Oversampler ist hier nicht der Hauptengpass. Vollständige Symboltabelle statt erfundener exklusiver Modulzeiten im Evidenzverzeichnis.

Der Profiling-Lauf misst im Mittel 12700,4 µs/Callback, der anschließende unprofilierte Lauf 11583,8 µs. Beobachtete Differenz etwa +9,64 %; sie umfasst Profiling-Aufwand **und** unbekannte VM-Lastdrift. Profilierte Zeiten sind deshalb keine CPU-Abnahme. Einzelne `addr2line`-Warnungen sind erhalten; 0 Sample-Verlust ist keine Garantie vollständiger Stack-/Inline-Auflösung. Keine Kernel-Mikrosekunden aus diesem Sampling ableiten. Die kontrollierte End-to-End-Matrix wird separat ohne perf ausgeführt.

## Optimierungen und verworfene Kandidaten

1. **Unverändertes Pedal-Mischgewicht wiederverwenden.** Der Cosinus wird bei Erstinitialisierung und jedem tatsächlichen Mischwert-Schritt berechnet; bei identischem Argument wird genau derselbe Double-Wert weiterverwendet. Beide Pedale bleiben vollständig warm. Kein Entfernen des inaktiven Pedals, keine neue Mischkurve.
2. **Nur konsumierte Rail-Quellen schreiben.** Die exakt aktuellen Versorgungsspannungen werden pro Sample gespeichert und unmittelbar vor jedem tatsächlich laufenden Preamp-Schaltkreis als Quellen gesetzt. Bei Kanal-/Crunch-Blenden bekommt jeder laufende Pfad dieselben aktuellen Rails. Inaktive Schaltungen verbrauchen keine Quelle. Keine zeitliche Vorwegberechnung der Versorgung, kein Block-`runAhead`, keine geänderte Versorgungskopplung. Custom-Initialrails, Blenden und alle drei Kanäle gegen eingefrorenen Original-Preamp geprüft: Maxdelta 0 für Audio und beide Rail-Ströme.
3. **MSVC-AVX-Helfer innerhalb des isolierten Einstiegs inlinebar.** Der AVX-Einstieg bleibt `noinline` und durch CPU/OS-Prüfung geschützt. Nur der private, ausschließlich von diesem Einstieg aufgerufene Tile-Helfer darf unter MSVC `__forceinline` werden. GCC erhält seine bisherige Attribut-/Inline-Regel. Keine globalen AVX-/FMA-/fast-math-Flags; Windows-Sicherheit und Nutzen werden am tatsächlichen Build geprüft.

**Verworfen:** Separater SIMD-Eliminationshelfer und vollständig isolierte SIMD-Port-Solve-Variante. Beide erhalten mathematisch dieselbe Elimination; der erste besteht 12000 bitgenaue Matrizenprüfungen pro AVX/SSE2/Scalar-Backend. Trotzdem kein belegter End-to-End-Vorteil: der erste Kandidat im HIGH-Mittel der 16 gepaarten Fälle −8,77 % Einsparung, also langsamer; die zweite Variante ebenfalls kein Gewinn im erneuten langen Callback-Lauf. VM-Drift begrenzt kausale Prozentbehauptungen. Die Entscheidung ist konservativ: **keiner dieser Solver-Kandidaten ist im Produkt aktiv**, `PortSolve.h` bleibt bytegleich zur Basis. Prototypen, fehlerfreie Rechenprüfungen und negative Performance-Daten bleiben zur Nachvollziehbarkeit unter Tests/Evidenz erhalten.

## Bisher tatsächlich ausgeführt

| Anforderung | Nachweis | Status / Grenze |
|---|---|---|
| HIGH-Sound zur Arbeitsbasis | 27 PCM-Vergleiche bei 44,1/48/96 kHz, alle Kanäle, Drive aus/PD/TS9, Gain/Presence/Kanal/Tube-Set plus Pedalmodell/Boost-Wechsel | PASS, Max-Sampledelta 0; Log-Fehlerenergie bei Nullfehler ist ein künstlicher Zahlenfloor, kein −3000-dB-Audiomesswert |
| Rail-Rescheduling | unabhängiger eingefrorener Preamp, 24000 Samples mit variierenden Rails, Kanal/Crunch-Blenden, eigenen Initialrails; Audio + Rail-Ströme | PASS, Maxdelta 0 |
| Solver/Matrix/Cuts | native Root-CTest-Suite mit unveränderten bestehenden Tests plus Rail-Orakel | PASS, 5/5 |
| Bestehende Klang-/Fehlerreferenzen | alter Tone-Test und Fault-Recovery, keine neuen Referenzen | PASS: 74 Tone-Fälle; Fault-Rohlog inklusive Extremsignalen |
| Echter Linux-VST3 | alter Wrapper-Vertrag plus Transport/rasche Qualitäts-/Moduswechsel/Bypass, IR-Projekt auf Disk speichern/Instanz zerstören/frisch laden, fehlende IR mit Gain-Rollback, Rückkehr Built-in | PASS; tatsächliche PCM-Orakel für Transport und IR-Recall Maxdelta 0; kein Windows-/ASIO-Nachweis |
| Erweiterte Aliasdaten | 64 Render, 44,1/48 kHz, 1/3/5/9 kHz, −30,46/−18,42 dBFS, HIGH/STANDARD/ECO/16×-Vergleich | PASS nur für endliche/nichtstumme Messdaten; keine Klangfreigabe |
| Endgültige CPU-Matrix | 44,1/48 kHz × Mono/Stereo × 128/256 × alle Qualitäten, offline/getaktet, 160 gemessene Blöcke je Fall | MEASURED: vier Läufe; Echtzeit-Gate FAIL (nachher 77/96 Fälle FAIL), Details unten |
| Windows Release / tatsächlicher VST3 | gebündelter gezielter Workflow vorbereitet | NOT RUN bis belegtes CI-Ergebnis |
| Windows Callback-Heap | processweite RtlAllocateHeap/RtlReAllocateHeap/RtlFreeHeap-Hooks, nur Audio-Thread-Zeitfenster, tatsächliche DLL-/CRT-Grenze | NOT RUN; Host-only `operator new` wäre hierfür unzureichend |
| 125A Tester / Steinberg | Tester-Source `3dcf3fe607008e520dcf20d1f793d9c534a9b9e1` (v0.2.9), SDK `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96` (3.8.1 build84), Selbsttests und Positiv-/Negativfixtures vor Produkturteil vorgesehen | NOT RUN; kein geprüftes Tester-Releasepaket aus dem veralteten CURRENT-Eintrag erfunden |
| Tatsächlicher i9-11900K, ASIO/Studio One, echte DI und Hörvergleich | keine entsprechende Hardware/DAW oder freigegebene DI in dieser Umgebung | NOT RUN, keine Aussetzer-/Klangfreigabe |

## Endgültige Linux-CPU-Matrix – kein zusätzlicher Echtzeit-PASS

Vier unprofilierte Läufe **Vorher A1 → Nachher B1 → Nachher B2 → Vorher A2**, je 48 Fälle: 44,1/48 kHz, MONO/STEREO, ECO/STANDARD/HIGH, 128/256 Samples, offline/getaktet. Je Fall 0,2 s Warm-up und 160 gemessene Blöcke. Gleicher Compiler/O3/Debugsymbol-Build, gleicher vollständiger Processor und identische Eingänge/Controls; keine parallelen eigenen Builds in diesen CPU-Läufen. Leichte Git-/Dokumentations-/Netzabfragen und unbekannte physische VM-Auslastung sind kein kontrollierter Realtime-Messplatz.

| Qualität | Mittel der 16 relativen Einsparungen aus je zwei Laufmitteln | mittlere absolute Differenz zwischen Wiederholungen | belastbares Urteil |
|---|---:|---:|---|
| HIGH | 6,48 % | 8,07 % | beobachteter Vorteil, keine garantierte zusätzliche Einsparquote |
| STANDARD | 3,04 % | 8,16 % | Vorteil kleiner als Wiederholungsdrift |
| ECO | 3,54 % | 8,94 % | Vorteil kleiner als Wiederholungsdrift |

Die Vergleichsrechnung mittelt zuerst je Konfiguration die beiden Before-/After-Mittelzeiten und berechnet dann deren relative Differenz. Perzentile werden **nicht** aus Perzentilen gemittelt und als gepoolte Quantile ausgegeben. Alle p95/p99/Maxima bleiben für jeden der vier tatsächlichen Läufe separat in den CSVs erhalten. Alle 48 Ausgangsenergien stimmen über sämtliche Wiederholungen gedruckt überein; der strengere separate PCM-Test prüft echte Samplewerte.

**FAIL:** Vorher 86 von 96 Fällen mit mindestens einer Deadline-Verletzung, nachher **77 von 96**; nachher 19 Fälle ohne beobachtete Verletzung. Weder diese kurzen Einzel-PASS noch bessere Mittelwerte begründen eine Aussetzer-Freigabe. CPU-Overruns und getaktete Fertigstellungs-Misses sind separat ausgewiesen.

Beispiel des ersten Paares A1/B1, HIGH bei 48 kHz, getaktet. Zellen jeweils Vorher → Nachher, Zeiten in µs:

| Amp / Samples | Mittel | p95 | p99 | Maximum | Callback-Overruns / 160 | Deadline |
|---|---:|---:|---:|---:|---:|---:|
| MONO / 128 | 3152.46 → 2805.11 | 4248.50 → 3536.44 | 5513.74 → 5074.41 | 5840.67 → 5096.15 | 142 → 77 | 2666.67 |
| MONO / 256 | 6256.42 → 5623.91 | 7737.50 → 6615.55 | 8770.01 → 7481.25 | 12843.40 → 8024.30 | 153 → 110 | 5333.33 |
| STEREO / 128 | 6346.26 → 5555.94 | 7533.17 → 5985.84 | 17678.90 → 7645.09 | 18792.40 → 7683.87 | 160 → 160 | 2666.67 |
| STEREO / 256 | 13272.30 → 12335.70 | 24950.70 → 15247.30 | 29939.60 → 21466.30 | 32246.30 → 36272.30 | 160 → 160 | 5333.33 |

Volle Daten: [A1](qa/step3-cpu-20261026/final-before-1.csv), [B1](qa/step3-cpu-20261026/final-after-1.csv), [B2](qa/step3-cpu-20261026/final-after-2.csv), [A2](qa/step3-cpu-20261026/final-before-2.csv), [Vergleich](qa/step3-cpu-20261026/cpu-comparison.csv), [Zusammenfassung](qa/step3-cpu-20261026/cpu-summary.json).

Das abschließende Sampling-Profil des endgültigen Kandidaten enthält 12877 Samples und 0 gemeldete Verluste. Profilierte/unprofilierte Langläufe: 12585,5 / 12321,9 µs Mittel; diese beobachtete Differenz von rund 2,14 % enthält erneut Lastdrift und darf nicht als universeller Profiler-Aufschlag subtrahiert werden. Preamp/Poweramp bleiben die Schwerpunkte, Matrix-AVX rund 11,87 % inklusive. Inline-Symbole erscheinen teils in mehreren Kontexten; der vollständige Bericht bleibt erhalten. Windows-CI und insbesondere der reale i9 sind separat zu beurteilen.

## ECO / STANDARD und Aliasing

Zusätzlich zu den früheren 3-kHz-Messungen werden kohärente 1/3/5/9-kHz-Töne bei zwei Eingangspegeln im tatsächlichen nichtlinearen Amp gerendert. FFT 8192, Hann-Fenster; echte Grundfrequenz wird auf ein ungerades FFT-Bin gelegt. Messmaske umfasst auf Host-Nyquist gefaltete harmonische Bins bis Ordnung 256, ohne die ±4-Bin-Schutzbereiche echter hörbarer Harmonischer. Das Ergebnis `folded_bins_db` ist ein gezielter Alias-/Artefaktindikator relativ zur Fundamentalenergie, **keine perfekte Trennung von Aliasing, transienten Resten und sonstigen inharmonischen Anteilen**. Die zusätzliche 16×-Referenz ist ebenfalls nicht mathematisch aliasfrei und verwendet eine andere interne Integrationsrate.

Schon die neuen Messungen zeigen erhebliche Pegel-/Frequenzabhängigkeit. ECO ist bei starkem 5-kHz-Eingang deutlich schlechter als HIGH; auch HIGH ist beim extremen 9-kHz-Test keine aliasfreie Referenz. HIGH bleibt der unveränderte Klangvertrag der Arbeitsbasis, nicht die Behauptung einer fehlerfreien analogen Referenz. Vollständige Zahlen: [64 Messungen](qa/step3-cpu-20261026/alias-data.csv). Beispiel 48 kHz, 5-kHz-Ton, −18,42 dBFS Eingang: gefaltete-Bin-Energie relativ zur Fundamentalenergie HIGH −45,84 dB, STANDARD/ECO siehe Rohdaten (ECO −7,34 dB). Beim 9-kHz-Ton gleicher Stärke HIGH −27,76 dB, 16×-Referenz −48,77 dB. Das ist eine Grenze des unveränderten HIGH-Referenzmodells, kein durch diese Optimierung eingeführter Unterschied. Keine automatische oder subjektiv unbelegte Klangfreigabe für ECO/STANDARD.

## Erster Windows-Lauf – Build-PASS, Testhost-Compiler-FAIL

[Run 40375703107](https://github.com/challanger2000/125A-Dystorator/actions/runs/40375703107), Quellcommit `4efb20e5d6df4282ff33bdf744335131d6278dd3`. Windows Server 2022 / NT10.0.20348, MSVC14.44, AMD EPYC9V74, 2 Kerne/4 logische CPUs, MaxClock2596MHz laut CIM; **kein i9-11900K**. Befunde wurden vom Runner in Commit `4c6fa4726751dc9130d38c1bbd37bfb23180bb1b` gesichert.

- PASS: tatsächliche Before- und After-Release-VST3s gebaut; native Root-CTest5/5; Tone74; Fault-Regression.
- FAIL (Testinfrastruktur): Windows-Makro `max` aus dem neu eingebundenen Win32-Header expandiert bei `std::max` im echten VST3-Testhost (MSVC C2589/C2059). Der IR-Test selbst war gelinkt; der gemeinsame Build stoppt am Host.
- NOT RUN: nachgelagerte Integration, Callback-Heap, tatsächliche Host-/IR-/Transport-Matrix, CPU, 125A Tester und Validator. Keine dieser Prüfungen wurde aus dem grünen Produktbuild abgeleitet.
- Korrektur ausschließlich am Testhost: `NOMINMAX` vor Win32-Header und als Targetdefinition, keine DSP-/Grenzwertänderung. Ein gezielter gebündelter Folgelauf ist notwendig. [Vollständige erste Windows-Befunde](qa/step3-windows-4efb20e5d6df4282ff33bdf744335131d6278dd3/results.json).

## Zweiter Windows-Lauf – tatsächliche Fehler, keine CPU-Daten

[Run 40378455766](https://github.com/challanger2000/125A-Dystorator/actions/runs/40378455766), Source `d3238810d0d6a2d2495d1c8eaccacb97f95670d8`, Evidenzcommit `9e918ec` (vollständiger Hash im Git-Verlauf). Windows Server2022, **AMD EPYC7763**, 2 Kerne/4 Threads, CIM MaxClock2445MHz. Der andere Runner ist ausdrücklich keine Wiederholungsmessung auf derselben CPU wie Lauf1 oder Linux.

- PASS: reale Before-/After-Release-VST3s; Root5/5; Tone74; Fault;132 bisherige IR-/Preset-/Ownership-Prüfungen; Quality/Modes66, historische PCM27, Stress3; neue HIGH-PCM27 zur Arbeitsbasis; Mono-/Stereo-Orakel;64 Aliasdaten endlich/nichtstumm.
- FAIL: eigener realer VST3-Testhost vor der ersten Ergebniszeile mit `0xC0000005`; beide CPU-Aufrufe mit `0xC0000409`. **Keine CPU-Matrix gemessen.** Der Prozessstatus beweist einen Testfehler, nicht dessen Ursache; keine vermeintlichen Deadline-Zahlen aus diesen Aufrufen.
- PASS: unveränderter125A Tester aus gepinnter Quelle gebaut; Job-Object-/Validator-Selbsttest; sieben positive/negative Fixtures mit den jeweils erwarteten konkreten Diagnosen. Die negativen Exit1 sind ausschließlich für die absichtlich defekten Fixtures erwartet.
- FAIL: tatsächlicher125A-Produktbericht39 PASS/0 WARNING/1 FAIL. Steinberg46/47: `Scan Programs` beanstandet `Programlist 000->Program 000: has no name!!!`. `PluginProcessor.h::getProgramName` liefert bei angebotenem Programm0 einen leeren String. Separates Windows-I/O, fünf frische Instanzen/State-Roundtrips und fünf Editor-Lebenszyklen bestehen; das ersetzt keine manuelle GUI-/DAW-Prüfung. 64-bit-Processing ist ausdrücklich nicht unterstützt; die entsprechenden Validator-„Succeeded“-Einträge sind keine ausgeführten Double-DSP-Tests.

[Vollständige Rohdaten](qa/step3-windows-d3238810d0d6a2d2495d1c8eaccacb97f95670d8/results.json). Absturzdiagnostik (ungepufferte Stufen, C++-/CRT-/SEH-Fehler samt Modul/Stack) wird ausschließlich im Testhost ergänzt. Kein Ausblenden der Fehler, kein Lockern der Validator-Anforderung. Der ergänzte echte Processor-Test reproduziert den leeren Namen als FAIL: [133 Checks, genau dieser eine Fehler](qa/step3-cpu-20261026/host-program-before.txt); Korrektur: nur Programm0 erhält den festen Namen `Default`. Danach PASS Name und unveränderter vollständiger Szenenzustand beim erneuten Anwählen, [2 Checks](qa/step3-cpu-20261026/host-program-after.txt). Parameter/IDs und Preset-Namen bleiben unverändert; die Windows-Validator-Wiederholung bleibt bis zum Ergebnis offen.

## Zusätzliche Sanitizer-Prüfung des IR-Kandidaten

Nach der großen IR-Block-Korrektur, JUCE-Module vollständig instrumentiert, Linux GCC14.2/O1: **PASS ASan+UBSan**66 Modes/Recall-Prüfungen +3 nebenläufige IR/Preset/Host-State/Bypass-Prüfungen; **PASS TSan** dieselben66+3 Fälle. Kein Sanitizer-Report; `halt_on_error=1`, Exit0. [ASan Modes](qa/step3-cpu-20261026/asan-modes.txt), [ASan Stress](qa/step3-cpu-20261026/asan-stress.txt), [TSan Modes](qa/step3-cpu-20261026/tsan-modes.txt), [TSan Stress](qa/step3-cpu-20261026/tsan-stress.txt). Dies ist eine endliche synthetische Laufzeitprüfung; kein Beweis gegen jeden Race und keine CPU-Messung.

## Zusätzlicher bestätigter RT-Fehler: großer IR-Block benutzt C-Heap

Die bisherige C++-new/delete-Prüfung erfasst direkte C-Allokationen nicht. Quellpfad JUCE8.0.4 `juce_FFT.cpp:134–172`: der Fallback-Real-FFT nimmt ab 256KiB Scratch einen `HeapBlock<char>`. Bei Convolution-Maxblock >8192 wird die FFT auf mindestens32768 gerundet; `16 + FFTSize * sizeof(complex<float>)` überschreitet die Stack-Grenze. Der Processor akzeptiert solche Blockgrößen.

**Reproduziert vor Reparatur:** derselbe echte Cabinet-/JUCE-Code, 6 Aufrufe, `malloc/calloc/realloc/free` am Linker beobachtet: bei16384 **24 mallocs +24 frees**, sowohl Mono- als auch Stereo-IR.128/256/8192 sind ohne Heap-Aufruf. Exit1, 2 von8 Fällen FAIL; PCM zur unmodifizierten JUCE-Convolution dabei Delta0. [Vorher](qa/step3-cpu-20261026/large-ir-before.txt).

**Korrektur:** Nur das interne Convolution-Quantum wird in `CabinetIR::makeEngine` auf8192 begrenzt; große AudioBlocks werden als nichtownende Views in `renderConvolution` verarbeitet. Amp-Hostblock, Regler-Rampen, Quality/OS, Decoder/Normalisierung, IR-Publikation und gemeinsame Szenenblende bleiben unverändert. Ein IR-Wechsel wird weiterhin nur einmal pro ursprünglichem Aufruf angenommen. Keine neue Latenz: Convolution bleibt zero-latency. Alle Prepare-/Allokationsarbeiten bleiben auf dem Control-Pfad.

**PASS nach Reparatur:** 10 Fälle (Mono/Stereo ×128/256/8192/**8193**/16384), null beobachtete C-Heap-Aufrufe. Vergleich gegen unmodifizierte große JUCE-Convolution über sämtliche PCM-Samples: maximal8,84756e−9, unveränderte Grenze1e−6; bis8192 Delta0. Zusätzlich gebautes echtes Linux-VST3 mit IR/Host-Reprepare16384: Delta0 zur äquivalenten Referenz, nichtstumm; alle132 bestehenden IR-/Preset-/Routing-/Ownership-Checks erneut PASS. [Nachher](qa/step3-cpu-20261026/large-ir-after.txt), [reales VST3](qa/step3-cpu-20261026/large-ir-host.txt).

Die vorherige „bytegleiches Cabinet“-Hashliste bezieht sich ausdrücklich auf den CPU-Kandidaten4efb20e. Dieser spätere, belegte Grenzfall erweitert die Implementierung, ohne die IR-Reparaturverträge aufzugeben. Die CPU-Matrix4efb20e verwendet Built-in-Cab und128/256; sie wird nicht als Messung einer langen IR ausgegeben. Der abschließende Windows-Lauf muss den neuen Quellstand und den Prozessheap auch bei16384 verifizieren. **Windows-Nachweis dieses Zusatzes bis dahin NOT RUN.**

## Windows-Funktionsnachweis des bereinigten Produkts

[Run40386792595](https://github.com/challanger2000/125A-Dystorator/actions/runs/40386792595), Source`8a442d9d2a3ad10ba331b7cc849136db78ca08ac`, Windows2022/AMD EPYC9V74,2Kerne/4Threads, MSVC-Release/LTO. Vollständige Befunde in Dokumentationscommit`06ad8f10407a86503307ebfb936df4425c8bb5de`. **53 einzelne Build-/Test-Gates PASS; ausschließlich die beiden CPU-Aufrufe FAIL.** Keine FINAL-Freigabe aus dem Tool-Text `RELEASE: PASS` ableiten.

- PASS: Windows-x64-VST3 Before/After, Root5/5, Tone74, Fault,133 IR/Preset/Routing/Ownership-Checks,66 Modes/Recall,3 Concurrent-Stress, historische und neue27er HIGH-PCM-Orakel, Mono/Stereo-Referenzen und64 endliche Aliasdaten. Beide HIGH-Orakel Maxdelta0.
- PASS: tatsächliche13 VST3-Host-Gates, darunter positiver Heap-Selbsttest, Stop/Play/Cycle mit dichten Mode/Quality/Bypass-Requests, IR-Disk-Save/Destroy/Fresh-Recall, fehlendeIR/Gain-Rollback und Built-in-Rückkehr. Transport-, Recall- und16384er PCM jeweils Maxdelta0.
- PASS: echter Prozessheap unterhalb getrennter Host-/Plugin-CRTs: **0 Alloc/Realloc,0 Frees** während der beobachteten Callbacks. Separater positiver Heap-Control besteht. Mono/Stereo-IR ×128/256/8192/8193/16384:10/10,0Heap, größtes Sampledelta8,84756e−9 zur unmodifizierten JUCE-Referenz. Keine Behauptung über sämtliche OS-Speicher-/Lock-/Treiberaufrufe.
- PASS: unveränderter125A Tester v0.2.9: **40 PASS/0 WARNING/0 FAIL**, Steinberg **47/47**, Selbsttests und alle7 Good/Bad-Fixtures mit konkretem Erwartungstext. Automatisierte Editor-Zyklen/I/O/Fresh-State enthalten, manuelle Bedien-/DAW-Abnahme NOT RUN. 64-bit-DSP weiterhin nicht unterstützt.
- FAIL (Benchmark-Aufbau): beide CPU-Aufrufe werfen vor Messbeginn eine C++-Exception. Die neue Diagnose lokalisiert sie im CPU-Fixture. Dort wird der nicht vorhandene Anzeigename `Boost Level` gesucht; exportiert ist seit der Basis **`Drive Volume`**, ID`boostLevel`. Neuer tatsächlicher VST3-Preflight reproduziert lokal `Missing hosted parameter: Boost Level`; nach Korrektur acht PASS bei44,1/48k undMono/Stereo. Der Test stellt DriveVolume zuerst auf den nicht voreingestellten Wert3,1 und dann7 und liest alle13 Benchmark-Controls aus dem tatsächlichen IComponent-State zurück. Keine Parameter-/GUI-Änderung, keine Grenzwertlockerung. Die CPU-Tabellen aus den abgestürzten Läufen bleiben ausdrücklich **NOT RUN**.

[Rohstatus](qa/step3-windows-8a442d9d2a3ad10ba331b7cc849136db78ca08ac/results.json), [tatsächlicher DLL-/Heap-/Transport-Test](qa/step3-windows-8a442d9d2a3ad10ba331b7cc849136db78ca08ac/packaged-vst3-host.txt), [IR-Heap-Grenzen](qa/step3-windows-8a442d9d2a3ad10ba331b7cc849136db78ca08ac/large_ir_heap.txt), [Preflight vor Korrektur](qa/step3-cpu-20261026/cpu-fixture-reproduced-failure.txt), [danach](qa/step3-cpu-20261026/cpu-fixture-after.txt).

Der anschließende gezielte CPU-Lauf darf lange Funktionssuiten nur überspringen, wenn Source-Tree und Root-CMake exakt zu diesem funktional geprüften Commit passen und alle benötigten früheren Funktionsgates belegt PASS sind. Diese Identität wird im Runner geprüft und protokolliert. Vollsuiten werden im CPU-Lauf als NOT RUN vermerkt und mit ihrer ursprünglichen Evidenz verknüpft; keine neue Ausführung behauptet. Heap-Selbsttest, tatsächlicher DLL-Host und der neue CPU-Preflight werden trotzdem erneut ausgeführt. Aktueller Produkt-Source-Tree: `23fe6fa178f2248c23726a31ad080a82f9e991bc`.

## Windows-Verfahren und Reproduktion

Siehe [tests/step3/README.md](../tests/step3/README.md). Der Workflow baut Basis und Kandidat als Windows-x64-Release-VST3 mit identischen ursprünglichen JUCE-/LTO-Flags und misst beide mit demselben realen VST3-Testhost auf demselben Runner. Durchschnitt, p95, p99, Maximum, Callback- und Scheduling-Deadlines werden aufbewahrt. Ein einziges Overrun bleibt FAIL. Der Root-QA-Sammelbuild wird automatisch aus den registrierten Tests abgeleitet, damit kein CTest-Executable beim gezielten Windows-Build fehlt.

Der Windows-Testhost beobachtet den Heap unterhalb verschiedener CRTs mit Microsoft Detours v4.0.1, SHA `e4bfd6b03e50de46b47abfbd1e46b384f0c5f833`; Detours wird ausschließlich in den Testhost gelinkt, niemals in den Amp. Ein echter Allocate/Reallocate/Free-Selbsttest muss die Beobachtung positiv bestätigen. Das ist eine Heap-Prüfung, kein vollständiger Beweis gegen jeden direkten OS-Aufruf. Auf Linux wird dieser Windows-Test ausdrücklich NOT RUN gemeldet.

Windows-Läufe werden ausschließlich für den tatsächlichen Kandidaten und belegte notwendige Compiler-/Integrationskorrekturen ausgeführt; alle fehlgeschlagenen Läufe bleiben dokumentiert. Die Actions-API ist in dieser Cloud mit Forbidden blockiert; Git funktioniert. Deshalb publiziert der Workflow seine Rohbefunde einschließlich FAIL direkt als eigenen Dokumentationscommit auf denselben Entwicklungsbranch sowie als Actions-Artefakt. Der getestete Quellcommit wird explizit vermerkt. Dokumentationscommits lösen keinen weiteren Build aus. Windows-CI-Messungen sind ausdrücklich **keine Messung auf dem i9-11900K des Eigentümers**.

Die neuen PCM-Orakel prüfen zusätzlich beide Referenz-/Kandidatenströme und alle Rail-Ströme explizit auf Endlichkeit. Ein `std::max`-Delta allein könnte NaN übersehen. Die bestehenden Fehlergrenzen bleiben unverändert; strengere Assertions dienen der Aussagekraft, keinem künstlichen PASS.


Nach der Metadatenkorrektur: PASS133/133 echte IR/Preset/Routing/Ownership-Checks und PASS11/11 gebautes Linux-Release-VST3, einschließlich strenger Endlichkeitsprüfung beider PCM-Ströme. [Integration](qa/step3-cpu-20261026/final-ir-preset-regressions.txt), [VST3](qa/step3-cpu-20261026/final-linux-vst3-host.txt). Alle17 eingefrorenen Baseline-Header erneut gegen398204e inklusive SHA256 und ausschließlich Namespace-Ersetzung geprüft. [Produkt-Manifest](qa/step3-cpu-20261026/product-source-40f1655.json).

Der Windows-Heap-Beobachter wird außerdem gegen Zugriffe während der Thread-/Loader-Initialisierung abgesichert: Hooks lesen zuerst einen reservierten festen Win32-TEB-TLS-Slot, bevor C++-TLS-Counter berührt werden. Scope initialisiert die Counter, bevor dieser Slot aktiv wird; GetLastError bleibt erhalten. Fehlende Slots, fehlgeschlagene Installation/Selbsttest oder Detach sind Fehler, kein PASS. Das behebt eine unsichere Annahme des Beobachters; die konkrete Absturzursache und Funktionsfähigkeit bleiben bis zum neuen Windows-Ergebnis unbestätigt. Ausschließlich Testhost, keine Plugin-Hooks.


## Anforderungs-/Testmatrix und Grenzen

| Vertrag und Kette | Positiver Nachweis | Negativ-/Grenzfall und tatsächliches Orakel | Ergebnis / verbleibende Grenze |
|---|---|---|---|
| HIGH: Controls → beide Amp-Lanes → PCM |27 Render zur eingefrorenen398204e-Basis,44,1/48/96k, alle Kanäle/Drive-Modelle | wechselnde Gain/Presence/Tube/Channel/Boost/Model; finite Referenz und echter Samplefehler statt nur Pegelvergleich | Linux und Windows2 PASS Maxdelta0; reales DI/level-matched Hören NOT RUN |
| Rails → aktive/fadende Schaltung → Audio/Versorgungsströme |24000 Samples gegen separat eingefrorenen Preamp | Custom-Initialrails311/326, Sample-Railmodulation, Kanal/Crunch-Blenden; alle sechs Werte endlich und exakt gleich | Linux PASS; frühere Windows-Railprüfung PASS, verschärfte Endlichkeitsassertion im neuen Lauf offen |
| Solver/Matrix/CPU-Dispatch | unabhängiger vollständiger Jacobian, Scalar/SSE2/Auto; Root5/5 | Conducting/Nonconducting-Ports, Pivot/Singularität, SIMD-Restlängen/Guards; bestehende Grenzen unverändert | Linux/Windows2 PASS; abgelehnte Prototypen nicht im Produkt |
| LOW/HIGH Cuts und bestehende Signalmodelle |74 alte Tone-Checks, echte Processor-Cut-Orakel, Stereo-Referenz | hohe Pegel/Extreme/Fault-Recovery; Hashgleichheit der übrigen Röhren-/Solverquellen | Linux/Windows2 PASS; keine neue analoge Klangbehauptung |
| IR-Datei → Decoder → vorbereitete Engine → Mono/Stereo-Audio | echte WAV/AIFF-Impulse, Resampling, zwei unabhängige Kanäle | beschädigte/trunkierte/fremde/fehlende Dateien, unsupported Multichannel, Größenlimit; vorherige vollständige Szene bleibt gültig | Linux133/133 einschließlich neuer Program-Metadaten; Windows2 bisherige132/132 PASS |
| Große IR-Blöcke → JUCE-FFT → PCM/Heap |Mono/Stereo ×128/256/8192/8193/16384, unmodifizierte JUCE als Sample-Orakel | vorher16384:24 malloc+24 free/6Calls; nachher0 und Delta<9e−9, strikte1e−6-Grenze | Linux PASS; Windows-Prozessheap muss eigenen positiven Selbsttest bestehen |
| Preset/Host-Scene → Disk → frische Instanz → hörbarer Recall | reale VST3-Speicherung, Instanz zerstört, aus Datei neu geladen | fehlende IR plus geänderter Gain: vollständiger Rollback; ClearIR muss Pfad leeren und PCM ändern | Linux PASS Delta0; Windows native Recall PASS; endgültiger DLL-Host noch offen |
| Audio/Control nebenläufig: IR/Presets/Quality/Bypass |66 Modes +3 Stresstests, tatsächliche Audioausgabe | schnelle überlappende Requests, zuletzt angeforderter Zustand wird aktiv, keine C++new/delete; ASan/UBSan und TSan ohne Report | Linux PASS; Windows native66+3 PASS; C++new/delete allein beweist keinen DLL-/C-Heap |
| Modus/Quality/Automation/Transport → VST3 → PCM | bewegter Stop/Play/Cycle-Kontext gegen durchgehenden Referenzstream,180 Blöcke |30 dichte Modus-/Quality-Requests, Bypass70/95, keine Latenzänderung und nichtstumm; tatsächliche Samplewerte | Linux11 Wrapper-Checks PASS; keine Abnahme der Sample-Offset-Genauigkeit sämtlicher Parameter oder realer Host-Transportwechsel |
| Latenz/Bypass → Host-Dry/Warm-Wet |44,1/48/96k Mono/Stereo und alle effektiven OS-Faktoren; gepaddete HIGH-Latenz | R-only, Reprepare, leere/kleine/große Blöcke; Dry muss zeitverschoben exakt stimmen und Wet weiterlaufen | native Linux/Windows2 PASS; Windows-DLL-Nachweis im abschließenden Lauf |
| Qualitätsdaten → Alias-/Artefaktindikator |64 synthetische Spektralrender mit16×-Vergleich |1/3/5/9k, zwei Pegel, tatsächliche kohärente Frequenz und Messmasken ausgewiesen | Messdaten PASS; ECO/STANDARD Klangfreigabe NOT RUN, deutliche Alias-Nachteile belegt |
| Realtime-CPU → komplette Verarbeitung → Deadline | ABBA192 Konfigurationen, je160 gemessene Callbacks |44,1/48k ×128/256 ×Mono/Stereo ×alle Qualitäten ×offline/getaktet, bereits eine Verletzung FAIL | Linux FAIL nachher77/96; bisherige Windows-CPU-Aufrufe abgestürzt, keine Messmatrix |
| Export-Metadaten → VST3 Programlist → Validator | fester Programm0-Name und unveränderte Szene beim Anwählen | alter leerer Name reproduziert133Checks/1FAIL; Validator46/47 im zweiten Lauf | lokale Korrektur2/2 PASS; Windows-Validatorwiederholung offen |
| Unabhängige125A QA → Produkt-VST3 | unveränderter gepinnter Tester, Job-Object-/Validator-Selbsttest und7 Positiv-/Negativfixtures | konkrete erwartete Fehlertexte, kein pauschales Akzeptieren von Exit1; Editor5Zyklen, I/O und Fresh-State geprüft | Tool-Selbstprüfung Windows2 PASS; Produkt FAIL wegen Programlist, keine FINAL |
| Tatsächlicher Arbeitsplatz → ASIO/DAW → aufgezeichnete Audioausgabe | Owner-i9-11900K, StudioOne/ASIO, reale DI, Export/Realtime-Vergleich erforderlich | lange Sitzung, Dropout-Aufzeichnung, schnelle Umschaltungen, Projekt schließen/öffnen, Hörabgleich | NOT RUN: entsprechende Hardware/DAW/Fixtures fehlen; CI ersetzt diese Kette nicht |
