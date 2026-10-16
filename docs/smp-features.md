# SMP: Storage ESP, Stash Finder, Spawner ESP, Base ESP und Chunkfinder

Die fÃ¼nf Module stehen unter **Blocks** im bestehenden kompakten Client-MenÃ¼.
**Linksklick** oder **Leertaste** schaltet ein Modul um. **Rechtsklick**,
**MenÃ¼-Taste** oder **Shift+F10** Ã¶ffnet seine Einstellungen im unabhÃ¤ngigen
Fenster. In kleinen Viewports scrollt dieses Fenster bis zu allen Controls und
Aktionen; **X** fÃ¼hrt zurÃ¼ck zum Raster. **Strg-Klick** auf einen Slider erlaubt
die direkte Zahleneingabe. [MenÃ¼ und Keybinds](client-ui.md).

Der aktuelle Client verwendet **IPC v31**. Bereits laufendes Minecraft vollstÃ¤ndig
neu starten, bevor der aktualisierte Client erneut angehÃ¤ngt wird. Ein schon
geladener Agent behÃ¤lt sonst das alte gemeinsame Datenlayout.

## Chunkfinder: Deepslate, Kelp und Amethyst

**Blocks â†’ Chunkfinder** einschalten; Rechtsklick Ã¶ffnet alle Einstellungen.
Der frÃ¼here Base Finder ist jetzt Teil des Chunkfinders. Ein Chunk wird als
**Possible Base** markiert, sobald mindestens einer der getrennten Grenzwerte
fÃ¼r ausgewÃ¤hlte Hinweise erreicht ist:

- **Horizontal deepslate**: standardmÃ¤ÃŸig mindestens ein roher Deepslate-Block
  mit horizontaler X/Z-Achse. Erze, polierter und befallener Deepslate zÃ¤hlen nicht.
- **Tall kelp**: standardmÃ¤ÃŸig mindestens drei Pflanzen mit jeweils mindestens
  18 BlÃ¶cken HÃ¶he. Jede Spitze zÃ¤hlt einmal; die zusammenhÃ¤ngende SÃ¤ule wird
  abwÃ¤rts bis maximal 64 BlÃ¶cke gemessen, auch unterhalb des ScanwÃ¼rfels.
- **Large amethyst / clusters**: standardmÃ¤ÃŸig mindestens sechs groÃŸe Knospen
  oder vollstÃ¤ndig gewachsene Cluster. Kleine/mittlere Knospen und normale
  AmethystblÃ¶cke zÃ¤hlen nicht.

**Highlight whole chunks** und **Highlight individual blocks** sind standardmÃ¤ÃŸig
beide an und unabhÃ¤ngig einstellbar. Der Chunk erhÃ¤lt einen 3D-Rahmen um seinen
exakten 16 Ã— 16-Block-FuÃŸabdruck mit Linien alle 16 HÃ¶henblÃ¶cke. Die HÃ¶he reicht
in der Overworld von âˆ’64 bis 320, in Nether/End von 0 bis 256. Beobachtete Hinweise
auÃŸerhalb erweitern den Rahmen; eigene DimensionshÃ¶hen werden nicht ausgelesen.
Jeder verdÃ¤chtige Block im erkannten Chunk erhÃ¤lt zusÃ¤tzlich WÃ¼rfelkanten, bei
Kelp auch jeder aus der gemessenen HÃ¶he rekonstruierte StÃ¤ngelblock. Rahmen,
BlÃ¶cke und Labels sind durch WÃ¤nde sichtbar. Die Markierung beweist keine Base.

Scanradius (Standard 20, Bereich 4â€“512), Intervall (900 ms), Entfernung (64 m,
8â€“1024 m), Farbe, Labels und Tracer gelten gemeinsam fÃ¼r alle drei Hinweise.
Beim VergrÃ¶ÃŸern des Radius wÃ¤chst die Zeichenentfernung bei Bedarf mit; sie
kann danach unabhÃ¤ngig reduziert werden. 512 BlÃ¶cke entsprechen 32 Chunks Radius.
Der Scan arbeitet nahe Spalten zuerst und Ã¼berspringt HÃ¶hen auÃŸerhalb âˆ’64 bis
319 bereits beim Aufbauen des Scanbereichs. Ein groÃŸer Radius braucht deutlich
lÃ¤nger fÃ¼r einen vollstÃ¤ndigen Durchlauf. Die Possible-Base-Auswertung wartet
weiterhin auf einen erfolgreichen Abschluss; bekannte Treffer bleiben sichtbar.
Nur erfolgreich abgeschlossene Scans und lokal geladene BlÃ¶cke werden ausgewertet.
Es werden keine entfernten oder ungeladenen Chunks gefunden.

Die Session-Liste enthÃ¤lt bis zu 128 mÃ¶gliche Bases mit Chunk-Koordinaten,
Hinweiszahlen und Position. **Copy coordinates** kopiert die Position;
**Save waypoint** erhÃ¤lt sie Ã¼ber Neustarts. Die stÃ¤rkste Beobachtung pro Chunk
bleibt erhalten und kann veraltet sein. **Clear possible bases** leert die Liste
bis zum nÃ¤chsten Scan. Server-/Dimensionswechsel, Verbindungsabbruch, Panic und
Ã„nderungen an Filtern, Radius oder Mindestwerten setzen sie zurÃ¼ck.

Alle Optionen speichern automatisch und in benannten Configs. Ã„ltere Base-Finder-
Einstellungen werden beim Laden in den gemeinsamen Chunkfinder Ã¼bernommen.
Ein vorhandener zweiter Keybind bleibt als **Chunkfinder (alternate)** nutzbar;
beide Bindings steuern denselben Schalter. Base ESP bleibt eigenstÃ¤ndig.

## Block ESP: groÃŸer Scanbereich

**Blocks â†’ Block ESP** bietet ebenfalls **Scan radius** von 4 bis 512 BlÃ¶cken
(Standard 12). **Draw distance** und die gemeinsame Tracer-Entfernung reichen
von 8 bis 1024 Metern. Eine Radius-ErhÃ¶hung hebt die Zeichenentfernung bei Bedarf
auf die Diagonale des Scanbereichs an, damit entfernte Treffer sichtbar bleiben.
Der grÃ¶ÃŸere Bereich gilt im MenÃ¼, im Agenten und in benannten Configs; Ã¤ltere
Configs behalten ihre bisherigen gÃ¼ltigen Werte. Der Chunkfinder-Radius und
seine Entfernung speichern weiterhin auch automatisch.

Der gemeinsame Scanner benutzt weiterhin ein Zeitbudget von etwa 2 ms pro
Sampler-Durchlauf, prÃ¼ft es alle 32 Zellen und begrenzt Ergebnisse auf 1024
BlÃ¶cke. Ein einzelner Spielaufruf kann dieses Budget Ã¼berschreiten. Die
grÃ¶ÃŸeren Maximalwerte erhÃ¶hen nicht automatisch die bisherigen Standardradien.
Nahe Spalten und die SpielerhÃ¶he kommen zuerst; die Suche lÃ¤sst sich Ã¼ber viele
DurchlÃ¤ufe fortsetzen, ohne ein Array mit allen Scanpositionen anzulegen.
Scan-Fortschritt wird auch in den Block-ESP-Einstellungen angezeigt. Nur von
Minecraft geladene BlÃ¶cke stehen zur VerfÃ¼gung, unabhÃ¤ngig vom gewÃ¤hlten Radius.

## Storage ESP

**Blocks â†’ Storage ESP** Ã¶ffnen und **Enable Storage ESP** einschalten. Unter
**Block types and colours** lÃ¤sst sich jeder der 13 Typen einzeln auswÃ¤hlen:

| Typen | Auswahl und Farbe |
|---|---|
| Chests, Trapped chests, Ender chests | Jeweils eigener Filter und RGBA-Farbwert |
| Shulker boxes, Barrels, Hoppers | Jeweils eigener Filter und RGBA-Farbwert |
| Furnaces, Blast furnaces, Smokers | Jeweils eigener Filter und RGBA-Farbwert |
| Dispensers, Droppers, Crafters, Brewing stands | Jeweils eigener Filter und RGBA-Farbwert |

Die Typbezeichnung oder Checkbox schaltet den Filter um. Die FarbschaltflÃ¤che
rechts daneben Ã¶ffnet den nativen FarbwÃ¤hler einschlieÃŸlich Alpha. Farbige
Shulkerboxen gehÃ¶ren zum gemeinsamen Typ **Shulker boxes**. Die Typen sind
standardmÃ¤ÃŸig alle ausgewÃ¤hlt; Labels und Tracer sind zunÃ¤chst aus.
**Reset storage colours** stellt nur die 13 Standardfarben wieder her.

**Block names and distance** zeigt Typ und Entfernung. **Tracers** zieht eine
Linie vom unteren Bildschirmrand zum Block. **Scan radius** reicht von 4 bis
512 BlÃ¶cken; der Radius bleibt auch in gespeicherten Einstellungen und Configs
erhalten. **Opacity** multipliziert die Alpha-Komponente der jeweiligen Farbe,
**Glow** verÃ¤ndert den Effekt. Ein Alpha-Wert von null macht diesen Typ unsichtbar.
Ohne ausgewÃ¤hlten Typ erscheint ein Hinweis, einen Typ zu wÃ¤hlen.

Storage ESP zeichnet rÃ¤umliche WÃ¼rfelrahmen mit zwÃ¶lf projizierten Weltkanten.
Auch an der Kamera werden Kanten an der Near Plane und am Bildschirmrand
abgeschnitten. Die Rahmen bleiben durch WÃ¤nde sichtbar. Wenn zugleich das
allgemeine Block ESP lÃ¤uft, Ã¼berlÃ¤sst es ausgewÃ¤hlte Storage-Typen diesem
3D-Renderer, damit keine zusÃ¤tzliche flache Box darÃ¼berliegt.

| Einstellung | Standard | Bereich |
|---|---:|---:|
| Scan interval | 1000 ms | 200â€“3000 ms |
| Scan pulse | an | an / aus |
| Pulse cycle | 1500 ms | 500â€“6000 ms |

**Scan interval** ist die Pause nach einem Scan, nicht dessen garantierte
Gesamtdauer. Storage ESP hat einen eigenen Suchlauf und wird nicht vom Radius
oder Intervall anderer Blockmodule aufgehalten. Der schnelle Pfad lÃ¤uft durch
die bereits geladenen Chunks und prÃ¼ft ausschlieÃŸlich deren Block-Entity-
Positionen, statt jeden Luft-/Steinblock des ScanwÃ¼rfels abzufragen. Auch groÃŸe
Radien erzeugen dadurch keine kubisch wachsende Zahl von Blockabfragen.
Ein neuer Lauf beginnt sofort nach dem Einschalten; nach Abschluss gilt die
eingestellte Pause. Der bestehende visuelle Puls hat seine eigene Geschwindigkeit.
Der Agent behÃ¤lt bekannte Treffer wÃ¤hrend des nÃ¤chsten Scans und ergÃ¤nzt neue
Fundstellen schrittweise. Erst ein erfolgreich abgeschlossener Scan ersetzt
die alte Liste; verschwundene Container blenden im Overlay weich aus.
Ein fehlgeschlagener Teilscan lÃ¶scht die bekannte Liste nicht.

Der Status zeigt den eigenen Fortschritt und die Zahl passender Container.
Falls verÃ¤nderte Spielklassen den direkten Zugriff verhindern, verwendet der
Client einen unabhÃ¤ngigen **Compatibility block scan**. Beide Pfade haben ein
1-ms-Schrittbudget; einzelne Java-Aufrufe kÃ¶nnen es Ã¼berschreiten. Die Suche
behÃ¤lt maximal 1024 Treffer und eine Chunk-SchlÃ¼sselliste gleichzeitig, ohne
ungezÃ¤hlte Scanpositionen oder zusÃ¤tzliche Spielmodelle anzulegen. UnterstÃ¼tzte
11 Laufzeitprofile wurden aus den lokal geprÃ¼ften offiziellen Klassen abgeleitet.
Nur bereits geladene Container kÃ¶nnen gefunden werden; Radius 512 lÃ¤dt keine
entfernten Chunks nach. Der Standardradius bleibt 24 BlÃ¶cke.

Neue Container blenden versetzt ein. **Scan pulse** lÃ¤sst einen helleren Streifen
nacheinander Ã¼ber ihre 3D-Rahmen wandern, wÃ¤hrend alle bekannten Rahmen sichtbar
bleiben. **Pulse cycle** steuert diesen visuellen Umlauf unabhÃ¤ngig von der
eigentlichen Blocksuche. Bei vielen Containern Ã¼berlappen benachbarte Pulse.
**Settings â†’ Overlay â†’ Reduced motion** zeigt sofort ruhige Rahmen ohne Puls
oder Ein-/Ausblendung. Farben, Deckkraft, Glow, Labels und Tracer gelten weiter.
Beide Zeitwerte und der Puls-Schalter speichern automatisch und in benannten
Configs. Ã„ltere vollstÃ¤ndige Dateien erhalten die genannten Defaults, ohne
ihre Farben und Filter zu verlieren.

## Stash Finder

**Blocks â†’ Stash Finder** Ã¶ffnen und **Enable Stash Finder** einschalten. Der
Finder gruppiert ausgewÃ¤hlte Container-BlÃ¶cke nach ihrem 16 Ã— 16 Block groÃŸen
Chunk in X/Z und nimmt einen Chunk auf, wenn die beobachtete Anzahl mindestens
**Minimum containers / chunk** erreicht.

| Einstellung | Standard | Bereich |
|---|---:|---:|
| Scan radius | 32 BlÃ¶cke | 4â€“48 |
| Scan interval | 1000 ms | 200â€“3000 ms |
| Minimum containers / chunk | 8 | 2â€“128 |
| Marker distance | 128 m | 8â€“512 m |

StandardmÃ¤ÃŸig zÃ¤hlen **Chests**, **Trapped chests**, **Shulker boxes** und
**Barrels**. Unter **Count these containers** lassen sich alle 13 Storage-Typen
als Filter wÃ¤hlen; Ender chests sind standardmÃ¤ÃŸig ausgeschlossen. Ein Double
Chest besteht aus zwei Blockpositionen und zÃ¤hlt deshalb als **zwei**. Es zÃ¤hlen
BlÃ¶cke, nicht InventarplÃ¤tze oder enthaltene Items. Ein Scan kann nur einen Teil
eines Chunks erfassen; seine Zahl ist keine vollstÃ¤ndige Chunk-Inventur.

Nur erfolgreiche beendete Scans werden ausgewertet, jeweils einmal. Laufende
oder fehlgeschlagene Scans erzeugen keine Entdeckung. Nutzen andere Blockmodule
einen grÃ¶ÃŸeren Scan, zÃ¤hlt Stash Finder nur den Ausschnitt innerhalb seines
eigenen Radius. **Labels**, **Tracers** und **Marker colour** gestalten den Marker.
**Notify on new stash** meldet neue Chunks; dafÃ¼r mÃ¼ssen HUD und
**HUD â†’ Notifications** sichtbar sein.

**Discoveries this session** zeigt Chunk, beobachtete Anzahl und Koordinaten.
Die Liste behÃ¤lt hÃ¶chstens **128** Entdeckungen, auch nach dem Verlassen des
Scanbereichs. FÃ¼r einen bekannten Chunk bleibt die hÃ¶chste beobachtete Anzahl
erhalten; spÃ¤tere kleinere Beobachtungen reduzieren sie nicht. Die Angaben kÃ¶nnen
deshalb veraltet sein, etwa nach dem Entfernen von Containern.

- **Copy coordinates** kopiert eine Entdeckung; **Copy all coordinates** kopiert
  die ganze Liste einschlieÃŸlich ihrer Anzahlen.
- **Save waypoint** speichert die Position der beobachteten Containergruppe als
  `Stash <Chunk X>, <Chunk Z>` und aktiviert Waypoints. Der Wegpunkt bleibt im
  bestehenden server-/dimensionsgebundenen Store nach einem Neustart erhalten.
  Ist der Wegpunkt-Speicher voll, zuerst einen bestehenden Wegpunkt entfernen.
- **Clear discoveries** leert die Session-Liste. Erst ein neuer beendeter Scan
  kann sie wieder fÃ¼llen. Bei 128 EintrÃ¤gen Koordinaten kopieren und die Liste
  leeren, um weitere Chunks aufzunehmen.

Server- oder Dimensionswechsel, eine unterbrochene Agent-Verbindung und ein
Neustart beenden die bisherige Liste. Ã„nderungen an Typfiltern oder Mindestanzahl
leeren sie ebenfalls. Entdeckungen selbst werden nicht in Configs gespeichert.

## Spawner ESP

**Blocks â†’ Spawner ESP** Ã¶ffnen und **Enable Spawner ESP** einschalten. Das Modul
markiert gescannte Mob-Spawner-BlÃ¶cke mit eigener RGBA-Farbe, **Labels**,
**Tracers** und **Glow**. Labels sind standardmÃ¤ÃŸig an, Tracer aus.
Scan-Radius ist anfangs 24 BlÃ¶cke, Scan-Intervall 1000 ms und Draw distance 64 m.
Radius erlaubt 4â€“48 BlÃ¶cke, Intervall 200â€“3000 ms, Draw distance 8â€“128 m und
Glow 0â€“2. Mobtyp und Aktivierungszustand des Spawners werden nicht erfasst.

## Base ESP

**Blocks â†’ Base ESP** einschalten; Rechtsklick Ã¶ffnet die Einstellungen.
**Display â†’ Minecraft textures** ist der Standard fÃ¼r Minecraft **1.21.11**
(Mojmap, Vanilla und Fabric Intermediary). Geladene BlÃ¶cke im erkannten Bereich
werden mit den echten Blockmodellen, Textur-Atlanten, FarbtÃ¶nungen und der Lightmap
des laufenden Spiels durch verdeckende WeltblÃ¶cke gezeichnet. Die Base-BlÃ¶cke
verdecken sich untereinander weiterhin. **Texture opacity** stellt die
Transparenz ein; standardmÃ¤ÃŸig sind die Texturen vollstÃ¤ndig sichtbar.

Base ESP funktioniert auch bei ausgeschaltetem Stash Finder. Beide verwenden
denselben Scan-Radius, dieselben Container-Filter und dieselbe Mindestanzahl
pro Chunk (standardmÃ¤ÃŸig acht). Die Darstellung umfasst beobachtete LagerblÃ¶cke
plus **Expand detected area**, standardmÃ¤ÃŸig vier zusÃ¤tzliche BlÃ¶cke auf allen
Seiten. Die Erkennung folgt LagerblÃ¶cken und ihrem zusÃ¤tzlichen Rand; sie bestimmt
nicht automatisch die AuÃŸengrenzen eines ganzen GebÃ¤udes. Innerhalb dieses
Bereichs verwendet der Texturmodus die tatsÃ¤chliche geladene Blockgeometrie.

**Display â†’ Area marker** bietet weiterhin die farbige FlÃ¤che mit Rahmen,
**Base colour**, **Fill opacity** und **Frame width**. Der Modus ist auch auf
anderen unterstÃ¼tzten Minecraft-Versionen verfÃ¼gbar. Labels und Tracer sind
optional. StandardmÃ¤ÃŸig betrÃ¤gt die Anzeigeentfernung 192 m. Die Anzeigeentfernung
erweitert den Scan-Radius nicht. Aussehen und Modul-Bind werden in automatischen
Einstellungen und benannten Configs gespeichert. Bestehende vollstÃ¤ndige Configs
ohne Base ESP erhalten dessen ausgeschaltete Defaults und einen ungebundenen Key.
VollstÃ¤ndige Ã¤ltere Base-Configs erhalten den neuen Texturmodus mit Deckkraft 1;
teilweise beschÃ¤digte neue Texturfelder werden bei benannten Configs abgelehnt.

Der Texturmodus bearbeitet hÃ¶chstens acht nahe Bereiche, 16.384 Zellen und
786.432 Dreiecksvertices pro Aufbau. Die Erfassung lÃ¤uft in Abschnitten mit
einem Zielbudget von 2 ms auf Minecrafts Render-Thread; mindestens eine Zelle
wird pro Abschnitt geprÃ¼ft. Ein einzelner aufwendiger Block kann dieses Budget
Ã¼berschreiten. Nach einem fertigen Aufbau wird etwa jede Sekunde neu erfasst.
Animierte Atlas-Texturen bleiben live. Ãœberlappende Bereiche
werden ohne doppelte Blockgeometrie erfasst. Die Anfrage lÃ¤uft nach 750 ms ohne
Overlay-Erneuerung ab und ist an Server und Dimension gebunden.

Normale Blockmodelle, FlÃ¼ssigkeiten und spritebasierte Block-Entity-Modelle
(etwa Kisten und Shulker) werden Ã¼ber die Spielrenderer erfasst. Spezielle
Zusatzdarstellungen wie Text, Portale und manche Mod-Renderer werden Ã¼bersprungen
und im Status gemeldet. Der Status zeigt auÃŸerdem Aufbau, leere Bereiche,
Grenzen oder Fehler. Bei einem Fehler Base ESP aus- und wieder einschalten,
um erneut aufzubauen, oder **Area marker** wÃ¤hlen. Auf anderen Versionen und
bei fehlendem Texturrenderer zeigt das MenÃ¼ die passende Version-/Neustart-
Empfehlung; **Area marker** bleibt die alternative Darstellung.

## Scan-Status und Grenzen

Scans laufen im Agent-Worker und lesen ausschlieÃŸlich Blockdaten, die der Client
in seiner Welt bereitstellt. Die Scanform ist ein WÃ¼rfel mit dem gewÃ¤hlten Radius
um die Spielerposition beim Scanbeginn. Der aktuelle Scanner berÃ¼cksichtigt
Y-Koordinaten von âˆ’64 bis 319. Er deckt keine ungeladenen Chunks auf und liest
keine Container-Inhalte. Die externe Scan-Darstellung verarbeitet nur die
verÃ¶ffentlichten Snapshots; der Texturmodus hat den oben beschriebenen eigenen
Callback im Spielrenderer.

Block ESP, Chunkfinder, Beacon radius, Stash Finder, Base ESP und Spawner ESP
teilen einen Scanner: ihre Typfilter werden zusammengefasst,
der grÃ¶ÃŸte Radius und das kÃ¼rzeste Intervall werden angefordert. Die einzelnen
Module behalten ihre eigenen Filter und Anzeigegrenzen. Ein Scan kann mehrere
Worker-Schritte dauern; das eingestellte Intervall garantiert keine fertige
Auswertung in diesem Abstand.
Storage ESP und Hole ESP haben eigene begrenzte SuchlÃ¤ufe und Ergebnislisten.

Der Status unter den Einstellungen zeigt Verbindung/Weltbeitritt, Fortschritt,
Abschluss oder fehlende Runtime-Bindings. Ohne Verbindung Minecraft auswÃ¤hlen und
eine Welt betreten. Bei **Block scanning is unavailable** unter
**Settings â†’ Agent** die erkannte Version und Diagnose prÃ¼fen.

Ein Resultat des gemeinsamen Scanners enthÃ¤lt hÃ¶chstens **1024 passende BlÃ¶cke**
fÃ¼r seine aktiven Module zusammen. Storage ESP hat unabhÃ¤ngig davon hÃ¶chstens
1024 Container-Ergebnisse. Bei Erreichen des gemeinsamen Limits zeigt der Client:
**Counts are lower bounds**. Auch ein so begrenzter beendeter Scan kann einen
Stash finden, aber seine Anzahl kann zu niedrig sein. Radius verkleinern oder
weniger Typen auswÃ¤hlen. **Marker distance** und **Draw distance** erweitern
nur die Anzeige, nicht den verfÃ¼gbaren Scanbereich.

## Speicherung und Bindings

Alle SMP-Einstellungen, Farben, Filter und Modul-Binds speichern automatisch in
`client-settings.ini` neben `Keim.exe` und in benannten Profilen unter
`configs/<Name>.ini`. Storage ESP nutzt seinen vorhandenen Bind; Stash Finder und
Spawner ESP ergÃ¤nzen neue Aktionen. Alle Modul-Binds beginnen ungebunden und
lassen sich unter **Settings â†’ Keybinds** oder direkt am Aktivierungsschalter
mit **Toggle/Hold** setzen.

VollstÃ¤ndige Ã¤ltere benannte Configs ohne den gesamten SMP-Block erhalten die
SMP-Defaults und ungebundene neue Aktionen. Stash Finder und Spawner ESP bleiben
dabei aus. Ein teilweise vorhandener oder ungÃ¼ltiger SMP-Block lÃ¤sst ein benanntes
Profil vollstÃ¤ndig scheitern; die aktuellen Einstellungen bleiben unverÃ¤ndert.
Auch automatische Einstellungen wenden einen beschÃ¤digten SMP-Block nicht
teilweise an. FÃ¼r dauerhafte Fundorte **Save waypoint** verwenden und beim
Umzug `client-settings.ini` und den `configs`-Ordner mitnehmen.

## PrÃ¼fbelege

FÃ¼r die aktuelle Base-Texturerweiterung bestanden alle **26** verfÃ¼gbaren
RegressionsprÃ¼fungen Ã¼ber die abschlieÃŸenden LÃ¤ufe: 25 in der Sandbox und
`frame_attach` separat auÃŸerhalb nach einem Windows-Pipe-Zugriffsfehler seines
isolierten Fixture-JVMs. Nach der letzten Ressourcenfreigabe-Ã„nderung bestanden
`base_texture_gl`, `base_texture_models`, `smp_features` und `portable_client`
erneut; `depth_hook` bestand im vorangehenden gezielten Lauf. Die Produktions-
EXE und Share-ZIP wurden neu gebaut.

`base_texture_gl` verwendet echtes WGL/OpenGL und prÃ¼ft Pixel sowie erhaltenen
Grafikzustand: Atlas-UVs, Kameraprojektion, Verdeckung zwischen Base-BlÃ¶cken,
Durch-Wand-Compositing, unverÃ¤nderte Pixel auÃŸerhalb der Base, live aktualisierte
Atlas-Texturen, Deckkraft 0,5, das Mischen transparenter Texel und erneute
Renderer-Initialisierung nach Ressourcenfreigabe. Ausschalten gibt die GL-
Ressourcen frei, sodass Aus-/Einschalten auch einen fehlgeschlagenen Renderer
neu erstellen kann.
`base_texture_models` prÃ¼ft Java-Proxy-Defaults, Modell-/Sprite-Pfade und
Quad-Erfassung mit UVs, Tint und Lichtwerten. `depth_hook` deckt beide
Render-Callbacks ab; `smp_features` prÃ¼ft Texturmodus, Profilmigration,
Anfrage-Lease und Welttrennung. Die 38 Klassen und 73 Member des 1.21.11-
Texturbridges wurden gegen zwischengespeicherte offizielle Klassen fÃ¼r Mojmap,
Vanilla und Fabric Intermediary geprÃ¼ft.

Die fÃ¼nf endgÃ¼ltigen Settings-Belege verwenden native ImGui/D3D11-WARP-
Fixtures mit synthetischen Daten:

| Zustand | Beleg |
|---|---|
| Desktop, 1280 Ã— 800 | [Base-Einstellungen](../.impeccable/review/base-textures/base-esp-settings-desktop-verified.png) |
| Kleines Fenster oben, 480 Ã— 360 | [Textur-Controls](../.impeccable/review/base-textures/base-esp-settings-small-top-verified.png) |
| Kleines Fenster gescrollt, 480 Ã— 360 | [Erkennung und Grenzen](../.impeccable/review/base-textures/base-esp-settings-small-verified.png) |
| Aufbau, 480 Ã— 360 | [Ladestatus](../.impeccable/review/base-textures/base-textures-loading-small-verified.png) |
| Fehler, 480 Ã— 360 | [Fehler und Wiederherstellung](../.impeccable/review/base-textures/base-textures-error-small-verified.png) |

Die unabhÃ¤ngige AbschlussprÃ¼fung bestÃ¤tigte **ship fÃ¼r den UI-Umfang**.
Es gibt keine Live-Minecraft-Aufnahme oder PrÃ¼fung des Texturmodus im laufenden
Spiel. Shader- und Mod-Grafik-KompatibilitÃ¤t bleibt ungeprÃ¼ft.

Die frÃ¼heren fÃ¼nf Base-PrÃ¼fungen `smp_features`, `depth_rendering`,
`control_panel`, `client_controls` und `portable_client` sowie die fÃ¼nf
`-final.png`-Bilder unter `.impeccable/review/base-esp/` betreffen die farbige
FlÃ¤che und den Rahmen des **Area marker**. Der damalige D3D11-WARP-Pixeltest
bestÃ¤tigt diese FlÃ¤che hinter Tiefenwerten und ohne Tiefendaten; er prÃ¼ft keine
Minecraft-Modelle oder Texturen. Diese historischen Belege bleiben erhalten.

FÃ¼r die ursprÃ¼ngliche Erweiterung mit Storage ESP, Stash Finder und Spawner ESP
bestanden alle **24** verfÃ¼gbaren RegressionsprÃ¼fungen Ã¼ber die abschlieÃŸenden
LÃ¤ufe. Der isolierte `frame_attach`-Test bestand nach einem Windows-Pipe-
Zugriffsfehler auÃŸerhalb der Sandbox mit seinem eigenen Fixture-JVM.
`smp_features` prÃ¼ft Klassifikation, beendete Scans, Filter, negative Chunks,
Welt-/Session-Trennung, hÃ¶chste Anzahlen, Limits, Profilmigration und Rendering
mit unterschiedlichen Typfarben. `control_panel` prÃ¼ft echte Typbezeichnungs-
Klicks, den FarbwÃ¤hler, erreichbare Settings und automatische Speicherung.
`portable_client` bestand; Produktions-EXE und Share-ZIP wurden neu gebaut.

Die sechs aufbewahrten Bilder verwenden den echten nativen ImGui/D3D11-WARP-
Renderer mit synthetischen Fixtures bei 1280 Ã— 800 und 480 Ã— 360:

| Einstellungen | Desktop | Klein |
|---|---|---|
| Storage ESP | [Farben](../.impeccable/review/smp/smp-storage-colours-desktop.png) | [Farben](../.impeccable/review/smp/smp-storage-colours-small.png) |
| Stash Finder | [Entdeckungen](../.impeccable/review/smp/smp-stash-discoveries-desktop.png) | [Entdeckungen](../.impeccable/review/smp/smp-stash-discoveries-small.png) |
| Spawner ESP | [Settings](../.impeccable/review/smp/smp-spawner-desktop.png) | [Settings](../.impeccable/review/smp/smp-spawner-small.png) |

Die unabhÃ¤ngige AbschlussprÃ¼fung Ã¶ffnete alle sechs nach dem Typbezeichnungs-
Fix erneut erzeugten Bilder und bestÃ¤tigte **ship**, ohne sichtbare Regression.
Diese Belege prÃ¼fen Settings und synthetische Darstellung. FÃ¼r SMP wurde kein
Live-Minecraft-Test durchgefÃ¼hrt; Scanverhalten, Optik und Attach im laufenden
Spiel sowie alle GPU-Kombinationen sind dadurch nicht bestÃ¤tigt.

## Base Finder: PrÃ¼fung

FÃ¼r Base Finder bestanden alle **27** RegressionsprÃ¼fungen Ã¼ber die abschlieÃŸenden
LÃ¤ufe. `runtime_profiles` prÃ¼ft Kelp-HÃ¶hen auch unterhalb des ScanwÃ¼rfels, genau
eine Meldung pro Pflanzenspitze und beide Amethyst-Typen Ã¼ber die vorhandenen
JNI-Profile. `smp_features` prÃ¼ft unabhÃ¤ngige Mindestwerte, kurze Pflanzen,
negative Chunks, vollstÃ¤ndige Scans, Filter, Session-Trennung, Config-Migration
und Marker durch WÃ¤nde. `control_panel` prÃ¼ft erreichbare Controls und Aktionen,
die feste Kopfzeile bei kleiner FenstergrÃ¶ÃŸe, Preferences und Panic.
Der isolierte `frame_attach`-Test bestand auÃŸerhalb der Sandbox nach deren
Zugriffsfehler auf eine lokale Java-Pipe. Client und Share-ZIP wurden neu gebaut.

Die sechs nativen synthetischen Screenshots und die unabhÃ¤ngige UI-PrÃ¼fung
mit Ergebnis **ship** liegen unter `.impeccable/review/base-finder/`.
Sie zeigen Einstellungen, leere Liste und Entdeckungen bei 1280 Ã— 800 und
480 Ã— 360. Ein Live-Minecraft-Test wurde nicht durchgefÃ¼hrt.

## Storage ESP 3D: PrÃ¼fung

FÃ¼r die 3D-/Puls-Erweiterung bestanden alle **30** aktuellen PrÃ¼fungen: 29 in
der Sandbox (93,40 s), der isolierte `frame_attach` separat auÃŸerhalb (0,45 s).
`smp_features` prÃ¼ft die exakten zwÃ¶lf projizierten WÃ¼rfelkanten einschlieÃŸlich
Near-Plane-FÃ¤llen, stabile Treffer wÃ¤hrend Teilscans, erfolgreiche leere Scans,
Positionswechsel, KapazitÃ¤tsgrenzen, versetztes Einblenden, Puls-Geschwindigkeit,
weiches Entfernen, reduzierte Bewegung und Session-Trennung. Benannte Configs
und automatische Preferences prÃ¼fen neue Werte und die Migration alter Dateien.
Der gemeinsame Scanner behÃ¤lt das Zeitbudget und maximal 1024 Treffer; die
Cluster-Detektoren werten weiterhin nur abgeschlossene erfolgreiche Scans aus.
Block ESP und Chunkfinder erlauben inzwischen bis zu 512 BlÃ¶cke Radius.

Die vier nativen Settings-/Weltbelege mit synthetischen Snapshots liegen unter
[Storage-3D-PrÃ¼fung](../.impeccable/review/storage-3d/review.md). Sie zeigen den
tatsÃ¤chlichen Renderer bei 1280 Ã— 800 und 480 Ã— 360. Client-EXE und Share-ZIP
wurden neu gebaut und mit `portable_client` geprÃ¼ft. Es gab keinen Test im
laufenden Minecraft und keine Messung von FPS oder RAM-Verbrauch.
