# P001 — Revisionsgebundene Engineering-Pflichtlektüre

Stand 2026-11-25. **COMPLETE — Pflichtlektüre für den aktuell definierten P001–P005-Produktumfang.** Keine QA- oder Releasefreigabe. Produkt-Baseline vor diesem reinen Dokumentationscommit: b27279cf8437137f875e8a91815b21d594139000 (main); AKTUELLE-VERSION.md und docs/P001-REFERENZ-UND-PRODUKTUMFANG.md vollständig gelesen. Keine DSP- oder Buildänderung.

Engineering-Repository: challanger2000/125A-Engineering, unveränderlicher Commit 53b10bd4a81fe6c5d62e110b932aef7155172ad6. Lesebeleg dieses Durchlaufs (tatsächliche Inhalte im Modell sichtbar, keine Ausgabe-Trunkierung dieser Bereiche; Zeilenzählung split("\n"), einschließlich etwaiger finaler Leerzeile).

| Datei | Blob | Umfang | Gelesen | Offen |
|---|---|---:|---|---|
| START-HERE.md | 91a0e14dd24ff72874bbedf58962e0e8c14c444e | 164 | vollständig | keine |
| README.md | bd94e5a896bcc905af65ca5659e533eee854b46f | 74 | vollständig | keine |
| ENGINEERING-GUARDRAILS.md | 8a076e21d8226a5b1ded7061362590415fe4839f | 3187 | 1–3187 | keine |

**Nächster Lauf:** aktuelle START-HERE.md und Engineering-Revision/Blobs prüfen. Unveränderte vollständig gelesene Quellen anhand dieses Lesebelegs wiederverwenden. Neu relevante oder geänderte Quellen vor Anwendung lesen. Unmittelbar mit P001-Referenzprüfung/Quellcodevergleich fortfahren; nicht erneut dieselbe Pflichtlektüre starten.

## Anwendbare Regeln aus bisher gelesenen Quellen

- START-HERE vollständig: Architektur vereinfachen ohne Qualitätsverlust, C++/JUCE und gepinnte Abhängigkeiten; vollständige Quellen-/Baseline-Prüfung; reale unabhängige Messungen, Negativkontrollen, echte Host-/Recall-/RT-Pfade, keine spekulativen CI-Builds, p95/p99/max und Fristüberschreitungen, reale Audiofixtures plus synthetische Messung, keine Releasefreigabe ohne vollständige Nachweise. User-approved P001 scope und konsolidierten main beachten.
- README vollständig: zentrale Standards nicht divergent duplizieren; etabliertes DSP/Mathematik/Physik, Branding-Quelle, Lizenzgrenzen, Tests und nachvollziehbare Messung.
- Guardrails 1–70: bestehende Architektur gegen echte Alternativen prüfen (hier Original 1.0.0 vs. vorhandene Stereo-/Host-Integration); keine ungeprüfte Vereinfachung der Röhrenschaltung. Exakte Owner-GUI und alle vorgesehenen Bedienelemente mit DSP/State/Host verifizieren. IR-Ladefehler atomar konsistent zurückrollen oder invalidieren, off-thread. Extreme endliche Eingangswerte und Recovery, keine konkurrierenden rohen State-Felder; echte Eingangsbusse mit nichtstillem kanalverschiedenem Signal für RT-Messungen, negative Host-Fixtures, keine Zero-Frame-Messung als Audio-PASS.
- Guardrails 71–130: zeitabhängige Übergänge und inaktive DSP-Engine-Grenzen auf exaktem Sample, unabhängig von Hostblockpartition; Stereo-PCM-Orakel und negative Gegenprobe; keine falsche Bestätigung bei partieller Lektüre (alte Wiederholungsregel durch explizite Owner-Änderung in START-HERE abgelöst). Recall/IR/Preset/Automation in chronologisch konsistenter Transaktion; keine älteren asynchronen Aufträge über neuere gewinnen lassen, unmittelbar nach akzeptierter Änderung speichern und auch nächsten Audioblock prüfen, keine RT-Locks/Allokationen oder unbeschränkten CAS-Schleifen.

- Guardrails 131–200: ausdrückliche VST3-Lifecycle- und Save/Recall-Tests auch während normaler Wiedergabe; atomare zusammengehörige Zustandsdaten, UI-Fehler/Recovery. Dystorator-spezifisch (Abschnitte 2026-10-27): alle sichtbaren Controls über APVTS/Processor bis zum gemessenen PCM verfolgen, obsolete Controls nach expliziter Migration entfernen; native JUCE Editor-Interaktion statt Textprüfung. Cabinet-K-weighted matching der 1.0.0 mit unabhängigen IR/Convolution-Orakeln prüfen (gemeinsamer Stereo-Gain, Polarität, Kanalverhältnis, Sample-Rate, Stille, echte DI und lautheitsangepasster Hörvergleich); alte Normalise::yes-Orakel nicht stillschweigend wiederverwenden. ZIP- und Payload-Identität getrennt prüfen.
- Guardrails 201–270: tatsächliche Editor-Zoom- und Popup-Zustände vor Screenshot assertieren, nicht Bildgröße/Dateiname als Funktionsbeleg nehmen. Transportkontexte mit expliziten Validitätsflags; keine erfundenen Hostdaten. DSP-Reuse nur unter vollständiger Identität von Signal/State/Parametern, kontrollierte gepaarte RT-Messungen statt Vergleich verschiedener CI-Runner; kein Mutant-KILL bei fehlgeschlagenem positiven Orakel. Echte End-to-End-Anforderungen, aktive Busse, echte Fixtures und adversariale Save/Recall/Asset-Pfade, genaue SDK-Schnittstellen prüfen. Ausgelassene Tests NOT RUN, nicht PASS.

- Guardrails 271–320: Workflow-YAML nach literal-safe replacement strukturell validieren und zurücklesen; Windows-Makros/ABI beachten, Modulo und Integer-Grenzen testen; Bus-Arrangement nur bestätigen, wenn tatsächlich umgesetzt, falsche Eingabearrays ohne Mutation ablehnen.
- Guardrails 321–380: auch gleichwertige bzw. ABA-Host-/MIDI-Ereignisse generationstreu behandeln; Mehrfeldänderungen atomar und Undo vollständig; echte native Erreichbarkeit jedes Controls, deaktivierte ungültige Operationen, keine unerwünschten Änderungen an inaktiven Daten; Allokationsfehler an Host-ABI-Grenzen behandeln.
- Guardrails 381–440: erst Kandidaten vollständig vorbereiten/validieren, bei OOM/IR-/State-Fehlern alten gültigen Zustand und Audio erhalten; Controller-Metadaten nach akzeptiertem Commit nicht allokierend aktualisieren; sowohl State-before-activate als auch State-after-activate testen. Gain-Ramps sample-rate-abhängig und pro Sample einmal für alle Kanäle fortschreiben, dichtes Retargeting, exakte Endpunkte, Recall ohne unnötige Startup-Rampe. Mehrfeld-Snapshots für Audio und State kohärent, kontrollierte Ressourcen-Lebensdauer, kein Control-Mutex im Audiothread.

- Guardrails 441–900: vollständige State-Transaktionen und Gesteninvalidierung bei erfolgreichem Recall, Fehler-Rollback, aktive Release-Assertions und reale Audio-/Host-Orakel; C++-Standard explizit auswählen, SDK und CTest-Targets vollständig bauen, User-IR-Bytes portabel speichern; korrekte Paketnamen, Bus-/Editor-Lifecycle, unübliche Host-Raten und unabhängige Stereo-PCM-Tests. Keine versteckten Testklänge, keine fälschliche PASS-Aussage bei fehlendem Feature-Nachweis.
- Guardrails 901–1500: rekursive GUI-Geometrie, tatsächliche Interaktion, SDK-Signaturen/Linkage, kontrolliertes A/B, Teststimuli passend zur Schaltung, Testzustand isolieren; kontinuierliche Automation O(1), keine GUI-/Audio-Blockierung, keine stummen Fallbacks als RT-PASS; UI-Thread-Eigentum und konsistente Drag-Koordinaten.
- Guardrails 1501–2000: gepacete RT-Tests mit Frist, p95/p99/max und Fehlerzählern; Bypass-DSP warm halten, dichte Automation kohärent, Overflow explizit, FIR-Randbedingungen korrekt, keine willkürlichen Sicherheitsaufschläge; Artefakte tatsächlich materialisieren; interne Messdomänen und kombinierte Filterantwort getrennt prüfen; Subsystemgrenzen/Mehrfachinstanzen/Lifecycle berücksichtigen.
- Guardrails 2001–2500: Gain-/Dynamikprüfung bei variierenden Eingangspegeln, musikalischer Nutzen bei praxisgerechten Einstellungen, level-matched A/B, Harmonische/IMD/Aliasing; UI-Listener, tatsächliche Klicks, ein Bypass-Toggle, korrekte diskrete Parameterabbildung, Tail/PDC getrennt, sample-rate-abhängige Rampen; explizite Stereo-Domänen und exakte finale Bundle-Verifikation; unabhängige Vollständigkeitsmatrix, aktive Release-Tests.
- Guardrails 2501–2900: Tail-Metadaten an vorbereiteten/abgelehnten IR koppeln, native Host-Lifecycle/DPI/Wheel/Text/Drag/Recall prüfen; CI-Abfragen über echte Run-IDs/SHA, Screenshotinhalt kontrollieren, UI- und Asset-Caches an tatsächliche Revision binden, RT-Pfad begrenzen; Hardwarevergleich nur unter verifizierten identischen effektiven Bedingungen; keine versteckten globalen Zustände, Host-Eingaben validieren/iterieren mit begrenztem Budget, recoverable Release/Fehlerevents kohärent abarbeiten.
- Guardrails 2901–3187 (EOF): Quieszenz zeitlich und anhand realer PCM belegen, per-instance Pointers stabil halten, sichere off-RT-Retirement-Transaktionen und sample-genaue Handover; gültige/ungültige State-Streams inklusive UTF-8, OOM, stale/mixed/rejected Saves und echter Audio-Recall, Dateiidentität über Validierung und Verwendung sichern; Paketintegrität und Lizenznachweise, tatsächliche native Attach-Fehler und Resize/DPI, inaktive Busse nicht berühren, Host-Feldvalidität unabhängig, Returncode UND Bytezahl bei Streams prüfen. Vorangegangene Guardrails bis EOF tatsächlich in aufeinanderfolgenden Ausgaben gelesen; initiale Trunkierung 3031–3187 durch vollständige getrennte Abrufe 3031–3105 und 3106–3187 behoben. Die ergänzenden relevanten Quellen wurden anschließend vollständig gelesen (siehe unten).

Alle QA-, Klang- und Performance-Nachweise bleiben offen. Keine Freigabe.

## Ergänzende Pflichtlektüre — abgeschlossen am 25.11.2026

Alle 46 nachfolgenden Markdown-Dateien wurden in vollständigen, nicht abgeschnittenen Tool-Ausgaben gelesen, jeweils am oben gepinnten Engineering-Commit. Kein Anspruch, sämtliche themenfremde Research-Notizen gelesen zu haben.

| Datei | Git-Blob | Abdeckung |
|---|---|---|
| STANDARDS/AI-ASSISTED-ENGINEERING.md | 12ce40b84b2acd79544dbd56ddb8c88cf342f4fd | vollständig |
| STANDARDS/DSP-GUIDELINES.md | 0307c6c8d9b7158f9c1f9ffb467d83ee8d23a4ae | vollständig |
| STANDARDS/PROFESSIONAL-PLUGIN-BEHAVIOR.md | d9c696ee522faf84010f783f358b11c8c11dbdb5 | vollständig |
| STANDARDS/SOURCE-BASELINE-GATE.md | 47a7ba32d333158b2bd84d7464e4c084a2bb40c4 | vollständig |
| STANDARDS/REPOSITORY-WORKFLOW.md | e3553244e78a82f2ba1d4cc4936daca0e57d7bac | vollständig |
| STANDARDS/GUI-GUIDELINES.md | 556fdbf3568a77b8b2aebc2c7c14190e2ced31f1 | vollständig |
| STANDARDS/BRANDING.md | 0a8f3748494461087fcf2621dae45ca893386eb6 | vollständig |
| STANDARDS/V3-DEVELOPMENT.md | 7a7b1300301c416bc53b2ed51c0f4cd40877310d | vollständig |
| STANDARDS/DEMO-LICENSING.md | 6e523fb3a28dd90258842784683d6c31c1a14592 | vollständig |
| QA/AUDIT-COMPLETENESS-CONTRACT.md | 1bb7508e1fd482503d0c1198f2a8e38f40e0a88f | vollständig |
| QA/RELEASE-QA.md | fb11d2a3057817b14234d13563edc200b5e7b147 | vollständig |
| QA/REALTIME-QA.md | 9a157249bd1e6a561bf8a527554ea2fdbe6f8a8a | vollständig |
| QA/AUDIO-FIXTURES.md | 367c478d8c96970fc23d5d58ab712830ed537687 | vollständig |
| RESEARCH/GainStageFX-v0.24.0.md | f104ef747e113892a7efc023442aef7b933ffbd2 | vollständig |
| RESEARCH/RESAMPLING-OVERSAMPLING.md | 2aa90ce2c60bffa8f1c489b9706de98e23dc90b8 | vollständig |
| RESEARCH/LOUDSPEAKER-MODELLING.md | bf51c9833325e542c06364e3affa77a2af21677e | vollständig |
| RESEARCH/PSYCHOACOUSTICS-PERCEPTION.md | 3bcf10c4994c4a9ab7819a0f43b6497e09e9d9d8 | vollständig |
| RESEARCH/OPEN-QUESTIONS.md | c7bb5ed4a798b7d9b19bec61af849d4af940abc1 | vollständig |
| RESEARCH/REJECTED-APPROACHES.md | e21ea8ccc179aff8a789d942cbd05141997dc7cd | vollständig |
| RESEARCH/SOURCES.md | 7dc00c32374c3976aa0780825eccc29e5306ee1f | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/INDEX.md | 98b10398745e0bbc263713d5774113a45560e4df | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/chowdsp_wdf.md | 392acda57df1a0e9277b39b6a7d9fc9510c4f669 | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/Spice-FX.md | 056507250b68426a7c97f04ca851536e22e5db9b | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/NeuralAmpModelerCore.md | b4f1edfcc093f1a573c2339a1bf7eeef70a24d2e | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/nilamp.md | 39e07fe9b4b5a58ee05348ec1650c28e175d4ce6 | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/Chow-Centaur.md | ad64029cf6ec8f79b3c3e2fd8f866fe9a02746ac | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/VIOLA.md | 73447142f9847aea0a6eb41dad5912b9153771dc | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/TONE3000-Plugin.md | f4b29d23e19f6979e18e1a12f5099e76b1dea221 | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/Rustortion.md | 35a1531c2f187707e4858198cf11d47580737137 | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/Zones-Convolver.md | 2c084f4fa20e5b48d68d6d20e5615467d36e0214 | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/Oversimple.md | fbc0000540f23973b2633dff6787260f780d1eb0 | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/r8brain.md | fc182b79b5346515defc1db8d7be55085d3d635b | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/pluginval.md | f665f5ca356e6b4ff5cb8b61c8855f3cddad730d | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/freefx.md | 21a292c6125d9095b6aae495f9ec8d353344368a | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/BYOD.md | b7c79a1a229fcc1a35124aa3c831e27ad92103d2 | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/RTNeural.md | 8c3bc2c868e3fe47203b0fec0a22f9891ccd1c78 | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/ADAC.md | 3a63d95797aecadfc5d543493c536d771b71f9cb | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/NeuralAudio.md | 63ab96a10cb35b913cf1689dc1ec76b35bbbfc76 | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/LSP-DSP-Units.md | 415c2498abe7aae44cacf72cf6fc76332223b96a | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/DSPark.md | fcfa7768d6f5abc00cf964b3882f306e6d34ea86 | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/Glint.md | d6261acf4680e8923664bd34acb803221bd05a10 | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/x42-meters.md | 46f37a368142c50649f404df670533fda4df9152 | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/Airwindows.md | 8c5bbe28c50dffd3b273b6e511920d7e007d2d7c | vollständig |
| RESEARCH/EXTERNAL-PROJECTS/Airwindows-DEEP-DIVE.md | 92df2b510d6e83399db9cbd2bb2b09be40187fc2 | vollständig |
| RESEARCH/DIFFERENTIABLE-DSP.md | ea2d235616d7b78293587d78bca002a749567196 | vollständig |
| ROADMAP/V3-CANDIDATES.md | b41bbfb179b2e64debe2783603082cb7985daa2f | vollständig |

Branding: README.md vollständig, Commit 20c4d9d2a3b37e6a8d17e693ed76d9351203c288, Blob 22fb9f988d0f53ec984981722f9a2a5acaa00d7c; autoritativer SVG-Master im Tree verifiziert (bd398ffa3b728b1a847bb519fad8168222bccb48), Asset noch nicht integriert. Knob-Designer README vollständig, Commit 45565ff036eb469b9b0c092744233961b1dec837, Blob c8ae306ab8c0ca8aedf424ae2d029ccff2ad08ee; dokumentiertes MixEngine-Set nicht automatisch als Dystorator-Design einsetzen.

Geltungsbereich: neuer, unveröffentlichter Windows-VST3-Amp, kein bestehendes Final-/FULL-Upgrade, kein Demo-/Gumroad-Rollout, keine Marketinggrafiken. Thematisch nicht erforderliche Synthese-, Tape-, Reverb-, Pitch/Time-, Spatial-, Limiter- und MixEngine-Experimentdokumente sowie Promo- und Demo-Roadmap sind für den beauftragten Umfang nicht verpflichtend; bei Scopeänderung nachlesen. Primärliteratur und tatsächliche externe Implementierungen müssen vor konkreter Verwendung zusätzlich geprüft werden; diese Lesebestätigung ersetzt das nicht.

Konsequenzen: JUCE-8.0.4-Referenz und tatsächliche Lizenz-/Dependency-Notices prüfen; Dystorator-spezifisch 1.0.0 statt 0.5.6 als Klang-/DSP-Oracle. Physikalische Schaltung und Kopplung erhalten, analytische Jacobians, exakte lineare Reduktion, bounded Newton, keine invaliden Zustände nach Nichtkonvergenz; SIMD nur nach kontrollierter Messung, Oversampling ausschließlich begründet und mit Alias-/Latenz-/RT-Vergleich, Cabinet linear möglichst Hostrate. Referenztests mit tatsächlicher 1.0.0, Pegel-/Zeitabgleich, echte DI, unabhängige Erwartungen/Mutanten, Solver-Fehlerzähler, p95/p99/max und Deadline-Counts (offline und paced getrennt); kein NAM als Ersatz. Anforderungen und jede UI/DSP/Host-Kette bidirektional prüfen. Originale GUI ausschließlich nach Owner-Vorlage plus autoritativem Branding; JUCE-Resize-Pattern vor Implementation prüfen, noch keine DAW-Freigabe. Bestehender unreleased main ist ausdrücklich vom Owner konsolidierte Arbeitsbasis, nicht stabiler Final-Release. Keine zusätzlichen Branches ohne Bedarf, keine CI durch reine Lesedokumentation. DSP-/Klang-/Windows-/Studio-Pro-Akzeptanz weiterhin NOT RUN / NO-GO.
