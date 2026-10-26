# Kompakter Stereo-Amp: technische Fortsetzung und QA

Historischer Stand `8ca580d`: Die anschließende beauftragte CPU-/Qualitätsänderung und ihre neuen Messungen sind in [CPU-QUALITY-MODES-20261026.md](CPU-QUALITY-MODES-20261026.md) dokumentiert. Die folgenden ursprünglichen Befunde bleiben als Baseline erhalten.

Datum: 2026-10-26 (Europe/Berlin). Entwicklungsbericht, **keine FINAL-Freigabe**.

## Verbindliche Basis und Änderungsgrenze

- Repository `challanger2000/125A-Dystorator`, Branch `dev/compact-stereo-on-ir-fix-20261026`.
- Ausgangscommit `30988fcae3785e1efb5e491277948416d1c97935`, auf dem bereits begonnenen Stereo-Umbau.
- Übernommene IR-/Preset-Reparatur `1009fa6906746f46516107baf059bf12d473a04f`; unabhängiger Audit im Repository.
- Zentrale Vorgaben zuerst über `125A-Engineering/START-HERE.md` gelesen; Engineering-Stand `b6df84b0a04d01e240cff745ccebf60d67750486`. Anwendbar: Guardrails, DSP, Plugin-Verhalten, Workflow, AI-Assisted Engineering, Release-/Realtime-QA, Audio-Fixtures und Oversampling-Referenzen.
- Kein etabliertes 125A-Windows-FULL/Gumroad-Paket dieses neuen Forks als Release-Basis erfunden. Verbindliche Entwicklungsbasis ist der ausdrücklich beauftragte Commit.
- `main` und ursprünglicher Reparaturbranch werden nicht verändert, nichts wird gemergt, keine Actions angestoßen.
- GUI, Parameterdefinitionen, AmpSim, Preamp5150, PowerAmp5150, Röhrenmodelle, Solver und Oversampling bleiben gegenüber der Entwicklungsbasis unverändert. Der gesamte ursprüngliche Amp-DSP außer dem bereits hinzugefügten GuitarCuts ist außerdem bytegleich zum IR-Reparaturstand. Nachweise: Git-Diff und [Quelltext-SHA-256](qa/compact-stereo-20261026/source-sha256.txt).

## Tatsächlich korrigiert

### S01: Host-Bypass hatte weder Latenzausgleich noch warme Amp-Historie

Der Processor überschreibt auf der Basis den Float-Bypass nicht. JUCE 8.0.4 `AudioProcessor::processBypassed` leitet trocken ohne die gemeldete Latenz durch, löscht zusätzliche Ausgänge und ruft die Amp-Verarbeitung nicht auf. Das verletzt insbesondere Mono→Stereo und die kontinuierliche Zustandsentwicklung beim Bypass.

Der zuerst ergänzte echte Audiotest reproduziert **8 Fehler in 12 Prüfungen**, vier Buskombinationen, Exit 1: [vorher](qa/compact-stereo-20261026/bypass-before.txt). Die Korrektur in `Source/PluginProcessor.h` und `Source/PluginProcessor.cpp:100,117,193,259` führt einen vorallokierten trockenen Verzögerungspfad mit exakt der gemeldeten ganzzahligen Amp-Latenz ein. Amp, Cabinet und Cuts laufen während Host-Bypass weiter. Eine gemeinsame lineare 5-ms-Blende wählt das Ausgangssignal; bei erneutem Umschalten setzt sie vom aktuellen Mischwert fort. Keine Änderung der Amp-Arithmetik.

Die 5 ms sind eine **empirisch gewählte Übergangszeit**, keine behauptete Hardware-Eigenschaft. Nach Abschluss der Blende werden tatsächlich verzögertes Dry-Audio und eine durchgehend laufende Wet-Referenz verglichen. Die bestehende Latenzrundung des originalen AmpSim bleibt erhalten.

### S02: CAB Off fror die Convolution und ausstehende IR-Wechsel ein

Die Basis rief `cabinet.processStereo` nur bei `cabOn` auf. Jetzt verarbeitet die Cabinet-Stufe kontinuierlich und der vorallokierte Cabinet-Dry-Pfad blendet über 5 ms ein/aus (`Source/PluginProcessor.cpp:227`). Dadurch können IR-Szenen auch bei ausgeschalteter Cab übernommen werden. Ein-/Ausschalten nach laufender Stereo-IR wird gegen eine immer aktive Referenz geprüft. Der dauerhaft eingeschaltete Signalweg und die fünf Built-in-Biquads behalten ihre ursprüngliche Arithmetik.

### S03 / B04-Ergänzung: innere WAV-/AIFF-Chunks konnten hinter EOF zeigen

Ein korrekt begrenzter äußerer RIFF-Header allein genügt nicht: JUCEs WAV-/AIFF-Reader können fehlende Payload mit Nullen auffüllen und trotzdem erfolgreich lesen. Ein abgeschnittenes Stereo-WAV mit nachgezogener äußerer RIFF-Größe wurde angenommen und ersetzte die zuvor gültige IR. Isolierter Negativtest: **3 Prüfungen, 1 Fehler, Exit 1**, [vorher](qa/compact-stereo-20261026/inner-chunk-before.txt).

`Source/CabinetIR.h:64` prüft nun zusätzlich sämtliche inneren RIFF-/FORM-Chunk-Header, Nutzdatenlängen und Padding innerhalb des unveränderlichen Snapshots. Erst danach erfolgen Decoder, Finite-Prüfung und die vorhandene vorbereitete Veröffentlichung. Die Prüfung liegt vollständig auf dem Control-Pfad. Ablehnung erhält die vorherige Szene. Positive Stereo-WAV-/AIFF-Fälle sowie Right-only-NaN und innerlich abgeschnittene WAV-/AIFF-Dateien gehören zu den Regressionen. Keine Änderung gültiger IR-Normalisierung, Kanalzuordnung, Resampling oder Ownership.

## Testqualität und historische Fehlversuche

- Die ursprüngliche JUCE-Integration des Ausgangscommits kompilierte auf Linux ohne Produkt-Compilerfehler. Ihr vorhandener Cut-Test meldete jedoch FAIL: [erster Lauf](qa/compact-stereo-20261026/initial-30988-integration.txt).
- Ursache des Cut-Orakelfehlers war die nicht berücksichtigte ursprüngliche JUCE-Ausgangsgain-Rampe von 0 auf 1 über 50 ms. Der Tap liegt davor. Das isolierte Orakel bildet jetzt diese Gain-Stufe exakt nach; Cab Off wird vor Prepare gewählt, um die neu definierte Cab-Blende aus dem isolierten Cut-Test herauszuhalten. **Die Grenze 1e−6 wurde nicht verändert.** Zusätzlich prüfen unabhängige bilinear transformierte Butterworth-Gleichungen die Cut-Frequenzgänge; das Processor-Orakel belegt die Verkabelung, nicht unabhängig den Algorithmus.
- Der erste neue Amp-Vergleich modellierte die Gain-Rampe mathematisch mit `frame/rampLength`. JUCE akkumuliert die lineare Rampe in Float; vor dem nichtlinearen Amp führten diese unterschiedlichen Rundungen zu bis zu 2,95e−6 Sampleabweichung. Verwendung derselben ursprünglichen JUCE-Gain-Stufe im Referenzpfad ergibt Delta 0. Weder Amp-Code noch Grenzwert wurden angepasst.
- Ein erster Preset-Test fragte die Dateiliste eines schon vor dem Speichern konstruierten zweiten Managers ohne `refresh()` ab. Die explizite Aktualisierung korrigiert diesen Fixturefehler; keine Preset-Logik wurde dafür verändert. [Erste Matrix mit diesen Testfehlern](qa/compact-stereo-20261026/first-matrix-diagnostic.txt).
- Der absichtlich fehlerhafte Build mit verworfenem rechten Eingang meldet **2 Fehler in 6 Prüfungen, Exit 1**: L→R-Übersprechen und verlorenes R-only. [Mutationsnachweis](qa/compact-stereo-20261026/discard-right-negative.txt). Nur eine generierte Build-Kopie wird verändert; der ausgelieferte Source enthält die Mutation nicht.
- Ein älterer Sanitizer-Zwischenlauf wurde nach der zusätzlichen Chunk-Korrektur abgebrochen und durch einen Lauf mit der korrigierten Quelle ersetzt. Der abgebrochene Lauf zählt ausdrücklich nicht als PASS.
- Keine Referenzdaten neu generiert, keine Assertions deaktiviert, keine Toleranz gelockert. Alle Abnahmechecks bleiben in Release/NDEBUG aktiv.

## Umgebung und Reproduktion

Linux 6.18.55 x86_64, GCC 14.2.0, CMake 4.4.4/Ninja, JUCE 8.0.4 (`51d11a2be6d5c97ccf12b4e5e827006e19f0555a`), AMD EPYC 9V74, fünf sichtbare CPUs, cgroup-Quote vier CPU-Kerne, 32 GiB. Governor, physische Hostauslastung und Realtime-Scheduler nicht kontrolliert. Entwicklungsbuild Release/O3; Sanitizer O1/g, alle verlinkten JUCE-Module instrumentiert. Sanitizer- und Ownership-Trace-Zeiten sind keine CPU-Abnahme.

Das originale Root-CMake baut das tatsächliche Linux-VST3 und Standalone mit den ursprünglichen empfohlenen JUCE-/LTO-Flags. Lokale Entwicklungsabhängigkeiten kommen aus dem vorhandenen verifizierten Sysroot. Fehlendes GTK/WebKit betrifft hier deaktivierte Browserfunktionen; es wurde keine TLS-/Paketprüfung umgangen. Bestehende Compilerwarnungen bleiben im Buildlog sichtbar.

Reproduzierbare Konfigurationen und fokussierte Modi: [Test-README](../tests/ir_presets/README.md). Root-Targets: `DjentyDonkey_VST3`, `DjentyDonkey_Standalone`, `guitar_cuts_test`, `tone_regress`, `dsp_test`. Native Checks: `ctest --test-dir <root-build> -V`, `<root-build>/tone_regress`, `<root-build>/dsp_test fault`. Für alle Läufe wird der echte Exit-Code übernommen; die bewusst fehlerhafte Mutation muss Exit 1 liefern.

Fixtures sind generierte Impulse, asymmetrische/opposit sign Stereo-IRs, Sinus-/Mehrtonsignale, Host-/Preset-XML und Fehlereinspeisungen. **NOT RUN:** echte Gitarren-DI und Hörfreigabe. JUCEs Datei namens `guitar_amp.wav` ist ein Cabinet-IR-Demoasset und wird nicht als Gitarrenperformance ausgegeben.

## Anforderungs-/Testmatrix

| Anforderung | Reale Prüfung / Gegenprobe | Beobachtung / Status |
|---|---|---|
| Unabhängige L/R-Amps, A01 | L-only und R-only gegen separaten stillen Processor; Mutation verwirft rechts | **PASS**; Mutation erkennt 2 Fehler. Kein stummes Signal als Funktionsnachweis |
| Ursprünglicher 5150/6505-Pfad | tatsächliche L/R-Ausgänge gegen unveränderte AmpSim + ursprüngliche Gain-Stufen, 44,1/48/96 kHz; Kanal-, Drive-Modell- und Tube-Set-Wechsel | **PASS**, maximale Sampleabweichung 0; 8x/8x/4x Oversampling erhalten |
| Blockgrenzen | 0/1/17/32/64/128/256/512/777 Samples, Prepare-Maximum 512; Oversize-Chunks separat in Referenz | **PASS**, nichtstille Referenz und Delta 0 |
| Mono-/Stereo-Busse | 1→1, 1→2, 2→1, 2→2 im Bypass-/Warm-Wet-Test; Wet-Downmix und Mono-Split bei wiederholtem 48→44,1→96-kHz-Prepare | **PASS**, Downmix-/Split-Deltas 0; NaN/Inf/Subnormal-Einspeisungen bleiben endlich |
| Mono-/Stereo-IR | echte Zweikanal-Convolution gegen originales JUCE-File-Load-Orakel, 44,1/48/96 kHz, variable Blöcke und unterschiedliche Kanalantworten | **PASS**, Delta 0; Polarität und getrennte L/R-Inhalte erhalten |
| AIFF und beschädigte Dateien | gültige Stereo-AIFF hörwirksam; innere WAV-/FORM-Trunkierung, 3-kanalige IR, Right-only NaN | **PASS** im vollständigen Trace-Lauf; Ablehnung erhält gültigen Pfad |
| B01 Ownership / schnelle Wechsel | API-Trace aller echten Convolver; 40 parallele Mono-/Stereo-/Clear-Anforderungen und finaler Wechsel ohne weitere Producer-Anrufe | **PASS**; beide Kanäle nehmen letzte Szene kohärent an, keine Load-Operation auf bereits publiziertem/renderndem Convolver |
| A04/B03/B04 übernommen | Factory/User Built-in, Scene-Save/Reload, invalid/future/duplicate/range/XML, fehlende Datei, nichtstill ausstehende Convolution-Tail nach Reject | **PASS**; alte Negativ-/Rollbackfälle bleiben aktiv, nicht nur Pfadvergleich |
| Stereo-State / neue Cuts | Restore vor/nach Aktivierung, Preset-Reload, Stereo-IR und Cut-Werte, Legacy-State ohne neue IDs | **PASS**, tatsächlicher Output Delta 0; alte Projekte erhalten Cuts OFF |
| Low-/High-Cut im Processor | abgegriffener Post-Amp-Eingang → isolierter Cuts/Gain-Referenzpfad; ergänzend unabhängige Butterworth-Frequenzgleichungen bei 44,1/48/96 kHz | **PASS**, Processor-Delta 0 und deutlicher Filtereffekt; native alte Grenzen unverändert |
| Entfernte aktive FX | sämtliche Compressor-/Transpose-/Phaser-/Delay-/Reverb-/EQ-/Loop-Gate-Parameter nur auf einem von zwei Processoren bis zu Endpunkten automatisieren | **PASS**, nichtstilles Ausgangsaudio Delta 0, obwohl alle alten IDs weiter existieren |
| Live-Automation | Cut-Frequenzen/On-Off, Kanal/Gain mit Richtungswechseln während Glättung; stereoidentischer Zustand in Referenz | **PASS**, endlich und deterministisch; keine samplegenaue Host-Automationsfreigabe daraus ableiten |
| Host-Bypass S01 | 4 Buskombinationen, korrekt verzögertes Dry, nach Reaktivierung kontinuierliche Wet-Referenz | **PASS**, alle Dry-/Warm-Wet-Deltas 0; vorher 8/12 Fehler |
| CAB-Bypass S02 | Stereo-IR weiter verarbeiten, nur Ausgabe aus-/einblenden | **PASS**, Warm-Wet-Delta 0 |
| Callback-C++-Heap | voller Stereo-Processor bei Automation plus Cabinet-Fades/Parallelwechsel | **PASS** für erfasste new/delete, beide Zähler 0; keine vollständige OS-/malloc-Zertifizierung |
| Tatsächlich gebautes Plugin | separater JUCE-VST3-Host lädt Linux-Bundle, Stereo-Aushandlung, R-only, State, Wrapper-Bypassparameter | **PASS**, Bleed 0, Recall-/Bypass-Deltas 0, Latenz 5 Samples bei 48 kHz, 70 Wrapperparameter = 69 Produktparameter + JUCE-Bypass |
| Ursprüngliche Native-Gates | unveränderte `tone_regress` und `dsp_test fault`; erweiterter nativer Cuts-Test | **PASS**, 74/74 Tone-Fälle, 19/19 Fault-Fälle, Cuts 0 Fehler / CTest 1/1 |
| Vollständig instrumentierte Speicher-/Thread-Gates | TSan und ASan/UBSan ohne Suppressions | **PASS**, jeweils 131/131 / CTest 1/1, keine Meldung |
| CPU / tatsächliche Deadlines | ganzer Processor und tatsächlich gebautes VST3, 1/2 Amp-Wege, 44,1/48 kHz, 32–512 Frames, offline und zeitlich gepacet | **FAIL auf dieser VM**, jeweils 40/40 Konfigurationen mit überschrittenen Deadlines |
| Windows-/Host-Release-Gates | Windows x64 VST3, ASIO, Studio One, Validator, Plugin Tester, GUI-Lebensdauer | **NOT RUN** |

Die vollständige kartesische Kombination aller Raten, Blockgrößen, I/O-Layouts, IR-Dateiformate und Host-Lifecycles ist **NOT RUN**; obige Matrix benennt die tatsächlich ausgeführten Fälle.

## Abschließende Läufe

| Lauf | Resultat | Evidenz |
|---|---|---|
| Linux-JUCE/VST3 + Standalone kompilieren | **PASS**, Exit 0; Standalone-Audiogerät/GUI **NOT RUN** | [finaler Produktbuild](qa/compact-stereo-20261026/plugin-final-build.txt), [Binär-SHA-256](qa/compact-stereo-20261026/linux-artifacts-sha256.txt) |
| Uninstrumentiertes Release | **PASS**, 131/131, CTest 1/1, Exit 0, 119,04 s | [vollständiger Lauf](qa/compact-stereo-20261026/release-untraced.txt) |
| Release mit echtem JUCE-Ownership-Trace | **PASS**, 132/132, CTest 1/1, Exit 0, 121,91 s | [Trace-Lauf](qa/compact-stereo-20261026/release-ownership-trace.txt) |
| Vollständig instrumentierter ThreadSanitizer | **PASS**, 131/131, CTest 1/1, Exit 0, 1340,36 s; keine Warnung, keine Suppression | [TSan](qa/compact-stereo-20261026/thread-sanitizer.txt) |
| Vollständig instrumentierter ASan/UBSan | **PASS**, 131/131, CTest 1/1, Exit 0, 952,92 s; keine Meldung, keine Suppression | [ASan/UBSan](qa/compact-stereo-20261026/address-undefined-sanitizer.txt) |
| Originaler Tone-Test | **PASS**, 74 Fälle / 0 Fehler; ursprüngliche Referenzen unverändert | [Tone-Regression](qa/compact-stereo-20261026/tone-regress.txt) |
| Originale Fault-Injection | **PASS**, 19 PASS-Ausgaben / Exit 0 | [Fault-Regression](qa/compact-stereo-20261026/dsp-fault.txt) |
| Native Cuts einschließlich analytischer Zusatzfälle | **PASS**, 0 Fehler, CTest 1/1 | [Cuts](qa/compact-stereo-20261026/native-cuts.txt) |
| Tatsächliches Linux-VST3 im separaten Host | **PASS**, ausdrücklich nichtdefault Gain/Master/Cuts im Component-State und PCM wiederhergestellt; R-only-Energie 0,334305, Übersprechen 0, Recall-/Dry-/Warm-Wet-Deltas 0, Exit 0 | [Host-Smoke](qa/compact-stereo-20261026/vst3-host.txt) |
| Bewusste R-Verwerfungs-Mutation | **PASS der Gegenprobe**: erwartete 2/6 Fehler und Exit 1; keine positive Abnahme des Mutanten | [Negativlauf](qa/compact-stereo-20261026/discard-right-negative.txt) |
| CPU direkt im Release-Processor | **FAIL**, 40/40 Deadline-Gates, Exit 1; alle Audio-Energie-/Finite-Prüfungen bestanden | [Processor-CPU](qa/compact-stereo-20261026/cpu-processor.txt), [CSV](qa/compact-stereo-20261026/cpu-processor.csv) |
| CPU im tatsächlichen Linux-VST3 mit ursprünglichem LTO | **FAIL**, 40/40 Konfigurationen, Exit 1; nichtstilles endliches Audio, tatsächliche Setupwerte geprüft | [VST3-CPU](qa/compact-stereo-20261026/cpu-vst3.txt), [CSV](qa/compact-stereo-20261026/cpu-vst3.csv) |

## CPU-Ergebnis und Konsequenz

**P1 / Echtzeit-Gate FAIL auf der vorhandenen Cloud-VM.** Auch ohne parallele Builds oder andere Tests überschreitet bereits ein Amp die Blockzeit. Das tatsächliche VST3 bestätigt dies mit den ursprünglichen LTO-Flags. Größere Blöcke beheben den grundsätzlich zu hohen Rechenbedarf auf dieser Maschine nicht. Daraus wird keine ungeprüfte Aussage über die konkrete Windows-CPU des Besitzers abgeleitet.

Beide Messwege benutzen denselben Arbeitspunkt: RED, Gain 5, Master 3, Boost/Input Gate/Noise OFF, Built-in-Cab und beide Cuts ON, 0 dB Input/Output/Amp Volume, ursprüngliches 8x Oversampling. Stereo-Eingänge enthalten unterschiedliche Zweitonsignale, Mono→Stereo benutzt einen Amp. Pro Konfiguration 24 ungemessene Warm-up-Blöcke und 160 gemessene Blöcke; Offline und `sleep_until`-gepacet getrennt. Timer-p99 der direkten Messung: 0,031 µs. Die Callback-C++-new/delete-Prüfung erfolgte separat im direkten Processor; eine globale Heap-Prüfung des VST3-Wrappers wird nicht behauptet. Keine Stummschaltung als Performance-Abkürzung.

Auszug des tatsächlich geladenen VST3, gepacet, 64 Frames, Angaben in Millisekunden:

| Samplerate | Amp-Wege | Mean | p95 | p99 | Max | Deadline | Work-Overruns / 160 | Status |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| 44,1 kHz | 1 | 2,008 | 2,341 | 2,618 | 2,892 | 1,451 | 160 | **FAIL** |
| 44,1 kHz | 2 | 4,487 | 4,828 | 9,304 | 73,697 | 1,451 | 160 | **FAIL** |
| 48 kHz | 1 | 2,042 | 2,398 | 3,526 | 4,983 | 1,333 | 160 | **FAIL** |
| 48 kHz | 2 | 4,259 | 5,366 | 7,474 | 10,187 | 1,333 | 160 | **FAIL** |

Alle 40 Konfigurationen pro Messweg umfassen 32/64/128/256/512 Frames bei beiden Raten und 1/2 Wegen. In jedem Messweg sind **6.400/6.400** gemessene Work-Blöcke über der Deadline; alle 3.200 gepaceten Blöcke beenden auch ihre Sollperiode zu spät. Vollständige Mean/p95/p99/Max, Deadline, Auslastung, Work-Overruns, Schedule-Misses und Audio-Energie stehen in den CSV-/Rohprotokollen. Die Extremwerte enthalten VM-/Scheduler-Jitter; schon die Mittelwerte überschreiten die Deadlines deutlich.

Ein zweiter Amp erhöht den End-to-End-Rechenbedarf ungefähr auf das Doppelte. Zusammen mit dem unveränderten Quellpfad (zwei separate `AmpSim::process` mit der kompletten 8x nichtlinearen Kette) grenzt dies den wesentlichen Aufwand auf die Amp-Wege ein. Ein detailliertes instrumentiertes Solver-/Tube-/Matrix-Profil ist **NOT RUN**; es wird kein genauer Kernel-Prozentanteil erfunden. Keine Reduktion von Oversampling, Solver-Qualität oder Pedal-Historie implementiert.

Der erste zusätzliche VST3-CPU-Versuch nahm irrtümlich eine lineare Amp-Volume-Normalisierung an. Die bereits vorhandene Range hat jedoch den Skew-Mittelpunkt 0 dB: `40/52` ergab tatsächlich +7,08412 dB. Das neue Component-State-Preflight erkennt dies als Setupfehler und bricht ab ([Gegenprobe](qa/compact-stereo-20261026/cpu-vst3-setup-negative.txt)). Nur der anschließend korrigierte 0-dB-Lauf wird oben verwendet. Kein Produktparameter, Grenzwert oder DSP-Wert wurde dafür geändert; die aufgezeichneten Audio-Energien stimmen jetzt mit dem direkten Processor-Arbeitspunkt überein. Der ältere Versuch bleibt als klar gekennzeichnete Diagnose erhalten.

**Empfohlener nächster Reparaturschritt:** denselben Zweikanal-Arbeitspunkt auf Windows x64 im tatsächlichen Studio-One-/ASIO-Host messen und die nichtlinearen Amp-Kernel mit gemessenem Profiler-Overhead aufschlüsseln. Danach ausschließlich durch PCM-/Alias-/Dynamik-A/B belegte verlustfreie Optimierungen prüfen. Eine kompakte GUI oder FINAL-Veröffentlichung löst dieses CPU-Gate nicht.

## Geänderte Dateien

- Produkt: `Source/PluginProcessor.cpp`, `Source/PluginProcessor.h`, `Source/CabinetIR.h`.
- Tests: `tests/guitar_cuts_test.cpp`, `tests/ir_presets/ir_presets_test.cpp`, `tests/ir_presets/compact_stereo_regressions.h`, `tests/ir_presets/compact_vst3_host.cpp`, `tests/ir_presets/HostedCPUBenchmark.h`, `tests/ir_presets/CMakeLists.txt`, `tests/ir_presets/README.md`.
- Dokumentation: dieser Bericht, historischer Verweis in `docs/COMPACT-STEREO-IMPLEMENTATION-20261026.md`, Rohprotokolle/Kommandos/Hashes unter `docs/qa/compact-stereo-20261026/`.

Die Produktdateien außerhalb dieser drei Source-Dateien bleiben unangetastet. [Unverändertheitsprüfung](qa/compact-stereo-20261026/preservation-check.txt).

## Offene Grenzen

- **NOT RUN:** Windows x64 VST3/ASIO, Studio One, offizielle Steinberg-Validatoren, 125A Plugin Tester, echte Editor-/Dialog-Lebensdauer und Hardware-CPU-/Dropout-Abnahme. Linux-Host-Smoke ist eine eigene, engere Evidenzklasse.
- **NOT RUN:** professionelle Gitarren-DI, level-matched Listening, Hardware-/NAM-Abgleich, vollständiger erneuter Alias-/THD-/IMD-Sweep, vollständiges IR-Format-/Längen-Corpus, 64-Bit-Audio und hostseitige samplegenaue Automation. Float-Processor-Automation wird blockweise geprüft; der ursprüngliche Amp-/Oversampling-Quelltext bleibt unverändert.
- Die bisherige GUI bleibt unverändert und enthält obsolete FX-Regler; diese sind absichtlich wirkungslos. Neue Cut-Parameter sind über Host/State verfügbar. Keine GUI-Neugestaltung in diesem Auftrag.
- Alte Presets mit entfernten Compressor-/Transpose-/EQ-/Phaser-/Delay-/Reverb-Effekten behalten ihre IDs/Werte, können den früheren Effektklang aber absichtlich nicht reproduzieren. Der kompakte Produktumfang war bereits in der beauftragten Basis festgelegt.
- Echte Stereo-Busse verarbeiten jetzt beide Eingänge. Projekte, die zuvor das Verwerfen ihres rechten Eingangssignals oder das Mono-Duplikat nutzten, ändern sich dadurch absichtlich. Unveränderte Amp-Modellierung bedeutet keine pauschale Klanggleichheit des umgebauten Gesamtprodukts mit dem alten Effekt-Routing.
- Originale Plugin-Identität und Version 0.5.6 bleiben in Entwicklung erhalten; neue 125A-Paketidentität und AGPL-/JUCE-Vertriebspflichten müssen vor einer Veröffentlichung separat geklärt werden.
- IRs bleiben absolute Dateipfade. Projektportabilität/Embedding und Hashbindung an den Dateiinhalt sind weiterhin offen. Fehlende IR verwirft den gesamten Recall und erhält die bisherige Szene; die alte GUI zeigt noch keinen neuen Reject-Dialog.
- Sicherheitsbudgets bleiben 64 MiB, maximal zwei Kanäle, 4.194.304 Samples je Kanal und ca. 5,46 s Dauer vor Trim im Cross-Rate-Budget. Keine heimliche Kürzung einer angenommenen IR.
- C++-new/delete-Proben, ThreadSanitizer und Quelltextprüfung ersetzen keine vollständige RT-Zertifizierung. Direkte C-Allokationen, OS-Aufrufe, Scheduling und sämtliche Host-Interleavings sind damit nicht vollständig abgedeckt.
- Die unveränderte GuitarCuts-Implementierung begrenzt Frequenzen auf 0,45 × Samplerate. Dadurch liegt das nominelle High-Cut-Ende 20 kHz bei 44,1 kHz tatsächlich bei 19,845 kHz. Eine vollständige Frequenzgangmatrix sämtlicher Control-Endpunkte ist **NOT RUN**; diese vorhandene Schutzbegrenzung wird nicht als gemessene exakte 20-kHz-Abstimmung ausgegeben.
- Andere ursprüngliche Auditbefunde, insbesondere ungeprüfte Modellgrenzen und GUI-Lebensdauer, sind durch diesen Auftrag nicht pauschal geschlossen. Keine klangverschlechternde CPU-Abkürzung und keine FINAL-Freigabe.
