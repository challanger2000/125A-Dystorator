# CPU-Optimierung und Leistungsmodi – Entwicklungsnachweis

2026-10-26, Europe/Berlin (Messlogs in UTC). Branch `dev/compact-stereo-on-ir-fix-20261026`, Basis `8ca580d37cfc7823df7a62d40e13a0ae8e1b17b8`. **Keine FINAL-Freigabe.** Dieser Bericht ersetzt nicht die historische Stereo-QA, sondern dokumentiert die anschließende, ausdrücklich beauftragte DSP-Änderung.

## Ergebnis und Grenzen

**MEASURED:** HIGH benötigt in den 48 identischen Vorher/Nachher-Konfigurationen im arithmetischen Mittel der relativen Einsparungen **38,55 % weniger mittlere Callback-Zeit** (Spanne 27,26–49,16 %). Die bisherigen Stereo-, IR-, Cabinet-, Preset- und Bypass-Verträge bestehen die erneut ausgeführten Regressionen. HIGH bleibt gegenüber dem eingefrorenen Ausgangs-Amp in 27 PCM-Vergleichen innerhalb der vorher festgelegten Grenzen: maximal **2,98023e−8** Sampleabweichung, schlechtester Fehlerenergieabstand **−152,96 dB**. Keine ursprünglichen Klangreferenzen oder Grenzwerte wurden ersetzt oder gelockert.

**FAIL: vollständige Echtzeitfähigkeit auf dieser Cloud-CPU.** Vorher 48/48 CPU-Fälle FAIL, nachher 17/144 PASS und **127/144 FAIL**, weil mindestens ein gemessener Callback oder ein getakteter Fertigstellungstermin die Deadline verfehlt. Durchschnittswerte unter der Deadline sind keine Echtzeitfreigabe. Die Einsparung erfüllt eine messbare Verbesserung, aber keine allgemeine Deadline-Garantie.

**NOT RUN: musikalische Freigabe der niedrigeren Qualitätsmodi.** ECO und STANDARD sind funktional implementierte, experimentelle Leistungsoptionen; sie erhalten ausdrücklich keine Gleichklang-Zusage. Insbesondere ECO zeigt bei RED/hohem Gain/Drive erheblich mehr inharmonische Energie. HIGH bleibt Referenz und Default, auch bei Migration alter Projekte. Windows, ASIO, reale DAWs und Hörvergleiche fehlen weiterhin.

## Vorgaben, Quellen und Änderungsauswahl

Pflichteinstieg und relevante Vorgaben wurden in der vorgeschriebenen Reihenfolge gelesen und mit „verstanden“ bestätigt: privates `125A-Engineering`, Stand `b6df84b0a04d01e240cff745ccebf60d67750486`, `START-HERE.md`, README, Guardrails, DSP-/Professional-/Workflow-/AI-Standards, Release-/Realtime-/Audio-Fixture-QA, Oversampling-Research und relevante Roadmap. Die verbindliche Entwicklungsbasis ist der vom Eigentümer benannte Commit; eine unverifizierte Windows-FULL-/Gumroad-Version wurde nicht erfunden.

Originalprojekt: [CPU-Commit 51597a8](https://github.com/SteveAsAConstant/djenty-donkey/commit/51597a8767dfc7f4b7ac020baedb762a7658ae3a). Zum Recherchezeitpunkt zeigen Original-HEAD und Tag `v1.0.0` auf `3bc4969a60a64927ba6459574145682ca47f0601`. Der vollständige Baum, die Historie und die Unterschiede der aktiven Amp-Abhängigkeiten wurden geprüft; kein pauschaler Merge. AGPL-3.0-Lizenz und ursprüngliche Urheberschaft bleiben erhalten. Upstream-Diffs und Quellhashes liegen im [Evidenzverzeichnis](qa/cpu-quality-20261026/).

| Upstream-Bereich | Entscheidung / Nachweis |
|---|---|
| `PortSolve.h`, `TriodeNetwork.h`, `PowerAmp5150.h` | Reduzierte Port-/Newton-Elimination übernommen. Gleiche Iterations-, Residual- und Stabilitätsgrenzen; keine abgeschnittenen Newton-Iterationen. |
| `DKCircuit.h`, `Simd.h` | Transponierte/zusammenhängende Matrixzugriffe, SSE2/AVX-Vektorisierung mit Laufzeitprüfung von CPU/OS, geordnete Summen, Vermeidung unveränderter Matrix-Neuberechnung, faktorierte Ausgangsauswertung übernommen. Keine FMA-/fast-math-Abkürzung. |
| Röhrenberechnung | Upstream-`exp(exponent*log(x))` statt wiederholtem `pow`, unveränderte Kennlinienparameter. Positiver Normalbetrieb im PCM-Vergleich; zusätzlicher Schutz für Screen-Spannung ≤0 gegen Division durch Null/log(0), separat getestet (A06-Randfall). |
| Preamp `runAhead` | Nicht übernommen: optionaler, upstream standardmäßig deaktivierter Block-Vorlauf der Versorgung ist keine algebraisch identische sampleweise Kopplung. Kein Nachweis eines kompatiblen Vorteils. |
| `SemiNetwork`, 1176/Urei/Comp, Pitch/WSOLA | Kein Bestandteil unseres aktiven kompakten Amp-Pfads; keine neuen Effektdateien/-Abhängigkeiten importiert. |
| Delay-OOB-Fix `c2585cf`, Host-Test-Fix `fc56c5c` | In Originalhistorie geprüft. Delay ist hier aus aktiver Verarbeitung entfernt; Original-Hosttest ist kein Produktfix unseres Wrappers. |
| Nach CPU-Commit: `eb3bfa2` CabMatch, IR-Preset-Lautheit, 1176 | CabMatch verändert gültige IR-Lautheit und verwendet die andere Original-Preset-Architektur. Nicht mit unseren bestätigten IR-/Preset-Verträgen kompatibel; keine Übernahme. 1176 entfällt. |
| v1.0.0 Packaging/Mac-CI/GUI/Parameter | Kein CPU-Vorteil unseres aktiven Pfads; nicht übernommen. Root-Plugin-Version/GUI bleiben unverändert. |

**CIRCUIT/MATH DERIVED:** Aus `(I − K D) dp = r` folgt mit `q = D dp` das System `(I − D K) q = D r`, danach `dp = r + K q`. Exakt verschwindende Ableitungszeilen haben `q=0` und werden entfernt; nichtleitende Gitter reduzieren typischerweise 12 auf 6 Unbekannte. Es handelt sich um dieselbe Newton-Gleichung bis auf Rundung, keine neue Röhrenapproximation. Unabhängige volle Gauß-Elimination prüft 2000 leitende/nichtleitende Zufallsfälle, inklusive Teilmengen und Singularität. SIMD prüft 512 Matrixformen mit ungeraden Resten und Schutzwerten in AVX-, erzwungenem SSE2- und Scalar-Build. MSVC-AVX-Funktionen sind gegen Inlining in den SSE2-Einstieg abgegrenzt; physischer MSVC-/Nicht-AVX-Rechner-Test bleibt NOT RUN.

## AMP MODE, QUALITY und Latenz

Zwei neue Choice-Parameter werden **hinter den bisherigen 69 IDs angefügt**: `ampMode` = MONO/STEREO, Default STEREO; `quality` = ECO/STANDARD/HIGH, Default HIGH. Bestehende IDs und Wertebereiche bleiben erhalten. Im tatsächlichen VST3 sind es 72 Parameter einschließlich JUCE-Bypass. Alte Zustände ohne neue IDs werden durch die bestehende Default-Migration auf STEREO/HIGH ergänzt. Bedienung derzeit über Host-Parameter; keine neue GUI und kein GUI-Redesign.

| Host-Rate | ECO | STANDARD | HIGH (ursprüngliche Referenz) |
|---|---|---|---|
| 44,1 kHz | 2× / 88,2 kHz intern | 4× / 176,4 kHz | 8× / 352,8 kHz |
| 48 kHz | 2× / 96 kHz | 4× / 192 kHz | 8× / 384 kHz |
| 96 kHz | 1× / 96 kHz | 2× / 192 kHz | 4× / 384 kHz |

Die Qualitätsnamen beziehen sich auf 44,1/48 kHz. Die tatsächliche Stufe wird aus Host-Rate und Zielrate bestimmt; HIGH erhält die originale adaptive Regel einschließlich minimal 2× bei hohen Host-Raten. Andere als die drei gemessenen Host-Raten: **NOT RUN**.

MONO summiert bei Stereo-Eingang vor dem nichtlinearen Amp `0,5 L + 0,5 R`, verarbeitet einen Amp und dupliziert anschließend vor Cabinet/Cuts. Ein Mono-Host-Eingang läuft direkt durch einen Amp. STEREO behält getrennte nichtlineare Zustände links/rechts. Stereo-IRs bleiben auch im MONO-Amp-Modus stereo; Mono beschreibt die Anzahl Amp-Pfade, nicht die Anzahl Cabinet-Ausgänge. Mono-Summierung kann phaseninvertierte Eingangssignale bestimmungsgemäß auslöschen.

`AmpModes.h` bereitet **neun Amp-Zustände** (L/R/Mono je Qualität), Gates, Verzögerungen und Scratchbuffer ausschließlich in `prepareToPlay` vor. Umschalten konstruiert oder zerstört keine Amp-Instanz, bereitet keine Filter vor und meldet dem Host keine neue Latenz. Gegenüber zwei ursprünglichen Pfaden steigen Startaufwand und Speicherverbrauch; deren absolute Größe wurde nicht separat profiliert (**NOT RUN**). Die Ganzzahl-Latenz bleibt pro Prepare konstant auf ursprünglichem HIGH-Wert; niedrigere Qualitätswege erhalten die Differenz ihrer gerundeten Latenz als Verzögerung. Die ursprüngliche JUCE-IIR-Latenzrundung bleibt erhalten. Dies garantiert gleiche gemeldete Ganzzahl-Latenz, **keine identischen frequenzabhängigen Phasengänge** zwischen unterschiedlichen IIR-Stufen und keinen behaupteten zusätzlichen Fractional-Delay-Ausgleich.

Beim Wechsel läuft der eingehende Pfad zunächst 50 ms warm und wird danach 50 ms gemeinsam für beide Kanäle linear eingeblendet. Diese Zeiten sind **EMPIRICALLY TUNED**, keine Hardwarekonstanten. Weitere Automationsanforderungen unterbrechen den Übergang nicht; nach Abschluss wird die neueste Auswahl übernommen. Dadurch bleibt der Arbeitsumfang begrenzt auf höchstens zwei gleichzeitig gerenderte Konfigurationen, aber die Übergangs-CPU kann vorübergehend annähernd doppelt sein. Inaktive nichtlineare Historien entsprechen nicht einem unendlich durchlaufenden Zielpfad; die Aufwärmung ist keine bewiesene Garantie gegen jede hörbare Änderung. Getestet: endliches Audio, echte Pfadauswahl, feste Latenz, rasche Anforderungen und kein C++ `new/delete` im Callback. Unmessbare „klickfrei“- oder samplegenaue Qualitätsautomation wird nicht behauptet.

## Objektive Messung

Umgebung: Linux 6.18.55 x86_64, AMD EPYC 9V74, fünf sichtbare CPUs, cgroup-Quote vier Kerne, 32 GiB; GCC 14.2, CMake 4.4.4/Ninja, JUCE 8.0.4 (`51d11a2be6d5c97ccf12b4e5e827006e19f0555a`). Kein kontrollierter Governor, Realtime-Scheduler oder physischer Host. Release-Benchmark des vollständigen Processors mit O3/NDEBUG ohne LTO, Sanitizer oder Ownership-Trace (vorher/nachher identisch); Wrapper-Zeit ist darin nicht enthalten; keine parallel laufenden eigenen Builds während der CPU-Messläufe. Sanitizer/Trace-Zeiten sind keine Performance-Daten. Root-VST3/Standalone mit unveränderten ursprünglichen JUCE-/LTO-Empfehlungen gebaut.

### CPU

`quality_bench.cpp` misst den **vollständigen echten Processor** mit RED, Gain 9, Master 3, Precision Drive an (Drive 8, Level 7), Built-in-Cab und beiden Cuts an, Gates/Noise aus. Gleiche deterministische Zweiton-Eingänge, 0,2 s Warm-up, 160 gemessene Callbacks pro Fall, drei Raten × zwei Amp-Lanes × vier Blockgrößen (64/128/256/512) × offline/getaktet. Vorher wird der vollständige Processor aus dem gepinnten Commit separat kompiliert. Bei einem Amp bekommt er einen Mono-Bus mit derselben Eingangsmischung; nachher wird der neue MONO-Modus am Stereo-Bus geprüft. HIGH nutzt in beiden Builds dieselben 8/8/4×. Niedrigere Qualität ist **keine** Bestandteil der behaupteten HIGH-Einsparung.

p95/p99 verwenden sortierte Positionen 151/158 (0-basiert, 160 Werte); Maximum und strikte Callback-Overruns sowie getaktete Fertigstellungsfehler werden separat erfasst. `sleep_until`/VM-Scheduling ist kein Audiointerface; kurze Messdauer erlaubt keine Langzeit- oder harte Echtzeitzusage. Ein Fall besteht nur ohne eine einzige Deadline-Verfehlung und mit endlichem, nichtstummem Ausgang. Exit 1 der beiden Benchmarks ist korrekt und dokumentiert, kein Testgrenzwert wurde dafür geändert.

Beispiel **48 kHz, 64 Samples, getaktet**, Deadline 1333,33 µs:

| Amp-Pfade / Qualität | Mittel µs | p95 µs | p99 µs | Max µs | Callback-Overruns / 160 | Ergebnis |
|---|---:|---:|---:|---:|---:|---|
| Mono HIGH | 1499,78 | 1948,51 | 4022,16 | 5203,76 | 119 | FAIL |
| Mono STANDARD | 926,69 | 1295,02 | 1710,97 | 1838,25 | 7 | FAIL |
| Mono ECO | 589,14 | 1168,36 | 1434,82 | 1507,93 | 4 | FAIL |
| Stereo HIGH | 2997,51 | 3570,01 | 4936,05 | 5834,85 | 160 | FAIL |
| Stereo STANDARD | 1771,71 | 2130,89 | 2470,55 | 3510,48 | 160 | FAIL |
| Stereo ECO | 1065,34 | 1230,11 | 1347,56 | 1465,64 | 3 | FAIL |

Alle 192 Fälle einschließlich Vorher, Scheduling-Misses und Energie: [cpu-before.csv](qa/cpu-quality-20261026/cpu-before.csv), [cpu-after.csv](qa/cpu-quality-20261026/cpu-after.csv); [48 gepaarte HIGH-Einsparungen](qa/cpu-quality-20261026/high-savings.csv). Restlicher dominanter Aufwand bleibt die sampleweise gekoppelte nichtlineare Vor-/Endstufe bei hoher interner Rate; noch keine Aufteilung durch einen störenden Profiler in diesen Callback-Zeiten. Vollständiger Hotspot-Profiling-Lauf: **NOT RUN**. Weitere Optimierung muss den unveränderten HIGH-PCM-Nachweis erneut bestehen; keine weniger strengen Newton-Kriterien als Schnelllösung.

### PCM, Spektren, Dynamik

Der eingefrorene Amp-Abhängigkeitsbaum aus `8ca580d` unter `tests/cpu_quality/baseline` ist ausschließlich für den unabhängigen Vorher-Vergleich vorhanden; nur der Namespace wurde umbenannt. Ursprüngliche Hashes: `baseline/SOURCE.txt`. Kein neuer Effekt wird dadurch im Produkt aktiv. 27 Vergleiche: drei Raten × drei Kanäle × Drive aus/Precision Drive/TS9, inklusive Gain-/Presence-Automation, Kanal- und Tube-Set-Wechsel sowie Noise-Fall. Festgelegte Kriterien: Max <1e−6 **und** Fehlerenergie <−100 dB **und** positive Referenzenergie. Alle 27 PASS. Die zwei PCM-Zeugen sind je 10240 Float32-Samples, Little Endian, mono, 48 kHz, RED/Precision Drive/Noise mit den dokumentierten Umschaltungen; tatsächlich gerendert, keine musikalische DI-Aufnahme.

45 zusätzliche Render messen RMS, Peak, DC, THD, Drittordnungs-IMD und leise/laute 110-Hz-Dynamik im RED/hohen Gain/Drive-Fall. Drei Raten × BEFORE_HIGH/HIGH/STANDARD/ECO/16×-Referenz × drei Signale, jeweils 0,3 s Einlauf und 8192 Samples. FFT-Hann-Fenster, kohärenter Ton um 3 kHz, Harmonikmasken ±4 Bins; IMD mit kohärenten Tönen um 700/900 Hz. THD/IMD sind Ausgangsenergieratios dieses gesättigten Amp-Modells, kein linearer HiFi-Verzerrungssollwert.

Der inharmonische Rest ist ein **Aliasing-/Transienten-/Noise-Proxy**, keine isolierte exakte Aliasenergie. Die 16×-Referenz ist derselbe ursprüngliche Amp bei höherer interner Rate, **nicht mathematisch aliasfrei**; ihre andere numerische Integration kann ebenfalls Unterschiede verursachen. Reale-Gitarren-DI/levelgematchte Blindhörtests und breitbandige unabhängige Alias-Zerlegung: **NOT RUN**. Keine Behauptung, die 45 Endlichkeits-/Nichtstummheitschecks würden Klangqualität freigeben.

| Rate | HIGH inharmonisch dB | STANDARD dB | ECO dB | Referenz 16× dB |
|---|---:|---:|---:|---:|
| 44,1 kHz | −53,38 | −47,77 | −24,60 | −64,90 |
| 48 kHz | −57,50 | −50,29 | −29,04 | −65,06 |
| 96 kHz | −57,75 | −50,04 | −28,23 | −65,34 |

BEFORE_HIGH und HIGH stimmen bei allen gedruckten Spektral-/Dynamikwerten überein; der strengere PCM-Test misst zusätzlich die kleinsten Abweichungen. Beispiel 48 kHz: THD HIGH/STD/ECO −4,253/−4,300/−4,538 dB; IMD3 −9,102/−9,103/−9,106 dB. ECO erhöht den inharmonischen Anteil um etwa 28–29 dB, keine klangneutrale CPU-Optimierung. Vollständige unveränderte Daten: [spectral-dynamics.csv](qa/cpu-quality-20261026/spectral-dynamics.csv), [PCM-Differenzen](qa/cpu-quality-20261026/high-pcm-comparison.csv), rohe Logs und Float32-Dateien im Evidenzverzeichnis.

## Anforderungs-/Testmatrix

| Vertrag | Tatsächlicher Nachweis | Status / Grenze |
|---|---|---|
| Originaler HIGH-Klang | 27 eingefrorene Vorher/Nachher-PCM-Paare; alter Tone-Test 74 und Fault-Test 19 Fälle | PASS für gemessene Signale, keine neue Hörfreigabe |
| Reduzierter Solver / AVX / SSE2 / Scalar | unabhängiges volles Gleichungssystem, Matrixprodukte, Singularität, Screen≤0; drei ausführbare Backends, Root-CTest inkl. Cuts 4/4 | PASS; NEON/MSVC/physische Nicht-AVX-CPU NOT RUN |
| Stereo, Mono-Host, Cuts, Bypass, Routing, Recall | unveränderte bestehende Integrationssuite 131/131; Trace-Suite zusätzlich Ownership 132/132 | PASS; keine Gleichsetzung mit einer Windows-DAW |
| Rechter Eingang im neuen MONO-Modus | unabhängiger Durchschnitt → HIGH-Amp → Duplikation einschließlich Gain-Rampen, drei Raten, Maxdelta 0 | PASS; Negativmutation verwirft rechten Eingang: 3/3 erwartete FAIL, Exit 1 |
| Alle Qualitäten erhalten beide Audiokanäle und korrekten Ganzzahl-Ausgleich | Neun tatsächliche Stereo-Processor-Render gegen zwei unabhängige AmpSim-Pfade und unabhängig implementiertes FIFO; R-only, dann verschiedene L/R-Signale; drei Raten × drei Qualitäten, Maxdelta 0 | PASS; beide Referenzenergien positiv, unveränderte Grenze 1e−6 |
| Neue Auswahl steuert echten DSP | 66 Checks, drei Raten, alle sechs Konfigurationen, effektiver Faktor/Lanes, feste Latenz, 25 rasche Anforderungen, Host-/Preset-Recall, Legacy-Migration | PASS |
| Audio-Thread-Ressourcen beim Umschalten | C++ new/delete-Probe in laufendem echten Processor; 1200 Audio-Blöcke parallel zu 35 IR-/Preset-/State-/Modusänderungen, Host-Bypass | PASS (66 + 3 Checks); kein allgemeiner malloc/syscall-Profiler |
| Data Races / Memory / UB | vollständig instrumentierte JUCE- und Produktmodule: TSan modes 66 + stress 3; ASan+UBSan modes 66 + stress 3 | PASS; ganze alte 131er-Suite unter neuen Sanitizern NOT RUN |
| IR-/Cab-/Preset-Rollback | bestehende vollständige Suite erneut; `CabinetIR.h`, `PresetManager.h` bytegleich zur bestätigten Basis | PASS für bestehende positiven/negativen Fixtures |
| Tatsächliches Linux-VST3 | Discovery/Instantiate, unabhängiges R-only, nichtdefault State/PCM-Recall, verzögertes/warmes Host-Bypass, live MONO/alle QUALITY-Werte/State/feste Latenz | PASS, sechs Wrapper-Checks; Windows/ASIO/Studio One NOT RUN |
| CPU-Durchsatzverbesserung bei HIGH | 48 gepaarte vollständige Processor-Messungen, keine niedrigere Stufe als Einsparung ausgegeben | PASS: 38,55 % mittlere relative Einsparung |
| Durchgehend fristgerechte Verarbeitung | 48 Vorher- und 144 Nachher-Konfigurationen, strikte Max-/Overrun-Grenzen | FAIL: 127 Nachher-Fälle verletzen Deadline |
| Spektral-/Dynamikdaten | 45 endliche, nichtstumme Render mit PCM-/FFT-basierten Messungen | PASS für Datenerhebung; musikalische ECO-/STANDARD-Eignung NOT RUN |
| GUI, Audio-Interfaces, reale Projekte, Start/Stop/Loop, 47/47 Validator/pluginval | in dieser Umgebung keine realen Windows-/DAW-/Validator-Nachweise | NOT RUN, keine FINAL-Freigabe |

## Diagnostik und Testgrenzen

Der erste neue VST3-Mono-Test verlangte gleiche L/R-Samples hinter noch unterschiedlich belegten Float-IIR-Cabinet-/Cut-Historien. Zwei Qualitätsfälle überschritten dabei knapp 1e−6 (maximal 1,19e−6). Dieser Gleichheitsvertrag gilt am Amp-Ausgang vor dem Cabinet, nicht hinter einem zulässigerweise stereo arbeitenden Cabinet. Die neue isolierte Wrapper-Prüfung umgeht deshalb Cab/Cuts; der ursprüngliche Test mit aktiven Cab/Cuts, nichtdefault State und Bypass bleibt vollständig erhalten. Jetzt Delta 0 in allen drei Qualitäten, **Grenze unverändert**. Erstdiagnostik bleibt als `vst3-first-cab-history-diagnostic.txt` archiviert; weder Cabinet noch Amp wurden zum Grünwerden verändert.

Ein zusätzlich versuchtes Stereo-Orakel setzte bei einem Null-Eingang nach Parameter-/Moduswechsel exakt verschwindende AC-Varianz voraus. Es meldete sechs Fehler in neun neuen Assertions (75 Gesamtchecks). Diese Annahme ignoriert die Eigenhistorie der nichtlinearen Schaltung: Eine unabhängig mit Null gespeiste Referenz muss dieselben Bias-/Einschwingvorgänge durchlaufen. Das stärkere `stereo_quality_reference.cpp` vergleicht jetzt jedes Sample beider Kanäle einschließlich R-only-Phase, verschiedener Stereo-Signale und unabhängigem Latenz-FIFO; **alle neun Vergleiche Delta 0 bei unverändertem 1e−6-Grenzwert**. Die ursprünglichen 66 Moduschecks bleiben erhalten; die ungeeigneten neu eingeführten Null-Assertions und ihre Sanitizer-Diagnoseläufe sind archiviert. Weder Produktcode noch bestehende Tests wurden dafür geändert. Dieses Orakel gilt für die Verkabelung; die unabhängige physikalische Kennlinie wird zusätzlich durch den eingefrorenen HIGH-Vergleich abgesichert.

Ein späterer Sanitizer-Reconfigure-Aufruf ohne die vorhandene Sysroot-Umgebung scheiterte bereits am Buildtool `juceaide` (`X11/Xlib.h` bzw. `ft2build.h` fehlten). Das ist als Umgebungs-/Build-FAIL archiviert; nach Wiederherstellen derselben CPATH/LIBRARY_PATH/PKG_CONFIG-Umgebung wurde erneut gebaut. Kein Produktpatch und keine Unterdrückung von Sanitizer-Befunden. Die Wiederholungen stehen in den finalen Build-/Testlogs.

## Dateien, Reproduktion und verbleibender nächster Schritt

Produktänderungen: `Source/dsp/{DKCircuit,TriodeNetwork,PowerAmp5150,AmpSim}.h`, neu `PortSolve.h`, `Simd.h`, `AmpModes.h`; `Source/Parameters.h`, `PluginProcessor.h/.cpp`. Test-/Buildänderungen: Root- und `tests/ir_presets/CMakeLists.txt`, neue Solver-/Vorherreferenz-/Audio-/Mono-Orakel-/CPU-Tests, VST3-Host-Test-Erweiterung. Keine Änderung an Editor/GUI, `CabinetIR.h`, `PresetManager.h`, Preamp-/Supply-Schaltung oder aktivem Effektumfang. Exakte Dateiliste: Commit-Diff; Source-/Artefakthashes im Evidenzverzeichnis.

Reproduzierbare native Befehle und getrennte Testprofile stehen in [tests/cpu_quality/README.md](../tests/cpu_quality/README.md). Die Logs enthalten die tatsächlich ausgeführten Builds. Für CPU-Messungen vor/nach müssen derselbe Compiler, dieselben Flags/JUCE-Abhängigkeiten verwendet und andere Lasten gestoppt werden. Die vollständige vorherige `Source` wird direkt aus dem gepinnten Git-Commit extrahiert; die aktuelle Produktion darf nicht als „before“ eingesetzt werden. Sanitizer/Trace sind von CPU-Messungen zu trennen. Alle Messdaten bleiben einschließlich FAIL erhalten.

**Empfohlener nächster Schritt:** Zuerst kontrollierte Windows-/ASIO-Messung mit realem DI, festen HIGH-Referenzen und levelgematchtem Hörvergleich der Qualitätsmodi, insbesondere RED/Drive und Übergänge. Danach gezieltes Hotspot-Profiling des unverändert hochqualitativen Pfads und weitere mathematisch äquivalente Optimierung gegen dieselben PCM-/Deadline-Gates. Bis dahin ECO/STANDARD experimentell, HIGH bei Stereo/kleinen Puffern auf dieser CPU nicht echtzeitfest. Keine Version als FINAL veröffentlichen.

Übergabe: Commit und Push erfolgen ausschließlich auf dem bestehenden Entwicklungsbranch; `main` und ursprünglicher Reparaturbranch bleiben unverändert. Der vollständige Commit-Hash und die unabhängig gelesene Remote-HEAD-Bestätigung werden in der Übergabenachricht genannt (kein selbstreferenzieller Commit-Hash in diesem Dokument).
