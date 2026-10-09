# Kompaktes, transparentes Client-, Injector- und HUD-Theme

Das bestaetigte visuelle Theme folgt `design/reference-translucent-menu.png`:
eine ruhige anthrazitfarbene Oberflaeche mit schmaler Sidebar, kleinen Reglern
und gedaempftem Korallorange. Das aktuelle Client-Layout folgt der vom Benutzer
bereitgestellten Paint-Skizze `design/reference-module-widgets.png`: Module stehen
direkt als Widgets in ihren Kategorien; Rechtsklick oeffnet ein zweites,
unabhaengiges Settings-Fenster. Dies ersetzt Modul-Tabs, Inline-Settings und
Quick toggles unter Beibehaltung des freigegebenen Themes.
Echte mcov-Module bleiben erhalten; Bezeichnungen/Funktionen aus dem fremden
Referenzprogramm werden nicht als neue Features uebernommen.

Das produktive Menue liegt in `src/overlay/control_panel.cpp`: `RenderMenu()` wird
ueber den bestehenden kompatiblen `Render()`-Einstieg von `Overlay::DrawMenu()`
aufgerufen. `src/common/compact_style.hpp` stellt `ApplyCustomStyle()` bereit.
`CompactStyle` wendet das Theme lokal an und stellt danach Style und Font wieder her.
Der Injector verwendet dasselbe freigegebene Theme und denselben lokalen
`CompactStyle`; die fruehere Lavender-/Liquid-Glass-Darstellung ist abgeloest.
Die 19 Snapshot-HUD-Elemente und das optionale Combat-Debug-HUD verwenden nun
dieselbe Gestaltung. `src/common/compact_theme.hpp` zentralisiert ihre Zeichentokens;
der HUD-Editor verwendet `CompactStyle` lokal. Welt-ESP, Aura, Partikel und Hats
behalten ihre Gestaltung.

## Layout und Bedienung

- Hauptfenster: 740 x 520 Pixel, Settings: 480 x 580 Pixel. Beide sind auf den
  Viewport minus 24 Pixel begrenzt und bleiben beim Ziehen darin. Rahmenlos mit
  12 Pixel Rundung und Alpha 0.88. Die erste Hauptfenster-Platzierung reserviert
  nach Moeglichkeit bis zu 496 Pixel rechts fuer Settings.
- Sidebar: 150 Pixel, unter 620 Pixel Fensterbreite 112 Pixel. Sie enthaelt
  sieben Kategorien, eine Suche und einen leisen Verbindungsstatus.
- Die 76 vorhandenen Module/Abschnitte stehen im scrollbaren `module_grid`-Child.
  Ein Raster mit gleich breiten Spalten verwendet floor(verfuegbare Breite / 174),
  begrenzt auf eine bis drei Spalten, und 5 x 5 Pixel Tabellen-Zellabstand.
  Native Widgets sind 66 Pixel hoch und mit 8 Pixel Rundung gezeichnet.
- Die Suche filtert die Modulnamen der Kategorie ohne Gross-/Kleinschreibung.
  Ein leerer Trefferzustand bietet Clear search. Kategorienwechsel leert die Suche.
- Linksklick oder Space aktiviert das echte Toggle-/Action-Modul; Settings-only
  oeffnet seine Details. Rechtsklick, Menu oder Shift+F10 oeffnet Settings ohne
  Toggle. Der Widget-Titel beginnt bei x/y 12 mit Inter 14; die Statuszeile bei
  y 39 zeigt Enabled, Disabled, Open settings oder Run action. Toggle-Indikator
  unten rechts, vorhandene Belegung im Tooltip.
- Widget-Ruhefarbe ist das vorhandene Control-Weiss mit Alpha .045, Hover .085.
  Enabled verwendet Korall mit Alpha .10, Enabled-Hover das bestehende
  Selected-Tab-Korall .16 und Pressed Korall .22.
- Settings hat einen eigenen verschiebbaren Header, X-Schliessen und einen
  scrollbaren Content-Child mit 24 Pixel horizontalem und 52 Pixel oberem Inset.
  Ein anderes Modul setzt Settings-Scroll zurueck; dasselbe erneut oeffnen erhaelt
  ihn. Kategorienwechsel schliesst Details und leert die Suche.
- Desktop zeigt beide Fenster nebeneinander. Unter 1260 Pixel Viewport-Breite
  ist bei offenen Settings nur dieses Fenster sichtbar; X bringt das Raster
  zurueck und vermeidet ueberlagerte transparente Texte.
- Labels und Werte haben eigene Tabellenspalten. Checkboxen und Farbfelder sitzen
  rechts; Labels schalten Checkboxen ebenfalls. Keybinds stehen vor der Checkbox.
  Native Slider unterstuetzen Maus, Tastatur und Ctrl-Klick zur Zahleneingabe.
- Unter 420 Pixel Content-Breite steht in Keybinds jeder Modulname in einer
  eigenen Zeile ueber dem Tastenfeld und Toggle/Hold-Modus. Tastenfelder bleiben
  innerhalb der verfuegbaren Breite; Widget-IDs bleiben beim Layoutwechsel stabil.
- Beide Header lassen sich unabhaengig ziehen. Center setzt beide Platzierungen
  zurueck, Reset die aktuelle Kategorie und Close schliesst Menue und Details.
  Enabled und Run action im Settings-Header sind an die vorhandenen Module gebunden.
  HUD-Editor, Keybinds, Friend-Listen und Minecraft/JNI-Logik bleiben erhalten.

## Injector

Der Launcher oeffnet mit 640 x 640 Pixeln und 12 Pixel Aussenabstand.
Die Start/Diagnose-Sidebar ist 142 Pixel breit, unter 620 Pixel Viewport-Breite
112 Pixel. Kleine selbst gezeichnete Icons und 14-Pixel-Inter entsprechen dem
Client. Die echte Instanzliste zeigt PID/Java/Client-Status und den vollstaendigen
Fenstertitel im Tooltip. Status, JDK-Auswahl und Diagnose scrollen; der
34-Pixel-Startbutton und Schliessen-Footer bleiben fest erreichbar, auch bei
480 x 380. Aktiver Starttext ist fast schwarz, deaktivierter Text weiss mit dem
gemeinsamen DisabledAlpha. Native Titelleisten-Drag, Minimieren/Schliessen,
asynchroner Start, Instanzauswahl, JDK, Logs und Packaging bleiben erhalten.
Weitere Bedienung und Laufzeitdetails: [launcher.md](launcher.md).

## HUD

Alle Elemente verwenden den gemeinsamen 14-Pixel-Inter-`MenuFont` und einen
einzelnen gerundeten Fill (8 Pixel), ohne Border, Stripe oder Schatten.
Ueberschriften und Divider bleiben ruhig; aktive Zustaende sind korallfarben.
Positive Rueckmeldung ist gruen, Warnung amber und Gefahr rot. Die Zeichentokens
verwenden Surface RGBA(34,34,41,224), Text (236,236,239), Muted (174,174,184),
Accent (239,139,112), AccentActive (255,163,136), Separator weiss mit Alpha 17,
Track weiss mit Alpha 12 und Pressed coral mit Alpha 46. Positive (165,199,173),
Warning (232,187,115) und Danger (242,128,128) tragen semantisches Feedback.

Die F6-Toolbar ist maximal 540 Pixel breit oder Viewport minus 24, mit 14 x 10
Pixel Padding. Ihre Hoehe ist regulaer 90 Pixel und 110 bei Viewport-Breite unter
640 oder Hoehe unter 480 Pixeln. In der kleinen Editoransicht wird nur das
ausgewaehlte Element gezeigt; die Auswahl erreicht alle 19. Auswahloutline coral,
Disabled-Badge nur am ausgewaehlten deaktivierten Element. Drag, Reset, Tasten und
Positions-/Skalen-/Sichtbarkeits-Persistenz bleiben erhalten.

Zeilen-Clipping begrenzt lange Name-/Item-/Effect-/Keybind-Texte auch nach Skalierung.
Der Reach-Readout verwendet die Surface unterhalb der offenen Fadenkreuzklammern; sein
Name clippt vor einer getrennten HP-/Distanz-Spalte. Das HUD bleibt ein
Snapshot-Verbraucher ohne Render-Thread-JNI, Desktop-Capture, Blur oder neuen Renderer.

## Material und Leistung

Anthrazit `#222229` mit Alpha 0.88 wird direkt ueber die transparente
Overlay-Ausgabe komponiert. Die Sidebar ist leicht dunkler. Text `#ECECEF`,
Sekundaertext `#AEAEB8`, Akzent `#EF8B70`, aktiv `#FFA388`. Kleine Controls haben
4 Pixel Rundung; Child-Style 8, Popups 6 Pixel. Fenster- und Frame-Border sind 0.

Client und Injector zeichnen gewoehnliche ImGui-Geometrie und benoetigen weder
eine Desktop-Aufnahme noch Blur oder Refraction. Der bestehende Glasrenderer bleibt
im Client inaktiv. `FlatApp` hat LiquidGlass-Member, Initialisierung, BeginFrame,
Rendern und Shutdown entfernt. Der native Injector loescht RGBA auf 0 und verwendet
DWM-Margins −1 sowie eine Fensterregion mit 24 Pixel Eckdurchmesser passend zum
12-Pixel-Radius. Der erste Frame wird vor `ShowWindow` praesentiert.
Die eingestellte Capture-Affinity des Clients gilt auch bei offenem Menue.
Settings > Overlay zeigt Translucent dark / 88% opacity und Not required an.
Material, Lizenz und historische Capture-Tests des weiterhin vorhandenen
Glasrenderers stehen in [liquid-ui.md](liquid-ui.md).

## Verifikation

Der native D3D11/WARP-Check prueft reale Rechtsklicks fuer alle 76 Abschnitte,
Toggle-Aktionen gegen echte Stores, Space/Shift+F10, unabhaengiges Drag/Close,
kleine Settings schliessen mit Rueckkehr zum Raster, Such-Wiederherstellung,
Keybinds, Slider, Combos, Farben, Marker, Persistenz und Style-/Font-/Window-Stack
ohne ImGui-Fehler. Produktionsbuild, nativer Check und `portable_client` bestanden;
`mcov.exe` und Share-ZIP wurden aktualisiert.
Offscreen-Vorschauen mit synthetischem Hintergrund beweisen die ImGui-Darstellung
und Alpha-Komposition; sie sind keine Live-Aufnahme aus Minecraft.
Die elf aktuellen Ansichten unter `.impeccable/review/module-widgets/` sind
`combat-grid.png`, `visuals-grid.png`, `box-settings.png`, `aim-settings-scene.png`,
`grid-small.png`, `settings-small.png`, `settings-large.png`,
`keybinds-desktop.png`, `keybinds-small.png`, `keybinds-small-bottom.png` und
`marker-settings.png`. Sie decken 1280 x 800, 1920 x 1080 und 480 x 360 ab;
Aim Assist verwendet einen synthetischen hellen Hintergrund. Die unabhaengige
Abschlusspruefung hat alle elf Bilder geoeffnet und **ship** ohne wesentliche
Befunde bestaetigt. Die frueheren Tab-Bilder unter
`.impeccable/review/compact-client/` sind historisch.

Die aktuellen Injector-Checks bestehen fuer Auswahl, Start, Verhinderung doppelter
Starts, beide Sidebar-Tabs, feste Footer und Titel-Minimieren/Schliessen. Der native
versteckte HWND-/Swapchain-Check prueft Panel-Alpha, transparente Aussenraender,
lesbare Controls, Resize und Fensterregion. Alle neun Bilder unter
`.impeccable/review/compact-injector/` sind freigegeben. Die `launcher-native-*`
Dateien sind versteckte Swapchain-Ausgaben vor OS-Clipping; sie beweisen keine
Live-Desktop-Komposition oder Minecraft-Optik. `mcov`, `mcov_injector`, native/UI-
Checks und das portable ZIP mit exakten Payloads bestehen.

Die sieben HUD-Bilder unter `.impeccable/review/compact-hud/` sind
`modular-hud.png`, `modular-hud-editor.png`, `modular-hud-small.png`,
`rockstar-hud.png`, `hud-catalog.png`, `hud-scene.png` und `hud-long-values.png`.
Alle verwenden den echten nativen ImGui/D3D11-WARP-Renderer mit synthetischen
Fixtures und sind keine Live-Minecraft-Captures. `hud-catalog.png` ist die
Test-Galerie aller 19 Elemente, kein Produktionslayout. Alle sieben sind gueltig;
die unabhaengige Impeccable-Abschlusspruefung meldet den Material-Fix als behoben
und keine Fixregression. Native HUD-Eingabe-/Layout-/Style-/Font-/Clip-Checks,
`combat_hud`, `rockstar_features` und `portable_client` bestanden; `mcov.exe`
und ZIP wurden neu gebaut. Injector-/HUD-Bilder bleiben aktuell; alte Client-Tab-
Bilder bleiben als historische Belege erhalten.
