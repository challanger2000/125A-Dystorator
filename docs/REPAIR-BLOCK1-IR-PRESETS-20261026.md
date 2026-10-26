# Reparaturblock 1 – IR und Presets

Datum: 2026-10-26, Europe/Berlin. Basis: bestätigter Audit `75a638bd2cf42f609b71838a5864f580f9a55356`, unveränderter v0.5.6-beta-Ampkern. Branch: `fix/ir-presets-block1-20261026`. Umfang ausschließlich **B01, A04, B03, B04** und deren Integrationstests. Kein Merge, keine main-Änderung, keine FINAL-/Windows-Freigabe.

## Änderungen und Verträge

### B01 – getrennte Convolution-Eigentümerschaft

`Source/CabinetIR.h` ersetzt das direkte GUI-Load auf der gleichzeitig verarbeiteten Convolution. Datei-Snapshot, vollständiges Decoding, Validierung, JUCE Load und synchrones `prepare()` finden auf dem aufrufenden **Nicht-Audiothread** statt. JUCE 8.0.4 garantiert nach `prepare()` die Aktivierung der zuletzt geladenen IR. Erst dann wird die neue Instanz veröffentlicht. Keine veröffentlichten Convolver erhalten weitere Load-/Prepare-Aufrufe; `prepareToPlay` darf wie bisher nur bei gestopptem Callback laufen.

Audio übernimmt höchstens einen vorbereiteten Pointer pro Block, rendert und blendet 50 ms zwischen altem/neuem Pfad über. Die Referenzdatei wird nicht später vom JUCE-Worker erneut geöffnet. Die mono/Trim/Normalise-Optionen, Built-in-Filterkoeffizienten und Amp-Skalierung bleiben erhalten. Neue IR-Übergänge starten aus einem vorbereiteten Nullzustand, der bisherige Pfad bleibt während des Fades aktiv; daraus wird keine bitidentische Übergangsphase zum früheren unsicheren Ladepfad behauptet.

Ownership: `pending.exchange(nullptr)` überträgt ausschließlich dem Audio-Consumer die neueste Instanz. Ein überschriebenes, noch nicht übernommenes Pending wird nur auf der Control-Seite gelöscht. `previous` bleibt bis Fade-Ende audio-owned; anschließend geht es in einen atomaren Retired-Slot, auf den Audio nicht mehr zugreift. Ein Reclamation-Thread bzw. der Control-Producer übernimmt dessen Löschung per Exchange. Solange der Slot belegt ist, rendert Audio weiter und wartet nicht. Der Reclamation-Thread pollt 10 ms: auch ohne weitere Ladeanfrage bleibt der letzte Pending-Auftrag ausführbar. Objektzahl bleibt begrenzt (aktuell, Fade-Vorgänger, Pending, in Vorbereitung/Abbau); es gibt keine unbegrenzt wachsende IR-Warteschlange.

Kein File-I/O, Decode, Prepare, Mutex, Pointer-Deletion oder eigener Thread-Wakeup im neuen Cabinet-Audio-Code. Pointer-Atomics sind auf der Zielplattform statisch als lock-free vorausgesetzt. C++-Heap-Probe und vollständige TSan-Instrumentierung ergänzen diese Codeprüfung. Eine Vollplugin-/Windows-Deadlinezertifizierung wird nicht behauptet.

### A04 – explizite Built-in-Rückkehr / Recall

Factory-Presets und User-Presets mit leerem `irPath` selektieren explizit Built-in. Presets mit nichtleerem, fehlendem oder beschädigtem IR-Verweis werden dagegen vollständig abgewiesen: bisherige Parameter, Presetname und Cabinet bleiben erhalten. Derselbe Vertrag gilt für Hostrestore. Damit entfällt die alte Diskrepanz „Preset behält altes IR, Hostrestore wechselt still auf Built-in“.

`loadIR`, `clearIR` und `PresetManager::load` melden Erfolg als `bool`; existierende GUI-Callbacks dürfen den Rückgabewert weiterhin ignorieren. Load-Fehlertexte stehen über `getIRLoadError()` bzw. `presets.getLastError()` zur Verfügung; kein neues GUI und kein Message-/Dialog-Aufruf aus Audio. `getIRPath()` bezeichnet das validierte **akzeptierte Ziel** (bei aktiver Runtime bereits vorbereitet), das Audio an einer Blockgrenze bzw. nach einem laufenden Fade übernimmt, nicht eine unvalidierte Dateianfrage. Vor dem ersten `prepareToPlay` wird nur der validierte Snapshot gespeichert; die Engine wird beim anschließenden Prepare mit der dann bekannten Hostrate gebaut.

### B03 – Preflight vor Szenenmutation

XML muss zum Host-/Preset-Root passen, unterstützte Versionsmarker und mindestens einen bekannten Parameter enthalten. Unbekannte/duplizierte IDs, ungültige numerische Tokens, NaN/Inf, Werte außerhalb des jeweiligen Parameters und Zwischenwerte diskreter Parameter werden abgewiesen. Alte partielle Presets bleiben unterstützt: gültige vorhandene IDs werden migriert, fehlende Parameter erhalten definierte Defaults. Die bisherigen Knob-Curve-Migrationen bleiben erhalten. Produktversionsprüfung wird aus `JucePlugin_VersionString` abgeleitet; keine IDs/Defaults geändert.

Hoststate-Parsing, Default-Vervollständigung und IR-Preflight liegen vor der Live-State-Übernahme. Nicht-Audio-Kontrollzugriffe für Name, Presetliste, Save/Load und Hoststate teilen einen rekursiven Szenenmutex. Der Callback nimmt diesen Mutex nicht. Erfolgreiche Szenenwechsel werden weiterhin blockweise über APVTS plus Cabinet-Fade hörbar; keine neue Behauptung sampleatomarer Multi-Parameter-Automation. Abgewiesene Datei-/Validierungsoperationen ändern die akzeptierte Szene nicht.

Preset-Save schreibt zuerst eine temporäre Datei und ersetzt das Ziel erst nach erfolgreichem vollständigem XML-Write. Ein fehlgeschlagener Save ändert den aktuellen Namen nicht und zerstört kein bestehendes Ziel. Die zusätzliche optionale Presetordner-Injektion dient ausschließlich isolierten Tests; der Standardpfad und die GUI bleiben unverändert.

### B04 – Dateidaten vor Veröffentlichung prüfen

Stabiler, begrenzt eingelesener Dateisnapshot, lesbarer Decoder, positive Länge/Rate/Kanäle, erfolgreicher Sample-Read und endliche Samples sind Voraussetzungen. Für RIFF-WAV/FORM-AIFF wird zusätzlich eine über das Snapshot-Ende hinaus deklarierte Containerlänge abgewiesen, statt eine Decoder-Nullauffüllung als valide IR zu akzeptieren. Decoder und mono-Kanal-0-Verhalten bleiben JUCE-basiert. Ein fehlgeschlagener Drop/Load kann den angezeigten akzeptierten Pfad damit nicht mehr durch eine unlesbare Datei ersetzen.

Schutzbudgets: 64 MiB Dateigröße, 4.194.304 decodierte Mono-Samples und ebenso viele nach Resampling; IR-Dateirate 8–768 kHz; Preset-/Stategröße 1 MiB. Überschreitungen werden nicht still gekürzt. Schon vor Aktivierung wird jeder Kandidat gegen eine Hostrate von 768 kHz gerechnet: daraus folgt eine maximale Quelldauer von rund 5,46 Sekunden **vor Trimming**. So kann ein zuvor akzeptiertes IR bei späteren normalen Hostraten bis 768 kHz nicht erst während Prepare die Resampling-Speichergrenze verletzen. Eine zusätzliche Regression lädt vor Aktivierung einen darüber liegenden Kandidaten, fordert Reject mit erhaltenem Asset und bereitet danach die alte gültige IR bei 192 kHz vor. Außerhalb des Budgets liegende IRs/Hostraten müssen vor Verwendung geklärt werden; dies ist eine bewusste Sicherheitsgrenze gegenüber dem zuvor unbeschränkten Loader.

## Testbasis und Reproduktion

Echte JUCE-Integration unter Linux x86_64 / GCC 14.2 / JUCE 8.0.4 (`51d11a2be6d5c97ccf12b4e5e827006e19f0555a`), CMake 4.4.4, Release. Alle Audio-/XML-Fixtures werden lokal erzeugt; keine User-Presetdateien, keine NAM-Captures. Die vorhandene GUI wird mitkompiliert, aber nicht geöffnet. Abhängigkeiten wurden in einem Workspace-Sysroot eingerichtet; keine Systemdateien verändert.

Vor der Reparatur wurde Testcommit **`9a1ce3e`** angelegt. Die originale Produktlogik mit bloßer Presetordner-Testinjektion liefert **9 Checks / 5 Fehler / Exit 1**: Factory-Built-in, User-Built-in, fremdes XML, beschädigte WAV und die Eigentumsregel für bereits gerenderte Convolver. [Originalprotokoll](qa/ir-presets-20261026/baseline.txt).

Der Test-Trace beobachtet die echten JUCE-API-Aufrufe in build-lokalen Quellkopien; keine Ersatz-Convolution und keine Modifikation des Dependency-Checkouts. Er erzwingt die für diese Reparatur gewählte stärkere Regel „kein Load nach Veröffentlichung/Rendering“. Ein sequenzieller Load nach Process ist nicht pauschal JUCE-illegal; aus diesem roten Ownership-Test allein wird deshalb kein beobachteter Host-Crash oder dynamischer Race-Nachweis des Originals abgeleitet.

Build-/CTest-/Sanitizerbefehle: [tests/ir_presets/README.md](../tests/ir_presets/README.md). Alle Assertions sind auch mit NDEBUG aktiv. `main`, `Source/Parameters.h`, alle Dateien unter `Source/dsp`, Editor und UI-Dateien bleiben gegenüber Auditbasis unverändert. Keine GitHub Actions gestartet.

## Anforderungs-/Evidenzmatrix

| Vertrag | Negativ-/Grenzfall | Tatsächlicher Nachweis |
|---|---|---|
| B01 Eigentümerschaft | neue IR während Audio / Fade / weitere Updates | API-Trace; paralleler echter Processor; vollständig instrumentierter TSan-Lauf |
| B01 RT-Übergabe | Kandidaten während 1.200 gepaceter Cabinet-Blöcke, Reclamation | keine Cabinet-C++-new/delete; keine eigenen Callbacklocks/I/O; Timings nur Diagnose |
| A04 Factory/User Built-in | IR A geladen → leeres Preset/Factory | Pfad leer; IR→Builtin-Callback nach queued Wechseln; zusätzlich tatsächlicher Processor-Ausgang gegen Fünf-Filter-Orakel mit abgegriffenem Eingang |
| A04 Hostrestore | Restore vor/nach Prepare, erneuter Prozessor | tatsächliches nichtstilles Stereo-Audio nach Fade mit Original verglichen, Toleranz 1e−6 |
| A04 Preset Save/Reload | neuer Prozessor, Reset/andere IR vorher | gespeicherte Szene/IR und tatsächliches Audio stimmen nach Übergang überein |
| B03 XML/Parameter | falscher Root, leer, truncated, future, nan, range, Tokenjunk, unbekannt, Duplikat, diskrete Zwischenwerte, relativer Pfad | Ablehnung plus unveränderter vollständiger serialisierter Zustand |
| B03 fehlende Ressource | Preset verschwindet nach Scan, referenzierte IR fehlt/ist beschädigt | Parameter/Name/Pfad unverändert; Hoststate ebenfalls unverändert |
| B03 Savefehler | nicht ersetzbares Zielverzeichnis mit Sentinel | Fehler gemeldet, Sentinel und vorige Szene erhalten |
| B03 Legacy | partielles älteres KnobCurve-2-Preset mit Attack 8 | akzeptiert, Attack→1 migriert, andere Werte Defaults, Built-in |
| B04 Datenfehler | ungültige WAV, fehlende Datei, unvollständiger RIFF-Payload, IEEE-float-NaN | Ablehnung; vorheriger Pfad und tatsächliches Convolution-Audio erhalten |
| B04 gültiger Klang | identisches generiertes IR im neuen und alten JUCE-File-Pfad | identische mono/Trim/Normalize-Audiofolge bis Toleranz 1e−6; zusätzlich realer Processor-Ausgang gegen Original-JUCE-Convolution am abgegriffenen Cabinet-Eingang |
| Lifecycle | 96 kHz / 17 Samples nach angenommenem 48-kHz-IR, Datei inzwischen gelöscht | retained Snapshot wird neu vorbereitet und produziert Audio |
| Parallelbetrieb | 1.000 echte Processor-Blöcke während Save/Reload/Hostrestore und Load/Clear | endliche Ausgaben, alle Save-/Loadoperationen erfolgreich |

## Grenzen / offene QA

- **NOT RUN:** Windows-VST3-Wrapper, Studio One, ASIO, echte GUI-/Dialog-/Drag-Drop-Lebensdauer, installierte Binärdatei, offizielle Validatoren, professionelle DI-Hörfreigabe.
- **NOT RUN:** ASan/UBSan-Konfiguration, vollständige Hostraten-/Blockgrößenmatrix, alle Audioformate und Hardware-IR-Corpora. Geprüfte primäre Dateifixtures sind WAV; AIFF-Decodepfad ist mitgebaut, aber kein eigener AIFF-Funktions-PASS behauptet.
- Die Heap-Probe erfasst C++ new/delete im Cabinetpfad; direktes malloc/free, sämtliche OS-Aufrufe und die übrige Amp-/Plugin-RT-Sicherheit sind damit nicht vollständig zertifiziert. Sanitizer verändert Last und Laufzeit deutlich. VM-Scheduler/Power-Governor sind nicht kontrolliert.
- Ein partieller TSan-Aufbau meldete einen JUCE-Teardown-Konflikt. [Diagnoseprotokoll](qa/ir-presets-20261026/tsan-partial-diagnostic.txt). Er wurde nicht unterdrückt: JUCEs detached-thread/atomic-exit-Synchronisierung wurde anschließend durch vollständige Instrumentierung aller Module sichtbar gemacht und der Lauf separat wiederholt. Nur der vollständige Lauf zählt als Sanitizerevidenz.
- Load/State-APIs sind weiterhin Control-/Hostthread-Operationen; Decoding kann den aufrufenden Thread belasten. Ein Host darf `prepareToPlay`/Zerstörung nicht mit dem Callback überlappen. Der GUI-Entwurf und neue Fehlerdialoge sind ausdrücklich außerhalb des Auftrags.
- IR-Referenzen bleiben absolute Pfade (A10 bleibt offen); Dateiinhalt unter gleichem Pfad wird beim Projektöffnen nicht durch einen gespeicherten Hash authentifiziert. Fehlende IR führt nun zum expliziten Reject statt unbemerktem Klangwechsel. Asset-Einbettung ist keine heimliche Erweiterung dieses Blocks.
- A01/A02/A03/A06 usw., globale Bypass-/Tail-Probleme und GUI-Lebensdauerrisiken bleiben unverändert offen. Keine FINAL-Freigabe.

Die unveränderte Preset-GUI rollt ihre Dropdown-Auswahl bei einem Load-Reject weiterhin nicht selbst zurück und zeigt keinen neuen Fehlerdialog; interner Presetname, Parameter und Audiozustand bleiben jedoch erhalten. Das offene Feedback-Thema B07 wurde wegen der ausdrücklichen GUI-Grenze nicht mitbearbeitet.

## Abschließende Testergebnisse

| Ausführung | Ergebnis | Evidenz |
|---|---|---|
| Testbasis vor Fix `9a1ce3e` | **9 Checks, 5 Fehler, Exit 1** | [baseline.txt](qa/ir-presets-20261026/baseline.txt) |
| Release, echter JUCE-API-Ownership-Trace | **68 Checks, 0 Fehler, Exit 0** | [release-trace.txt](qa/ir-presets-20261026/release-trace.txt) |
| Vollständig instrumentiertes TSan, Trace OFF | **67 Checks, 0 Fehler, Exit 0**, keine TSan-Meldung; keine Suppression | [tsan.txt](qa/ir-presets-20261026/tsan.txt) |
| Release, Trace OFF, via CTest | **67 Checks, 0 Fehler**, CTest **1/1**, Exit 0 | [ctest-release.txt](qa/ir-presets-20261026/ctest-release.txt) |
| Erstes Zweiprozessor-Cabinet-Orakel | Test-FAIL: 66 Checks / 1 Fehler, Grenze 1e−6 unverändert; ersetzt durch direkten Eingangstap | [tsan-twin-oracle-diagnostic.txt](qa/ir-presets-20261026/tsan-twin-oracle-diagnostic.txt) |
| Partiell instrumentierter TSan-Versuch | Diagnose mit Exit 66, **kein PASS**; ersetzt durch voll instrumentierte Wiederholung | [tsan-partial-diagnostic.txt](qa/ir-presets-20261026/tsan-partial-diagnostic.txt) |

Uninstrumentierte Cabinet-Diagnose im abschließenden CTest: p95 **9,964 µs**, p99 **21,102 µs**, max **130,332 µs**, nominelle 64/48k-Deadline 1333,33 µs. Synthetischer Treiber mit 1-ms-Sleep zwischen Blöcken, **keine exakte ASIO-/Realtime-Scheduler-Simulation**, kein kalibriertes Performancezertifikat. TSan-Werte sind wegen Instrumentierungs-Overhead separat im Log und nicht mit Release-Grenzen bewertet. Die C++-new/delete-Probe im neuen Cabinet-Audio-/Fade-/Handoff-Pfad blieb bei null.

Bewusst keine erneute allgemeine Amp-DSP-Suite als Ersatznachweis: Ampquellen/Parameterdefinitionen sind bytegleich zur Auditbasis; der relevante vollständige Processor-/Cab-/State-Pfad wird in den neuen Audio-Recall-Tests tatsächlich ausgeführt. Windows-/DAW-Gates bleiben NOT RUN. Reparaturblock lokal mit Testergebnissen dokumentiert, **keine FINAL-Freigabe**.

Die letzte funktionale Fixture-Verschärfung platziert vor abgewiesenen IR-Loads einen Impuls am Blockende: Der Vergleich fordert danach eine nichtstumme, erhaltene IR-Tail gegen die kontinuierliche JUCE-Referenz. Dieser stärkere Fall besteht in den abschließenden Release-Läufen. Der abschließende TSan-Lauf wurde nach dieser Fixture-Verschärfung und der vorgezogenen Ressourcenprüfung ausgeführt.

### Präzisierung des Cabinet-Orakels

Ein erster zusätzlicher Built-in-Vergleich über zwei getrennte RED-Amp-Instanzen ergab eine Abweichung von 1,15e−5, also einen echten Test-FAIL bei der unveränderten 1e−6-Grenze ([Diagnose](qa/ir-presets-20261026/tsan-twin-oracle-diagnostic.txt)). Die Identität der beiden Upstream-Signale war dabei nicht belegt; diese instanzübergreifende Differenz wurde keinem IR-Fehler zugeordnet und gehört bei Bedarf in eine separate Amp-Untersuchung. Es wurde weder der Amp verändert noch die Toleranz gelockert.

Die präzisierte Prüfung erfasst stattdessen ausschließlich im Testbuild (`VAMP_IR_TEST_CAPTURE`) den echten Eingang unmittelbar vor Cabinet. Der beobachtende Tap kopiert in einen vorallozierten Testpuffer und verändert keine Samples. Gegen exakt diese Samples laufen unabhängig die originale JUCE-File-Convolution bzw. die dokumentierte Fünf-Biquad-Kurve. Der reale Stereo-Processor-Ausgang muss beide Orakel treffen und darf nicht bloß stumm oder unverändert sein. In Shipping-Builds ist der Tap vollständig herauspräprozessiert. Die Suite enthält damit auch einen Nachweis, der bei einem im Processor versehentlich ignorierten externen IR scheitern würde.

Endgültige numerische Orakelwerte: externe IR im realen Processor max. Fehler **0**, Built-in-Cab max. Fehler **0**, Host-/Preset-Audio-Recall max. Fehler **0**, erhaltene IR-Tail max. Fehler **0** bei Referenzpeak **0,0559017**. In allen Fällen bleibt die Vergleichstoleranz 1e−6 unverändert; nichtstumme Referenz-/Wirkungsbedingungen verhindern einen leeren Audio-PASS.
