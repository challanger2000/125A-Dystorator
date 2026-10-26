# 125A Dystorator - CPU Test am eigenen Windows-Rechner

**Dies ist ein Entwicklungstest, keine veröffentlichte V1 und keine vollständige Echtzeit-/Studio-One-Freigabe.**

Doppelklick auf **START-CPU-TEST.cmd** im entpackten Paket. Der Test benötigt keinen Installer, schreibt keine VST3-Dateien nach C:\Program Files und verändert keine Studio-One-Projekte. Die vorliegende EXE und beide Plugins werden nur aus diesem Paket geladen.

Das Skript speichert die echte CPU/Windows-Version, ein Powerplan-Profil, VST3-/Testhost-SHA256, 4 vollständige Rohmessungen, 192 CSV-Messfälle, Vorher/Nachher-ABBA-Vergleich, p95/p99/max und gesonderte ECO-STEREO-Deadline-/Overrun-Zahlen im Ordner **Results**. Wenn der Test abstürzt oder die Konfiguration ungültig ist, bleibt die Diagnose erhalten, wird aber nicht als PASS bewertet.

**Wichtig:** Vor einem erneuten Durchlauf den Ordner Results umbenennen oder verschieben. Das Skript verweigert Überschreiben. Windows SmartScreen kann bei nicht signierten Testprogrammen warnen; keine Freigabe durch Sicherheitssysteme umgehen, wenn du die Quelle nicht überprüft hast.

## Messvorbereitung

Windows 11 x64, möglichst normale und stabile Leistungsbedingungen; alle sonstigen aufwendigen Arbeiten beenden und Hintergrundlast/CPU-Drosselung prüfen. Der Test kann die CPU für einen längeren Zeitraum hoch belasten.

**Keine automatische Installation, kein ASIO, kein Studio One**. Der CPU-Vergleich verwendet den reproduzierbaren JUCE/VST3-Testhost und zeigt nicht, ob Studio One mit Audiointerface, ASIO, virtuellen Stereo-Gitarren, mehreren Instanzen und den realen Buffer-Einstellungen knackfrei läuft. Dafür bleibt ein gesonderter echter DAW-Test notwendig.

**Produktziel:** ECO/LOW muss Stereo-Echtzeit ohne Aussetzer ermöglichen. HIGH darf mehr CPU benötigen, soll aber einen qualitativ einwandfreien Offline-Bounce liefern. Aktuell stellt das Plugin bei Offline-Export HIGH **nicht automatisch** ein: HIGH vor dem Bounce manuell auswählen.

## Vorher / Nachher

Before ist die geprüfte Schritt-3-CPU-Basis `398204e26d84c3f40bcd1c0af55dfa8e97893ca7`. After basiert auf identischem Produkt-Source-Tree `23fe6fa178f2248c23726a31ad080a82f9e991bc` aus dem Windows-Funktionslauf `8a442d9d2a3ad10ba331b7cc849136db78ca08ac`. Der Packaging-Workflow dokumentiert die tatsächlichen Build-Commits und Hashes im Paketmanifest; bitte Ergebnis-Hashes nicht mit unbeteiligten Binärdateien verwechseln.

Die historischen Namen in der Test-DLL sind unverändert, da die spätere Produktumbenennung auf **125A Dystorator** bewusst separat erfolgt. AGPLv3 und die Urheber-/Drittanbieterhinweise werden beibehalten. Dies ist kein fertig gebrandetes Produkt.
