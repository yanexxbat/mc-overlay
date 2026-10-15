# SMP: Storage ESP, Stash Finder, Spawner ESP, Base ESP und Chunkfinder

Die fünf Module stehen unter **Blocks** im bestehenden kompakten Client-Menü.
**Linksklick** oder **Leertaste** schaltet ein Modul um. **Rechtsklick**,
**Menü-Taste** oder **Shift+F10** öffnet seine Einstellungen im unabhängigen
Fenster. In kleinen Viewports scrollt dieses Fenster bis zu allen Controls und
Aktionen; **X** führt zurück zum Raster. **Strg-Klick** auf einen Slider erlaubt
die direkte Zahleneingabe. [Menü und Keybinds](client-ui.md).

Der aktuelle Client verwendet **IPC v28**. Bereits laufendes Minecraft vollständig
neu starten, bevor der aktualisierte Client erneut angehängt wird. Ein schon
geladener Agent behält sonst das alte gemeinsame Datenlayout.

## Chunkfinder: Deepslate, Kelp und Amethyst

**Blocks → Chunkfinder** einschalten; Rechtsklick öffnet alle Einstellungen.
Der frühere Base Finder ist jetzt Teil des Chunkfinders. Ein Chunk wird als
**Possible Base** markiert, sobald mindestens einer der getrennten Grenzwerte
für ausgewählte Hinweise erreicht ist:

- **Horizontal deepslate**: standardmäßig mindestens ein roher Deepslate-Block
  mit horizontaler X/Z-Achse. Erze, polierter und befallener Deepslate zählen nicht.
- **Tall kelp**: standardmäßig mindestens drei Pflanzen mit jeweils mindestens
  18 Blöcken Höhe. Jede Spitze zählt einmal; die zusammenhängende Säule wird
  abwärts bis maximal 64 Blöcke gemessen, auch unterhalb des Scanwürfels.
- **Large amethyst / clusters**: standardmäßig mindestens sechs große Knospen
  oder vollständig gewachsene Cluster. Kleine/mittlere Knospen und normale
  Amethystblöcke zählen nicht.

**Highlight whole chunks** und **Highlight individual blocks** sind standardmäßig
beide an und unabhängig einstellbar. Der Chunk erhält einen 3D-Rahmen um seinen
exakten 16 × 16-Block-Fußabdruck mit Linien alle 16 Höhenblöcke. Die Höhe reicht
in der Overworld von −64 bis 320, in Nether/End von 0 bis 256. Beobachtete Hinweise
außerhalb erweitern den Rahmen; eigene Dimensionshöhen werden nicht ausgelesen.
Jeder verdächtige Block im erkannten Chunk erhält zusätzlich Würfelkanten, bei
Kelp auch jeder aus der gemessenen Höhe rekonstruierte Stängelblock. Rahmen,
Blöcke und Labels sind durch Wände sichtbar. Die Markierung beweist keine Base.

Scanradius (Standard 20, Bereich 4–48), Intervall (900 ms), Entfernung (64 m,
8–128 m), Farbe, Labels und Tracer gelten gemeinsam für alle drei Hinweise.
Nur erfolgreich abgeschlossene Scans und lokal geladene Blöcke werden ausgewertet.
Es werden keine entfernten oder ungeladenen Chunks gefunden.

Die Session-Liste enthält bis zu 128 mögliche Bases mit Chunk-Koordinaten,
Hinweiszahlen und Position. **Copy coordinates** kopiert die Position;
**Save waypoint** erhält sie über Neustarts. Die stärkste Beobachtung pro Chunk
bleibt erhalten und kann veraltet sein. **Clear possible bases** leert die Liste
bis zum nächsten Scan. Server-/Dimensionswechsel, Verbindungsabbruch, Panic und
Änderungen an Filtern, Radius oder Mindestwerten setzen sie zurück.

Alle Optionen speichern automatisch und in benannten Configs. Ältere Base-Finder-
Einstellungen werden beim Laden in den gemeinsamen Chunkfinder übernommen.
Ein vorhandener zweiter Keybind bleibt als **Chunkfinder (alternate)** nutzbar;
beide Bindings steuern denselben Schalter. Base ESP bleibt eigenständig.

## Storage ESP

**Blocks → Storage ESP** öffnen und **Enable Storage ESP** einschalten. Unter
**Block types and colours** lässt sich jeder der 13 Typen einzeln auswählen:

| Typen | Auswahl und Farbe |
|---|---|
| Chests, Trapped chests, Ender chests | Jeweils eigener Filter und RGBA-Farbwert |
| Shulker boxes, Barrels, Hoppers | Jeweils eigener Filter und RGBA-Farbwert |
| Furnaces, Blast furnaces, Smokers | Jeweils eigener Filter und RGBA-Farbwert |
| Dispensers, Droppers, Crafters, Brewing stands | Jeweils eigener Filter und RGBA-Farbwert |

Die Typbezeichnung oder Checkbox schaltet den Filter um. Die Farbschaltfläche
rechts daneben öffnet den nativen Farbwähler einschließlich Alpha. Farbige
Shulkerboxen gehören zum gemeinsamen Typ **Shulker boxes**. Die Typen sind
standardmäßig alle ausgewählt; Labels und Tracer sind zunächst aus.
**Reset storage colours** stellt nur die 13 Standardfarben wieder her.

**Block names and distance** zeigt Typ und Entfernung. **Tracers** zieht eine
Linie vom unteren Bildschirmrand zum Block. **Scan radius** reicht von 4 bis
48 Blöcken; **Opacity** multipliziert die Alpha-Komponente der jeweiligen Farbe,
**Glow** verändert den Effekt. Ein Alpha-Wert von null macht diesen Typ unsichtbar.
Ohne ausgewählten Typ erscheint ein Hinweis, einen Typ zu wählen.

Storage ESP zeichnet räumliche Würfelrahmen mit zwölf projizierten Weltkanten.
Auch an der Kamera werden Kanten an der Near Plane und am Bildschirmrand
abgeschnitten. Die Rahmen bleiben durch Wände sichtbar. Wenn zugleich das
allgemeine Block ESP läuft, überlässt es ausgewählte Storage-Typen diesem
3D-Renderer, damit keine zusätzliche flache Box darüberliegt.

| Einstellung | Standard | Bereich |
|---|---:|---:|
| Scan interval | 1000 ms | 200–3000 ms |
| Scan pulse | an | an / aus |
| Pulse cycle | 1500 ms | 500–6000 ms |

**Scan interval** ist die Pause nach einem Scan, nicht dessen garantierte
Gesamtdauer. Andere aktive Blockmodule können kürzere Pausen anfordern.
Der Agent behält bekannte Treffer während des nächsten Scans und ergänzt neue
Fundstellen schrittweise. Erst ein erfolgreich abgeschlossener Scan ersetzt
die alte Liste; verschwundene Container blenden im Overlay weich aus.
Ein fehlgeschlagener Teilscan löscht die bekannte Liste nicht.

Neue Container blenden versetzt ein. **Scan pulse** lässt einen helleren Streifen
nacheinander über ihre 3D-Rahmen wandern, während alle bekannten Rahmen sichtbar
bleiben. **Pulse cycle** steuert diesen visuellen Umlauf unabhängig von der
eigentlichen Blocksuche. Bei vielen Containern überlappen benachbarte Pulse.
**Settings → Overlay → Reduced motion** zeigt sofort ruhige Rahmen ohne Puls
oder Ein-/Ausblendung. Farben, Deckkraft, Glow, Labels und Tracer gelten weiter.
Beide Zeitwerte und der Puls-Schalter speichern automatisch und in benannten
Configs. Ältere vollständige Dateien erhalten die genannten Defaults, ohne
ihre Farben und Filter zu verlieren.

## Stash Finder

**Blocks → Stash Finder** öffnen und **Enable Stash Finder** einschalten. Der
Finder gruppiert ausgewählte Container-Blöcke nach ihrem 16 × 16 Block großen
Chunk in X/Z und nimmt einen Chunk auf, wenn die beobachtete Anzahl mindestens
**Minimum containers / chunk** erreicht.

| Einstellung | Standard | Bereich |
|---|---:|---:|
| Scan radius | 32 Blöcke | 4–48 |
| Scan interval | 1000 ms | 200–3000 ms |
| Minimum containers / chunk | 8 | 2–128 |
| Marker distance | 128 m | 8–512 m |

Standardmäßig zählen **Chests**, **Trapped chests**, **Shulker boxes** und
**Barrels**. Unter **Count these containers** lassen sich alle 13 Storage-Typen
als Filter wählen; Ender chests sind standardmäßig ausgeschlossen. Ein Double
Chest besteht aus zwei Blockpositionen und zählt deshalb als **zwei**. Es zählen
Blöcke, nicht Inventarplätze oder enthaltene Items. Ein Scan kann nur einen Teil
eines Chunks erfassen; seine Zahl ist keine vollständige Chunk-Inventur.

Nur erfolgreiche beendete Scans werden ausgewertet, jeweils einmal. Laufende
oder fehlgeschlagene Scans erzeugen keine Entdeckung. Nutzen andere Blockmodule
einen größeren Scan, zählt Stash Finder nur den Ausschnitt innerhalb seines
eigenen Radius. **Labels**, **Tracers** und **Marker colour** gestalten den Marker.
**Notify on new stash** meldet neue Chunks; dafür müssen HUD und
**HUD → Notifications** sichtbar sein.

**Discoveries this session** zeigt Chunk, beobachtete Anzahl und Koordinaten.
Die Liste behält höchstens **128** Entdeckungen, auch nach dem Verlassen des
Scanbereichs. Für einen bekannten Chunk bleibt die höchste beobachtete Anzahl
erhalten; spätere kleinere Beobachtungen reduzieren sie nicht. Die Angaben können
deshalb veraltet sein, etwa nach dem Entfernen von Containern.

- **Copy coordinates** kopiert eine Entdeckung; **Copy all coordinates** kopiert
  die ganze Liste einschließlich ihrer Anzahlen.
- **Save waypoint** speichert die Position der beobachteten Containergruppe als
  `Stash <Chunk X>, <Chunk Z>` und aktiviert Waypoints. Der Wegpunkt bleibt im
  bestehenden server-/dimensionsgebundenen Store nach einem Neustart erhalten.
  Ist der Wegpunkt-Speicher voll, zuerst einen bestehenden Wegpunkt entfernen.
- **Clear discoveries** leert die Session-Liste. Erst ein neuer beendeter Scan
  kann sie wieder füllen. Bei 128 Einträgen Koordinaten kopieren und die Liste
  leeren, um weitere Chunks aufzunehmen.

Server- oder Dimensionswechsel, eine unterbrochene Agent-Verbindung und ein
Neustart beenden die bisherige Liste. Änderungen an Typfiltern oder Mindestanzahl
leeren sie ebenfalls. Entdeckungen selbst werden nicht in Configs gespeichert.

## Spawner ESP

**Blocks → Spawner ESP** öffnen und **Enable Spawner ESP** einschalten. Das Modul
markiert gescannte Mob-Spawner-Blöcke mit eigener RGBA-Farbe, **Labels**,
**Tracers** und **Glow**. Labels sind standardmäßig an, Tracer aus.
Scan-Radius ist anfangs 24 Blöcke, Scan-Intervall 1000 ms und Draw distance 64 m.
Radius erlaubt 4–48 Blöcke, Intervall 200–3000 ms, Draw distance 8–128 m und
Glow 0–2. Mobtyp und Aktivierungszustand des Spawners werden nicht erfasst.

## Base ESP

**Blocks → Base ESP** einschalten; Rechtsklick öffnet die Einstellungen.
**Display → Minecraft textures** ist der Standard für Minecraft **1.21.11**
(Mojmap, Vanilla und Fabric Intermediary). Geladene Blöcke im erkannten Bereich
werden mit den echten Blockmodellen, Textur-Atlanten, Farbtönungen und der Lightmap
des laufenden Spiels durch verdeckende Weltblöcke gezeichnet. Die Base-Blöcke
verdecken sich untereinander weiterhin. **Texture opacity** stellt die
Transparenz ein; standardmäßig sind die Texturen vollständig sichtbar.

Base ESP funktioniert auch bei ausgeschaltetem Stash Finder. Beide verwenden
denselben Scan-Radius, dieselben Container-Filter und dieselbe Mindestanzahl
pro Chunk (standardmäßig acht). Die Darstellung umfasst beobachtete Lagerblöcke
plus **Expand detected area**, standardmäßig vier zusätzliche Blöcke auf allen
Seiten. Die Erkennung folgt Lagerblöcken und ihrem zusätzlichen Rand; sie bestimmt
nicht automatisch die Außengrenzen eines ganzen Gebäudes. Innerhalb dieses
Bereichs verwendet der Texturmodus die tatsächliche geladene Blockgeometrie.

**Display → Area marker** bietet weiterhin die farbige Fläche mit Rahmen,
**Base colour**, **Fill opacity** und **Frame width**. Der Modus ist auch auf
anderen unterstützten Minecraft-Versionen verfügbar. Labels und Tracer sind
optional. Standardmäßig beträgt die Anzeigeentfernung 192 m. Die Anzeigeentfernung
erweitert den Scan-Radius nicht. Aussehen und Modul-Bind werden in automatischen
Einstellungen und benannten Configs gespeichert. Bestehende vollständige Configs
ohne Base ESP erhalten dessen ausgeschaltete Defaults und einen ungebundenen Key.
Vollständige ältere Base-Configs erhalten den neuen Texturmodus mit Deckkraft 1;
teilweise beschädigte neue Texturfelder werden bei benannten Configs abgelehnt.

Der Texturmodus bearbeitet höchstens acht nahe Bereiche, 16.384 Zellen und
786.432 Dreiecksvertices pro Aufbau. Die Erfassung läuft in Abschnitten mit
einem Zielbudget von 2 ms auf Minecrafts Render-Thread; mindestens eine Zelle
wird pro Abschnitt geprüft. Ein einzelner aufwendiger Block kann dieses Budget
überschreiten. Nach einem fertigen Aufbau wird etwa jede Sekunde neu erfasst.
Animierte Atlas-Texturen bleiben live. Überlappende Bereiche
werden ohne doppelte Blockgeometrie erfasst. Die Anfrage läuft nach 750 ms ohne
Overlay-Erneuerung ab und ist an Server und Dimension gebunden.

Normale Blockmodelle, Flüssigkeiten und spritebasierte Block-Entity-Modelle
(etwa Kisten und Shulker) werden über die Spielrenderer erfasst. Spezielle
Zusatzdarstellungen wie Text, Portale und manche Mod-Renderer werden übersprungen
und im Status gemeldet. Der Status zeigt außerdem Aufbau, leere Bereiche,
Grenzen oder Fehler. Bei einem Fehler Base ESP aus- und wieder einschalten,
um erneut aufzubauen, oder **Area marker** wählen. Auf anderen Versionen und
bei fehlendem Texturrenderer zeigt das Menü die passende Version-/Neustart-
Empfehlung; **Area marker** bleibt die alternative Darstellung.

## Scan-Status und Grenzen

Scans laufen im Agent-Worker und lesen ausschließlich Blockdaten, die der Client
in seiner Welt bereitstellt. Die Scanform ist ein Würfel mit dem gewählten Radius
um die Spielerposition beim Scanbeginn. Der aktuelle Scanner berücksichtigt
Y-Koordinaten von −64 bis 319. Er deckt keine ungeladenen Chunks auf und liest
keine Container-Inhalte. Die externe Scan-Darstellung verarbeitet nur die
veröffentlichten Snapshots; der Texturmodus hat den oben beschriebenen eigenen
Callback im Spielrenderer.

Aktive Blockmodule teilen einen Scanner: ihre Typfilter werden zusammengefasst,
der größte Radius und das kürzeste Intervall werden angefordert. Die einzelnen
Module behalten ihre eigenen Filter und Anzeigegrenzen. Ein Scan kann mehrere
Worker-Schritte dauern; das eingestellte Intervall garantiert keine fertige
Auswertung in diesem Abstand.

Der Status unter den Einstellungen zeigt Verbindung/Weltbeitritt, Fortschritt,
Abschluss oder fehlende Runtime-Bindings. Ohne Verbindung Minecraft auswählen und
eine Welt betreten. Bei **Block scanning is unavailable** unter
**Settings → Agent** die erkannte Version und Diagnose prüfen.

Ein Resultat enthält höchstens **1024 passende Blöcke** für alle aktiven
Blockmodule zusammen. Bei Erreichen dieses Limits zeigt der Client:
**Counts are lower bounds**. Auch ein so begrenzter beendeter Scan kann einen
Stash finden, aber seine Anzahl kann zu niedrig sein. Radius verkleinern oder
weniger Typen auswählen. **Marker distance** und **Draw distance** erweitern
nur die Anzeige, nicht den verfügbaren Scanbereich.

## Speicherung und Bindings

Alle SMP-Einstellungen, Farben, Filter und Modul-Binds speichern automatisch in
`client-settings.ini` neben `mcov.exe` und in benannten Profilen unter
`configs/<Name>.ini`. Storage ESP nutzt seinen vorhandenen Bind; Stash Finder und
Spawner ESP ergänzen neue Aktionen. Alle Modul-Binds beginnen ungebunden und
lassen sich unter **Settings → Keybinds** oder direkt am Aktivierungsschalter
mit **Toggle/Hold** setzen.

Vollständige ältere benannte Configs ohne den gesamten SMP-Block erhalten die
SMP-Defaults und ungebundene neue Aktionen. Stash Finder und Spawner ESP bleiben
dabei aus. Ein teilweise vorhandener oder ungültiger SMP-Block lässt ein benanntes
Profil vollständig scheitern; die aktuellen Einstellungen bleiben unverändert.
Auch automatische Einstellungen wenden einen beschädigten SMP-Block nicht
teilweise an. Für dauerhafte Fundorte **Save waypoint** verwenden und beim
Umzug `client-settings.ini` und den `configs`-Ordner mitnehmen.

## Prüfbelege

Für die aktuelle Base-Texturerweiterung bestanden alle **26** verfügbaren
Regressionsprüfungen über die abschließenden Läufe: 25 in der Sandbox und
`frame_attach` separat außerhalb nach einem Windows-Pipe-Zugriffsfehler seines
isolierten Fixture-JVMs. Nach der letzten Ressourcenfreigabe-Änderung bestanden
`base_texture_gl`, `base_texture_models`, `smp_features` und `portable_client`
erneut; `depth_hook` bestand im vorangehenden gezielten Lauf. Die Produktions-
EXE und Share-ZIP wurden neu gebaut.

`base_texture_gl` verwendet echtes WGL/OpenGL und prüft Pixel sowie erhaltenen
Grafikzustand: Atlas-UVs, Kameraprojektion, Verdeckung zwischen Base-Blöcken,
Durch-Wand-Compositing, unveränderte Pixel außerhalb der Base, live aktualisierte
Atlas-Texturen, Deckkraft 0,5, das Mischen transparenter Texel und erneute
Renderer-Initialisierung nach Ressourcenfreigabe. Ausschalten gibt die GL-
Ressourcen frei, sodass Aus-/Einschalten auch einen fehlgeschlagenen Renderer
neu erstellen kann.
`base_texture_models` prüft Java-Proxy-Defaults, Modell-/Sprite-Pfade und
Quad-Erfassung mit UVs, Tint und Lichtwerten. `depth_hook` deckt beide
Render-Callbacks ab; `smp_features` prüft Texturmodus, Profilmigration,
Anfrage-Lease und Welttrennung. Die 38 Klassen und 73 Member des 1.21.11-
Texturbridges wurden gegen zwischengespeicherte offizielle Klassen für Mojmap,
Vanilla und Fabric Intermediary geprüft.

Die fünf endgültigen Settings-Belege verwenden native ImGui/D3D11-WARP-
Fixtures mit synthetischen Daten:

| Zustand | Beleg |
|---|---|
| Desktop, 1280 × 800 | [Base-Einstellungen](../.impeccable/review/base-textures/base-esp-settings-desktop-verified.png) |
| Kleines Fenster oben, 480 × 360 | [Textur-Controls](../.impeccable/review/base-textures/base-esp-settings-small-top-verified.png) |
| Kleines Fenster gescrollt, 480 × 360 | [Erkennung und Grenzen](../.impeccable/review/base-textures/base-esp-settings-small-verified.png) |
| Aufbau, 480 × 360 | [Ladestatus](../.impeccable/review/base-textures/base-textures-loading-small-verified.png) |
| Fehler, 480 × 360 | [Fehler und Wiederherstellung](../.impeccable/review/base-textures/base-textures-error-small-verified.png) |

Die unabhängige Abschlussprüfung bestätigte **ship für den UI-Umfang**.
Es gibt keine Live-Minecraft-Aufnahme oder Prüfung des Texturmodus im laufenden
Spiel. Shader- und Mod-Grafik-Kompatibilität bleibt ungeprüft.

Die früheren fünf Base-Prüfungen `smp_features`, `depth_rendering`,
`control_panel`, `client_controls` und `portable_client` sowie die fünf
`-final.png`-Bilder unter `.impeccable/review/base-esp/` betreffen die farbige
Fläche und den Rahmen des **Area marker**. Der damalige D3D11-WARP-Pixeltest
bestätigt diese Fläche hinter Tiefenwerten und ohne Tiefendaten; er prüft keine
Minecraft-Modelle oder Texturen. Diese historischen Belege bleiben erhalten.

Für die ursprüngliche Erweiterung mit Storage ESP, Stash Finder und Spawner ESP
bestanden alle **24** verfügbaren Regressionsprüfungen über die abschließenden
Läufe. Der isolierte `frame_attach`-Test bestand nach einem Windows-Pipe-
Zugriffsfehler außerhalb der Sandbox mit seinem eigenen Fixture-JVM.
`smp_features` prüft Klassifikation, beendete Scans, Filter, negative Chunks,
Welt-/Session-Trennung, höchste Anzahlen, Limits, Profilmigration und Rendering
mit unterschiedlichen Typfarben. `control_panel` prüft echte Typbezeichnungs-
Klicks, den Farbwähler, erreichbare Settings und automatische Speicherung.
`portable_client` bestand; Produktions-EXE und Share-ZIP wurden neu gebaut.

Die sechs aufbewahrten Bilder verwenden den echten nativen ImGui/D3D11-WARP-
Renderer mit synthetischen Fixtures bei 1280 × 800 und 480 × 360:

| Einstellungen | Desktop | Klein |
|---|---|---|
| Storage ESP | [Farben](../.impeccable/review/smp/smp-storage-colours-desktop.png) | [Farben](../.impeccable/review/smp/smp-storage-colours-small.png) |
| Stash Finder | [Entdeckungen](../.impeccable/review/smp/smp-stash-discoveries-desktop.png) | [Entdeckungen](../.impeccable/review/smp/smp-stash-discoveries-small.png) |
| Spawner ESP | [Settings](../.impeccable/review/smp/smp-spawner-desktop.png) | [Settings](../.impeccable/review/smp/smp-spawner-small.png) |

Die unabhängige Abschlussprüfung öffnete alle sechs nach dem Typbezeichnungs-
Fix erneut erzeugten Bilder und bestätigte **ship**, ohne sichtbare Regression.
Diese Belege prüfen Settings und synthetische Darstellung. Für SMP wurde kein
Live-Minecraft-Test durchgeführt; Scanverhalten, Optik und Attach im laufenden
Spiel sowie alle GPU-Kombinationen sind dadurch nicht bestätigt.

## Base Finder: Prüfung

Für Base Finder bestanden alle **27** Regressionsprüfungen über die abschließenden
Läufe. `runtime_profiles` prüft Kelp-Höhen auch unterhalb des Scanwürfels, genau
eine Meldung pro Pflanzenspitze und beide Amethyst-Typen über die vorhandenen
JNI-Profile. `smp_features` prüft unabhängige Mindestwerte, kurze Pflanzen,
negative Chunks, vollständige Scans, Filter, Session-Trennung, Config-Migration
und Marker durch Wände. `control_panel` prüft erreichbare Controls und Aktionen,
die feste Kopfzeile bei kleiner Fenstergröße, Preferences und Panic.
Der isolierte `frame_attach`-Test bestand außerhalb der Sandbox nach deren
Zugriffsfehler auf eine lokale Java-Pipe. Client und Share-ZIP wurden neu gebaut.

Die sechs nativen synthetischen Screenshots und die unabhängige UI-Prüfung
mit Ergebnis **ship** liegen unter `.impeccable/review/base-finder/`.
Sie zeigen Einstellungen, leere Liste und Entdeckungen bei 1280 × 800 und
480 × 360. Ein Live-Minecraft-Test wurde nicht durchgeführt.

## Storage ESP 3D: Prüfung

Für die 3D-/Puls-Erweiterung bestanden alle **30** aktuellen Prüfungen: 29 in
der Sandbox (93,40 s), der isolierte `frame_attach` separat außerhalb (0,45 s).
`smp_features` prüft die exakten zwölf projizierten Würfelkanten einschließlich
Near-Plane-Fällen, stabile Treffer während Teilscans, erfolgreiche leere Scans,
Positionswechsel, Kapazitätsgrenzen, versetztes Einblenden, Puls-Geschwindigkeit,
weiches Entfernen, reduzierte Bewegung und Session-Trennung. Benannte Configs
und automatische Preferences prüfen neue Werte und die Migration alter Dateien.
Der gemeinsame Scanner und die Cluster-Detektoren behalten ihre bisherigen
Grenzen und die Auswertung ausschließlich abgeschlossener erfolgreicher Scans.

Die vier nativen Settings-/Weltbelege mit synthetischen Snapshots liegen unter
[Storage-3D-Prüfung](../.impeccable/review/storage-3d/review.md). Sie zeigen den
tatsächlichen Renderer bei 1280 × 800 und 480 × 360. Client-EXE und Share-ZIP
wurden neu gebaut und mit `portable_client` geprüft. Es gab keinen Test im
laufenden Minecraft und keine Messung von FPS oder RAM-Verbrauch.
