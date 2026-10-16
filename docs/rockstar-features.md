# Rockstar-inspiriertes Feature-Paket

Referenz: [Rockstar-Client-src-deobf / smirnov](https://github.com/serbioZ/Rockstar-Client-src-deobf/tree/f9ae03e4fe85bb8214527b2eeb2ea07548eadd98/smirnov),
Commit `f9ae03e4fe85bb8214527b2eeb2ea07548eadd98`. Die 122 Java-Moduldateien wurden
als Verhaltensreferenz gelesen. Dieses Paket implementiert die passenden Effekte
und Bedienfunktionen eigenständig in C++20; es lädt keinen Code des fremden Clients.
Die vollständige Zuordnung steht in [rockstar-catalog.md](rockstar-catalog.md).

## Start und Bedienung

1. Minecraft vollständig beenden und neu starten, falls die bisherige DLL noch geladen ist.
   Die Scanner-Typen erfordern jetzt IPC-Version 18. Ein bereits geladener alter Agent
   lässt sich nicht durch das erneute Öffnen des Clients austauschen.
2. `build/client/Keim.exe` öffnen, Minecraft auswählen, **Client starten**.
3. Mit **INSERT** das Menü öffnen. Die neuen Funktionen sind zunächst ausgeschaltet
   und ihre Keybinds ungebunden. Im jeweiligen Tab **Enabled** aktivieren.
4. **Settings → Keybinds** oder das Bind-Feld der Funktion benutzen. Toggle schaltet
   einmal pro Druck, Hold aktiviert bis zum Loslassen. Tastatur, Maus und
   Ctrl/Shift/Alt-Kombinationen werden unterstützt.
5. HUD einschalten und **F6** drücken, um Widgets einzeln zu verschieben, zu skalieren
   und ein- oder auszublenden. Effekte besitzen eigene Farb-, Glow- und Größenregler.

## 29 neue Funktionen

20 konfigurierbare Module, acht zusätzliche eigenständige HUD-Widgets und Panic.
Notifications und Detect players besitzen jeweils auch ein HUD-Element; diese
werden in der Zahl 29 nicht doppelt gezählt.

| Menü | Funktion | Verhalten / relevante Regler |
|---|---|---|
| Visuals | Jump circles | Drei sanft drehende Nebel-/Lichtband-Schichten mit Lichtstaub an den Füßen. Weiches Wachstum und Ausblenden bei beobachteter Aufwärtsbewegung. Größe, Lebensdauer, Animation speed, Glow, Farben. |
| Visuals | Wings | Gefüllte, weich auslaufende Lichtfedern an den Schultern. Größe und Flügelschlag; eigener Spieler in F5, höchstens 32 weitere Spieler innerhalb 64 m. |
| Visuals | Halo / Nimb | Leuchtender Ring über Spieler-Köpfen; eigener Halo in F5. Größe, Glow, Farben. |
| Visuals | Glow ESP | Abgerundete Hitbox-Umrisse mit weichen Schichten und Farbverlauf. Spieler oder alle lebenden Entitäten; Freunde grün. Distanz und Linienstärke. |
| Visuals | Damage markers | Aufsteigende Zahlen bei beobachtetem HP-Verlust. Textgröße, Aufstieg, Lebensdauer und Farben. |
| Visuals | Damage vignette | Rot ausblendende Bildschirmränder bei eigenem HP-Verlust. Randgröße, Deckkraft und Lebensdauer. |
| Visuals | Target circle | Bodenring oder wandernder Scanring am aktuellen Combat-/Fadenkreuz-Ziel. Radius und Scan-Geschwindigkeit. |
| Visuals | Crosshair | Kreuz, Kreis, Punkt oder bewegungsabhängiger Abstand. Größe, Abstand, Strichbreite und Glow. |
| Visuals | Offscreen arrows | Richtungsmarker mit Distanz für Spieler außerhalb des Bildes, auch hinter der Kamera. Reichweite, Größe und Glow. |
| Combat | Hit sounds | Drei Tonhöhen bei beobachtetem HP-Verlust des aktuellen Ziels. Lautstärke; begrenzte Wiedergaberate. |
| Blocks | Storage ESP | Umrisse für Truhen, Shulker-Boxen, Fässer, Hopper und Öfen aus dem bestehenden Scanner. Scan-Distanz 4–48 m, Glow und Farben. |
| Blocks | Beacon radius | Radius-Hilfe und vertikale Linie an gescannten Beacons. Scan-Distanz 4–48 m, gewählter Radius 10–80 m. |
| Blocks | Chunk borders | Grenzen und 16 m hohe Hilfslinien des aktuellen Chunks und bis zu drei Nachbar-Chunks pro Richtung; korrekt bei negativen Koordinaten. |
| Misc | Name protect | Ersetzt Spielernamen im externen ESP, HUD und in der Statistik durch einen Alias. |
| Misc | Module sounds | Kurze Aktivierungs-/Deaktivierungstöne für Moduländerungen durch Menü oder Keybind. |
| Misc | Middle click friend | Mittelklick auf den Spieler im Fadenkreuz fügt seine UUID zu Friends hinzu oder entfernt sie. Friend-Hat und Combat-Filter verwenden dieselbe Liste. |
| Misc | Death coordinates | Speichert den letzten beobachteten Todesort und zeigt ihn in passendem Server und Dimension. Koordinaten kopieren oder Punkt entfernen. |
| Misc | Detect players | Liste naher Spieler und Nachricht beim ersten Erkennen eines neuen Spielers im Bereich. Reichweite 8–128 m. |
| Misc | Notifications | Verschiebbares HUD mit den letzten drei kurzlebigen Modul-, Freund-, Nähe- und Todesmeldungen. |
| Misc | Waypoints | Bis zu 16 gespeicherte Positionen einschließlich Todesort, mit Name, Entfernung, Server und Dimension. Speichern, löschen und Koordinaten kopieren. |
| HUD | Radar | Spielerpositionen relativ zur Kamerarichtung, Freunde grün. Nutzt die Reichweite von Detect players. |
| HUD | Keystrokes | WASD, Space, linke und rechte Maustaste; aktuelle echte Eingaben. |
| HUD | Speedometer | Horizontale Geschwindigkeit in m/s und km/h sowie vertikale Geschwindigkeit. Teleports werden ausgeschlossen. |
| HUD | Session statistics | Zeit im aktuellen Weltabschnitt, Wegstrecke, erhaltener/beobachteter Schaden und beobachtete Todesfälle des aktuellen Ziels. |
| HUD | Armor strip | Vier Rüstungsslots mit prozentualer Haltbarkeit und Balken. Die bestehende Equipment-Anzeige behält ihre Item-Icons. |
| HUD | Combat indicators | Eigene HP und HP des Fadenkreuz-Ziels mit Vergleichsbalken. |
| HUD | Clock | Lokale Windows-Uhrzeit. |
| HUD | Click rate | Linke und rechte CPS über ein gleitendes Ein-Sekunden-Fenster, ohne gehaltene Buttons mehrfach zu zählen. |
| Misc | Panic | Schaltet Combat, Macros, Module und HUD ab, unterdrückt auch automatische Friend-Hats und leert kurzlebige Effekte. Freunde und gespeicherte Punkte bleiben erhalten. Eigener Keybind. |

## Speicherung und Grenzen

- Keybinds, alle 19 HUD-Platzierungen, Sichtbarkeit, neue Modulparameter und Wegpunkte
  werden atomar in `client-settings.ini` neben der Client-EXE gespeichert. Alte
  Bind-/HUD-Indizes bleiben gültig; direkt gestartete Entwickler-Binaries verwenden
  ihre bisherige lokale Einstellungsdatei. Bestehende Features und UUID-Freunde
  behalten ihr bisheriges automatisches Speicherverhalten. Vollständige benannte
  Profile unter **Settings > Configs** liegen in `configs/<Name>.ini` neben dem
  Client und enthalten auch UUID-Freunde sowie die gespeicherten Modulzustände.
- Seiten-Reset betrifft die Funktionen der jeweiligen Seite. Panic beendet auch
  vorher aktivierte Hold-Module: Loslassen der Taste aktiviert sie nicht erneut.
  Bewusstes Aktivieren eines Moduls setzt die Panic-Sperre zurück.
- Schaden, Todesfälle und Sprünge werden aus Snapshots abgeleitet. Sie weisen
  keinen Angreifer nach; Treppen und Knockback können Sprungringe auslösen. Die
  Session-Anzeige zählt deshalb **beobachtete Ziel-Todesfälle**, keine sicher
  zugeordneten eigenen Kills. Session-Werte beginnen bei Welt-/Serverwechsel neu.
- Welt-Effekte können unter 1.21.11 Fabric / normalem OpenGL mit dem neuen
  [Tiefenvergleich](world-depth.md) verdeckt werden. ESP bleibt unabhängig davon
  sichtbar. Crosshair ergänzt das Vanilla-Fadenkreuz,
  Name protect verändert Minecrafts eigene Beschriftungen und Combat-Menütexte nicht.
- Beacon radius ist ein manuell gewählter Radius-Hinweis; Pyramidenstufe und
  tatsächlich vergebene Beacon-Effekte werden nicht erfasst.
- Storage ESP zeigt geladene Blöcke. Der gemeinsame Scanner arbeitet fortlaufend
  mit seinem bisherigen 2-ms-Budget und maximal 1024 Treffern; große Radien füllen
  sich schrittweise. Lager-/Beacon-Typen werden nur bei aktivem Modul angefordert.
- Detect players verwendet die vorhandene Entity-Liste (maximal 256 Datensätze).
  Es erkennt keinen weit entfernten, nicht geladenen Spieler oder versteckten Staff.
- CPS zählt native Maustasten-Flanken bei Spielfokus, keine automatischen
  Combat-Attacken. Menü, HUD-Editor, Bind-Capture und Fokusverlust pausieren die Eingabe.

## Architektur und Prüfung

`rockstar_features.hpp/.cpp` hält begrenzte Arrays für 256 HP-Beobachtungen,
96 Schadensanzeigen, 32 Sprungringe, acht Nachrichten und 16 Wegpunkte. Weltgeometrie,
Bildschirmgeometrie und HUD-Zeichnung sind getrennt. Alle Welt-/Screen-Effekte nutzen
dieselbe Kamera-Projektion einschließlich FOV-Trim wie ESP. `soft_glow.hpp` zeichnet
Gauß-Streifen statt breiter Linien und verwendet einmalig erzeugte Alpha-Masken
für Ringe und gefüllte Federn. Bodenringe bestehen aus kleinen Textur-Zellen;
Near-Plane- und Viewport-Clipping erhalten den sichtbaren Teil eines nahen Rings.
Höchstens acht Sprungringe mit je 24 Lichtstaub-Punkten werden gleichzeitig
gezeichnet. China-Hats behalten ihren transparenten Kegel, mit weichem Lichtsaum
ohne Speichen. Trails, Crosshair, Offscreen-Arrows und Lagerumrisse teilen den
weichen Glow. Additives D3D11-Blending bleibt gekapselt; Text/HUD verwenden danach
normales Alpha-Blending. Kein zusätzlicher Blur- oder Shader-Pass, keine neuen
Texturen oder Effekt-Pools pro Frame. Reduced motion unterbindet Ringrotation,
Wachstum, Staubaufstieg und Flügelschlag; das Ausblenden bleibt erhalten.

Der neue Tiefenkanal benötigt aktualisierte Agent-/Java-Helfer und IPC 18.
Minecraft vollständig neu starten und die neue `build/client/Keim.exe` öffnen.
Unter **Visuals → Jump circles**
lassen sich Farben unabhängig von der Aura wählen, wenn **Aura colours** aus ist.

`overlay_rockstar.cpp` integriert Einstellungen, Sounds, Panic und Ereignisse;
`overlay_input.cpp` ordnet Keybinds zu und speichert Parameter. Der Render-Thread
führt weiterhin **keine JNI-Aufrufe** aus. Neue Lager-/Beacon-Schlüssel erweitern
die vorhandene Translation-Key-Tabelle des separaten Agent-Scanners.

Die Prüfungen decken Snapshot-Deduplizierung, HP-Ereignisse, Respawn/Weltwechsel,
Teleport-Ausschluss, CPS-Zeitfenster, Parametergrenzen, Panic mit Hold-Key,
Mittelklick/Fokus, Speicherung samt Anführungszeichen im Wegpunktnamen und endliche
Projektionsgeometrie ab. Maskenkanten, transparente Ringmitte und Clipping inklusive
UV-/Draw-State-Grenzen werden ebenfalls geprüft. Die tatsächliche UI wird offscreen mit D3D11 gerendert,
inklusive aller Subtabs, Glow-Effekten, neuer HUD-Widgets und kleinen Viewports.
Das ersetzt keinen Live-Test in Minecraft.
