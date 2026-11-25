# P001 — Endstufe, DK-Solver und SIMD: kombinierte 1.0.0-Parität

2026-11-25, geprüfter Produktstand 413c78b72d275779ef810aa03cc8493577159cdd, Engineering unverändert 53b10bd4a81fe6c5d62e110b932aef7155172ad6. START-HERE vollständig gelesen; revisionsgebundener Lesebeleg gültig. Keine Produkt-DSP-Änderung.

## Quellvergleich

Die vollständigen aktuellen PowerAmp5150.h und DKCircuit.h wurden gelesen und aus der unveränderten 1.0.0-Quelle lokal exakt rekonstruiert. LF-normalisierte Git-Blob-Identität überprüft:

| Header | Aktueller Git-Blob | Befund |
|---|---|---|
| PowerAmp5150.h | 8db9ced22488b472643f172568f88f7f1f91f975 | Schirmgitter <=0 V früh abweisen, inverse Spannung 0 statt Division durch 0, Logarithmus nur für positiven Eingang; ansonsten Schaltung, Newton und Supply unverändert |
| DKCircuit.h | 24ae44fa7375dc6557c8ffd0a4f3569039e53959 | Entfall nur der für SemiNetwork benötigten Topologie-/Induktor-APIs; AVX-Helper private und target-isoliert; Rechenreihenfolge unverändert |
| Simd.h | b38b24bfac11e83ce4d78f248134e76489fc10bc | erzwungene Scalar-/SSE2-Backends und getrennte AVX-Entry/Helper |
| PotTapers.h | e5aa6bd7a0197f122d81807ea33e702f90849c75 | ungenutzte taperAInverse und Include entfallen, Vorwärtskennlinien unverändert |
| Preamp5150.h | 2f85f7373676fce94a57707c57bed85e73d0d9c7 | bereits belegte verzögerte Spannungsübergabe |

Weitere tatsächliche Kernheader Amp5150, FullPreamp5150, LinearNetwork, PortSolve, Supply5150, TriodeNetwork, TriodeStage, TubeNoise, TubeSet, Filters stimmen nach LF-Normalisierung jeweils mit dem dokumentierten GitHub-Blob überein. Damit wird der kombinierte gekoppelte Amp-Kern geprüft, nicht AmpSim/JUCE oder Cabinet.

## Ausgeführte Messung

Unverändertes Original gegen den kombinierten 125A-Kern mit den fünf obigen Änderungen. Linux x86_64 / GCC 14.2.0, C++17 -O3 -DNDEBUG. Gleicher Test tests/p001/rail_parity.cpp wie vorige P001-Messung. 88,2/176,4/384 kHz intern, jeweils 0,22 s nach DC settle, STOCK, Kanal-/Crunch-/Bright-Wechsel, Gain/Tone/Master, Stille. Sechs double-Werte pro Sample: Ausgangsspannung, Vorstufe, V1/V2, zwei Ströme.

| Variante | SHA256 aller 142.692 Records (856.152 double-Werte) |
|---|---|
| Original 1.0.0, unverändert | 7cc7c8d0ca7b87bb9d9c758744676ce3d9322c48a6ae879282dfcead4ce5852b |
| 125A auto | identisch |
| 125A VAMP_FORCE_SCALAR=1 | identisch |
| 125A VAMP_FORCE_SSE2=1 | identisch |

**PASS, begrenzter Prüfbereich:** cmp identisch, 0 abweichende Werte in allen sechs Feldern, Max- und RMS-Delta exakt 0. Alle drei 125A-Varianten erfolgreich kompiliert/ausgeführt. Die bereits ausgeführte V2-Negativkontrolle aus P001-VORSTUFEN-PARITAET.md bleibt gültig. Kein CPU-Gewinn aus diesem Vergleich abgeleitet. Kein Windows/MSVC-/OSXSAVE-/JUCE-End-to-End-Nachweis.

## Neue adversariale Schirmgitter-Grenzprüfung

tests/p001/screen_guard.cpp: sechs Schirmgitterspannungen -485/-1/0/1/100/485 V, jeweils normaler Tabellen- und exakter Formelpfad. Prüft endlich/kein FE_DIVBYZERO/FE_INVALID/FE_OVERFLOW, bei nichtpositiver Spannung exakt keine Leitung, bei positiver Spannung positive Ströme und Ableitungen. Release-aktive Fehlerausgabe und Exitcode. Tatsächliche Resultate:
- Unverändertes 1.0.0: **6/12 PASS, exit 1**. Bei 0 V inverse Spannung inf, Plattenstrom NaN, fenv 5 (FE_INVALID | FE_DIVBYZERO in diesem Linux-Environment). Bei negativen Spannungen inverse Spannung nicht 0.
- 125A-Kern: **12/12 PASS, exit 0**. Bestehender Schutz schließt den nachgewiesenen Original-Grenzfall, ohne die gemessenen normalen PCM-/Rail-Referenzen zu verändern.

Reproduktion: `g++ -std=c++17 -O3 -DNDEBUG -I <Variante>/Source tests/p001/screen_guard.cpp -o screen-guard && ./screen-guard`. Für kombinierte Referenz analog rail_parity.cpp mit aktuellem Source/dsp und unverändertem Original. Die Screen-Prüfung isoliert das Device-Modell; sie ist kein Beweis, dass die reale Versorgung bei normaler Spielweise auf 0 V fällt oder dass alle Fehlerpfade der Endstufe sicher sind. CTest-/Windows-Integration noch nicht ausgeführt.

## Entscheidung / Nächster Schritt

Vorhandene fünf geprüfte Kernänderungen beibehalten. Kein Grund für einen Rewrite des DK-Solvers. P002: tatsächlichen Processor/Parameter/Preset/Editor und Build-/QA-Abhängigkeiten untersuchen und die nicht gewünschten Pedale, TubeSet-Auswahl und Standalone/ASIO-SDK aus dem Produktpfad entfernen, ohne die geprüfte Schaltung zu verändern. Gate-Auswahl, Cabinet, Stereo-Quality-Umschaltung, Host-Recall, echte DI, CPU und native Windows/Studio-Pro bleiben NOT RUN. Keine Releasefreigabe, keine CI ausgelöst. Lokale Quellen rekonstruiert und per Git-Blob verifiziert, kein vollständiger Checkout (DNS-Zugriff auf raw.githubusercontent.com gescheitert).
