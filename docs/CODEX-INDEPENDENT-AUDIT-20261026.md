# Unabhängiger technischer Audit – Djenty Donkey v0.5.6-beta

Prüfdatum: **2026-10-26, Europe/Berlin**. Keine Reparatur, Optimierung oder GUI-Änderung. **Kein Gesamt-QA-PASS, kein Release-Ready, keine Windows-/Studio-One-Freigabe.** Dies ist ein Quellcodeaudit mit nativen Messungen; offene Hostprüfungen bleiben ausdrücklich NOT RUN.

## 1. Grundlage, Regeln und Evidenzgrenzen

| Gegenstand | Verifizierter Stand |
|---|---|
| Zielrepository / geprüfter Code | `challanger2000/125A-Dystorator`, `81cc90dfd076f1510c02bf6ccc8318575dc4aa45` |
| Berichtsbranch | `audit/codex-independent-20261026`, vom unveränderten Import abgeleitet |
| Vorbericht | `550e8d096f76ef7f72b73194cc69074bbb07e2ee`, `docs/AUDIT-PRE-CODEX-20261026.md`; einzige Baumänderung gegenüber Import: dieser Bericht |
| Originalentwickler / HEAD | `SteveAsAConstant/djenty-donkey`, `e363027d7a125018bb1cb56acc14958bc9baf9ff` |
| Unabhängiger Herkunftsnachweis | `git rev-parse HEAD^{tree}` und Upstream-Commitbaum: beide `f8d392bec9972bc70483fbe21bd4859e105c644f` |
| Engineering | Privates `challanger2000/125A-Engineering`, main `b6df84b0a04d01e240cff745ccebf60d67750486`, per HTTPS lesbar |
| Ausführung | Linux 6.18.55 x86_64, GCC 14.2, AMD EPYC 9V74, 5 sichtbare CPUs, C++17, `-O2 -DNDEBUG` |
| Ausdrücklich nicht geprüft | Ausgelieferte Windows-Binärdatei, Installer-Ausführung, VST3-Wrapper, ASIO-Gerät, Studio One, reale NAM-Captures, Hörfreigabe |

Pflichteinstieg `START-HERE.md` vollständig gelesen. Angewendet: Engineering README; relevante Guardrails zu Testadäquanz, Release-Assertions, Busvertrag, Gain-Automation, Bypass, Zustand, Latenz, GUI-Lebensdauer und End-to-End-Kontrollinventar; Standards DSP-GUIDELINES, PROFESSIONAL-PLUGIN-BEHAVIOR, GUI-GUIDELINES, AI-ASSISTED-ENGINEERING, REPOSITORY-WORKFLOW, SOURCE-BASELINE-GATE, V3-DEVELOPMENT; alle drei QA-Dokumente; einschlägige Research zu Resampling, Pitch/Time, Lautsprechern, Wahrnehmung, verworfenen Ansätzen und offenen Fragen sowie V3-Roadmap. Keine Branding-Assets integriert, keine anderen Produkt-Repositories verändert.

Der Auftrag benennt ausdrücklich den importierten Beta-Quellcode als Auditbasis. Dies ist kein bestehendes genehmigtes kommerzielles 125A-FULL-Release: keine erfundene Gumroad-Version, kein erfundener Paket-Hash, keine Release-Baseline-Datei mit `verified=true`. Im Engineering-Release-Register findet sich kein Dystorator-Eintrag. FULL-Provenienz: benannter Import und Upstream-Beta; Demo-Provenienz: kein separater Demo-Zweig/Mechanismus im geprüften Zielstand nachgewiesen. Kundenpaketidentität und Owner-FULL-Freigabe: **NOT VERIFIED**, kein Hindernis für diesen ausdrücklich beauftragten Quellaudit, aber für eine Releasefreigabe.

Begriffe: **BESTÄTIGT (Code)** = nachvollziehbarer Daten-/Kontrollpfad; **MEASURED** = hier ausgeführt; **RISIKO** = plausible, nicht reproduzierte Auswirkung; **NOT RUN** = Prüfung nicht ausgeführt. Ein Codebefund ist kein behaupteter Host-Crash. Prioritäten: P0 nachgewiesener kritischer Datenverlust/Crash ohne brauchbare Umgehung; P1 wesentlicher Funktions-/Hostvertragsfehler; P2 begrenzter Defekt/Risiko/Qualitätslücke; P3 Wartung/Dokumentation. Kritische offene QA-Gates blockieren die Freigabe auch ohne P0.

## 2. Unabhängige Prüfung aller 13 Vorbefunde

| ID | Urteil / neue Priorität | Konkreter Nachweis und Einschränkung |
|---|---|---|
| A01 | BESTÄTIGT, **P1 statt P0** | `Source/PluginProcessor.cpp:45` akzeptiert mono/stereo unabhängig; `:207–212` liest nur Kanal 0; `:287–292` dupliziert den monophonen Amp auf L/R. Rechter Eingang kann nicht zum Amp gelangen. Der Kommentar „Guitar is mono“ erklärt die Absicht, erfüllt aber keinen Stereo-Busvertrag. R-only-Hostrepro NOT RUN. P0 ist ohne Crash/Datenverlust überzogen; erheblicher Routingfehler bleibt. |
| A02 | BESTÄTIGT als Engpass, P1 QA-Gate | `AmpSim.h:38–80,135–162`: 8× bei 44,1/48 kHz, vollständige nichtlineare Schaltung. Native CPU-Messung unten. Anwenderanzeige „60 % Studio One“ nicht unabhängig reproduziert. Keine Hochrechnung von Linux-4× auf Windows-8×. |
| A03 | BESTÄTIGT, P1 | `PitchShifter.h:353–376`, `PluginProcessor.cpp:125–136`: LIVE meldet 0, Messung −2 st bei 48 kHz: +6,2 ms Onset / 15,2 ms Durchschnittsdelay. HQ meldet 2052 Samples; +46,9 ms Onset ist allein kein Beweis falscher fester PDC, da Analysefenster, Hüllkurve und Detektionsschwelle wirken. |
| A04 | BESTÄTIGT (Code), P1 | `PresetManager.h:95–118`: Factory setzt Parameter zurück, aber nicht IR; User lädt nur vorhandene IR, sonst bleibt bisherige. Hostrestore `PluginProcessor.cpp:379–396` löscht dagegen fehlenden Pfad. A → Factory Default bzw. User ohne verfügbare IR lässt A bestehen. JUCE-Audio-Repro NOT RUN. |
| A05 | BESTÄTIGT, P1 Freigabegate | Kein Workflow/CTest-Gate im importierten Baum. CMake erzeugt Executables, registriert keine `add_test`/`enable_testing`. Native 192-kHz-Tests sind kein Nachweis der 8×-Gesamtkette. JUCE-Integration/Windows/Validator/Host NOT RUN. |
| A06 | BESTÄTIGT, P2 | Beide Varianten `KorenPentode::plate(300,0,0,...)` liefern NaN, unabhängig reproduziert mit ASan/UBSan; `PowerAmp5150.h:35–65,80–100`. −1 V wird im getesteten Punkt zu 0, kleine positive Spannung ist endlich. Erreichbarkeit einer exakt null werdenden Schirmspannung im Normalbetrieb nicht bewiesen. |
| A07 | BESTÄTIGT als Produktanforderung, P2 | `PluginEditor.cpp:318–320,336–389`: 1480×840, Minimum 1200×700, 13 Sektionen in 4 Reihen. „Zu groß“ ist kein numerischer DSP-Bug. Kompaktlayout fehlt; Windows-DPI/Lesbarkeit NOT RUN. Inventar unten, kein GUI-Entwurf. |
| A08 | BESTÄTIGT, P2 Option | `AmpSim.h:150–156`: beide Pedale pro OS-Sample, auch an Mix-Endpunkten. Absicht: Zustände warm halten. Nur den inaktiven Aufruf zu streichen wäre keine klanggesicherte Optimierung. |
| A09 | BESTÄTIGT, P2 offene Messung | `AmpSim.h:57–69`: 3×(0,1+0,1)+0,15 = 0,75 s simulierte Zeit, zusätzlich Module-Initialisierung. Kein Nachweis 0,75 s Wandzeit. JUCE-prepare-Timing NOT RUN. |
| A10 | BESTÄTIGT, P2 | `PluginProcessor.cpp:369–396`, `PresetManager.h:126–134`: absoluter Pfad, keine IR-Bytes/Hash/Assetversion. Andere Datei unter gleichem Pfad kann anderen Klang ergeben; fehlende Datei führt bei Hostrestore zu Built-in, ohne Fehleranzeige. |
| A11 | RISIKO BESTÄTIGT, P2; Crash NOT RUN | `PluginEditor.cpp:151–188`: Modal-Callbacks erfassen `[this]`, kein SafePointer/Cancel im PresetBar-Destruktor. Assoziierte Komponente in JUCE ist ohne Prüfung des Modal-Managers kein Lebensdauernachweis. Editor zerstören, dann Dialog bestätigen: gezielter Windows/ASan-Test fehlt. Kein behauptetes UAF-Repro. |
| A12 | BESTÄTIGT und erweitert, P1 QA-Gate | `dsp_test.cpp:1069–1092`: Standardlauf gated nur faultRecovery; Transpose/Gate/CPU/Noise/Tubes/Tabellen sind Diagnose. `tone_regress.cpp:225–247,354–376` besitzt aktive Fehlerschwellen. Auch `amp_path_test.cpp:508–629` beendet Messmodi mit 0 ohne Qualitätsgate; NAM-Tools primär Vergleich/Fitting, keine automatischen Toleranzgates. |
| A13 | BESTÄTIGT als Distributionsgate, P2 | `LICENSE`, `CMakeLists.txt:25–38`, `installer/THIRD-PARTY-NOTICES.txt`: AGPL und unveränderte Djenty-Identität. Kommerzielle Nutzung und proprietäre Weitergabe sind unterschiedliche Fragen; AGPL verbietet Verkauf nicht. Lizenzpflichten/SDK-Rechte vor Distribution separat prüfen; dies ist kein Rechtsgutachten. |

**Kein Vorbefund vollständig widerlegt**, aber A01-Schwere korrigiert und die Beweisklassen bei A02/A07/A09/A11 begrenzt. Keine „13 reproduzierten Bugs“-Behauptung: darunter sind Messbedarf, Designentscheidungen und Releasegates.

## 3. Zusätzliche Befunde

### B01 – P1: IR-Laden verletzt dokumentierten JUCE-Threadvertrag

`PluginProcessor.h:42` fordert Message-Thread-Aufruf; `PluginEditor.cpp:13–30,88–102` ruft `loadIR`; `PluginProcessor.cpp:326` ruft direkt `cab.loadImpulseResponse`, während `:265–266` im Audio-Thread `cab.process` laufen kann. `irLock` schützt nur den String, `irLoaded` nur die Flagge, nicht diese Aufrufe.

**BESTÄTIGTER SDK-Vertragskonflikt**, Laufzeit-Race/Crash **NOT RUN**. Unabhängige Primärquelle: [JUCE 8.0.4 Convolution.h, Zeilen 100–105](https://github.com/juce-framework/JUCE/blob/8.0.4/modules/juce_dsp/frequency/juce_Convolution.h#L100): Methoden nicht interleaven; Load mit Process synchronisieren, praktisch Load aus Audio-Thread; Load-Funktionen wait-free. Ein allgemeiner einleitender Hinweis auf „thread-safe functions“ überstimmt diese konkrete Einschränkung nicht. Reparaturziel: serialisierte, begrenzte Übergabe, kein neuer Audio-Thread-Mutex. Pending/Ready/Failed sowie mehrere schnelle Load-Anfragen testen.

### B02 – P1: Feste Host-Tail-Angabe passt nicht zum Delay

`PluginProcessor.h:31` meldet immer 3,0 s; `Parameters.h:155–156` erlaubt 2000 ms und Feedback 0,95. `StereoDelay.h:35–53` enthält rekursives Echo mit Tiefpass, ohne 3-s-Abschaltung. Bei 2-s-Delay liegt die zweite Wiederholung bei 4 s, mit niederfrequentem Feedback nahe 0,95. Eine 3-s-Obergrenze ist daher strukturell falsch. **CIRCUIT/DSP DERIVED**, tatsächlicher Render-/Host-Abschneidetest **NOT RUN**. Auch lange IRs (`size=0`, keine Längenbegrenzung) und Reverb gehören in das Tail-Konzept. Rechenwert ohne Filter: bis −60 dB rund `2*ln(.001)/ln(.95)=269 s`; dies ist keine gemessene Reverb-/Plugin-Taildauer.

### B03 – P2: Ungültiges Preset kann gültigen Zustand ersetzen

`PresetManager.h:107–116` prüft nur XML-Parse-Erfolg, nicht Root-Tag, unterstützte Version oder vollständige Parametersemantik. Selbst `<unrelated/>` wird geparst, danach `resetAllToDefault()` ausgeführt und `currentName` gesetzt. Das ist kein transaktionales Reject. `setStateInformation` prüft wenigstens den Root, aber keine explizite zukünftige Formatversion/Parameter-Vollständigkeit. **BESTÄTIGT (Code)**, Datei-/Audio-Integration NOT RUN. Test: gültigen Zustand mit IR sichern, falsches/trunkiertes/inkompatibles XML laden; bei Ablehnung müssen Parameter, IR und Name unverändert bleiben.

### B04 – P2: IR-Flagge bedeutet nicht erfolgreich geladene IR

`PluginProcessor.cpp:321–340`: Nur Existenztest; danach asynchroner Convolution-Auftrag, sofort Pfad und `irLoaded=true`. JUCE 8.0.4 `Convolution.h:176–183` sagt ausdrücklich, dass Aktivierung erst nach Verarbeitung erfolgt. Kein Decode-Erfolg, keine Größen-/Formatvalidierung, keine Pending-/Fehlermeldung. Eine beschädigte WAV kann als aktuelles Cabinet erscheinen. `filesDropped` akzeptiert die erste existierende Datei der Liste, auch wenn nur eine andere Datei die Drag-Extension-Prüfung bestanden hat. **BESTÄTIGTER Zustands-/UX-Pfad**, konkretes Audio beim Decodierfehler NOT RUN. Kein pauschaler Crashnachweis.

### B05 – P2: Bypass, Automation und Reset haben offene Übergangsverträge

`PluginProcessor.cpp:180–199,235–282`: EQ-Koeffizienten ändern blockweise ohne Crossfade; Cab/Gate/Comp werden hart ein-/ausgeschaltet; deaktivierte Module frieren ihren Zustand ein. Delay/Reverb/Phaser werden beim Einschalten zurückgesetzt. `PitchShifter.h:348–358` leert Puffer bei Mode-/Null-Pitch-Wechsel ohne Audiocrossfade. Das sind nachweisbare Übergangspfade, **hörbare Klicks und maximale Pegelsprünge NOT RUN**. Amp-Knöpfe dagegen haben ausdrücklich Rampen/20-ms-Glättung und 20-ms-Kanal-/Pedal-Crossfade; nicht pauschal als ungesmoothed bezeichnen.

`triggerAsyncUpdate()` bei Latenzwechsel (`PluginProcessor.cpp:136`) und `updatePeak` CAS-Schleife (`.h:113–117`) erfordern RT-Prüfung: Message-Posting und nicht strikt beschränkte Retryzahl sind kein formaler Deadline-Nachweis. Dateiprüfung/IR-Laden im Host-State-Callback sowie Preset-Stringzugriffe benötigen einen dokumentierten Thread-/Lebensdauervertrag. Keine pauschale Behauptung, irLock liege im normalen processChunk-Pfad; dort wird er nicht genommen.

### B06 – P2: Gate-Key ist bei LIVE zeitlich nicht ausgerichtet

`PluginProcessor.cpp:125–128,228–233` verzögert den Key nur bei HQ. LIVE-Shifter ist aber um mehrere ms verzögert, bei laufendem Sustain variabel. Zusätzlich läuft Amp-Oversampling zwischen Key und Gate. Release/Hold können das kaschieren, garantieren aber keine intakten Transienten bei schnellem Input-Gate/kurzem Release. **BESTÄTIGTE Ausrichtungsdifferenz**, hörbare Abschneidung/Schwere NOT RUN. Test: Impuls-/Chug-Bursts nahe Threshold, alle Pitchwerte, beide Gates getrennt und zusammen, Dry-/Shifted-Onsets messen.

### B07 – P3: Wartungs-/GUI-Rückmeldungslücken

`build.bat:11–12` zeigt noch `VibeAmp515_artefacts` statt tatsächlichem CMake-Produkt `DjentyDonkey`. Kein Compilerfehler, aber falsche Ausgabehinweise. `PresetBar::refreshList()` läuft beim Aufbau/Step/Save/Delete; ComboBox-Load und späteres Hostrestore aktualisieren Delete-Status/aktuellen Namen nicht über einen eigenen Listener. Zustandsfeedback im offenen Editor gezielt prüfen. `Knob`/`VSlider` definieren Doppelklick-Defaults; der geforderte Ctrl-Linksklick ist nicht explizit implementiert – tatsächliches JUCE/Windows-Verhalten **NOT RUN**, nicht als bestanden führen.

## 4. DSP-Modulprüfung und Modellgrenzen

| Modul / Fundstelle | Auditresultat / fehlende unabhängige Absicherung |
|---|---|
| `Amp5150.h`, `AmpSim.h` | Gemeinsame Rails, zweiwegige Pre-/Power-Kopplung; 0 dBFS = 1 V Eingang, Speaker→Digital 1/100, CLEAN-Trim 54. Gute explizite Gain-Domänen. Built-in-Pegel, +24-dB-In/Out-Extrema und vollständiger Signalweg NOT RUN. |
| `TriodeStage.h` | Koren 12AX7 und Hermite-Softplus-Tabelle; Diagnose meldet sehr kleine Tabellenfehler. Test gegen exakt dieselbe Modellformel belegt Approximation, nicht reales Röhrenverhalten. Grid-/Miller-Modelle/Derivate außerhalb nomineller Punkte benötigen unabhängigen Jacobian-Sweep. |
| `TriodeNetwork.h:104–220` | Begrenzte Newton-/Backtracking-Schleifen, Predictor, Retry, Hold und Idle-Fallback. Native Faulttests bestanden. Worst-Case bis 6+40+200+40 Iterationen plus Backtracking macht „bounded“ nicht automatisch deadline-sicher. |
| `DKCircuit.h:117–139,260–417` | Rebuild nutzt vorbereitete Puffer, keine offensichtliche neue Größe bei normalen Potänderungen. Gauss-Jordan mit Pivoting, aber kein expliziter Singularitätsfehler vor Division. Puffergrenzen teilweise `assert`, im Release weg. Aktuelle feste Topologien nicht als nachweislich singular behauptet; Topologiegrenzen/Fehlerinjektion fehlen. |
| `LinearNetwork.h:117–153` | State-Space/Trapez, feste Arrays und Pivoting, ebenfalls keine Singularitätsrückgabe. Kein normaler Pot-Endpunktfehler reproduziert. Tone-Stack-Phasen-/Frequenzreferenz und Monotonietests fehlen als Gate. |
| `FullPreamp5150.h`, `Preamp5150.h` | RED, BLUE, BLUE ohne Crunch, vereinfachtes CLEAN; gemeinsame Zustände und mehrwegiger Fade. CLEAN ist bewusst keine vollständige 6505-Rhythmusschaltung. Gain-Kurven teilweise capturekalibriert; Extrapolation außerhalb erfasster Dialpositionen EMPIRICALLY TUNED. |
| `PowerAmp5150.h` | Koren plus Early-Term, Screen-Strom, 100-Ohm-Screenwiderstände, PI, Transformer/reactive load, Presence/Resonance. A06 bleibt offen. Physikalisch plausible Fallbacks nicht mit beliebigen „besseren“ Zahlen ersetzen. |
| `Supply5150.h` | Expliziter Euler, 60-Hz-Brückengleichrichtung mit 120-Hz-Ripple, mehrere Rails; Zeitkonstanten bei vorgesehenen OS-Raten plausibel. Transformatorwiderstand/Regulation dokumentierte Annahmen. Keine beliebigen Sub-Audio-Sampleraten als unterstützt unterstellen. |
| `TubeSet.h`, `TubeNoise.h` | Feste reproduzierbare Sätze, Noise optional; thermische/Shot-Anteile physikalisch motiviert, Flicker-Corner 5 kHz angenommener Parameter. Keine Messung genau dieser individuellen Röhren. Rausch-PSD/Autokorrelation/Sampleratenvergleich NOT RUN. |
| `PrecisionDrive.h` | PD/TS9 teilen OpAmpDrive, begrenzte 24-Iterationen-Schleife, Exponential-Clamp; keine eigene gleichwertige Solver-Reject/Retry-Statistik wie Amp. Attack 1–6, TS9 ohne Attack. Extrem-/Automation-/Recovery-Gate fehlt; NAM-Belege extern. |
| Oversampling `AmpSim.h:38–50,169–174` | 8×/4×/2× abhängig von Hostrate, Polyphase-IIR, Bruchlatenz auf Integer gerundet (`useIntegerLatency=false`). Rundung ist kein Nachweis eines ganzen-Sample-Alignments; Impuls-/Gruppenlaufzeit/8×-Alias-/Phasenmessung NOT RUN. Kein Wechsel auf 4× freigegeben. |
| Cabinet `PluginProcessor.cpp:79–86,262–275,321–340` | Built-in ist 5-Biquad-Kurve, **keine mitgelieferte echte Cabinet-IR**. Externe IR mono, trim+normalize, volle Länge. Stereo-IR-Trennung nicht erhalten; Bedarf produktseitig festlegen. Asset-/Threadfehler B01/B04. |
| `Filters.h` / EQ | RBJ-Biquads, Frequenz auf 0,45 fs begrenzt, daher 16-kHz-Band bei niedriger fs abweichend; nicht automatisch instabil. Koeffizienten direkt, keine Glättung; B05. Gate offen ist wegen Allpassphase nicht bitidentisch zum Bypass. |
| `MultibandGate.h` | LR4 300/3000 Hz, Low-Allpass für Bandphasengleichheit, Clean-Key mit HP80, Hysterese/Hold; Depth 0 bedeutet keine Dämpfung, nicht phasenneutraler Bypass. Input-Gate separat preamp. Zeit-/Frequenzkurven nur Diagnose. |
| `PitchShifter.h`, `FFT.h` | LIVE WSOLA mit punktuell konzentrierter Korrelationssuche; HQ PV mit Peak-Locking/Onset-Reset; A03/B05/B06. FFT-Roundtrip n=2048 unabhängig gemessen, Fehler `5.67327e-17`; keine vollständige FFT-Größen-/Aliasingfreigabe. |
| Compressor / Phaser / Reverb | JUCE-Module; Parameter setzen beweist keinen Funktions-PASS. Dry-Reverb-Skalierung im Code begründet. Transferkurven, Stereo-/Mix-Endpunkte, Interaktions-/Dauerstabilität NOT RUN. |
| `StereoDelay.h` | Vorallozierte 2,5-s-Ringe, interpolierte taps, 200-ms-Zeitglättung, gedämpfte rekursive Ping-Pong-Pfade. Mix additiv zum unveränderten Dry; 100 % ist kein Wet-only. Feedback/Mix springen blockweise; Tailfehler B02. |

Kein unbelegtes Urteil „alle Röhrenmodelle korrekt“: dieselbe Formel mit engerem Solver ist kein externes Schaltungs-/Hardware-Orakel. LTspice-Netzlisten und ausführliche chinesische Dokumentation sind wertvoll, die unabhängigen LTspice-/NAM-/Impedanz-/Realamp-Replikationen hier jedoch **NOT RUN**. Originalentwickler trennt selbst Fits von Schaltungswerten; diese Trennung muss erhalten bleiben.

## 5. Tests: Ausführung, Aussagekraft und Reproduktion

Alle hier aufgeführten Audit-Ausführungen sind neu, keine Übernahme früherer PASS-Meldungen. Build und Läufe im unveränderten Checkout, Ausgaben unter ignoriertem `build/audit/`; Logs sind lokale Hilfsdateien, wesentliche Ergebnisse in diesem Bericht. Keine GitHub Actions ausgelöst.

```sh
mkdir -p build/audit
g++ -std=c++17 -O2 -DNDEBUG -ISource tests/dsp_test.cpp -o build/audit/dsp_test
g++ -std=c++17 -O2 -DNDEBUG -ISource -Itests tests/tone_regress.cpp -o build/audit/tone_regress
./build/audit/dsp_test > build/audit/dsp.log 2>&1
./build/audit/tone_regress > build/audit/tone.log 2>&1
./build/audit/dsp_test transpose > build/audit/transpose.log 2>&1
./build/audit/dsp_test cpu > build/audit/cpu.log 2>&1
```

Jeden Exitcode unmittelbar prüfen, nicht durch `tail`, `tee` ohne pipefail oder eine spätere erfolgreiche Shell-Anweisung ersetzen.

| Prüfung | Status | Aussage / Grenze |
|---|---|---|
| Beide GCC-Builds | PASS | erfolgreich kompiliert, keine JUCE-Abhängigkeit dieser beiden Quellen |
| Vollständiger `tone_regress` | PASS, Exit 0 | **74 Fälle, 0 Fehler**; 74 moved, 74 fixed nach Runnerdefinition. Baseline 32575 ms, current 42812 ms; nur native 192-kHz-Module. |
| Vollständiger `dsp_test` | PASS für Fault-Gate, Exit 0 | 19 PASS-Fälle: NaN/Inf, Retry, inkorrekte nichtkonvergierte Lösung, Hold, 2-ms-Fault, 1-kV-Spike, 100-V-Sinus. Restliche Tabellen nicht automatisch assertiert. Kein Vollkettentest. |
| `dsp_test transpose` | MEASURED, Exit 0, kein Qualitätsgate | LIVE +6,2 ms, Report 0; HQ +46,9 ms, Report 2052. −2 st Sinus 98,00 / 97,97 Hz bei Soll 98,00; kein Pitch-/Artefakt-Gesamt-PASS. |
| `dsp_test cpu` | MEASURED, Exit 0, kein RT-PASS | siehe Profil; keine paced Deadlineprüfung |
| Null-Schirmspannungsprobe | FAIL-REPRO | beide Modellpfade NaN; Sanitizerlauf ohne zusätzlichen ASan/UBSan-Diagnosebericht. IEEE-NaN selbst ist kein notwendiger Sanitizerfehler. |
| FFT Roundtrip 2048 | PASS | erwartetes forward/inverse-Ergebnis /N, Fehler 5,67327e−17 < 1e−12 |
| LIVE Reset-gegen-Neuinitialisierung, ein Sinusfall | MEASURED | Maxdelta 0; ein vermuteter hörbarer Reset-Fehler wurde damit **nicht** bestätigt. Kein Beweis aller Pitch-/Silence-Zustände. |
| `amp_path_test`, alle Modi | NOT RUN | benötigt JUCE; CMake fehlt in dieser Instanz, kein JUCE-Pluginbuild vorbereitet; keine behauptete Linux- oder Windows-8×-Verifikation |
| `nam_compare`, `pedal_compare`, `power_compare` | NOT RUN | authentische externe .nam-Fixtures fehlen; selbst Exit 0 würde nur abgeschlossene Diagnostik bedeuten |
| LTspice-Netzlisten gegen neue unabhängige Simulation | NOT RUN | LTspice hier nicht vorhanden |
| Windows VST3 / 47-Validator / pluginval / 125A Tester / Studio One | NOT RUN | kein Windows-/Hostzugang in dieser Linux-Instanz |
| Installer / Upgrade / Deinstallation / ASIO / echte DI-A/B-Hörprüfung | NOT RUN | Plattform/Assets/Owner-Abnahme fehlen |

### Eigenständige numerische Reproduktion

Als `build/audit/boundary.cpp` speichern und mit dem darunterstehenden Befehl ausführen; keine Produktquelle verändern:

```cpp
#include <cstdio>
#include <cmath>
#include <vector>
#include <complex>
#include "dsp/PowerAmp5150.h"
#include "dsp/FFT.h"
int main() {
    for (bool exact : {false, true})
        for (double v : {0.0, -1.0, 1e-12, 250.0}) {
            vamp::KorenPentode p; p.exact = exact;
            double a, b, c; p.plate(300, 0, v, a, b, c);
            std::printf("exact=%d screen=%g ip=%g dpk=%g dg=%g finite=%d\n",
                exact, v, a, b, c,
                int(std::isfinite(a) && std::isfinite(b) && std::isfinite(c)));
        }
    vamp::FFT f; f.prepare(2048);
    std::vector<std::complex<double>> x(2048); x[3] = 1;
    f.transform(x.data(), false); f.transform(x.data(), true);
    double e = 0;
    for (int i=0; i<2048; ++i)
        e = std::max(e, std::abs(x[i]/2048.0 - (i==3 ? 1.0 : 0.0)));
    std::printf("FFT roundtrip max_error=%g\n", e);
    return e > 1e-12;
}
```

```sh
g++ -std=c++17 -O1 -g -fsanitize=undefined,address -fno-omit-frame-pointer \
  -ISource build/audit/boundary.cpp -o build/audit/boundary
ASAN_OPTIONS=detect_leaks=0 ./build/audit/boundary
```

Beide `screen=0`-Zeilen: `ip=-nan dpk=-nan dg=-nan finite=0`; `screen=250`: Strom 0,180767 A, dpk 0,000108421 A/V, dg 0,00845232 A/V. Diese Probe ist absichtlich Diagnostik, kein neues stillschweigend grünes NaN-Regressionsgate; zukünftiger Test muss `finite==0` als Fehler zählen.

### Testadäquanz im gesamten vorhandenen Bestand

- `tone_regress`: 36 Preamp-, 18 Power-, 8 Drive-, 12 Chain-Fälle. Gate: endlicher Audiofehler, <−60 dB oder höchstens Baselinefehler +3 dB, Banddelta <0,5 dB. „moved“ ist kein Fehler. „fixed“ ist nur die Interpretation dieses Runners gegenüber seiner eingefrorenen v0.5-Baseline. Ein Filter, der keinen Namen trifft, kann 0 Fälle/Exit 0 erzeugen: Casecount muss extern >0 geprüft werden. Referenz teilt Schaltung und Modellannahmen mit Produkt; keine unabhängige Hardwarevalidierung. Kein AmpSim/Oversampler/IR/FX/State/UI.
- `dsp_test`: fault ist echtes Failgate; default gated diese Faultfälle, nicht sämtliche vorhergehenden Diagnosewerte. Die übrigen Modi können bei fachlich schlechten Messwerten weiterhin 0 melden. Fehlende Hostkette und Assertions sind wesentlichere Lücken als die reine Zahl der Tests.
- `amp_path_test`: wichtiges vorhandenes Werkzeug für tatsächlich oversampleten AmpSim inklusive bewegter Regler, Schaltvorgänge, Aliasing und optionaler NAM; dennoch Diagnose-Ausgänge statt deterministischer Akzeptanzschwellen. Enthält nicht den ganzen PluginProcessor mit IR/Comp/Gate/PostFX/State/GUI.
- Drei NAM-Vergleichsprogramme: Modell-/Gain-Kalibration, Spektren/Transienten und Fits; Fehler bei unlesbarer Datei werden gemeldet, aber erfolgreiche Datei bedeutet keine erreichte Klangtoleranz. Eigener eingeschränkter JSON-/WaveNet-Leser in `tests/NamWaveNet.h` ist kein allgemein validierter NAM-Referenzdecoder; Format-/Weights-/Versionsfehler separat testen.
- `tests/baseline/`: eingefrorene Gegenprobe, keine Testsuite und keine externe Ground Truth. Nicht erneuern, um Abweichungen grün zu machen.
- `spice/`: Referenzschaltungen, keine hier ausgeführte Simulation und kein automatisch ausgewerteter CI-Grenzwert. Testbench-Quellwerte nicht mit tatsächlichem aktuellen SPICE-Lauf verwechseln.
- Fehlend: Mutation-/Negativnachweise für Gates, vollständige Parameter- und State-Inventarregression, Corpus echter lizenzierter DI-Fixtures, 8×-Aliasgrenze, alle Tube-/Noise-/Crunch-Kombinationen, alle unterstützten Sampleraten und Blockpartitionen sowie formatnahe Lebensdauer-/Threadtests.

## 6. CPU-Engpässe und sichere Optimierungsreihenfolge

Native Diagnose (`cpu`, 48-kHz-Annahme, 64 Samples, **4×** intern):

| Pfad | Mittel % eines Kerns | größter gemessener Block % der nominalen Deadline |
|---|---:|---:|
| Precision Drive | 1,92 | 64,2 |
| TS9 | 1,82 | 27,7 |
| CLEAN Preamp | 8,28 | 175,1 |
| BLUE Preamp | 40,56 | 880,0 |
| RED Preamp Gain 0,7 | 44,33 | 409,7 |
| RED Preamp Gain 1,0 | 47,92 | 951,4 |
| Power amp | 34,01 | 652,0 |
| Drive + RED + Power | 92,69 | 1477,0 |
| CLEAN + Power | 46,80 | 857,3 |
| Gate | 0,19 | 80,9 |
| LIVE −2 | 0,16 | 30,7 |
| HQ −2 | 3,66 | 19,5 |

**Nicht als Host-Overrun-Beweis verwenden:** unpaced Offline-Runner, geteilte VM, gleichzeitig andere native Auditläufe, Scheduler/Warm-up nicht isoliert; Governor/physische Exklusivität unbekannt. Keine p95/p99-Erfassung und keine Profiler-Overheadkalibration. Relativer Hinweis auf Solverlast, keine belastbare Produktkapazitätsplanung. Unterschiedliche Tabellen des Runners verwenden unterschiedliche Lastsignale/Initialisierungen; Einzelwerte nicht addieren oder linear auf 8× hochrechnen.

Solverstatistik dieser Diagnose: CLEAN 1,00, BLUE 1,33, RED 1,69, Power 1,27 Newton-Auswertungen/Sample; nominell 0 gemeldete Solverfehler. Bounded Fallbacks, kontinuierliche Schaltungs-Rebuilds unter Gain/Presence/Resonance-Automation, Mehrwege-Channel-Fades sowie LIVE-Splice-Korrelation sind Kandidaten für Blockspitzen.

| Kandidat | Realistisches Potenzial / Risiko | Vor Freigabe erforderlich |
|---|---|---|
| konstante Kontrollarbeit überspringen | EQ-Trigonometrie/`dcBlock.set`/unveränderte Zustandsberechnungen reduzieren; wahrscheinlich kleiner als Solverkern | exakte Audio-/Automation-/Restart-Gleichheit, End-to-End-Messung |
| Matrix-/Jacobian-Kernels spezialisieren | größte Architekturfläche, kleine dichte Matrizen; keine pauschale Sparse-LU-Empfehlung | Residual-/Fallback-/Bit- bzw. Toleranzvergleich, fixed-size Benchmark und Hostp99 |
| Pedal-Endpunkt fast path | spart bei Boost die inaktive Pedalberechnung, verglichen mit Amp eher kleiner Gewinn | warm-state/catch-up-Konzept und schnelle A↔B↔A-Tests; kein simples Stoppen |
| Wsola-Suche zeitlich verteilen | reduziert punktuelle Korrelationsspitzen | identischer Kandidatenentscheid oder deklarierter, gemessener Klangunterschied |
| Settle-Zustand wiederverwenden | verkürzt prepare, nicht normalen Callback | exakter Röhrensatz/Rate/Versorgung/Noise-/Startzustand; keine versteckte Shared-State-Instanz |
| SIMD / Compiler-LTO | abhängig von konkretem Kernel/CPU | Windows-Releaseartefakt, keine Fast-Math-Annahme, NaN/Inf/Fallback/Denormal erneut prüfen |
| 8×→4×, weniger Iterationen, gröbere Reglerupdates | **keine klangneutrale Freigabe**; vorhandene Dokumentation nennt Alias-/Smoothing-Nachteile | unabhängige Alias-/THD/IMD/Transienten-/Null-/DI-Hörbelege, vorab definierte Toleranz |

Zuerst Windows-VST3 End-to-End messen: 44,1/48/88,2/96/176,4/192 kHz, 32/64/128/256/512/1024 Frames sowie 0/1/variable/über maxBlock; alle Kanäle, Boostmodelle, Noise/TubeSets, Pitch, IR-Länge, FX und schnelle Automation. Warm-/Cold-Start, mean/p95/p99/max, Deadline, Overruns, Instrumentierungs-Overhead, paced und offline getrennt. Stereoentscheidung und Zweikettenkosten vor einem CPU-Ziel festlegen.

## 7. Vollständige Parameter-/Bedienelementinventur

Inventar aus `Parameters.h` und `PluginEditor.cpp`, keine erfundenen GUI-Funktionen. Für **jede** folgende Zeile gilt: statischer UI→APVTS→Processor-Pfad geprüft; echte Maus-/Tastaturaktion, Hostautomation, Audiofolge und Save/Reload/Reopen **NOT RUN**. Keine Zeile behauptet E2E-PASS. Parameter werden in APVTS-State gespeichert, IR separat; Gleichheit des hörbaren Recalls bleibt zu testen. Stabile IDs und Defaults bei späterer GUI-Migration erhalten.

Gemeinsame Eingabepfade: Knob/VSlider → SliderAttachment; Section/Toggle/ToggleStack → ButtonAttachment; ChoiceButtons/ChannelSelector → ParameterAttachment mit vollständiger Host-Geste; TubeSet → ComboBoxAttachment. Feedback durch Attachments; Meter separat über Atomics/Timer. Default-Reset und Rand-/Zwischenwerte je Kontrolle testen, nicht bloß Existenz.

Float-Spalte enthält originale Min/Max/Default/Einheit, ggf. Dezimalstellen/Skew-Centre aus der Deklaration; Bool off/on und Default, Int Min/Max/Default, Choice Labels und Defaultindex.

| Parameter-ID | Funktion | Bereich / Default / Zusatz | Fundstellen in Source/ | Audiopfad | E2E |
|---|---|---|---|---|---|
| `inGain` | Input | `-24.0f, 24.0f, 0.0f, " dB"` | Parameters.h:94; Editor.cpp:213 | inputGain | NOT RUN |
| `inGateOn` | Input Gate On | `off/on; off` | Parameters.h:96; Editor.cpp:216 | InputGate | NOT RUN |
| `inGateThresh` | Input Gate Threshold | `-90.0f, 0.0f, -50.0f, " dB"` | Parameters.h:97; Editor.cpp:217 | InputGate | NOT RUN |
| `inGateRelease` | Input Gate Release | `2.0f, 200.0f, 20.0f, " ms", 0, 20.0f` | Parameters.h:98; Editor.cpp:218 | InputGate | NOT RUN |
| `gateOn` | Gate On | `off/on; on` | Parameters.h:100; Editor.cpp:220 | MultibandGate | NOT RUN |
| `gateThresh` | Gate Threshold | `-90.0f, 0.0f, -60.0f, " dB"` | Parameters.h:101; Editor.cpp:221 | MultibandGate | NOT RUN |
| `gateRelease` | Gate Release | `5.0f, 500.0f, 80.0f, " ms", 0, 80.0f` | Parameters.h:102; Editor.cpp:222 | MultibandGate | NOT RUN |
| `gateLow` | Gate Low Depth | `0.0f, 100.0f, 30.0f, " %", 0` | Parameters.h:103; Editor.cpp:223 | MultibandGate | NOT RUN |
| `gateMid` | Gate Mid Depth | `0.0f, 100.0f, 60.0f, " %", 0` | Parameters.h:104; Editor.cpp:224 | MultibandGate | NOT RUN |
| `gateHigh` | Gate High Depth | `0.0f, 100.0f, 100.0f, " %", 0` | Parameters.h:105; Editor.cpp:225 | MultibandGate | NOT RUN |
| `transpose` | Transpose | `-12, 12, 0` | Parameters.h:107; Editor.cpp:229 | PitchShifter / PDC | NOT RUN |
| `transposeMode` | Transpose Mode | `Live / HQ; Index 0` | Parameters.h:108; Editor.cpp:230 | PitchShifter / PDC | NOT RUN |
| `compOn` | Comp On | `off/on; off` | Parameters.h:111; Editor.cpp:234 | JUCE Compressor / Makeup | NOT RUN |
| `compThresh` | Comp Threshold | `-60.0f, 0.0f, -20.0f, " dB"` | Parameters.h:112; Editor.cpp:235 | JUCE Compressor / Makeup | NOT RUN |
| `compRatio` | Comp Ratio | `1.0f, 20.0f, 4.0f, ":1", 1, 4.0f` | Parameters.h:113; Editor.cpp:236 | JUCE Compressor / Makeup | NOT RUN |
| `compAttack` | Comp Attack | `0.1f, 100.0f, 5.0f, " ms", 1, 10.0f` | Parameters.h:114; Editor.cpp:237 | JUCE Compressor / Makeup | NOT RUN |
| `compRelease` | Comp Release | `10.0f, 1000.0f, 100.0f, " ms", 0, 150.0f` | Parameters.h:115; Editor.cpp:238 | JUCE Compressor / Makeup | NOT RUN |
| `compMakeup` | Comp Makeup | `0.0f, 24.0f, 0.0f, " dB"` | Parameters.h:116; Editor.cpp:239 | JUCE Compressor / Makeup | NOT RUN |
| `boostOn` | Boost On | `off/on; off` | Parameters.h:118; Editor.cpp:241 | AmpSim PD / TS9 | NOT RUN |
| `boostDrive` | Boost Drive | `0.0f, 10.0f, 0.0f` | Parameters.h:119; Editor.cpp:246 | AmpSim PD / TS9 | NOT RUN |
| `boostTone` | Drive Bright | `0.0f, 10.0f, 5.0f` | Parameters.h:120; Editor.cpp:247 | AmpSim PD / TS9 | NOT RUN |
| `boostLevel` | Drive Volume | `0.0f, 10.0f, 7.0f` | Parameters.h:121; Editor.cpp:245 | AmpSim PD / TS9 | NOT RUN |
| `boostAttack` | Drive Attack | `1, 6, 4` | Parameters.h:122; Editor.cpp:248 | AmpSim PD / TS9 | NOT RUN |
| `boostModel` | Drive Model | `Precision Drive / TS9; Index 0` | Parameters.h:123; Editor.cpp:242 | AmpSim PD / TS9 | NOT RUN |
| `channel` | Channel | `Clean / Blue / Red; Index 2` | Parameters.h:126; Editor.cpp:261 | AmpSim / Preamp / Power | NOT RUN |
| `ampBright` | Bright | `off/on; on` | Parameters.h:128; Editor.cpp:263 | AmpSim / Preamp / Power | NOT RUN |
| `ampCrunch` | Crunch | `off/on; on` | Parameters.h:129; Editor.cpp:264 | AmpSim / Preamp / Power | NOT RUN |
| `ampNoise` | Amp Noise | `off/on; off` | Parameters.h:130; Editor.cpp:266 | AmpSim / Preamp / Power | NOT RUN |
| `tubeSet` | Tube Set | `Stock / Set A / Set B / Set C / Set D / Set E; Index 0` | Parameters.h:131; Editor.cpp:268 | AmpSim / Preamp / Power | NOT RUN |
| `ampGain` | Gain | `0.0f, 10.0f, 6.9f` | Parameters.h:133; Editor.cpp:271 | AmpSim / Preamp / Power | NOT RUN |
| `bass` | Bass | `0.0f, 10.0f, 5.0f` | Parameters.h:134; Editor.cpp:272 | AmpSim / Preamp / Power | NOT RUN |
| `mid` | Mid | `0.0f, 10.0f, 5.0f` | Parameters.h:135; Editor.cpp:273 | AmpSim / Preamp / Power | NOT RUN |
| `treble` | Treble | `0.0f, 10.0f, 6.0f` | Parameters.h:136; Editor.cpp:274 | AmpSim / Preamp / Power | NOT RUN |
| `presence` | Presence | `0.0f, 10.0f, 5.0f` | Parameters.h:137; Editor.cpp:275 | AmpSim / Preamp / Power | NOT RUN |
| `resonance` | Resonance | `0.0f, 10.0f, 5.0f` | Parameters.h:138; Editor.cpp:276 | AmpSim / Preamp / Power | NOT RUN |
| `master` | Master | `0.0f, 10.0f, 4.6f` | Parameters.h:139; Editor.cpp:277 | AmpSim / Preamp / Power | NOT RUN |
| `ampVolume` | Amp Volume | `-40.0f, 12.0f, 0.0f, " dB"` | Parameters.h:140; Editor.cpp:278 | AmpSim / Preamp / Power | NOT RUN |
| `cabOn` | Cab On | `off/on; on` | Parameters.h:142; Editor.cpp:282 | Cab / Built-in | NOT RUN |
| `eqOn` | EQ On | `off/on; off` | Parameters.h:144; Editor.cpp:285 | EQ-Biquads | NOT RUN |
| `eq0` | EQ 31 Hz | `−12…+12 dB; 0 dB` | Parameters.h:146; Editor.cpp:287 | EQ-Biquads | NOT RUN |
| `eq1` | EQ 62 Hz | `−12…+12 dB; 0 dB` | Parameters.h:146; Editor.cpp:287 | EQ-Biquads | NOT RUN |
| `eq2` | EQ 125 Hz | `−12…+12 dB; 0 dB` | Parameters.h:146; Editor.cpp:287 | EQ-Biquads | NOT RUN |
| `eq3` | EQ 250 Hz | `−12…+12 dB; 0 dB` | Parameters.h:146; Editor.cpp:287 | EQ-Biquads | NOT RUN |
| `eq4` | EQ 500 Hz | `−12…+12 dB; 0 dB` | Parameters.h:146; Editor.cpp:287 | EQ-Biquads | NOT RUN |
| `eq5` | EQ 1000 Hz | `−12…+12 dB; 0 dB` | Parameters.h:146; Editor.cpp:287 | EQ-Biquads | NOT RUN |
| `eq6` | EQ 2000 Hz | `−12…+12 dB; 0 dB` | Parameters.h:146; Editor.cpp:287 | EQ-Biquads | NOT RUN |
| `eq7` | EQ 4000 Hz | `−12…+12 dB; 0 dB` | Parameters.h:146; Editor.cpp:287 | EQ-Biquads | NOT RUN |
| `eq8` | EQ 8000 Hz | `−12…+12 dB; 0 dB` | Parameters.h:146; Editor.cpp:287 | EQ-Biquads | NOT RUN |
| `eq9` | EQ 16000 Hz | `−12…+12 dB; 0 dB` | Parameters.h:146; Editor.cpp:287 | EQ-Biquads | NOT RUN |
| `phaserOn` | Phaser On | `off/on; off` | Parameters.h:148; Editor.cpp:290 | JUCE Phaser | NOT RUN |
| `phRate` | Phaser Rate | `0.05f, 5.0f, 0.5f, " Hz", 2, 0.8f` | Parameters.h:149; Editor.cpp:291 | JUCE Phaser | NOT RUN |
| `phDepth` | Phaser Depth | `0.0f, 1.0f, 0.6f, "", 2` | Parameters.h:150; Editor.cpp:292 | JUCE Phaser | NOT RUN |
| `phFeedback` | Phaser Feedback | `-0.9f, 0.9f, 0.3f, "", 2` | Parameters.h:151; Editor.cpp:293 | JUCE Phaser | NOT RUN |
| `phMix` | Phaser Mix | `0.0f, 1.0f, 0.5f, "", 2` | Parameters.h:152; Editor.cpp:294 | JUCE Phaser | NOT RUN |
| `delayOn` | Delay On | `off/on; off` | Parameters.h:154; Editor.cpp:296 | StereoDelay | NOT RUN |
| `dlTime` | Delay Time | `20.0f, 2000.0f, 380.0f, " ms", 0, 400.0f` | Parameters.h:155; Editor.cpp:297 | StereoDelay | NOT RUN |
| `dlFeedback` | Delay Feedback | `0.0f, 0.95f, 0.35f, "", 2` | Parameters.h:156; Editor.cpp:298 | StereoDelay | NOT RUN |
| `dlMix` | Delay Mix | `0.0f, 1.0f, 0.3f, "", 2` | Parameters.h:157; Editor.cpp:299 | StereoDelay | NOT RUN |
| `dlPingPong` | Ping Pong | `off/on; off` | Parameters.h:158; Editor.cpp:300 | StereoDelay | NOT RUN |
| `reverbOn` | Reverb On | `off/on; off` | Parameters.h:160; Editor.cpp:302 | JUCE Reverb | NOT RUN |
| `rvSize` | Reverb Size | `0.0f, 1.0f, 0.5f, "", 2` | Parameters.h:161; Editor.cpp:303 | JUCE Reverb | NOT RUN |
| `rvDamping` | Reverb Damping | `0.0f, 1.0f, 0.5f, "", 2` | Parameters.h:162; Editor.cpp:304 | JUCE Reverb | NOT RUN |
| `rvWidth` | Reverb Width | `0.0f, 1.0f, 1.0f, "", 2` | Parameters.h:163; Editor.cpp:305 | JUCE Reverb | NOT RUN |
| `rvMix` | Reverb Mix | `0.0f, 1.0f, 0.25f, "", 2` | Parameters.h:164; Editor.cpp:306 | JUCE Reverb | NOT RUN |
| `outGain` | Output | `-24.0f, 24.0f, 0.0f, " dB"` | Parameters.h:166; Editor.cpp:309 | outputGain | NOT RUN |

**65 Parameter** einschließlich zehn einzelner EQ-Bänder. Alle in der Tabelle benannten Definitionen stammen aus dem auditierten Import.

### Interaktive Nichtparameter und übrige Funktionen

| Element / Aktion | Callback → Wirkung / Feedback | State / Automation / Test |
|---|---|---|
| Preset-Auswahl | PresetBar box.onChange → `presets.load(index)`; 6 Factory-Presets + User-XML | Name separat, Parameter gestured; A04/B03/B07; E2E NOT RUN |
| Preset `<` / `>` | `step(-1/+1)` → Load + refreshList, Wrap-around | Command, kein Parameter; NOT RUN |
| SAVE / Namensfeld / Save / Cancel / Return / Escape | `savePreset`, asynchroner Alert, LegalFileName, XML schreiben | bestehender Name überschreibt ausdrücklich; Rückgabefehler nicht als UI-Fehler gezeigt; A11; NOT RUN |
| DELETE / Confirm / Cancel | `deletePreset`, asynchroner Callback, Datei entfernen | nur User, Dateifehler-Rollback und Lifecycle NOT RUN |
| FOLDER | Presetordner über `startAsProcess` öffnen | bewusst lokaler Shell-Open, keine DSP-Aktion; NOT RUN |
| IR LOAD / Dateiauswahl / Cancel | FileChooser → `proc.loadIR` | Pfad separat, nicht automatable; B01/B04; NOT RUN |
| IR `<` / `>` | `stepIR` sortiert WAV/AIF/AIFF im Ordner, Wrap-around | keine Parameter; fehlender aktueller Eintrag/Ordnerrechte testen; NOT RUN |
| BUILT-IN | `clearIR` → fünf Filter statt Convolution | Flag/Pfad, nicht eigene Parameter-ID; NOT RUN |
| IR Drag Enter/Exit/Drop | Highlight + erste existierende Datei → Load | Mehrfachdrop/falsches Format B04; NOT RUN |
| Input-/Outputmeter Klick | peak hold / clipTicks lokal zurücksetzen | reine Anzeige, keine Projektzustandsänderung; NOT RUN |
| 3 Gate-Bandanzeigen | Atomic gains → 30-Hz-Timer | keine interaktive DSP-Steuerung; NOT RUN |
| Fenster-Resize | vier proportionale Reihen, Limits 1200×700–2400×1500 | kein gespeicherter Zoom-/Fenstergrößenparameter; Host/DPI NOT RUN |
| TS9-Auswahl | Titel/Volume→Level/Bright→Tone, Attack deaktiviert | `ChoiceButtons.setOnChange`, DSP-Parameter bleibt vorhanden; Automation/Recall NOT RUN |
| Channel-Auswahl | Clean/Blue/Red + Akzentfarbe | gleiche Parameterbindung wie Host, Channel-Fades im DSP; NOT RUN im Editor |
| Regler-Doppelklick / Texteingabe / Wheel / Modifier | JUCE Slider; DoubleClickDefault beim Knob explizit, EQ auf 0 | Ctrl-Reset, Textgrenzen, vollständige Host-Gesten NOT RUN |
| Standalone Geräte-/Audioeinstellungen | JUCE StandaloneFilterWindow/Holder | ASIO unter WIN32, Mute-Input-/Feedbackwarnung bewusst deaktiviert (`StandaloneApp.cpp:44–46`); reale Rückkopplungsrisiken abhängig von Verkabelung; NOT RUN |
| Hostprogramme / MIDI / Globalbypass | ein Hostprogramm, kein MIDI-In/Out; kein eigenes Global-Bypass-Parameter | Hostbypass-/Latency-Verhalten des JUCE-Wrappers NOT RUN, keine eigene Funktion erfinden |

Für das spätere Kompakt-GUI daraus zunächst Informationshierarchie ableiten: Input/Transpose/Dynamics, Drive/Amp, Cab/EQ, FX, Presets/Output. Das ist eine Funktionsgruppierung, kein freigegebener Layoutentwurf. Attack im TS9, Bright im RED und Crunch außerhalb BLUE müssen als kontextabhängig gekennzeichnet bleiben; alle IDs auch bei unsichtbarer Seite erhalten.

## 8. Anforderungs-/Testmatrix für die Reparatur- und Freigabephase

Status bezieht sich auf den **genannten Vertrag**, nicht nur auf ein existierendes API. Bei statischem FAIL ist die noch ausstehende Laufzeitprüfung ausdrücklich angegeben. Positiv- und Negativfälle müssen vor Implementierung als tatsächlich fehlschlagende Tests vorliegen.

| Req | Kette / erwartetes Ergebnis | Positivtest | Negativ / Grenze / Rollback | Evidenz / Status heute |
|---|---|---|---|---|
| R01 Stereo | Hostbus→Amp→L/R, rechter Input wirkt, kein unbeabsichtigtes Übersprechen | L-only/R-only/verschiedene Frequenzen, getrennte Ketten | Anti-Phase, Stereo→Mono→Stereo | Code FAIL A01; Host NOT RUN |
| R02 Mono | Mono rein, beabsichtigter Stereo-FX-Out; Mono-Out korrekt gemittelt | gleiche Inputpegel, Ping-Pong/Reverb | 1→1, 1→2, 2→1, leere Blöcke | Codepfad vorhanden; Audio NOT RUN |
| R03 Numerik | akzeptierte Lösung endlich, Rails/Zustände konsistent | nomineller Burst/Decay | Nullscreen, NaN/Inf, ±Spikes, Persistenz, Wiederkonvergenz | Fault-Teil PASS; Zero-Screen FAIL; Vollkette NOT RUN |
| R04 Klangreferenz | unabhängige, pegelrichtige Reproduktion | DI, Sweeps, THD/IMD, alle Gain-/Röhrensätze | leise Decays, extreme Controls, Null-/Deltaspektren | native 74/74; unabhängiges Klangorakel NOT RUN; echte DI/Hardware NOT RUN |
| R05 OS | Alias-/Phasen-/Delayziel am finalen Amp-Pfad | Sinus 1–5 kHz und Multitone, alle Raten | schwere Clippingpegel, 8× vs hochwertigem Orakel | NOT RUN |
| R06 PDC | Report und definierte Audiodauer passen zusammen | LIVE/HQ Pitch ±, korrelierte Bursts | 0↔Pitch, LIVE↔HQ im Loop/Parallelbus | LIVE Differenz MEASURED, Host NOT RUN |
| R07 Gate | Hold/Release/Depth und Key-Ausrichtung erhalten Noten | Bursts ober-/unterhalb Threshold, alle Bänder | Pitch+beide Gates, Depth0-Phasentest, kurze Releases | Diagnose vorhanden, Gatevertrag NOT RUN |
| R08 Comp | Ratio/Threshold/Attack/Release/Makeup musikalisch korrekt | stationärer AC-Ton + Pegelsprünge | Ratio1, Bypass, extreme + Pegelkompensation | NOT RUN |
| R09 IR | gültige IR aktiv, richtige Mono-/Trim-/Normalize-Semantik | kurze/lange IR, verschiedene Raten, A→B | korrupte/leere/große Datei, Race, Fehlpfad, schnelle Load/Clear | Code FAIL B01/B04; Audio/TSan NOT RUN |
| R10 EQ/FX | parametrische Wirkung, sichere Feedback-/Mix-Grenzen | je Band/FX 0/25/50/75/100 %, Phasen-/Spektralfolge | Silence, Maxfeedback, lange Tail, Mono-Summe | NOT RUN |
| R11 Tail | Host darf Echo nicht vorzeitig abschneiden | Impuls + Delay2s/Feedback.95 | >3s Render, ReverbMax, IR>3s | Codevertrag FAIL B02; Host NOT RUN |
| R12 Bypass | definierte Neutralität/PDC und Wiedereinstieg | Host- und Modulbypass, Vergleich warmen Referenzpfads | lange Pause, Automation während Bypass | NOT RUN; B05-Risiko |
| R13 Preset | Parameter/IR/Name als konsistentes Ergebnis | A speichern, B laden, A laden | Factory nach IR, fehlende IR, fremdes XML, I/O-Fehler | Code FAIL A04/B03; Integration NOT RUN |
| R14 State | Hostprojekt klingt nach Reload gleich | save/close/reopen, Pfad/Name/alle 65 IDs | truncated/future XML, alten KnobCurve-Versionen, fehlenden Assets | NOT RUN; A10 Portabilitätslücke |
| R15 Automation | UI/Host/Werte/DSP stimmen überein, keine unzulässigen Sprünge | jede ID, Endpunkte/Default/Normzwischenwerte | dichte gekoppelte Moves, Restore während laufendem Audio | NOT RUN; Amp-Glättung Code vorhanden |
| R16 Realtime | keine Callbackallocs/I/O/Locks, Deadlines eingehalten | instrumentiertes finales Plugin, paced/offline | IR-Load, Matrixrebuild, alle Fades, sehr kleine Blöcke | native Diagnose, RT-Gate NOT RUN |
| R17 Lifecycle | Restart/prepare/Release reproduzierbar | activate/deactivate, Rate/Block wechseln | 0/1/ungerade/große Blöcke, mehrfach Instanzen, invalid Input | NOT RUN in Host |
| R18 GUI | jede Inventarzeile erreicht DSP und korrekte Rückmeldung | alle Controls inkl. Ctrl-Reset/Automation/Recall | Dialog offen→Editor zu→bestätigen, DPI/Resize/Reopen | NOT RUN; A11/B07 |
| R19 Produktidentität | Projekt-Recall und eigene Identität bewusst definiert | alter Hoststate im vorgesehenen Produkt | Parallelinstallation Original/Fork, Scanner-Caches | NOT RUN |
| R20 Packaging | echtes Windowsartefakt entspricht geprüftem Source | reproduzierbarer Build/Hash/Install/Validator | fehlende Ressourcen, Upgrade/Uninstall, Rechte | NOT RUN; kein Actionslauf |
| R21 Testhärte | Gates schlagen bei gebrochener Funktion fehl | aktive Checkcounts, Runner-Codeprüfung | Mutation: R verwerfen, IR beibehalten, falscher Pitch, 0 Fälle | A12 Code FAIL; Mutation NOT RUN |

Jede Inventarzeile aus Abschnitt 7 erbt zusätzlich R14/R15/R18, bis echte individuelle Belege vorliegen. Kein stilles Zusammenfassen von 65 ungetesteten UI-Ketten zu einem GUI-PASS.

## 9. Aktueller Originalentwickler-Stand

Read-only erneut geprüft am 2026-10-26: [Upstream Releases](https://github.com/SteveAsAConstant/djenty-donkey/releases), [Issues alle](https://github.com/SteveAsAConstant/djenty-donkey/issues?q=is%3Aissue), [Issues geschlossen](https://github.com/SteveAsAConstant/djenty-donkey/issues?q=is%3Aissue%20is%3Aclosed), Git-HEAD/Tag und Commitverlauf. Beide Issues-Suchen lieferten in der geladenen HTML-Ansicht **0 Ergebnisse**. Keine Aussage über private Fehlermeldungen, Discussions oder künftige Reports; die Abwesenheit öffentlicher Issues beweist keine Fehlerfreiheit.

Neuester sichtbarer Release: **v0.5.6-beta**, Pre-release, 2026-10-25 23:03 UTC = **2026-10-26 00:03 Europe/Berlin (CET)**. Annotiertes Tagobjekt `54095767d8f236e4e455a838838db1ce15743b95`, aufgelöst zu `e363027d7a125018bb1cb56acc14958bc9baf9ff`; genau derselbe Codebaum wie Import, keine stillschweigende Migration.

| Korrektur / Commit | Einordnung |
|---|---|
| `658375c` v0.5.2 | Presence-Netzwerk, Gleichrichter, echte Poweramp-Vergleiche laut Entwickler |
| `b14c4b5` v0.5.3 | 6L6-Datenblattverhalten / endlicher Ausgangswiderstand, physikalische 4×12, Bright |
| `c03e0f4` v0.5.4 | Resonance-Potentiometer aus Schaltplan, C201/C202, Speaker-Refit |
| `ebf3bfd` / `9342b14` v0.5.5 | Screenstrom, robustere Solverannahme, mehrwegiger Fade, Crunch/Noise/TubeSets |
| `982f90e` / `e363027` v0.5.6 | Speaker-Voicecoil-Eddy-Verlust von 15 auf 6 Ω empirisch gefittet |

Die Release-Notes berichten für v0.5.6 verringerte Höhenabweichung; **weiter offen**: LEAD-Preamp auf manchen Captures bei 12,8 kHz noch +1…+3 dB. Der Entwickler sagt ausdrücklich: keine gemessene V30-Impedanzkurve, Fit kann Capture-Chain-Verluste mit absorbieren. Diese Angabe ist EMPIRICALLY TUNED, kein bewiesener Schaltungsfehler. Kein spekulativer Höhen-EQ zur „Reparatur“. Detailquelle `docs/power_amp.md:115–125,158–172`. Die Zahlen aus den Release-Notes wurden hier nicht an realen Captures unabhängig nachgemessen.

Windows-Releaseasset heruntergeladen/ausgeführt und Hash gegen Source verifiziert: **NOT RUN**. Die Git-Baumgleichheit beweist Quellidentität, nicht Build-Reproduzierbarkeit oder Binäridentität.

## 10. Strukturierter Reparaturplan – keine Umsetzung in diesem Auftrag

1. **Regressionseinstieg und P1-Vertragsfehler gemeinsam spezifizieren.** Kleine headless JUCE-Processor-Integration unter unverändertem Import aufbauen; R-only-Routing, IR A→Factory/User-ohne-IR, ungültiges XML, lange Tail und Live-IR-Austausch als reproduzierbar rote Tests. Bestehende IDs/State/Referenzen einfrieren. Die mangelnde Aussagekraft vorhandener Tests nicht mit einem grünen Compile überdecken.
2. **IR-/Preset-Zustandsmaschine zuerst reparieren.** B01 und A04/B03/B04 als zusammenhängenden Pfad bearbeiten: serialisierte RT-sichere Convolution-Kommandos, Pending/Ready/Failed, eindeutige Built-in-Rückkehr, validierte transaktionale Presets. Fehler dürfen nicht die vorherige gültige Szene mutieren. Audio-Recall mit IR-Hash/Fixtures prüfen. Dies ist der empfohlene erste konkrete Reparaturschritt, vor GUI und Performancearbeit.
3. **Routingvertrag festlegen und umsetzen.** Stereo-Unterstützung entweder bewusst korrekt implementieren oder ehrlich auf vorgesehenen Busumfang beschränken. Nicht einfach L+R summieren. Zwei unabhängige nichtlineare Ketten ändern CPU-Budget; Monospuren und Stereo-FX müssen kompatibel bleiben. R01/R02 vor und nach Änderung messen.
4. **Numerische und Host-Zeitverträge schließen.** A06 mit physikalisch definiertem Nullscreen-Grenzfall und unabhängig überprüften Ableitungen; danach LIVE/HQ-PDC, Gate-Key und Tail/Bypass spezifizieren und R03/R06/R07/R11/R12 nachweisen. Keine bloße Delay-Konstante aus einem Onsettest ableiten.
5. **Automation, State und Lebensdauer absichern.** B05, A10/A11/B07: gültige/ungültige und alte States, fehlende/veränderte IR, offene Dialoge beim Zerstören, Hostrestore im offenen Editor, alle 65 Parameter samt GUI-Rückmeldung. Keine GUI-Neugestaltung erforderlich, um die Verträge zu testen.
6. **Performance separat nach Messung.** Windows-8×-Profilmatrix aus Abschnitt 6, dann genau einen Kandidaten gegen eingefrorene Audio-/Deadlinekriterien bewerten. CPU-Gewinn ohne unabhängige Klang- und Zustandsevidenz nicht akzeptieren.
7. **Kompakt-GUI erst nach Funktionsfreigabe entwerfen.** Alle 65 IDs und Nichtparameter erhalten, Kontext-Abhängigkeiten und fehlende Assets sichtbar machen, Branding/Knob-Designer-Master vor Design lesen; danach volle R18-Matrix auf Windows/DPI.
8. **Gebündelte Releaseprüfung.** Ein kohärenter Windowsbuild nach lokalen Tests, dann Steinberg/125A/Host, Packaging und pegelgematchte echte DI-A/B. Actions nicht für jeden Einzelbefund starten. Freigabe erst mit Belegen für jeden kritischen Matrixeintrag.

Ergebnis: mehrere bestätigte Funktions-/Vertragsmängel, zusätzliche IR-Thread-/Tail-/Zustandsprobleme und erhebliche nicht ausgeführte Integrationsgates. **Kein nachgewiesener P0 im Rahmen dieses Audits; mehrere P1 blockieren eine belastbare Releasefreigabe.** Bestandene native DSP-Tests rechtfertigen weitere Entwicklung, keine Behauptung einer vollständig geprüften Windows-Anwendung.

## 11. Abschlussnachweise

Nach dem vollständigen Tone-Lauf wurde die CPU-Diagnose nochmals ohne gleichzeitig von diesem Audit gestartete DSP-Läufe ausgeführt (weiterhin geteilte VM, unpaced, kein Windows-RT-Test): RED Gain0,7 **40,10 %**, BLUE **33,78 %**, Power **28,58 %**, Drive+RED+Power **76,46 %**, größter Full-Block **260,2 %**. Diese deutliche Abweichung zum parallelen Erstlauf bestätigt, warum die Tabelle in Abschnitt 6 kein Host-Deadline-PASS/FAIL ist. Gate 0,14 %, LIVE 0,12 %, HQ 2,23 % im Mittel; alle Diagnosekommandos Exit 0.

Die beiden nativen Gesamtläufe sind abgeschlossen: `dsp_test` Exit 0 mit 19 bestandenen Fault-/Extremfällen; `tone_regress` Exit 0, **74 Fälle / 0 Fehler**. Grenzen aus Abschnitt 5 bleiben bestehen. Keine Änderungen an Produktcode, Tests, CMake, Dependencies oder GUI. Nur dieser Bericht wird auf dem Auditbranch versioniert; lokale Build-/Probeausgaben bleiben ignoriert. Remote-main nochmals geprüft: `81cc90dfd076f1510c02bf6ccc8318575dc4aa45`. Keine Actions und kein Release ausgelöst.

Lokale Evidenzfingerprints (SHA-256; Reproduktion kann wegen Zeitmessungen andere Loghashes ergeben):

| Log unter build/audit/ | SHA-256 |
|---|---|
| dsp.log | d118ca377c1ad0fbbe03662fe34119048a1745e833f836fe3b14c97246f7694f |
| tone.log | 727f0f3397319237a22bd71a54591c957e3e284161040fe834415b306a515b3b |
| transpose.log | 0682f34d13f75cf0b49f8bc2bdff42a82b082730bc1d232ae413fabd7e16f7b1 |
| cpu.log | 66c2ad09d622a56db80dbfb75842fcb1cdd7f90eca150a002c5ec2720253dfcb |
| boundary.log | c700d785caea0e567ed88a308c4be2fa45241ac5ac5438d42c410ce3a70059a4 |

**Empfohlener nächster Reparaturschritt:** zuerst rote JUCE-Integrationstests für IR-Ladeübergabe und Preset-/IR-Recall aufsetzen, anschließend B01/A04/B03/B04 als zusammenhängenden Zustandspfad minimal korrigieren; keine Performance- oder GUI-Arbeit vor diesem Nachweis. Windows-Host-/Lifecycle-/Audio-QA bleibt NOT RUN und ist Voraussetzung jeder späteren Freigabe.
