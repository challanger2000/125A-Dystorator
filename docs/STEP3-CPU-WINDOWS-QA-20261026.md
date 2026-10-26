# Schritt 3 – CPU-Profil und Windows-x64-VST3-QA

2026-10-26, Europe/Berlin; technische Messlogs in UTC. Entwicklungsbranch `dev/compact-stereo-on-ir-fix-20261026`, bestätigte Basis `398204e26d84c3f40bcd1c0af55dfa8e97893ca7`. **Keine FINAL-Freigabe.**

## Kandidat und verbindliche Grenzen

Dieser Bericht wird nach dem gebündelten Windows-Lauf mit dessen tatsächlichen Ergebnissen ergänzt. **Windows-Build, Windows-Heap-Probe, 125A Tester und Steinberg Validator sind bis zum belegten Lauf ausdrücklich NOT RUN.** Eine vorbereitete Workflow-Datei ist kein Windows-Nachweis. Die CPU-Messungen des endgültigen Kandidaten werden getrennt von den abgelehnten Prototypen ausgewertet.

Pflichteinstieg und relevante Engineering-/QA-Vorgaben wurden erneut geprüft: `125A-Engineering/START-HERE.md`, unveränderter Stand `b6df84b0a04d01e240cff745ccebf60d67750486`; README, relevante Guardrails/Standards, Release-/Realtime-/Audio-Fixture-QA, Oversampling-Research und Roadmap. „verstanden“ bestätigt. Kein genehmigtes Windows-FULL-Paket dieses Entwicklungsforks erfunden. Benutzerdefinierte Arbeitsbasis ist der oben genannte Commit.

Keine Änderung an GUI, Parametern/IDs, IR-/Preset-Verträgen, Cabinet, Qualitätsstufen, Oversampling-Faktoren, Röhrenkennlinien, Solver-Iterationen oder Solver-Toleranzen. `main` und der ursprüngliche IR-Reparaturbranch bleiben unverändert.

## Reales HIGH-Stereo-Profil

Linux x86_64, GCC 14.2/O3/NDEBUG mit Debugsymbolen, JUCE 8.0.4. Vollständiger echter Processor, HIGH/Stereo, RED/Gain9, Precision Drive an/Drive8/Level7, Master3, Built-in-Cab und beide Cuts an, Noise/Gate aus, 48 kHz/256 Samples. Profilfenster erst nach Prepare und Warm-up, 2048 Audio-Callbacks. Linux `perf` 6.12.107, Software-CPU-Clock, 499 Hz, DWARF-Stack 16384 Bytes. Keine Source-Instrumentierung des Produkt-DSP; kontrollierter Enable/Disable mit bestätigtem perf-Ack. Aufbau und Aufwärmung sind nicht im Profilfenster.

**MEASURED:** 12985 Samples, 0 verlorene Samples im Baseline-Lauf. Inklusive Stichprobenanteile: AmpSim etwa 99,58 %, gekoppelter Amp etwa 91,34 %, Preamp etwa 57,49 %, Poweramp etwa 33,75 %. Matrix-AVX-Einstieg etwa 13,02 % inklusive. Newton-/Port-Elimination und Röhrenauswertung liegen innerhalb von Preamp/Poweramp und dürfen nicht nochmals zum Gesamtanteil addiert werden. Oversampling, Cabinet, Cuts und Host-/Processor-Umgebung liegen im verbleibenden kleinen Anteil; der Oversampler ist hier nicht der Hauptengpass. Vollständige Symboltabelle statt erfundener exklusiver Modulzeiten im Evidenzverzeichnis.

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
| Endgültige CPU-Matrix | 44,1/48 kHz × Mono/Stereo × 128/256 × alle Qualitäten, offline/getaktet, 160 gemessene Blöcke je Fall | Auswertung folgt; abgelehnte Kandidaten getrennt, keine PASS-Behauptung |
| Windows Release / tatsächlicher VST3 | gebündelter gezielter Workflow vorbereitet | NOT RUN bis belegtes CI-Ergebnis |
| Windows Callback-Heap | processweite RtlAllocateHeap/RtlReAllocateHeap/RtlFreeHeap-Hooks, nur Audio-Thread-Zeitfenster, tatsächliche DLL-/CRT-Grenze | NOT RUN; Host-only `operator new` wäre hierfür unzureichend |
| 125A Tester / Steinberg | Tester-Source `3dcf3fe607008e520dcf20d1f793d9c534a9b9e1` (v0.2.9), SDK `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96` (3.8.1 build84), Selbsttests und Positiv-/Negativfixtures vor Produkturteil vorgesehen | NOT RUN; kein geprüftes Tester-Releasepaket aus dem veralteten CURRENT-Eintrag erfunden |
| Tatsächlicher i9-11900K, ASIO/Studio One, echte DI und Hörvergleich | keine entsprechende Hardware/DAW oder freigegebene DI in dieser Umgebung | NOT RUN, keine Aussetzer-/Klangfreigabe |

## ECO / STANDARD und Aliasing

Zusätzlich zu den früheren 3-kHz-Messungen werden kohärente 1/3/5/9-kHz-Töne bei zwei Eingangspegeln im tatsächlichen nichtlinearen Amp gerendert. FFT 8192, Hann-Fenster; echte Grundfrequenz wird auf ein ungerades FFT-Bin gelegt. Messmaske umfasst auf Host-Nyquist gefaltete harmonische Bins bis Ordnung 256, ohne die ±4-Bin-Schutzbereiche echter hörbarer Harmonischer. Das Ergebnis `folded_bins_db` ist ein gezielter Alias-/Artefaktindikator relativ zur Fundamentalenergie, **keine perfekte Trennung von Aliasing, transienten Resten und sonstigen inharmonischen Anteilen**. Die zusätzliche 16×-Referenz ist ebenfalls nicht mathematisch aliasfrei und verwendet eine andere interne Integrationsrate.

Schon die neuen Messungen zeigen erhebliche Pegel-/Frequenzabhängigkeit. ECO ist bei starkem 5-kHz-Eingang deutlich schlechter als HIGH; auch HIGH ist beim extremen 9-kHz-Test keine aliasfreie Referenz. HIGH bleibt der unveränderte Klangvertrag der Arbeitsbasis, nicht die Behauptung einer fehlerfreien analogen Referenz. Vollständige Zahlen werden archiviert. Keine automatische oder subjektiv unbelegte Klangfreigabe für ECO/STANDARD.

## Windows-Verfahren und Reproduktion

Siehe [tests/step3/README.md](../tests/step3/README.md). Der Workflow baut Basis und Kandidat als Windows-x64-Release-VST3 mit identischen ursprünglichen JUCE-/LTO-Flags und misst beide mit demselben realen VST3-Testhost auf demselben Runner. Durchschnitt, p95, p99, Maximum, Callback- und Scheduling-Deadlines werden aufbewahrt. Ein einziges Overrun bleibt FAIL. Der Root-QA-Sammelbuild wird automatisch aus den registrierten Tests abgeleitet, damit kein CTest-Executable beim gezielten Windows-Build fehlt.

Der Windows-Testhost beobachtet den Heap unterhalb verschiedener CRTs mit Microsoft Detours v4.0.1, SHA `e4bfd6b03e50de46b47abfbd1e46b384f0c5f833`; Detours wird ausschließlich in den Testhost gelinkt, niemals in den Amp. Ein echter Allocate/Reallocate/Free-Selbsttest muss die Beobachtung positiv bestätigen. Das ist eine Heap-Prüfung, kein vollständiger Beweis gegen jeden direkten OS-Aufruf. Auf Linux wird dieser Windows-Test ausdrücklich NOT RUN gemeldet.

Nur ein gebündelter Windows-Lauf für den bereinigten Kandidaten vorgesehen. Die Actions-API ist in dieser Cloud mit Forbidden blockiert; Git funktioniert. Deshalb publiziert der Workflow seine Rohbefunde einschließlich FAIL direkt als eigenen Dokumentationscommit auf denselben Entwicklungsbranch sowie als Actions-Artefakt. Der getestete Quellcommit wird explizit vermerkt. Dokumentationscommits lösen keinen weiteren Build aus. Windows-CI-Messungen sind ausdrücklich **keine Messung auf dem i9-11900K des Eigentümers**.
