# Kompaktes Client-Menü, modulares HUD und Keybinds

Das Client-Menü folgt `design/reference-translucent-menu.png`: kompakt,
anthrazitfarben, transparent und mit gedämpftem Korallorange. Kleine Inter-Texte,
native Felder und Slider mit sichtbaren Werten halten Einstellungen lesbar.
**Strg-Klick** auf einen Slider erlaubt die direkte Zahleneingabe.
**Combat, Visuals, Blocks, HUD, Stats, Settings, Misc** stehen links. Der Header
verschiebt das Menü; **Center** zentriert es, **Reset** setzt die aktuelle Kategorie
zurück. Die Größe beträgt bis zu 1040 × 520 UI-Pixel, begrenzt auf den Viewport
minus 24 Pixel. Die Sidebar ist 150 Pixel breit, unter 620 Pixel Fensterbreite
112 Pixel. Der Launcher behält seine separate Liquid-Glass-Darstellung.
Layoutdetails: [compact-ui.md](compact-ui.md). Launcher-Material und Capture:
[liquid-ui.md](liquid-ui.md). HUD und Welt-Visuals behalten ihre bisherigen Styles.

Der Client komponiert seine dunkle Oberfläche direkt mit Alpha 0,88 über das
Spiel. **Settings → Overlay → Menu material** zeigt **Translucent dark / 88%
opacity**, **Background capture** zeigt **Not required**. Der Client benötigt
keine Desktop-Aufnahme, Unschärfe oder Brechung. **Exclude from screen capture**
gilt auch bei geöffnetem Menü.

## Kategorien, Module und Einstellungen

Eine Kategorie links und einen Modul-Tab oben auswählen. Alle 76 bisherigen
Bereiche bleiben erreichbar. Tabs wählen Einstellungen; sie schalten kein Modul
um. Überlaufende Tabs lassen sich mit den nativen ImGui-Pfeilen horizontal
scrollen. **Search** filtert Modulnamen innerhalb der Kategorie ohne Unterschied
zwischen Groß- und Kleinschreibung. Bei null Treffern stellt **Clear search** die
Tabs wieder her. Ein Kategorienwechsel leert die Suche.

Jede Kategorie behält ihren ausgewählten Abschnitt. Kategorie- und Modulwechsel
setzen den Content-Scroll zurück. Die Tabs stehen außerhalb dieses Scrollbereichs.
Einstellungen bleiben im Hauptfenster; ein zweites Modulfenster ist nicht nötig.
Ab 660 Pixel Content-Breite und bei mehr als zwei Toggle-Modulen teilt eine
60/40-Tabelle Detailregler links und **Quick toggles** für andere Module rechts.
Bei kleineren Fenstern stehen die Details in einer Spalte.

Der **Enabled**-Schalter im Header zeigt und ändert den echten Zustand des
aktuellen Moduls. **Quick toggles** und vorhandene Aktivierungsschalter wirken auf
dieselben Modulwerte. **Friends**, **Entity filters**, **Block types**, **Stats**
und **Settings** zeigen ihre tatsächlichen Einstellungen ohne erfundenen
Aktivierungszustand. **HUD → Editor** und **Misc → Panic** behalten ihre Aktion;
**Run action** im Header startet sie direkt.

- **Combat:** Kill Aura, Aim Assist, Triggerbot, Auto Totem, Friends, Crystal Macro, Anchor Macro, Hit sounds. Diagnostics
  bleibt innerhalb von Kill Aura eingeklappt.
- **Visuals:** Box ESP, Skeleton, Name tags, Health bars, Distance labels, Tracers,
  Target info, Target aura, Target marker, Trails, Hit particles, Ambient particles,
  Hit bubbles, Kill effect, BreadCrumbs, China hat, Entity filters, Jump circles,
  Wings, Halo, Glow ESP, Damage markers, Damage vignette, Target circle, Crosshair
  und Offscreen arrows. Gemeinsame Entity-Farben
  und Sichtbarkeitsfilter liegen unter Entity filters.
- **Blocks:** Block ESP (Scan-Einstellungen), Block types, Tracers, Chunkfinder,
  Storage ESP, Beacon radius und Chunk borders.
- **HUD:** Editor und ein Tab pro HUD-Element mit Sichtbarkeit, Position, Skalierung
  und Reset nur dieses Elements. Durability warning liegt unter Equipment.
- **Stats:** Local player, Client, Entities.
- **Settings:** Overlay, Focus, Keybinds, Agent.
- **Misc:** Middle click friend, Death coordinates, Detect players, Waypoints,
  Name protect, Module sounds, Notifications und Panic.

## Keybinds

**Settings → Keybinds** als Modul-Tab auswählen: dort steht jede
Belegung. Klick auf das Tastenfeld, dann die
gewünschte Taste oder Mouse 1–5 drücken. Strg/Shift/Alt-Kombinationen sind erlaubt.
Ein einzelner Modifier wird beim Loslassen erfasst, damit er zunächst noch Teil
einer Kombination sein kann. **Esc** bricht die Erfassung ab; **Backspace/Delete**
entfernt eine Modul-Belegung. Menü und Beenden müssen eine Taste behalten.
Doppelte Belegungen werden abgewiesen; die Meldung bleibt im Editor sichtbar.

Unter 420 Pixel Content-Breite steht der Modulname über einer zweiten Zeile mit
Tastenfeld und Toggle/Hold-Modus. Tastenfelder passen in die verfügbare Breite;
die Tastenerfassung behält beim Wechsel zwischen breiter und schmaler Ansicht
dieselben Widget-IDs.

| Standard | Aktion |
| --- | --- |
| INSERT | Menü öffnen/schließen |
| F6 | HUD-Editor öffnen/beenden |
| END | Overlay beenden |
| Ungebunden | Alle Modul-Aktionen |

Belegbare Module: Kill Aura, Aim Assist, Triggerbot, Auto Totem, Box ESP, Skeleton,
Name tags, Block ESP, Target Aura, Target Marker, Trails, Hit particles,
Ambient particles, Hit bubbles, Kill effect, BreadCrumbs, China hat, Crystal Macro,
Anchor Macro, Chunkfinder, Rockstar-Funktionen und gesamtes HUD. Die wichtigsten
Module bieten zusätzlich ein Tastenfeld direkt neben ihrem Aktivierungsschalter.

**Toggle** schaltet einmal pro Tastendruck um. **Hold** aktiviert bis zum Loslassen;
anschließend wird der vorherige Aktivierungszustand wiederhergestellt. Fokusverlust
beendet Hold ebenfalls. Tastenwiederholung löst keinen erneuten Toggle aus. Module
schalten während Menü, HUD-Editor oder Tastenerfassung nicht. System-Binds verwenden
Toggle. Alle Binds sind auf Minecraft/Overlay-Fokus begrenzt; eine bereits beim
Start oder Fokuswechsel gehaltene Taste löst keinen nachträglichen Toggle aus.

Die Modul-Binds steuern das Aktivieren der Funktion. Die bestehenden Aim-Assist-
und Triggerbot-Haltetasten unter Combat bleiben deren zusätzliche Auslösebedingungen.
Die Kill Aura braucht keine zusätzliche Aktivierungstaste und läuft nach Toggle
mit ihrem bisherigen Cooldown-/Ziel-Lock-Verhalten weiter.

## HUD-Editor

**HUD → Editor → Run action**, **Open HUD editor** in den Einstellungen oder
standardmäßig **F6** öffnet den Editor. Jedes Element
hat eigene Position, Sichtbarkeit und Skalierung. Die 19 Elemente umfassen
Equipment, Status effects, Coordinates, Performance, Compass, Reach indicator,
Watermark, Active modules, Keybinds und die ergänzten Rockstar-/Status-Widgets.
Active modules und Keybinds zeigen aktive Module und vergebene Modul-Binds.

Element anklicken und ziehen. Die Toolbar wählt auch verdeckte Elemente aus und
stellt **Enabled** sowie **Scale** (0,65–1,60 ×) ein. **Reset layout** entfernt alle
manuellen Positionen/Skalen. **Done**, **Esc** oder der Editor-Bind beendet den
Editor. Leere und deaktivierte Elemente erhalten eine erkennbare Vorschau.
Auf Viewports unter 640 px Breite oder 480 px Höhe zeigt der Editor nur das
ausgewählte Element; die Auswahl erreicht weiterhin alle Module. Das normale
HUD zeigt weiterhin die aktivierten Elemente.

Unter dem jeweiligen HUD-Element-Tab lassen sich dieselben Positionen und Skalen
auch numerisch einstellen.
Positionen sind im verfügbaren Bildschirmweg normalisiert; Auflösungswechsel
behalten die Anker, und Ziehen/Skalierung bleibt innerhalb des Viewports. Frei
positionierte Elemente dürfen sich überlappen; Sichtbarkeit, Maßstab und Editor
erlauben das gewünschte Layout. Leere Module verschwinden außerhalb des Editors.
Bei offenem Menü oder Editor pausiert die Combat-Eingabe-Lease; Aim Assist und
Triggerbot senden dann keine Eingaben.

## Speicherung und Technik

`%LOCALAPPDATA%\mcov\client-settings.ini` enthält Binds, HUD-Konfiguration,
Rockstar-Modulparameter und server-/dimensionsgebundene Wegpunkte
beim Start über die portable `mcov.exe`. Direkte Entwickler-Starts verwenden
weiterhin `build/bin/client-settings.ini` neben der Overlay-EXE. Änderungen werden verzögert
gespeichert, bestätigte Binds und das Beenden des Editors sofort. Eine temporäre
Datei wird nach vollständigem Schreiben ersetzt. Fehler stehen im Keybind-Editor;
ungültige/fehlende Werte fallen auf Defaults zurück. **Reset keybinds** stellt die
drei System-Tasten und ungebundene Module wieder her. Combat/ESP/Aura-Einstellungen
und Freunde werden durch diese Datei nicht automatisch aktiviert oder gespeichert.

`keybinds.hpp` trennt Eingabe-Zustand von Win32 und Moduländerungen.
`hud_layout.hpp` kapselt Platzierung und Grenzen. `overlay_input.cpp` bindet echte
Stores und Dateispeicherung an; `overlay_hud.cpp` zeichnet nur Snapshot-Daten.
Das neue Rockstar-Paket erweitert die Scanner-Typen mit IPC v19. JNI-Abfragen
bleiben außerhalb des Render-Threads. [Neue Funktionen und Bedienung](rockstar-features.md).

Geprüft: alle 76 Modul-Tabs, reale Toggle-Aktionen, Tastaturbedienung,
Suche samt leerem Trefferzustand und Wiederherstellung, Style-/Font-/Window-Stack,
Drag/Viewport-Grenzen, kleine Viewports und Slider einschließlich sichtbarer
Strg-Eingabe. Bestehende Keybind-, HUD-, Renderer-, Launcher- und portable
Build-Prüfungen bestehen ebenfalls.

Aktuelle native WARP-Vorschauen liegen unter
`.impeccable/review/compact-client/{combat-desktop,aim-desktop,aim-compact,visuals-desktop,keybinds-desktop,keybinds-compact,keybinds-compact-bottom}.png`.
Sie zeigen die echte ImGui-Ausgabe mit direkter Alpha-Komposition vor synthetischem
Hintergrund. Sie sind keine Minecraft-Screenshots und prüfen keine Live-Optik,
Attach-/Fokus-Verbindung zum Spiel oder sämtliche GPUs. Ältere Glas-/Rasterbilder
unter `.impeccable/review/` dokumentieren den abgelösten Client, nicht diese Ansicht.
Die weiterhin gültigen Launcher-/Glas-Capture-Tests stehen in
[liquid-ui.md](liquid-ui.md).

Crystal Macro und Anchor Macro laufen bei aktivierter Funktion, solange die normale
Platzieren-/Benutzen-Taste gehalten wird. Der Modul-Bind steuert die Aktivierung.
Bedienung und Statusmeldungen stehen in [Item Assist](item-assist.md).
