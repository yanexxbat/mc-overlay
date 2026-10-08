# Liquid-Glass-Menü, modulares HUD und Keybinds

Das Menü und der Injector verwenden das gewünschte Liquid-Glass-Material aus
[liquidDX11](https://github.com/poncippg-spec/liquidDX11): dunkel neutral getönt,
abgerundet, mit Hintergrundunschärfe, dezenter Brechung und weichen Lichtkanten.
Inter, ruhigere Felder und Slider mit separater Wertanzeige halten Einstellungen
lesbar. **Strg-Klick** auf einen Slider erlaubt die direkte Zahleneingabe.
**Combat, Visuals, Blocks, HUD, Stats, Settings, Misc** enthalten die bestehenden
Funktionen als Kategorien links. Der Titelbalken verschiebt das Menü; **Center**
zentriert es, **Reset** setzt die aktuelle Kategorie zurück. Das Menü ist bis zu
900 × 680 UI-Pixel groß und passt sich dem Viewport an. Die Modulkacheln stehen
in ein bis drei Spalten; kleine Viewports scrollen das Raster. Material,
Capture-Fallback und Technik:
[liquid-ui.md](liquid-ui.md). HUD und Welt-Visuals behalten ihre bisherigen Styles.

**Settings → Overlay → Menu material** zeigt **live backdrop** oder
**fallback backdrop**. Daneben nennt **Background capture** die aktive Quelle
(**Live / DXGI Desktop Duplication** oder **Live / Windows Graphics Capture**),
den Wartezustand oder den Fehler. Ohne nutzbares DXGI-Bild versucht das Menü nach
750 ms Windows Graphics Capture. Beim Schließen endet die Capture-Sitzung und
die konfigurierte Capture-Ausschlussoption gilt wieder.

## Kategorien, Module und Einstellungen

Eine Kategorie links auswählen, dann die gewünschte Modulkachel bedienen.
**Linksklick** schaltet ein echtes Modul an oder aus; die Kachel zeigt **Enabled**
oder **Disabled** und, soweit vorhanden, die Belegung. **Rechtsklick** öffnet die
bisherigen Einstellungen des Moduls in einem zweiten, unabhängig verschiebbaren
Glasfenster. Jeder Rechtsklick bringt dieses Fenster nach vorn, auch für das
bereits ausgewählte Modul. Alle 76 bisherigen Bereiche bleiben erreichbar.

Das Einstellungsfenster ist bis zu 480 × 610 UI-Pixel groß, hat einen eigenen
Titelbalken, **X** zum Schließen und einen scrollbaren Inhalt. Beide Fenster
passen in den Viewport mit 24 Pixel Größenreserve. Ein Kategorienwechsel setzt
das Modulraster nach oben, lässt aber das Einstellungsfenster beim ausgewählten
Modul samt Scrollposition. Ein anderes Modul beginnt mit seinen Einstellungen
oben; ein erneuter Rechtsklick auf dasselbe behält die Scrollposition. **Close** im
Hauptfenster schließt auch die Einstellungen.

**Friends**, **Entity filters**, **Block types** und alle Kacheln unter **Stats**
und **Settings** öffnen auch per Linksklick ihre Einstellungen. Sie zeigen
**Open settings** statt eines erfundenen Aktivierungszustands. **HUD → Editor**
und **Misc → Panic** sind Aktionen: Linksklick startet den Editor bzw. Panic;
Rechtsklick öffnet ihren bestehenden Einstellungsbereich.

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
- **HUD:** Editor und eine Kachel pro HUD-Element mit Sichtbarkeit, Position, Skalierung
  und Reset nur dieses Elements. Durability warning liegt unter Equipment.
- **Stats:** Local player, Client, Entities.
- **Settings:** Overlay, Focus, Keybinds, Agent.
- **Misc:** Middle click friend, Death coordinates, Detect players, Waypoints,
  Name protect, Module sounds, Notifications und Panic.

## Keybinds

**Settings → Keybinds** per Links- oder Rechtsklick öffnen: dort steht jede
Belegung. Klick auf das Tastenfeld, dann die
gewünschte Taste oder Mouse 1–5 drücken. Strg/Shift/Alt-Kombinationen sind erlaubt.
Ein einzelner Modifier wird beim Loslassen erfasst, damit er zunächst noch Teil
einer Kombination sein kann. **Esc** bricht die Erfassung ab; **Backspace/Delete**
entfernt eine Modul-Belegung. Menü und Beenden müssen eine Taste behalten.
Doppelte Belegungen werden abgewiesen; die Meldung bleibt im Editor sichtbar.

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

**Linksklick auf HUD → Editor** oder standardmäßig **F6** öffnet den Editor.
Alternativ rechtsklicken und **Open HUD editor** wählen. Jedes Element
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

Unter HUD lassen sich dieselben Positionen und Skalen nach Rechtsklick auf die Elementkachel
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

Geprüft: reale Menüeingaben, Tastenerfassung/Konflikte, Toggle/Hold/Fokus,
Store-Änderung, Speichern/Laden, HUD-Ziehen und kleine Viewports. D3D11-WARP-
Vorschauen liegen unter `build/ui-preview/classic-*.png` und `modular-hud*.png`.
Die historischen `classic-*.png`-Dateinamen zeigen jetzt das Liquid-Glass-Menü.
Die aktuellen Raster-/Einstellungsansichten liegen zusätzlich unter
`.impeccable/review/{glass-grid-desktop,glass-settings-desktop,glass-grid-compact,glass-settings-compact}.png`.
Sie zeigen das echte GPU-Material über synthetischen Texturen und sind keine
Minecraft-Screenshots. Die älteren Review-Bilder verwenden den WARP-Fallback.
Der ausdrücklich aktivierte native `mcov_backdrop_capture_check --live`-Test hat
auf diesem Host WGC-Start, vollständige LiquidGlass-Laufzeit, Capture-Pause,
Wiederherstellung der Display-Affinität und Live-Capture nach erneutem Öffnen
bestanden, ohne Desktopbilder zu speichern oder eine JVM anzubinden. Die Live-Optik im Spiel
und Attach/Fokus sind damit weiterhin nicht geprüft.

Crystal Macro und Anchor Macro laufen bei aktivierter Funktion, solange die normale
Platzieren-/Benutzen-Taste gehalten wird. Der Modul-Bind steuert die Aktivierung.
Bedienung und Statusmeldungen stehen in [Item Assist](item-assist.md).
