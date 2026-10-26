# 125A Dystorator – Schritt 4: CPU-Entscheidungsgrundlage auf dem realen i9

Stand: 2026-10-26. **Kein RELEASE, keine DSP-Änderung.** Ausgangspunkt: Entwicklungsbranch dev/compact-stereo-on-ir-fix-20261026, Quellstand 44a184599abeaaa928f6751ce10a71b574ea499c. Dieser Arbeitsbranch ist ein isoliertes Diagnoseexperiment, kein Merge in den Produktbranch.

## Messbasis und Engpass

Windows CI (AMD EPYC, nicht i9-11900K) ergab unter Run 40406692088 gegenüber dem genehmigten CPU-Ausgangsstand 398204e26d84c3f40bcd1c0af55dfa8e97893ca7 nach dem Zusatzschritt 33/48 statt vorher 30/48 Deadline-Verletzungsfälle. Nur 0,19 % HIGH-Einsparung in diesem Lauf, nicht signifikant gegenüber Lastdrift. Im 48-kHz-HIGH-STEREO-Fall mit 128 Samples lag die aktuelle Callback-Wallzeit bei durchschnittlich 4944,81 us gegenüber 2666,67 us Blockdeadline. Es ist keine ASIO-Messung. Auch die 2026-10-26 Linux-Profilierung ordnet die größten inklusiven Kosten Preamp und Poweramp/Newton zu, nicht primär Cabinet/Oversampling.

Deshalb keine ungesicherte Veränderung an Solveriteration, Konvergenz-/Fault-Schranken, Oversampling, Klangreferenz oder Stereo-Isolation. Die verworfenen SIMD-Port-Solve-Varianten bleiben verworfen.

## Neues Arbeitsmittel

Die PowerShell-Datei tests/step4/owner_cpu_vst3.ps1 startet den EXISTIERENDEN gebauten compact_vst3_host.exe viermal. Sie misst **Before → After → After → Before** mit der vom Testhost bereits definierten 48-Fälle-Matrix: 44,1/48 kHz, Mono/Stereo, ECO/STANDARD/HIGH, 128/256 Samples, offline/getaktet; 160 gemessene Callbacks nach Warm-up pro Fall.

Benötigt werden ein verifiziert gebauter Windows-x64-Testhost sowie getrennte, nachweisbar gepinnte Before- und After-VST3-Bundles. Das Werkzeug baut nichts, verwendet keine GitHub Actions und lädt keine fremden Binärdateien herunter. Es sollte auf der tatsächlichen i9-11900K-Zielmaschine ausgeführt werden, **nicht** als neue Cloud-CPU-Schleife.

Beispiel (Pfade sind Platzhalter; erst vorhandene echte Binärdateien mit ihren Build-Quellen verifizieren):

    powershell -ExecutionPolicy Bypass -File tests\step4\owner_cpu_vst3.ps1 -HostExe "C:\QA\compact_vst3_host.exe" -BeforeVst3 "C:\QA\before\Djenty Donkey.vst3" -AfterVst3 "C:\QA\after\Djenty Donkey.vst3" -OutputDir "C:\QA\dystorator-i9-evidence"

Gespeichert werden echte SHA-256 der Windows-VST3-Binärdateien und des Hosts, CPU/OS/Energieprofil, vier vollständige Rohlogs und stderr-Logs, alle 192 Messfälle in CSV, ABBA-Vergleich je Konfiguration sowie JSON-Zusammenfassung. Ein Deadline-FAIL des Testhosts wird **als FAIL mit vollständigen Daten** behandelt; nicht als Erfolg oder als unvollständiger Testlauf. Nichtfinite Messwerte, stumme Kanäle, fehlender Komponentenzustand, unvollständige Matrix und fehlerhafte Exitcode-/Status-Kombinationen brechen ab. Es wird keine vorbestehende Evidenz überschrieben.

**Grenze:** Aus SHA-256 allein lässt sich nicht beweisen, welcher Quellcommit in einer DLL steckt. Die Hashes mit vertrauenswürdigen Build-/Source-Manifesten vergleichen. Der Test ist Windows-VST3-Hostdiagnostik, keine echte Audio-ASIO-Deadline. Echte Dropouts müssen separat in Studio One mit reproduzierbaren DI-Aufnahmen, 44,1/48 kHz, 128/256 und Mono/Stereo aufgenommen werden.

## Abnahme weiterer CPU-Änderungen

Zuerst die Zielhardware-Budgetlücke gegen echte Referenzen bestimmen, dann nur die tatsächlich dominierende Solver-Ursache profilieren und als isolierten Kandidaten verbessern. Vor Aufnahme in Produktcode: vollständiger, unter identischen Bedingungen replizierter End-to-End-CPU-Vorteil über Wiederholungsdrift, 27/27 HIGH PCM Nulltests gegen Stand 398204e, Rail-/Fault-/IR-/State-/Bypass-/Transport-Regressionen, Heapkontrolle und Windows-Host-/Validator-Kette. Kein beliebiges Kürzen von Newton-Iterationen. Kein Windows-Build ohne überprüfbare Notwendigkeit.

Status: **Prüfwerkzeug implementiert, Windows/i9-Ausführung NOT RUN, tatsächliche neue DSP-Optimierung NICHT IMPLEMENTIERT, keine FINAL-Freigabe.** Der Plugin Tester v0.2.9 ist nur historischer Pin der Step3-QA; v0.3.2 muss vor neuem verbindlichem Validator-/125A-QA-Lauf separat geprüft und gepinnt werden.
