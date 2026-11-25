# Aktueller Entwicklungsstand — 25.11.2026

Status: **Entwicklungsbasis 0.1.0, nicht freigegeben (NO FINAL / NO RELEASE).**

Der Produktquellcode entspricht dem Stand `80d965f74f75b933df3e2f171870c80543efb4df` (refactor/compact-dead-code-presets-20261027). Die bisherige Branch-Historie bleibt durch die Konsolidierungs-Eltern erreichbar. Frühere Audit- und CPU-Diagnoseunterlagen werden als historische Referenz erhalten, nicht als aktuelle QA-Freigabe. Der historische Owner-CPU-Pack-Workflow bleibt in der Git-Historie, nicht als aktiver Workflow.

Verbindliche Entwicklungsrichtung: komponentenbasierte Röhren- und Schaltungsemulation bewahren (kein NAM-Ersatz), echte unabhängige Stereo-Signalwege, JUCE, Windows x64 VST3, unnötige Zusatzfunktionen vermeiden, CPU-Last durch nachgewiesene Optimierungen und kontrollierte Modellreduktion senken. Klangliche Gleichwertigkeit gegen das unveränderte Djenty Donkey 1.0.0 sowie den bisherigen Dystorator prüfen. Das gewünschte metallische Dystorator-GUI ist noch nicht final integriert.

Offen: native Windows-/ASIO-Echtzeitperformance, vollständige aktuelle QA, finaler GUI-Entwurf, DAW-Abnahme. Die Konsolidierung ändert keinen Produkt-DSP und ersetzt keinen Test. Original-ZIP 1.0.0 liegt im Chat vor; es ist noch nicht zusätzlich als unverändertes Archiv im Repository gesichert. AGPLv3 und bestehende Attribution beibehalten.

Historische Dokumente: `docs/AUDIT-PRE-CODEX-20261026.md`, `docs/COMPACT-STEREO-MIGRATION-20261026.md`, `docs/STEP4-OWNER-CPU-DIAGNOSTIC-20261026.md`, `tests/step4/`.
