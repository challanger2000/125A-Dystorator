# Schritt 3 – CPU-Profil und Windows-x64-VST3-QA

2026-10-26, Europe/Berlin; technische Messlogs in UTC. Entwicklungsbranch `dev/compact-stereo-on-ir-fix-20261026`, bestätigte Basis `398204e26d84c3f40bcd1c0af55dfa8e97893ca7`. **Keine FINAL-Freigabe.**

## Kandidat und verbindliche Grenzen

Dieser Entwicklungsbericht enthält tatsächlich ausgeführte Linux-Prüfungen und die unten einzeln belegten Windows-Läufe einschließlich ihrer Fehler. Die Windows-Funktionsprüfung des Produkt-Source-Trees `23fe6fa178f2248c23726a31ad080a82f9e991bc` ist unter Quellcommit `8a442d9d2a3ad10ba331b7cc849136db78ca08ac` ausgeführt. Die vollständige Windows-CPU-Wiederholung ist gemessen:33/48 Fälle FAIL, keine nennenswerte zusätzliche Einsparung belegt. Keine FINAL-Freigabe. CPU-Daten sind von abgelehnten Prototypen und Profiling-Zeiten getrennt.

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

## Ausgeführte Prüfungen

| Anforderung | Nachweis | Status / Grenze |
|---|---|---|
| HIGH-Sound zur Arbeitsbasis | 27 PCM-Vergleiche bei 44,1/48/96 kHz, alle Kanäle, Drive aus/PD/TS9, Gain/Presence/Kanal/Tube-Set plus Pedalmodell/Boost-Wechsel | PASS, Max-Sampledelta 0; Log-Fehlerenergie bei Nullfehler ist ein künstlicher Zahlenfloor, kein −3000-dB-Audiomesswert |
| Rail-Rescheduling | unabhängiger eingefrorener Preamp, 24000 Samples mit variierenden Rails, Kanal/Crunch-Blenden, eigenen Initialrails; Audio + Rail-Ströme | PASS, Maxdelta 0 |
| Solver/Matrix/Cuts | native Root-CTest-Suite mit unveränderten bestehenden Tests plus Rail-Orakel | PASS, 5/5 |
| Bestehende Klang-/Fehlerreferenzen | alter Tone-Test und Fault-Recovery, keine neuen Referenzen | PASS: 74 Tone-Fälle; Fault-Rohlog inklusive Extremsignalen |
| Echter Linux-VST3 | alter Wrapper-Vertrag plus Transport/rasche Qualitäts-/Moduswechsel/Bypass, IR-Projekt auf Disk speichern/Instanz zerstören/frisch laden, fehlende IR mit Gain-Rollback, Rückkehr Built-in | PASS; tatsächliche PCM-Orakel für Transport und IR-Recall Maxdelta 0; kein Windows-/ASIO-Nachweis |
| Erweiterte Aliasdaten | 64 Render, 44,1/48 kHz, 1/3/5/9 kHz, −30,46/−18,42 dBFS, HIGH/STANDARD/ECO/16×-Vergleich | PASS nur für endliche/nichtstumme Messdaten; keine Klangfreigabe |
| Linux-CPU-Matrix | 44,1/48 kHz × Mono/Stereo × 128/256 × alle Qualitäten, offline/getaktet, 160 gemessene Blöcke je Fall | MEASURED: vier Läufe; Echtzeit-Gate FAIL (nachher 77/96 Fälle FAIL), Details unten |
| Windows Release / tatsächlicher VST3 | vollständiger MSVC-x64-Release/LTO-Build und realer Wrapper-/State-/IR-/Transport-/Bypass-Test | PASS unter `8a442d9`,13/13 Host-Gates; reale DAW separat NOT RUN |
| Windows Callback-Heap | processweite RtlAllocateHeap/RtlReAllocateHeap/RtlFreeHeap-Hooks, nur Audio-Thread-Zeitfenster, tatsächliche DLL-/CRT-Grenze | PASS:0 beobachtete Alloc/Realloc und Frees, positiver Selbsttest, IR bis16384; keine Aussage über sämtliche OS-Aufrufe |
| 125A Tester / Steinberg | Tester-Source `3dcf3fe607008e520dcf20d1f793d9c534a9b9e1` (v0.2.9), SDK `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96` (3.8.1 build84), Selbsttests und7 Positiv-/Negativfixtures | PASS: Tester40/40, Steinberg47/47; eigenes Build aus gepinnter Quelle, kein verifiziertes Releasepaket aus dem veralteten CURRENT-Eintrag erfunden |
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
- FAIL: tatsächlicher125A-Produktbericht39 PASS/0 WARNING/1 FAIL. Steinberg46/47: `Scan Programs` beanstandet `Programlist 000->Program 000: has no name!!!`. `PluginProcessor.h::getProgramName` liefert bei angebotenem Programm 0 einen leeren String. Separates Windows-I/O, fünf frische Instanzen/State-Roundtrips und fünf Editor-Lebenszyklen bestehen; das ersetzt keine manuelle GUI-/DAW-Prüfung. 64-bit-Processing ist ausdrücklich nicht unterstützt; die entsprechenden Validator-„Succeeded“-Einträge sind keine ausgeführten Double-DSP-Tests.

[Vollständige Rohdaten](qa/step3-windows-d3238810d0d6a2d2495d1c8eaccacb97f95670d8/results.json). Absturzdiagnostik (ungepufferte Stufen, C++-/CRT-/SEH-Fehler samt Modul/Stack) wird ausschließlich im Testhost ergänzt. Kein Ausblenden der Fehler, kein Lockern der Validator-Anforderung. Der ergänzte echte Processor-Test reproduziert den leeren Namen als FAIL: [133 Checks, genau dieser eine Fehler](qa/step3-cpu-20261026/host-program-before.txt); Korrektur: nur Programm 0 erhält den festen Namen `Default`. Danach PASS Name und unveränderter vollständiger Szenenzustand beim erneuten Anwählen, [2 Checks](qa/step3-cpu-20261026/host-program-after.txt). Parameter/IDs und Preset-Namen bleiben unverändert; die folgende Windows-Validator-Wiederholung unter `8a442d9` besteht47/47.

## Zusätzliche Sanitizer-Prüfung des IR-Kandidaten

Nach der großen IR-Block-Korrektur, JUCE-Module vollständig instrumentiert, Linux GCC14.2/O1: **PASS ASan+UBSan**66 Modes/Recall-Prüfungen +3 nebenläufige IR/Preset/Host-State/Bypass-Prüfungen; **PASS TSan** dieselben66+3 Fälle. Kein Sanitizer-Report; `halt_on_error=1`, Exit0. [ASan Modes](qa/step3-cpu-20261026/asan-modes.txt), [ASan Stress](qa/step3-cpu-20261026/asan-stress.txt), [TSan Modes](qa/step3-cpu-20261026/tsan-modes.txt), [TSan Stress](qa/step3-cpu-20261026/tsan-stress.txt). Dies ist eine endliche synthetische Laufzeitprüfung; kein Beweis gegen jeden Race und keine CPU-Messung.

## Zusätzlicher bestätigter RT-Fehler: großer IR-Block benutzt C-Heap

Die bisherige C++-new/delete-Prüfung erfasst direkte C-Allokationen nicht. Quellpfad JUCE8.0.4 `juce_FFT.cpp:134–172`: der Fallback-Real-FFT nimmt ab 256KiB Scratch einen `HeapBlock<char>`. Bei Convolution-Maxblock >8192 wird die FFT auf mindestens32768 gerundet; `16 + FFTSize * sizeof(complex<float>)` überschreitet die Stack-Grenze. Der Processor akzeptiert solche Blockgrößen.

**Reproduziert vor Reparatur:** derselbe echte Cabinet-/JUCE-Code, 6 Aufrufe, `malloc/calloc/realloc/free` am Linker beobachtet: bei16384 **24 mallocs +24 frees**, sowohl Mono- als auch Stereo-IR.128/256/8192 sind ohne Heap-Aufruf. Exit1, 2 von8 Fällen FAIL; PCM zur unmodifizierten JUCE-Convolution dabei Delta0. [Vorher](qa/step3-cpu-20261026/large-ir-before.txt).

**Korrektur:** Nur das interne Convolution-Quantum wird in `CabinetIR::makeEngine` auf8192 begrenzt; große AudioBlocks werden als nichtownende Views in `renderConvolution` verarbeitet. Amp-Hostblock, Regler-Rampen, Quality/OS, Decoder/Normalisierung, IR-Publikation und gemeinsame Szenenblende bleiben unverändert. Ein IR-Wechsel wird weiterhin nur einmal pro ursprünglichem Aufruf angenommen. Keine neue Latenz: Convolution bleibt zero-latency. Alle Prepare-/Allokationsarbeiten bleiben auf dem Control-Pfad.

**PASS nach Reparatur:** 10 Fälle (Mono/Stereo ×128/256/8192/**8193**/16384), null beobachtete C-Heap-Aufrufe. Vergleich gegen unmodifizierte große JUCE-Convolution über sämtliche PCM-Samples: maximal8,84756e−9, unveränderte Grenze1e−6; bis8192 Delta0. Zusätzlich gebautes echtes Linux-VST3 mit IR/Host-Reprepare16384: Delta0 zur äquivalenten Referenz, nichtstumm; alle132 bestehenden IR-/Preset-/Routing-/Ownership-Checks erneut PASS. [Nachher](qa/step3-cpu-20261026/large-ir-after.txt), [reales VST3](qa/step3-cpu-20261026/large-ir-host.txt).

Die vorherige „bytegleiches Cabinet“-Hashliste bezieht sich ausdrücklich auf den CPU-Kandidaten4efb20e. Dieser spätere, belegte Grenzfall erweitert die Implementierung, ohne die IR-Reparaturverträge aufzugeben. Die CPU-Matrix4efb20e verwendet Built-in-Cab und128/256; sie wird nicht als Messung einer langen IR ausgegeben. **Windows-PASS unter `8a442d9`:**10/10 IR-Heap-Grenzfälle sowie tatsächliche DLL bei16384 ohne beobachteten Heap-Aufruf, siehe folgenden Abschnitt.

## Windows-Funktionsnachweis des bereinigten Produkts

[Run 40386792595](https://github.com/challanger2000/125A-Dystorator/actions/runs/40386792595), Quellcommit `8a442d9d2a3ad10ba331b7cc849136db78ca08ac`, Windows2022/AMD EPYC9V74,2Kerne/4Threads, MSVC-Release/LTO. Vollständige Befunde in Dokumentationscommit`06ad8f10407a86503307ebfb936df4425c8bb5de`. **53 einzelne Build-/Test-Gates PASS; ausschließlich die beiden CPU-Aufrufe FAIL.** Keine FINAL-Freigabe aus dem Tool-Text `RELEASE: PASS` ableiten.

- PASS: Windows-x64-VST3 Before/After, Root5/5, Tone74, Fault,133 IR/Preset/Routing/Ownership-Checks,66 Modes/Recall,3 Concurrent-Stress, historische und neue27er HIGH-PCM-Orakel, Mono/Stereo-Referenzen und64 endliche Aliasdaten. Neues Orakel zur bestätigten398204e-Arbeitsbasis: Maxdelta0. Historisches8ca580d-Orakel: Maxdelta2,98023e−8, innerhalb seiner unveränderten1e−6-/−100dB-Grenzen.
- PASS: tatsächliche13 VST3-Host-Gates, darunter positiver Heap-Selbsttest, Stop/Play/Cycle mit dichten Mode/Quality/Bypass-Requests, IR-Disk-Save/Destroy/Fresh-Recall, fehlendeIR/Gain-Rollback und Built-in-Rückkehr. Transport-, Recall- und16384er PCM jeweils Maxdelta0.
- PASS: echter Prozessheap unterhalb getrennter Host-/Plugin-CRTs: **0 Alloc/Realloc,0 Frees** während der beobachteten Callbacks. Separater positiver Heap-Control besteht. Mono/Stereo-IR ×128/256/8192/8193/16384:10/10,0Heap, größtes Sampledelta8,84756e−9 zur unmodifizierten JUCE-Referenz. Keine Behauptung über sämtliche OS-Speicher-/Lock-/Treiberaufrufe.
- PASS: unveränderter125A Tester v0.2.9: **40 PASS/0 WARNING/0 FAIL**, Steinberg **47/47**, Selbsttests und alle7 Good/Bad-Fixtures mit konkretem Erwartungstext. Automatisierte Editor-Zyklen/I/O/Fresh-State enthalten, manuelle Bedien-/DAW-Abnahme NOT RUN. 64-bit-DSP weiterhin nicht unterstützt.
- FAIL (Benchmark-Aufbau): beide CPU-Aufrufe werfen vor Messbeginn eine C++-Exception. Die neue Diagnose lokalisiert sie im CPU-Fixture. Dort wird der nicht vorhandene Anzeigename `Boost Level` gesucht; exportiert ist seit der Basis **`Drive Volume`**, ID`boostLevel`. Neuer tatsächlicher VST3-Preflight reproduziert lokal `Missing hosted parameter: Boost Level`; nach Korrektur acht PASS bei44,1/48k undMono/Stereo. Der Test stellt DriveVolume zuerst auf den nicht voreingestellten Wert3,1 und dann7 und liest alle13 Benchmark-Controls aus dem tatsächlichen IComponent-State zurück. Keine Parameter-/GUI-Änderung, keine Grenzwertlockerung. Die CPU-Tabellen aus den abgestürzten Läufen bleiben ausdrücklich **NOT RUN**.

[Rohstatus](qa/step3-windows-8a442d9d2a3ad10ba331b7cc849136db78ca08ac/results.json), [tatsächlicher DLL-/Heap-/Transport-Test](qa/step3-windows-8a442d9d2a3ad10ba331b7cc849136db78ca08ac/packaged-vst3-host.txt), [IR-Heap-Grenzen](qa/step3-windows-8a442d9d2a3ad10ba331b7cc849136db78ca08ac/large_ir_heap.txt), [Preflight vor Korrektur](qa/step3-cpu-20261026/cpu-fixture-reproduced-failure.txt), [danach](qa/step3-cpu-20261026/cpu-fixture-after.txt).

Der anschließende gezielte CPU-Lauf darf lange Funktionssuiten nur überspringen, wenn Source-Tree und Root-CMake exakt zu diesem funktional geprüften Commit passen und alle benötigten früheren Funktionsgates belegt PASS sind. Diese Identität wird im Runner geprüft und protokolliert. Vollsuiten werden im CPU-Lauf als NOT RUN vermerkt und mit ihrer ursprünglichen Evidenz verknüpft; keine neue Ausführung behauptet. Heap-Selbsttest, tatsächlicher DLL-Host und der neue CPU-Preflight werden trotzdem erneut ausgeführt. Aktueller Produkt-Source-Tree: `23fe6fa178f2248c23726a31ad080a82f9e991bc`.

## Windows-CPU-Vorher/Nachher – gemessen, Leistungsziel FAIL

[Run 40406692088](https://github.com/challanger2000/125A-Dystorator/actions/runs/40406692088), getesteter Quellcommit `0d8aa76bc9450f1f057ae06c02ef5164e55b5924`, Rohbefundcommit `dd85086abc6f52303d75ca59c073b6f54298005d`. Windows2022, **AMD EPYC9V45**,2Kerne/4Threads, CIM-MaxClock2596MHz. Das ist weder der i9-11900K noch derselbe Runner wie die Funktionsprüfung aufEPYC9V74. Verglichen werden ausschließlich Before und After **innerhalb dieses einen CPU-Laufs**.

PASS: beide Release-VST3s frisch aus gepinnten Sources gebaut; Source-Tree/Root-CMake zum funktional geprüften8a442d9 identisch; positiver Prozessheap-Test und tatsächliche13 DLL-/State-/IR-/Transport-/Heap-Gates erneut bestanden. Neuer CPU-Preflight jeweils8/8 vor und nach der Optimierung. Die langen nativen Suiten und der unabhängige Tester/Validator sind in diesem gezielten CPU-Lauf **NOT RUN**, ihre ursprünglichen PASS unter8a442d9 werden über `functional-reference.json` separat belegt.

**48/48 angeforderte Konfigurationen je Version vollständig gemessen**,44,1/48k ×MONO/STEREO ×ECO/STANDARD/HIGH ×128/256 ×offline/getaktet, je160 gemessene Callbacks nach0,2s Warm-up. Tatsächliche vollständige Release-DLL inkl. VST3-Wrapper, Built-in-Cab/Cuts an, RED/Gain9/Drive8/DriveVolume7, beide Kanäle nichtstumm. Alle96 Komponenten-State-Prüfungen PASS; gedruckte linke/rechte Ausgangsenergien in allen48 Paaren gleich. Der unabhängige27er PCM-Test bleibt das strengere Audio-Orakel, kein Ableiten von Klanggleichheit allein aus Energie.

| Windows-CI-Messung | Vorher398204e | Nachher |
|---|---:|---:|
| Konfigurationen mit bestandenem Echtzeit-Gate |18/48 |15/48 |
| Konfigurationen mit mindestens einer Deadline-Verletzung |**30/48 FAIL** |**33/48 FAIL** |
| Callback-Overruns, gemessene Wallzeit >Blockdeadline |2568/7680 |2591/7680 |
| getaktete Fertigstellungs-Misses |1887/3840 |1936/3840 |

Mittel der16 relativen Laufmittel-Unterschiede je Qualität: HIGH **+0,19% Einsparung**, STANDARD **−1,08%**, ECO **−1,02%** (negative Einsparung bedeutet in diesem Lauf langsamer). **Keine nennenswerte zusätzliche Windows-CPU-Reduktion belegt; Priorität1 bleibt nicht erreicht.** Ein einzelnes Vorher/Nachher-Paar auf geteilter CI erlaubt weder eine robuste Gewinnbehauptung noch die kausale Zuordnung der kleinen negativen Unterschiede. Windows-ABBA-Wiederholungen: NOT RUN; keine zusätzliche CI-Runde, um einen günstigeren Messwert zu suchen. Die mathematisch gleichwertigen Umstellungen bleiben als geprüfter Entwicklungsstand ohne Leistungs-/FINAL-Freigabe; ihre Zielhardwarebewertung ist offen.

HIGH, getaktet; Zeitenµs. Jede Zelle Vorher → Nachher. Alle p95/p99 sind echte **jeweilige Fallquantile**, keine aus Quantilen gemittelten Sammelwerte.

| Fs / Amp / Block | Mittel | p95 | p99 | Maximum | Callback-Overruns /160 | Scheduler-Misses /160 |
|---|---:|---:|---:|---:|---:|---:|
| 44.1k / MONO /128 | 2366.92 → 2284.36 | 2436.80 → 2354.80 | 2498.60 → 2422.60 | 2594.00 → 2441.50 | 0 → 0 | 108 → 87 |
| 44.1k / MONO /256 | 4673.72 → 4747.50 | 4840.20 → 4912.10 | 4975.00 → 4987.80 | 5530.40 → 4999.10 | 0 → 0 | 50 → 40 |
| 44.1k / STEREO /128 | 5095.24 → 5051.95 | 5211.50 → 5176.60 | 5757.00 → 5221.90 | 6406.90 → 5275.00 | 160 → 160 | 160 → 160 |
| 44.1k / STEREO /256 | 10688.50 → 10025.90 | 12730.00 → 10251.30 | 17721.80 → 10539.90 | 18222.40 → 10917.60 | 160 → 160 | 160 → 160 |
| 48k / MONO /128 | 2345.72 → 2382.20 | 2439.50 → 2455.50 | 2749.70 → 2553.80 | 3029.50 → 2908.20 | 2 → 1 | 129 → 130 |
| 48k / MONO /256 | 4650.33 → 4725.92 | 4752.50 → 4848.50 | 4905.30 → 7370.70 | 5130.90 → 7508.40 | 0 → 2 | 88 → 99 |
| 48k / STEREO /128 | 5279.01 → 4944.81 | 8204.40 → 5110.20 | 9763.70 → 5191.20 | 11143.80 → 5285.00 | 160 → 160 | 160 → 160 |
| 48k / STEREO /256 | 9952.76 → 10102.30 | 10115.10 → 11004.20 | 14626.20 → 16999.70 | 16154.60 → 19878.50 | 160 → 160 | 160 → 160 |

Blockdeadline:44,1k/128=2902,49µs;44,1k/256=5804,99µs;48k/128=2666,67µs;48k/256=5333,33µs. HIGH-Stereo verfehlt hier schon im Mittel die Blockdeadline. Kein stummer Kanal/abgeschalteter Drive als vermeintliche Einsparung; kein abgesenkter Solver- oder Testgrenzwert.

[Alle48 Vorherfälle](qa/step3-windows-0d8aa76bc9450f1f057ae06c02ef5164e55b5924/cpu-before.csv), [alle48 Nachherfälle](qa/step3-windows-0d8aa76bc9450f1f057ae06c02ef5164e55b5924/cpu-after.csv), [paarweiser Vergleich](qa/step3-windows-0d8aa76bc9450f1f057ae06c02ef5164e55b5924/cpu-comparison.csv), [Zusammenfassung inkl. Rohlog-SHA256](qa/step3-windows-0d8aa76bc9450f1f057ae06c02ef5164e55b5924/cpu-summary.json), [Original-Runnerstatus](qa/step3-windows-0d8aa76bc9450f1f057ae06c02ef5164e55b5924/results.json). Reproduzierbare Auswertung: `python docs/qa/step3-cpu-20261026/extract_windows_cpu.py docs/qa/step3-windows-0d8aa76bc9450f1f057ae06c02ef5164e55b5924`. Die gedruckten Fallaggregate bleiben unverändert; einzelne Callback-Dauern wurden vom Host nicht als separate Zeitreihe gespeichert.

## Vollständige Laufprovenienz

| Actions-Run | getesteter Quellstand | Tatsächlicher Ausgang |
|---|---|---|
| [40375703107](https://github.com/challanger2000/125A-Dystorator/actions/runs/40375703107) |4efb20e | Release-Build PASS, Testhost-Compiler FAIL; nachgelagert NOT RUN |
| [40378455766](https://github.com/challanger2000/125A-Dystorator/actions/runs/40378455766) |d323881 | native Audio-/IR-Tests PASS; Host-/CPU-Aufrufe FAIL; Programlist46/47 |
| [40382507982](https://github.com/challanger2000/125A-Dystorator/actions/runs/40382507982) |d11df7f | neuer IR-Quellstand gebaut/native Regressionen PASS; großer Heap-Probetest und DLL-Host AV, CPU-Setup FAIL, Programlist46/47; keine verwertbare Heap-/CPU-Freigabe |
| [40385148764](https://github.com/challanger2000/125A-Dystorator/actions/runs/40385148764) |40f1655 | in Warteschlange ersetzt, kein Windows-Job/Build ausgeführt, NOT RUN |
| [40386272396](https://github.com/challanger2000/125A-Dystorator/actions/runs/40386272396) |9226bdc | in Warteschlange ersetzt, kein Windows-Job/Build ausgeführt, NOT RUN |
| [40386792595](https://github.com/challanger2000/125A-Dystorator/actions/runs/40386792595) |8a442d9 |53 Build-/Funktionsgates PASS, einschließlich Heap und47/47 Validator; zwei CPU-Setup-Aufrufe FAIL |
| [40406692088](https://github.com/challanger2000/125A-Dystorator/actions/runs/40406692088) |0d8aa76 | gezielter CPU-Lauf:12 Build-/Host-/Setupgates PASS; zwei vollständige CPU-Matrizen wegen Deadlines FAIL; Vollsuiten NOT RUN mit geprüftem Funktionsreferenz-Link |

Alle fünf ausgeführten Windows-Jobs haben daher korrekt den Gesamtstatus **FAIL**; ein bestandener Funktionsabschnitt macht den gesamten Schritt nicht grün. Die zwei abgelösten Warteschlangenstände haben keinen zusätzlichen Windows-Build ausgeführt. Der falsche Anzeigename gehörte zum neu ergänzten Benchmark und war kein Plugin-Parameterfehler. Der gezielte letzte Lauf dauert laut nativen Einzellogs rund8,7 Minuten; die schon belegten langen Funktionssuiten wurden nicht erneut gebaut/ausgeführt. Keine weiteren Builds für die Dokumentationsübergabe.

## Windows-Verfahren und Reproduktion

Siehe [tests/step3/README.md](../tests/step3/README.md). Der Workflow baut Basis und Kandidat als Windows-x64-Release-VST3 mit identischen ursprünglichen JUCE-/LTO-Flags und misst beide mit demselben realen VST3-Testhost auf demselben Runner. Durchschnitt, p95, p99, Maximum, Callback- und Scheduling-Deadlines werden aufbewahrt. Ein einziges Overrun bleibt FAIL. Der Root-QA-Sammelbuild wird automatisch aus den registrierten Tests abgeleitet, damit kein CTest-Executable beim gezielten Windows-Build fehlt.

Der Windows-Testhost beobachtet den Heap unterhalb verschiedener CRTs mit Microsoft Detours v4.0.1, SHA `e4bfd6b03e50de46b47abfbd1e46b384f0c5f833`; Detours wird ausschließlich in den Testhost gelinkt, niemals in den Amp. Ein echter Allocate/Reallocate/Free-Selbsttest muss die Beobachtung positiv bestätigen. Das ist eine Heap-Prüfung, kein vollständiger Beweis gegen jeden direkten OS-Aufruf. Auf Linux wird dieser Windows-Test ausdrücklich NOT RUN gemeldet.

Windows-Läufe werden ausschließlich für den tatsächlichen Kandidaten und belegte notwendige Compiler-/Integrationskorrekturen ausgeführt; alle fehlgeschlagenen Läufe bleiben dokumentiert. Die Actions-API ist in dieser Cloud mit Forbidden blockiert; Git funktioniert. Deshalb publiziert der Workflow seine Rohbefunde einschließlich FAIL direkt als eigenen Dokumentationscommit auf denselben Entwicklungsbranch sowie als Actions-Artefakt. Der getestete Quellcommit wird explizit vermerkt. Dokumentationscommits lösen keinen weiteren Build aus. Windows-CI-Messungen sind ausdrücklich **keine Messung auf dem i9-11900K des Eigentümers**.

Die neuen PCM-Orakel prüfen zusätzlich beide Referenz-/Kandidatenströme und alle Rail-Ströme explizit auf Endlichkeit. Ein `std::max`-Delta allein könnte NaN übersehen. Die bestehenden Fehlergrenzen bleiben unverändert; strengere Assertions dienen der Aussagekraft, keinem künstlichen PASS.


Nach der Metadatenkorrektur: PASS133/133 echte IR/Preset/Routing/Ownership-Checks und PASS11/11 gebautes Linux-Release-VST3, einschließlich strenger Endlichkeitsprüfung beider PCM-Ströme. [Integration](qa/step3-cpu-20261026/final-ir-preset-regressions.txt), [VST3](qa/step3-cpu-20261026/final-linux-vst3-host.txt). Alle17 eingefrorenen Baseline-Header erneut gegen398204e inklusive SHA256 und ausschließlich Namespace-Ersetzung geprüft. [Produkt-Manifest](qa/step3-cpu-20261026/product-source-40f1655.json).

Der Windows-Heap-Beobachter wird außerdem gegen Zugriffe während der Thread-/Loader-Initialisierung abgesichert: Hooks lesen zuerst einen reservierten festen Win32-TEB-TLS-Slot, bevor C++-TLS-Counter berührt werden. Scope initialisiert die Counter, bevor dieser Slot aktiv wird; GetLastError bleibt erhalten. Fehlende Slots, fehlgeschlagene Installation/Selbsttest oder Detach sind Fehler, kein PASS. Das behebt eine unsichere Annahme des Beobachters; die Funktionsfähigkeit ist nun unter `8a442d9` durch positive Controls und tatsächliche DLL-Callbacks bestätigt. Die früheren AVs besitzen keine verwertbaren Stacklogs; deren exakter historischer Faulting-Zugriff wird deshalb nicht nachträglich als bewiesen ausgegeben. Ausschließlich Testhost, keine Plugin-Hooks.


## Anforderungs-/Testmatrix und Grenzen

| Vertrag und Kette | Positiver Nachweis | Negativ-/Grenzfall und tatsächliches Orakel | Ergebnis / verbleibende Grenze |
|---|---|---|---|
| HIGH: Controls → beide Amp-Lanes → PCM |27 Render zur eingefrorenen398204e-Basis,44,1/48/96k, alle Kanäle/Drive-Modelle | wechselnde Gain/Presence/Tube/Channel/Boost/Model; finite Referenz und echter Samplefehler statt nur Pegelvergleich | Linux und Windows unter `8a442d9` PASS Maxdelta0; reales DI/level-matched Hören NOT RUN |
| Rails → aktive/fadende Schaltung → Audio/Versorgungsströme |24000 Samples gegen separat eingefrorenen Preamp | Custom-Initialrails311/326, Sample-Railmodulation, Kanal/Crunch-Blenden; alle sechs Werte endlich und exakt gleich | Linux und Windows unter `8a442d9` PASS inklusive verschärfter Endlichkeitsassertion |
| Solver/Matrix/CPU-Dispatch | unabhängiger vollständiger Jacobian, Scalar/SSE2/Auto; Root5/5 | Conducting/Nonconducting-Ports, Pivot/Singularität, SIMD-Restlängen/Guards; bestehende Grenzen unverändert | Linux/Windows unter `8a442d9` PASS; abgelehnte Prototypen nicht im Produkt |
| LOW/HIGH Cuts und bestehende Signalmodelle |74 alte Tone-Checks, echte Processor-Cut-Orakel, Stereo-Referenz | hohe Pegel/Extreme/Fault-Recovery; Hashgleichheit der übrigen Röhren-/Solverquellen | Linux/Windows unter `8a442d9` PASS; keine neue analoge Klangbehauptung |
| IR-Datei → Decoder → vorbereitete Engine → Mono/Stereo-Audio | echte WAV/AIFF-Impulse, Resampling, zwei unabhängige Kanäle | beschädigte/trunkierte/fremde/fehlende Dateien, unsupported Multichannel, Größenlimit; vorherige vollständige Szene bleibt gültig | Linux133/133 einschließlich neuer Program-Metadaten; Windows unter `8a442d9` ebenfalls133/133 PASS |
| Große IR-Blöcke → JUCE-FFT → PCM/Heap |Mono/Stereo ×128/256/8192/8193/16384, unmodifizierte JUCE als Sample-Orakel | vorher16384:24 malloc+24 free/6Calls; nachher0 und Delta<9e−9, strikte1e−6-Grenze | Linux und Windows unter `8a442d9` PASS; Windows-Prozessheap mit bestandenem positiven Selbsttest |
| Preset/Host-Scene → Disk → frische Instanz → hörbarer Recall | reale VST3-Speicherung, Instanz zerstört, aus Datei neu geladen | fehlende IR plus geänderter Gain: vollständiger Rollback; ClearIR muss Pfad leeren und PCM ändern | Linux und Windows unter `8a442d9` PASS, tatsächlicher DLL-Recall Maxdelta0 |
| Audio/Control nebenläufig: IR/Presets/Quality/Bypass |66 Modes +3 Stresstests, tatsächliche Audioausgabe | schnelle überlappende Requests, zuletzt angeforderter Zustand wird aktiv, keine C++new/delete; ASan/UBSan und TSan ohne Report | Linux PASS; Windows native66+3 PASS; C++new/delete allein beweist keinen DLL-/C-Heap |
| Modus/Quality/Automation/Transport → VST3 → PCM | bewegter Stop/Play/Cycle-Kontext gegen durchgehenden Referenzstream,180 Blöcke |30 dichte Modus-/Quality-Requests, Bypass70/95, keine Latenzänderung und nichtstumm; tatsächliche Samplewerte | Linux11 und Windows13 Wrapper-Checks PASS; keine Abnahme der Sample-Offset-Genauigkeit sämtlicher Parameter oder realer Host-Transportwechsel |
| Latenz/Bypass → Host-Dry/Warm-Wet |44,1/48/96k Mono/Stereo und alle effektiven OS-Faktoren; gepaddete HIGH-Latenz | R-only, Reprepare, leere/kleine/große Blöcke; Dry muss zeitverschoben exakt stimmen und Wet weiterlaufen | native Linux/Windows unter `8a442d9` PASS; Windows unter `8a442d9` zusätzlich tatsächlicher DLL-Dry/Warm-Wet-/Quality-Nachweis |
| Qualitätsdaten → Alias-/Artefaktindikator |64 synthetische Spektralrender mit16×-Vergleich |1/3/5/9k, zwei Pegel, tatsächliche kohärente Frequenz und Messmasken ausgewiesen | Messdaten PASS; ECO/STANDARD Klangfreigabe NOT RUN, deutliche Alias-Nachteile belegt |
| Realtime-CPU → komplette Verarbeitung → Deadline | 48 Konfigurationen in4 ABBA-Läufen (192 Fallläufe), je160 gemessene Callbacks |44,1/48k ×128/256 ×Mono/Stereo ×alle Qualitäten ×offline/getaktet, bereits eine Verletzung FAIL | Linux FAIL nachher77/96; vollständige Windows-Matrix unter0d8aa76 FAIL nachher33/48, beide Kanäle/States gültig |
| Export-Metadaten → VST3 Programlist → Validator | fester Programm 0-Name und unveränderte Szene beim Anwählen | alter leerer Name reproduziert133Checks/1FAIL; Validator46/47 im zweiten Lauf | lokale Korrektur2/2 und Windows unter `8a442d9` Validator47/47 PASS |
| Unabhängige125A QA → Produkt-VST3 | unveränderter gepinnter Tester, Job-Object-/Validator-Selbsttest und7 Positiv-/Negativfixtures | konkrete erwartete Fehlertexte, kein pauschales Akzeptieren von Exit1; Editor5Zyklen, I/O und Fresh-State geprüft | Windows unter `8a442d9` Tool-Selbstprüfung, Produkt40/40 und Validator47/47 PASS; keine FINAL |
| Tatsächlicher Arbeitsplatz → ASIO/DAW → aufgezeichnete Audioausgabe | Owner-i9-11900K, StudioOne/ASIO, reale DI, Export/Realtime-Vergleich erforderlich | lange Sitzung, Dropout-Aufzeichnung, schnelle Umschaltungen, Projekt schließen/öffnen, Hörabgleich | NOT RUN: entsprechende Hardware/DAW/Fixtures fehlen; CI ersetzt diese Kette nicht |


## Änderungen und verbleibender Reparaturplan

Die Produktänderung gegenüber398204e betrifft ausschließlich sechs Dateien:

| Datei | Technische Änderung |
|---|---|
| `Source/dsp/AmpSim.h` | identisches Pedal-Mischgewicht zwischenspeichern |
| `Source/dsp/Preamp5150.h` | exakte Rails nur unmittelbar vor konsumierenden/fadenden Schaltungen schreiben |
| `Source/dsp/Simd.h` | AVX-Einstieg und privaten MSVC-Inline-Helfer getrennt kennzeichnen |
| `Source/dsp/DKCircuit.h` | Tile-Helfer privat und innerhalb des geschützten AVX-Einstiegs inlinebar |
| `Source/CabinetIR.h` | FFT-/Convolution-Quantum auf8192 begrenzen; große Hostblöcke ohne Besitz/Allokation unterteilen |
| `Source/PluginProcessor.h` | angebotenen festen Host-Programmeintrag0 als `Default` benennen |

Zusätzlich: Root-CMake baut sämtliche registrierten QA-Executables; neue unabhängige Referenz-/Rail-/PCM-/Aliastests, tatsächlicher VST3-Host und Windows-Prozessheap-Beobachter, begrenzter Windows-Workflow und Rohbefunde. Kein Detours-/Tester-Code im Produkt. Kein geänderter Parameter/ID, keine GUI-Datei, kein zusätzlicher Effekt, keine geänderte Röhrenkennlinie, Solver-Toleranz oder Iterationsgrenze. Der ausgeschlossene SIMD-Solver bleibt ausschließlich als abgelehntes Testexperiment erhalten.

**P1 – CPU-/Aussetzerfreigabe offen.** Die Linux-Matrix verfehlt die strikten Deadlines; Windows verfehlt das Gate nachher in33/48 Fällen; die zusätzliche HIGH-Einsparung von0,19% ist praktisch nicht belastbar. Normale CI-Threads und `sleep_until` sind keine ASIO-Terminierung. Callback-Wallzeiten, Scheduler-Misses und DSP-Hotspots sind getrennt zu interpretieren. Lange externe IRs und die zusätzliche Last während Quality-/Mode-Blenden sind nicht durch die Built-in-/Steady-State-CPU-Matrix abgenommen. Der i9-11900K ist NOT RUN.

**Empfohlener nächster Schritt:** denselben398204e-Before- und aktuellen After-Stand auf dem tatsächlichen i9-11900K in einem echten Windows-ASIO-/DAW-Signalweg bei44,1/48k und128/256 Samples messen, einschließlich RED/hohemGain/Drive, Built-in sowie repräsentativer Mono-/Stereo-IR und schnellen Umschaltungen. Erst damit das verbleibende CPU-Budget bestimmen und gezielt Preamp/Poweramp/Newton auf dieser Plattform profilieren. Keine Solver-Iteration kürzen, bevor unabhängige PCM-/Fehlerorakel eine mathematisch gleichwertige alternative Implementierung belegen. Die bisher geprüften langsameren Solver-Varianten bleiben verworfen.

**P2 – Klangabnahme offen.** ECO/STANDARD haben belegte frequenz-/pegelabhängige Alias-Nachteile. Reale lizenzierte DI, level-matched Hörvergleich, Dynamik-/Exportkontrolle und Langzeitsitzung im Zielhost bleiben NOT RUN. HIGH bleibt die398204e-Referenz; daraus folgt keine Behauptung aliasfreier Analogmodellierung.

**P2 – Host-/Echtzeitgrenze.** Der Prozessheap-Test belegt null beobachtete Heap-Aufrufe in seinen Callback-Fenstern, nicht sämtliche direkten OS-Allokationen/Locks/Treiberpfade. Automatisierte Prozesskontexte, State-Dateien und Editor-Zyklen ersetzen keine reale StudioOne-Projektsitzung mit aufgezeichneten Dropouts. Keine vollständige Sample-Offset-Automationsabnahme aller72 vorhandenen IDs behauptet. Die alte Host-Logzeile trägt noch die historische Beschriftung `Linux packaged VST3`; Plattformnachweis des Windows-Laufs sind dessen PE-Release-Build, `.exe`-Aufruf und Windows-Metadaten.

**Keine FINAL-/Release-Freigabe.** `main` und der ursprüngliche IR-Reparaturbranch bleiben unverändert. Kein Merge. Alle alten FAIL-Protokolle bleiben im Repository erhalten.

Vollständige Dateiliste gegenüber398204e: [changed-files.txt](qa/step3-cpu-20261026/changed-files.txt). [Abschließendes Source-/Hashmanifest](qa/step3-cpu-20261026/final-product-source-manifest.json):32 Produktdateien, davon26 bytegleich zur Basis und genau die oben genannten6 Änderungen; Source-Tree identisch zu beiden Windows-Nachweisen.
