# P001 — Revisionsgebundene Engineering-Pflichtlektüre

Stand 2026-11-25. **INCOMPLETE — keine Implementierungsfreigabe.** Produkt-Baseline vor diesem reinen Dokumentationscommit: b27279cf8437137f875e8a91815b21d594139000 (main); AKTUELLE-VERSION.md und docs/P001-REFERENZ-UND-PRODUKTUMFANG.md vollständig gelesen. Keine DSP- oder Buildänderung.

Engineering-Repository: challanger2000/125A-Engineering, unveränderlicher Commit 53b10bd4a81fe6c5d62e110b932aef7155172ad6. Lesebeleg dieses Durchlaufs (tatsächliche Inhalte im Modell sichtbar, keine Ausgabe-Trunkierung dieser Bereiche; Zeilenzählung split("\n"), einschließlich etwaiger finaler Leerzeile).

| Datei | Blob | Umfang | Gelesen | Offen |
|---|---|---:|---|---|
| START-HERE.md | 91a0e14dd24ff72874bbedf58962e0e8c14c444e | 164 | vollständig | keine |
| README.md | bd94e5a896bcc905af65ca5659e533eee854b46f | 74 | vollständig | keine |
| ENGINEERING-GUARDRAILS.md | 8a076e21d8226a5b1ded7061362590415fe4839f | 3187 | 1–130 | 131–3187 |

**Nächster Lauf:** aktuelle START-HERE.md und Engineering-Revision/Blobs prüfen. Bei unverändertem Guardrails-Blob unmittelbar mit Zeile 131 beginnen, begrenzte lückenlose Bereiche (z.B. 50–70 Zeilen pro Tool-Ausgabe), wirklich lesen, jeweils letzten vollständig sichtbaren Bereich notieren. Bei Änderungen gemäß aktueller START-HERE.md verfahren. Nach Guardrails weitere relevante Pflichtdokumente vollständig lesen; insbesondere DSP, Professional Plugin Behavior, Repository Workflow, Source Baseline Gate, GUI/Branding, Release/Realtime QA, Audio Fixtures, Audit Completeness Contract, einschlägige Schaltungsmodellierungs-/Solver-Research-Dokumente, ggf. Roadmap, Branding-Repo und projektspezifische Dokumente. Diese sind noch **NOT READ** in diesem Ledger. Kein Neustart der Lektüre unveränderter gelesener Bereiche.

## Anwendbare Regeln aus bisher gelesenen Quellen

- START-HERE vollständig: Architektur vereinfachen ohne Qualitätsverlust, C++/JUCE und gepinnte Abhängigkeiten; vollständige Quellen-/Baseline-Prüfung; reale unabhängige Messungen, Negativkontrollen, echte Host-/Recall-/RT-Pfade, keine spekulativen CI-Builds, p95/p99/max und Fristüberschreitungen, reale Audiofixtures plus synthetische Messung, keine Releasefreigabe ohne vollständige Nachweise. User-approved P001 scope und konsolidierten main beachten.
- README vollständig: zentrale Standards nicht divergent duplizieren; etabliertes DSP/Mathematik/Physik, Branding-Quelle, Lizenzgrenzen, Tests und nachvollziehbare Messung.
- Guardrails 1–70: bestehende Architektur gegen echte Alternativen prüfen (hier Original 1.0.0 vs. vorhandene Stereo-/Host-Integration); keine ungeprüfte Vereinfachung der Röhrenschaltung. Exakte Owner-GUI und alle vorgesehenen Bedienelemente mit DSP/State/Host verifizieren. IR-Ladefehler atomar konsistent zurückrollen oder invalidieren, off-thread. Extreme endliche Eingangswerte und Recovery, keine konkurrierenden rohen State-Felder; echte Eingangsbusse mit nichtstillem kanalverschiedenem Signal für RT-Messungen, negative Host-Fixtures, keine Zero-Frame-Messung als Audio-PASS.
- Guardrails 71–130: zeitabhängige Übergänge und inaktive DSP-Engine-Grenzen auf exaktem Sample, unabhängig von Hostblockpartition; Stereo-PCM-Orakel und negative Gegenprobe; keine falsche Bestätigung bei partieller Lektüre (alte Wiederholungsregel durch explizite Owner-Änderung in START-HERE abgelöst). Recall/IR/Preset/Automation in chronologisch konsistenter Transaktion; keine älteren asynchronen Aufträge über neuere gewinnen lassen, unmittelbar nach akzeptierter Änderung speichern und auch nächsten Audioblock prüfen, keine RT-Locks/Allokationen oder unbeschränkten CAS-Schleifen.

Alle QA-, Klang- und Performance-Nachweise bleiben offen. Keine Freigabe.
