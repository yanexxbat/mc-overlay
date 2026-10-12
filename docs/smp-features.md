# SMP: Storage ESP, Stash Finder, Spawner ESP und Base ESP

Die vier Module stehen unter **Blocks** im bestehenden kompakten Client-Menü.
**Linksklick** oder **Leertaste** schaltet ein Modul um. **Rechtsklick**,
**Menü-Taste** oder **Shift+F10** öffnet seine Einstellungen im unabhängigen
Fenster. In kleinen Viewports scrollt dieses Fenster bis zu allen Controls und
Aktionen; **X** führt zurück zum Raster. **Strg-Klick** auf einen Slider erlaubt
die direkte Zahleneingabe. [Menü und Keybinds](client-ui.md).

Die Erweiterung verwendet **IPC v21**. Bereits laufendes Minecraft vollständig
neu starten, bevor der aktualisierte Client erneut angehängt wird. Ein schon
geladener Agent behält sonst das alte gemeinsame Datenlayout.

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
Erkannte Containeransammlungen erscheinen als farbige, transparente 3D-Flächen
mit Rahmen durch Wände und andere Blöcke. Die globale Wandverdeckung anderer
Welteffekte beeinflusst Base ESP nicht.

Base ESP funktioniert auch bei ausgeschaltetem Stash Finder. Beide verwenden
denselben Scan-Radius, dieselben Container-Filter und dieselbe Mindestanzahl
pro Chunk (standardmäßig acht). Die Darstellung umfasst beobachtete Lagerblöcke
plus **Expand detected area**, standardmäßig vier zusätzliche Blöcke auf allen
Seiten. Dieser Bereich schätzt einen Base-Standort; er rekonstruiert keine
Gebäudegeometrie. Es gelten dieselben Grenzen für geladene Blöcke und Fundlisten.

**Base colour**, **Fill opacity** und **Frame width** ändern Farbe, Transparenz
und Rahmen. Labels und Tracer sind optional. Standardmäßig beträgt die
Anzeigeentfernung 192 m und die Flächentransparenz 0,12. Die Anzeigeentfernung
erweitert den Scan-Radius nicht. Aussehen und Modul-Bind werden in automatischen
Einstellungen und benannten Configs gespeichert. Bestehende vollständige Configs
ohne Base ESP erhalten dessen ausgeschaltete Defaults und einen ungebundenen Key.

## Scan-Status und Grenzen

Scans laufen im Agent-Worker und lesen ausschließlich Blockdaten, die der Client
in seiner Welt bereitstellt. Die Scanform ist ein Würfel mit dem gewählten Radius
um die Spielerposition beim Scanbeginn. Der aktuelle Scanner berücksichtigt
Y-Koordinaten von −64 bis 319. Er deckt keine ungeladenen Chunks auf und liest
keine Container-Inhalte. Rendering verarbeitet nur die veröffentlichten Snapshots.

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

Alle **24** verfügbaren Regressionsprüfungen bestanden über die abschließenden
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
