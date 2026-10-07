# Klassisches Menü, modulares HUD und Keybinds

Das Menü folgt `design/reference-classic-ui.png`: ein dunkler kantiger Rahmen,
violette Trennlinien, eingebettete 13-Pixel-Schrift, Tabs oben und kompakte
gefüllte Slider. **Combat, Visuals, Blocks, HUD, Stats, Settings** enthalten die
bestehenden Funktionen. Der Titelbalken verschiebt das Menü; **Center** zentriert
es, **Reset** setzt die aktuelle Seite zurück. Kleine Viewports scrollen und
stapeln die Einstellungsgruppen. Der Injector behält seine eigene Flat-Oberfläche.

## Untertabs nach Funktion

Jede Hauptseite hat eine zweite Tab-Leiste. Die Pfeile rechts erreichen Tabs,
die auf dem aktuellen Viewport nicht nebeneinander passen. Die Auswahl bleibt
beim Wechsel der Hauptseite erhalten; neue Funktionsseiten beginnen oben.

- **Combat:** Kill Aura, Aim Assist, Triggerbot, Auto Totem, Friends, Crystal Macro, Anchor Macro. Diagnostics
  bleibt innerhalb von Kill Aura eingeklappt.
- **Visuals:** Box ESP, Skeleton, Name tags, Health bars, Distance labels, Tracers,
  Target info, Target aura, Target marker, Trails, Hit particles, Ambient particles,
  Hit bubbles, Kill effect, BreadCrumbs, China hat, Entity filters. Gemeinsame Entity-Farben
  und Sichtbarkeitsfilter liegen unter Entity filters.
- **Blocks:** Block ESP (Scan-Einstellungen), Block types, Tracers, Chunkfinder.
- **HUD:** Editor und ein Tab pro HUD-Element mit Sichtbarkeit, Position, Skalierung
  und Reset nur dieses Elements. Durability warning liegt unter Equipment.
- **Stats:** Local player, Client, Entities.
- **Settings:** Overlay, Focus, Keybinds, Agent.

## Keybinds

**Settings → Keybinds** zeigt jede Belegung. Klick auf das Tastenfeld, dann die
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
Ambient particles, Hit bubbles, Kill effect, BreadCrumbs, China hat, Crystal Macro, Anchor Macro, Chunkfinder und gesamtes HUD. Die wichtigsten
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

**HUD → Editor → Open HUD editor** oder standardmäßig **F6** öffnet den Editor. Jedes Element
hat eigene Position, Sichtbarkeit und Skalierung: Equipment, Status effects,
Coordinates, Performance, Compass, Reach indicator, Watermark, Active modules,
Keybinds. Die letzten beiden zeigen aktive Module und vergebene Modul-Binds.

Element anklicken und ziehen. Die Toolbar wählt auch verdeckte Elemente aus und
stellt **Enabled** sowie **Scale** (0,65–1,60 ×) ein. **Reset layout** entfernt alle
manuellen Positionen/Skalen. **Done**, **Esc** oder der Editor-Bind beendet den
Editor. Leere und deaktivierte Elemente erhalten eine erkennbare Vorschau.
Auf Viewports unter 640 px Breite oder 480 px Höhe zeigt der Editor nur das
ausgewählte Element; die Auswahl erreicht weiterhin alle Module. Das normale
HUD zeigt weiterhin die aktivierten Elemente.

Unter HUD lassen sich dieselben Positionen und Skalen im jeweiligen Element-Tab
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
Diese Prüfungen laufen ohne Minecraft; die Live-Optik wurde nicht im Spiel geprüft.

Crystal Macro und Anchor Macro verwenden immer Hold und brauchen einen physisch gehaltenen Bind. Bedienung und Statusmeldungen stehen in [Item Assist](item-assist.md).
