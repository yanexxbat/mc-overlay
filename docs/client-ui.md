# Kompaktes Client-Menü, modulares HUD und Keybinds

Das Client-Layout folgt der Paint-Skizze `design/reference-module-widgets.png`:
Kategorien enthalten Modul-Widgets; Rechtsklick öffnet deren Einstellungen in
einem unabhängigen Fenster. Das bestätigte Theme bleibt kompakt,
anthrazitfarben, transparent und mit gedämpftem Korallorange. Kleine Inter-Texte,
native Felder und Slider mit sichtbaren Werten halten Einstellungen lesbar.
**Strg-Klick** auf einen Slider erlaubt die direkte Zahleneingabe.
**Combat, Visuals, Blocks, HUD, Stats, Settings, Misc** stehen links. Der Header
verschiebt das Menü; **Center** ordnet beide Fenster neu an, **Reset** setzt die aktuelle Kategorie
zurück. Das Hauptfenster beträgt bis zu 740 × 520 UI-Pixel, das Settings-Fenster
bis zu 480 × 580; beide sind auf den Viewport minus 24 Pixel begrenzt.
Die erste Platzierung lässt nach Möglichkeit bis zu 496 Pixel rechts für Settings
frei. Die Sidebar ist 150 Pixel breit, unter 620 Pixel Fensterbreite
112 Pixel. Der Injector verwendet dasselbe freigegebene kompakte Theme mit
Start/Diagnose-Sidebar und festem Start-/Schließen-Footer.
Layoutdetails: [compact-ui.md](compact-ui.md). Injector und Bedienung:
[launcher.md](launcher.md). Der erhaltene frühere Glasrenderer steht in
[liquid-ui.md](liquid-ui.md). Das HUD verwendet nun dasselbe kompakte Theme;
Welt-Visuals behalten ihre bisherigen Styles.

Der Client komponiert seine dunkle Oberfläche direkt mit Alpha 0,88 über das
Spiel. **Settings → Overlay → Menu material** zeigt **Translucent dark / 88%
opacity**, **Background capture** zeigt **Not required**. Der Client benötigt
keine Desktop-Aufnahme, Unschärfe oder Brechung. **Exclude from screen capture**
gilt auch bei geöffnetem Menü.

## Kategorien, Module und Einstellungen

Eine Kategorie links auswählen. Ihre Module stehen direkt in einem scrollbaren
Raster mit einer bis drei Spalten. Alle 82 Bereiche bleiben erreichbar.
Die 66 Pixel hohen Widgets zeigen Name, Zustand beziehungsweise Aktion und bei
Toggle-Modulen eine Anzeige unten rechts; der Tooltip zeigt vorhandene Belegungen.
**Search** filtert Modulnamen innerhalb der Kategorie ohne Unterschied zwischen
Groß- und Kleinschreibung. Bei null Treffern stellt **Clear search** die Widgets
wieder her. Ein Kategorienwechsel leert die Suche und schließt die Details.

**Linksklick** oder **Leertaste** schaltet ein echtes Toggle-Modul um oder führt
seine Aktion aus. **Rechtsklick**, **Menü-Taste** oder **Shift+F10** öffnet die
Einstellungen ohne Umschalten. Settings-only-Widgets öffnen diese auch per
Linksklick. **Friends**, **Entity filters**, **Block types**, **Stats** und
**Settings** zeigen ihre tatsächlichen Einstellungen ohne erfundenen
Aktivierungszustand. **HUD → Editor** und **Misc → Panic** behalten ihre Aktion.

Die Einstellungen haben einen eigenen verschiebbaren Header, **X** zum Schließen
und einen Scrollbereich. **Enabled** beziehungsweise **Run action** im Header
wirkt auf dieselben echten Modulwerte wie das Widget. Ein Modulwechsel setzt den
Settings-Scroll zurück; erneutes Öffnen desselben Moduls erhält ihn. Auf dem
Desktop stehen Hauptfenster und Settings nebeneinander. Unter 1260 Pixel
Viewport-Breite ist bei geöffneten Settings nur dieses Fenster sichtbar;
**X** führt zum Raster zurück. **Close** schließt Menü und Details.

- **Combat:** Kill Aura, Aim Assist, Triggerbot, Auto Totem, Friends, Crystal Macro, Anchor Macro, Hit sounds, Spinbot. Diagnostics
  bleibt innerhalb von Kill Aura eingeklappt.
- **Visuals:** Box ESP, Skeleton, Name tags, Health bars, Distance labels, Tracers,
  Target info, Target aura, Target marker, Trails, Hit particles, Ambient particles,
  Hit bubbles, Kill effect, BreadCrumbs, China hat, Entity filters, Jump circles,
  Wings, Halo, Glow ESP, Damage markers, Damage vignette, Target circle, Crosshair
  und Offscreen arrows. Gemeinsame Entity-Farben
  und Sichtbarkeitsfilter liegen unter Entity filters.
- **Blocks:** Block ESP (Scan-Einstellungen), Block types, Tracers, Chunkfinder,
  Storage ESP, Beacon radius, Chunk borders, Stash Finder, Spawner ESP und Base ESP.
- **HUD:** Editor und ein Widget pro HUD-Element mit Sichtbarkeit, Position, Skalierung
  und Reset nur dieses Elements. Durability warning liegt unter Equipment.
  **Region map** zeigt die vollständige DonutSMP-Serverkarte von −225.000 bis
  +225.000 auf X und Z. Die sechs farbigen Bereiche sind EU Central, EU West,
  NA East, NA West, Asia und Oceania. Rechtsklick öffnet Beschriftungen,
  Rasterlinien, Spielerkoordinaten und eine optionale Toggle/Hold-Taste.
  F6 erlaubt Verschieben und Skalieren. Die weiße Positionsmarkierung erscheint
  bei einer aktuellen Verbindung in der DonutSMP-Overworld; die Karte bleibt
  auch ohne Positionsdaten als Übersicht sichtbar. Kartengrundlage:
  [Donut.Auction](https://donut.auction/regions), Stand 15.10.2026.
- **Stats:** Local player, Client, Entities.
- **Settings:** Overlay, Focus, Keybinds, Agent, Configs.
- **Misc:** Middle click friend, Death coordinates, Detect players, Waypoints,
  Name protect, Module sounds, Notifications und Panic.

**Storage ESP** bietet 13 auswählbare Container-Typen mit jeweils eigener
RGBA-Farbe sowie Namen/Distanz und Tracern. **Stash Finder** zählt standardmäßig
mindestens acht Chest-/Trapped-Chest-/Shulker-/Barrel-Blöcke pro Chunk und
ermöglicht das Kopieren oder Speichern einer Entdeckung als Wegpunkt.
**Spawner ESP** hat eigene Farbe, Labels und Tracer. Die Einstellungen liegen
weiterhin unter **Blocks** und öffnen sich per Rechtsklick im unabhängigen Fenster.
Bedienung, Scan-Grenzen und Zählregeln: [SMP-Funktionen](smp-features.md).

**Base ESP → Display → Minecraft textures** zeichnet in 1.21.11 die echten
geladenen Blockmodelle rund um erkannte Containeransammlungen durch Wände.
Dieser Modus ist der Standard, mit **Texture opacity** anfangs 1. Der zusätzliche
Rand ist einstellbar; die Erkennung bestimmt Container-Ausdehnung plus Rand,
keine vollständigen Gebäudegrenzen. **Label colour** färbt die optionalen Labels
und Tracer, während die Blocktexturen aus dem laufenden Spiel kommen.
**Area marker** bietet
weiterhin die farbige 3D-Fläche mit Rahmen. Das Modul arbeitet auch ohne
eingeschalteten Stash Finder; Radius, Filter und Mindestanzahl werden gemeinsam
verwendet. Der Status zeigt laufenden Aufbau, leere Bereiche, Bereichsgrenzen,
übersprungene Spezialeffekte und Fehler. Zum erneuten Aufbau nach einem Fehler
Base ESP aus- und wieder einschalten. Auf anderen Versionen oder bei fehlendem
Texturrenderer **Area marker** wählen; die Meldung erklärt gegebenenfalls den
nötigen Minecraft-Neustart.

## Configs

**Settings → Configs** öffnet die Profilverwaltung im unabhängigen Settings-Fenster.
Unter **Config name** einen Namen eingeben und **Save config** wählen. Ein bereits
vorhandener Name zeigt **Overwrite config** zum ausdrücklichen Ersetzen.
Ein Profil unter **Saved configs** auswählen und **Load selected** drücken, um
seine gespeicherten Modulzustände und Einstellungen zu übernehmen. Die Auswahl
allein lädt nichts; beim Start wird kein benanntes Profil ausgewählt oder geladen.
**Refresh** aktualisiert die Liste nach dem Kopieren einer Datei in den Configs-Ordner.

Ein Profil enthält Modulzustände und Einstellungen einschließlich Visual-Farben,
Combat, Makros und Blocks sowie HUD-Sichtbarkeit und Layout, Binds, Freunde und
Wegpunkte. Es wird vollständig geprüft, bevor es angewendet wird; eine ungültige
oder nicht unterstützte Datei lässt die aktuellen Einstellungen unverändert.
Statusmeldungen zeigen erfolgreiche Aktionen und Fehler mit einem nächsten Schritt.

Profile liegen in `configs/<Name>.ini` neben `Keim.exe`. Namen dürfen 1–48
ASCII-Buchstaben, Ziffern, Leerzeichen, `-` oder `_` enthalten; führende und
abschließende Leerzeichen sowie reservierte Windows-Namen sind ausgeschlossen.
Dateien mit `.ini` oder `.INI` erscheinen nach **Refresh** in der Liste.
Das Feld **Folder** zeigt den Speicherort; Hover zeigt den vollständigen Pfad.
In kleinen Viewports scrollt das vorhandene Settings-Fenster bis zu allen Aktionen.
Beim Verschieben des Clients `configs` und `client-settings.ini` mitnehmen.

## Keybinds

**Settings → Keybinds** öffnen: dort steht jede
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
Anchor Macro, Chunkfinder, Storage ESP, Stash Finder, Spawner ESP,
Rockstar-Funktionen und gesamtes HUD. Die wichtigsten
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

Linksklick auf **HUD → Editor**, **Run action** in dessen Settings,
**Open HUD editor** in den Einstellungen oder
standardmäßig **F6** öffnet den Editor. Jedes Element
hat eigene Position, Sichtbarkeit und Skalierung. Die 19 Elemente umfassen
Equipment, Status effects, Coordinates, Performance, Compass, Reach indicator,
Watermark, Active modules, Keybinds und die ergänzten Rockstar-/Status-Widgets.
Active modules und Keybinds zeigen aktive Module und vergebene Modul-Binds.

Alle 19 Snapshot-Elemente und das optionale Combat-Debug-HUD verwenden eingebettetes
Inter mit 14 Pixeln sowie eine einzelne anthrazitfarbene Flaeche mit 8 Pixel Rundung,
ohne Rahmen, Zierstreifen oder Schatten. Ruhige Ueberschriften und Trennlinien,
korallfarbene aktive Zustaende und gruene, amberfarbene sowie rote Statusfarben halten die
Informationen lesbar. Lange Namen, Item-/Effect- und Keybind-Texte bleiben durch
Zeilen-Clipping auch bei Skalierung innerhalb des Elements. Der Reach-Readout
hat eine dunkle Flaeche unterhalb der offenen Fadenkreuzklammern; der Name clippt vor
der getrennten HP-/Distanz-Spalte.

Element anklicken und ziehen. Die Toolbar wählt auch verdeckte Elemente aus und
stellt **Enabled** sowie **Scale** (0,65–1,60 ×) ein. **Reset layout** entfernt alle
manuellen Positionen/Skalen. **Done**, **Esc** oder der Editor-Bind beendet den
Editor. Leere und deaktivierte Elemente erhalten eine erkennbare Vorschau.
Auf Viewports unter 640 px Breite oder 480 px Höhe zeigt der Editor nur das
ausgewählte Element; die Auswahl erreicht weiterhin alle Module. Das normale
HUD zeigt weiterhin die aktivierten Elemente.
Die lokal mit `CompactStyle` gestaltete Toolbar ist maximal 540 Pixel breit,
begrenzt auf Viewport minus 24 Pixel, mit 14 × 10 Pixel Innenabstand. Ihre Hoehe
betraegt regulaer 90 Pixel und in der kleinen Ansicht 110 Pixel. Eine korallfarbene
Outline markiert die Auswahl; ein Disabled-Badge erscheint nur beim ausgewaehlten
deaktivierten Element. Positionen, Skalierung und Sichtbarkeit speichern wie bisher.

In den Einstellungen des jeweiligen HUD-Element-Widgets lassen sich dieselben Positionen und Skalen
auch numerisch einstellen.
Positionen sind im verfügbaren Bildschirmweg normalisiert; Auflösungswechsel
behalten die Anker, und Ziehen/Skalierung bleibt innerhalb des Viewports. Frei
positionierte Elemente dürfen sich überlappen; Sichtbarkeit, Maßstab und Editor
erlauben das gewünschte Layout. Leere Module verschwinden außerhalb des Editors.
Bei offenem Menü oder Editor pausiert die Combat-Eingabe-Lease; Aim Assist und
Triggerbot senden dann keine Eingaben.

## Speicherung und Technik

`client-settings.ini` neben der portablen `Keim.exe` enthält die bisherigen
automatischen Teil-Einstellungen, darunter Binds, HUD-Konfiguration,
Rockstar-Modulparameter, alle SMP-Einstellungen und server-/dimensionsgebundene Wegpunkte. Diese Datei
ist kein vollständiges benanntes Profil; dafür **Settings → Configs** verwenden.
Direkte Entwickler-Starts verwenden weiterhin `build/bin/client-settings.ini`
neben der Overlay-EXE. Änderungen werden verzögert
gespeichert, bestätigte Binds und das Beenden des Editors sofort. Eine temporäre
Datei wird nach vollständigem Schreiben ersetzt. Fehler stehen im Keybind-Editor;
ungültige/fehlende Werte fallen auf Defaults zurück. **Reset keybinds** stellt die
drei System-Tasten und ungebundene Module wieder her. Ohne lokale Datei importiert
der portable Launcher bestehende automatische Einstellungen einmalig aus AppData;
lokale Einstellungen werden dabei nicht ersetzt. Benannte Configs werden mit einer
temporären Datei im selben `configs`-Ordner atomar gespeichert. Laufzeit-Helfer
und Logs bleiben unter `%LOCALAPPDATA%\Keim`.

`keybinds.hpp` trennt Eingabe-Zustand von Win32 und Moduländerungen.
`hud_layout.hpp` kapselt Platzierung und Grenzen. `overlay_input.cpp` bindet echte
Stores und Dateispeicherung an; `overlay_hud.cpp` zeichnet nur Snapshot-Daten.
Das neue Rockstar-Paket erweitert die Scanner-Typen mit IPC v19. JNI-Abfragen
dieser Scanner bleiben außerhalb des Render-Threads.
[Neue Funktionen und Bedienung](rockstar-features.md).

Die aktuelle Base-Finder-Erweiterung verwendet IPC v23, einschließlich der
gemeinsamen Blocktypen, Scan-Metadaten und Texturanfragen/-meldungen.
Bereits laufendes Minecraft
vor erneutem Attach mit dem aktualisierten Client und Agent vollständig neu starten.
Alle SMP-Einstellungen und Binds speichern automatisch und in benannten Configs.
Vollständige ältere Configs ohne SMP-Block erhalten Defaults und ungebundene neue
Aktionen; teilweise vorhandene oder ungültige SMP-Daten lassen das gesamte
benannte Profil vor dem Anwenden scheitern. Entdeckungen bleiben auf die Session
beschränkt; **Save waypoint** speichert den Ort dauerhaft im bestehenden Store.
Details und Grenzen stehen in [SMP-Funktionen](smp-features.md).

Für die aktuelle Base-Texturerweiterung bestanden alle 26 verfügbaren
Regressionsprüfungen über die abschließenden Läufe; `frame_attach` bestand nach
einem Windows-Pipe-Zugriffsfehler separat außerhalb der Sandbox. Die letzte
gezielte Prüfung bestand nach der Ressourcenfreigabe-Änderung für
`base_texture_gl`, `base_texture_models`, `smp_features` und `portable_client`;
`depth_hook` bestand im vorangehenden gezielten Lauf. Der echte WGL/OpenGL-
Pixeltest prüft auch Deckkraft 0,5, transparente Texel und Renderer-
Initialisierung nach Ressourcenfreigabe. Die fünf
`-verified.png`-Bilder unter `.impeccable/review/base-textures/` zeigen die
Desktop-Einstellungen, kleine obere/gescrollte Einstellungen und Lade-/Fehler-
zustände mit synthetischen ImGui/D3D11-WARP-Fixtures. Die unabhängige
Abschlussprüfung bestätigte **ship für den UI-Umfang**. Die EXE und ZIP sind neu
gebaut; Live-Minecraft-Texturen, Optik, Attach sowie Shader-/Mod-Grafik-
Kompatibilität wurden hier nicht geprüft. Die früheren Base-Bilder unter
`.impeccable/review/base-esp/` belegen den farbigen **Area marker**.

Für die ursprüngliche Erweiterung mit Storage ESP, Stash Finder und Spawner ESP
bestanden alle 24 verfügbaren Regressionsprüfungen über die abschließenden
Läufe; der isolierte `frame_attach`-Fixture-Test erforderte nach einem Windows-
Pipe-Zugriffsfehler einen Lauf außerhalb der Sandbox. Die nativen UI-Prüfungen
decken auch echte Klicks auf Typbezeichnungen und das Öffnen des Farbwählers ab.
Die sechs Vorschauen unter `.impeccable/review/smp/` zeigen synthetische Fixtures
mit dem echten ImGui/D3D11-WARP-Renderer. Die portable EXE und ZIP wurden neu
gebaut. Live-Minecraft-Scans, Optik und Attach wurden für SMP nicht geprüft.
Die unabhängige Abschlussprüfung öffnete alle sechs nach dem Typbezeichnungs-
Fix erneut erzeugten Bilder und bestätigte **ship**, ohne sichtbare Regression.

Geprüft: echte Rechtsklicks für alle 77 Abschnitte, reale Toggle-Stores,
Leertaste und Shift+F10, unabhängiges Ziehen/Schließen beider Fenster,
Schließen der kleinen Settings-Ansicht mit Rückkehr zum Raster, Suche samt
Wiederherstellung, Keybinds, Slider, Combos, Farben, Marker, Persistenz und
Style-/Font-/Window-Stack ohne ImGui-Fehler. Produktionsbuild, nativer Check
und `portable_client` bestanden; `Keim.exe` und Share-ZIP sind aktualisiert.
Bestehende HUD-, Renderer- und Launcher-Belege bleiben gültig.
Configs-Prüfungen decken vollständige Profil-Roundtrips, lokale Speicherpfade,
Save/Overwrite/Load/Refresh und unveränderte Einstellungen nach ungültigen Dateien ab.
Die fünf nativen Vorschauen unter `.impeccable/review/configs/` heißen
`configs-empty.png`, `configs-desktop.png`, `configs-error.png`, `configs-small.png`
und `configs-small-bottom.png`. Sie zeigen 1280 × 800 sowie 480 × 360 mit
synthetischen Fixtures; die Abschlussprüfung bestätigte **ship** nach dem
Overwrite-Fix. Die portable EXE und ZIP wurden gegen die aktuellen Payloads geprüft.
Die aktualisierten nativen HUD-Pruefungen fuer Eingabe/Layout, Style-/Font-Stacks
und Text-Clipping sowie `combat_hud`, `rockstar_features` und `portable_client`
bestanden. Die portable `Keim.exe` und das Share-ZIP wurden neu gebaut.

Aktuelle native WARP-Vorschauen liegen unter `.impeccable/review/module-widgets/`:
`combat-grid.png`, `visuals-grid.png`, `box-settings.png`, `aim-settings-scene.png`,
`grid-small.png`, `settings-small.png`, `settings-large.png`,
`keybinds-desktop.png`, `keybinds-small.png`, `keybinds-small-bottom.png` und
`marker-settings.png`. Die unabhängige Abschlussprüfung hat alle elf Bilder
geöffnet und mit **ship**, ohne wesentliche Befunde, freigegeben. Sie decken
1280 × 800, 1920 × 1080 und 480 × 360 ab; Aim Assist zeigt einen synthetischen
hellen Hintergrund.
Sie zeigen die echte ImGui-Ausgabe mit direkter Alpha-Komposition vor synthetischem
Hintergrund. Sie sind keine Minecraft-Screenshots und prüfen keine Live-Optik,
Attach-/Fokus-Verbindung zum Spiel oder sämtliche GPUs. Die früheren Tab-Bilder
unter `.impeccable/review/compact-client/` sowie ältere Glas-/Rasterbilder
dokumentieren historische Client-Layouts.
Die aktuellen kompakten Injector-Vorschauen stehen in
`.impeccable/review/compact-injector/`; die nativen Bilder sind versteckte
Swapchain-Ausgaben, keine Desktop-Aufnahmen. Die weiterhin gültigen Tests des
erhaltenen früheren Glas-/Capture-Renderers stehen in [liquid-ui.md](liquid-ui.md).

Die sieben aktuellen HUD-Belege liegen unter `.impeccable/review/compact-hud/`:
`modular-hud.png`, `modular-hud-editor.png`, `modular-hud-small.png`,
`rockstar-hud.png`, `hud-catalog.png`, `hud-scene.png` und `hud-long-values.png`.
Sie verwenden den echten nativen ImGui/D3D11-WARP-Renderer mit synthetischen
Fixtures, keine Live-Minecraft-Aufnahme. `hud-catalog.png` ist eine Test-Galerie
aller 19 Elemente und kein Produktionslayout. Alle sieben Belege sind gueltig;
die unabhaengige Impeccable-Abschlusspruefung bestaetigt den behobenen Material-Fix
ohne Regression. Injector-/HUD-Bilder und Testbelege bleiben gueltig; frühere
Client-Tab-Bilder sind historische Belege.

Crystal Macro und Anchor Macro laufen bei aktivierter Funktion, solange die normale
Platzieren-/Benutzen-Taste gehalten wird. Der Modul-Bind steuert die Aktivierung.
Bedienung und Statusmeldungen stehen in [Item Assist](item-assist.md).
