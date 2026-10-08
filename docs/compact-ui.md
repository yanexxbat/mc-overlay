# Kompaktes, transparentes Client-Menue

Die visuelle Referenz ist `design/reference-translucent-menu.png`, der vom Benutzer
bereitgestellte Screenshot. Er ersetzt das erste Cyan-Beispiel und das bisherige
Kachel-Menue des Clients. Ziel ist eine breite, ruhige anthrazitfarbene Oberflaeche
mit schmaler Sidebar, oberen Modul-Tabs, kleinen Reglern und gedaempftem Korallorange.
Echte mcov-Module bleiben erhalten; Bezeichnungen/Funktionen aus dem fremden
Referenzprogramm werden nicht als neue Features uebernommen.

Das produktive Menue liegt in `src/overlay/control_panel.cpp`: `RenderMenu()` wird
ueber den bestehenden kompatiblen `Render()`-Einstieg von `Overlay::DrawMenu()`
aufgerufen. `src/common/compact_style.hpp` stellt `ApplyCustomStyle()` bereit.
`CompactStyle` wendet das Theme lokal an und stellt danach Style und Font wieder her.
Der Launcher behaelt seine separate Liquid-Glass-Darstellung.

## Layout und Bedienung

- Root: 1040 x 520 Pixel, auf den Viewport minus 24 Pixel begrenzt und beim Ziehen
  im Viewport gehalten. Rahmenlos mit 12 Pixel Rundung und Alpha 0.88.
- Sidebar: 150 Pixel, unter 620 Pixel Fensterbreite 112 Pixel. Sie enthaelt
  sieben Kategorien, eine Suche und einen leisen Verbindungsstatus.
- Die 76 vorhandenen Module/Abschnitte sind direkt ueber obere Tabs erreichbar.
  Ueberlaufende Tabs scrollen horizontal mit den nativen ImGui-Pfeilen.
- Die Suche filtert die Modulnamen der Kategorie ohne Gross-/Kleinschreibung.
  Ein leerer Trefferzustand bietet Clear search. Kategorienwechsel leert die Suche.
- Genau zwei Child-Bereiche: Navigation und Content. Die Tabs stehen oberhalb des
  Content-Scrollbereichs und bleiben sichtbar. Jede Kategorie behaelt ihren
  gewaehlten Abschnitt; ein Modul-/Kategorienwechsel setzt Content-Scroll zurueck.
- Ab 660 Pixel Content-Breite stehen links Detailregler und rechts Quick toggles
  fuer andere Module derselben Kategorie. Bei schmaleren Fenstern bleiben alle
  Abschnitte ueber Tabs und der aktuelle Enable-Schalter erreichbar.
- Labels und Werte haben eigene Tabellenspalten. Checkboxen und Farbfelder sitzen
  rechts; Labels schalten Checkboxen ebenfalls. Keybinds stehen vor der Checkbox.
  Native Slider unterstuetzen Maus, Tastatur und Ctrl-Klick zur Zahleneingabe.
- Header ziehen, Center, kategorienbezogenes Reset und Close funktionieren weiter.
  Enable und Run action im Header sind an die vorhandenen Module gebunden.
  HUD-Editor, Keybinds, Friend-Listen und Minecraft/JNI-Logik bleiben erhalten.

## Material und Leistung

Anthrazit `#222229` mit Alpha 0.88 wird direkt ueber die transparente
Overlay-Ausgabe komponiert. Die Sidebar ist leicht dunkler. Text `#ECECEF`,
Sekundaertext `#AEAEB8`, Akzent `#EF8B70`, aktiv `#FFA388`. Kleine Controls haben
4 Pixel Rundung; Child-Style 8, Popups 6 Pixel. Fenster- und Frame-Border sind 0.

Der Client zeichnet gewoehnliche ImGui-Geometrie und benoetigt weder eine
Desktop-Aufnahme noch Blur oder Refraction. Der bestehende Glasrenderer bleibt
im Client inaktiv. Die eingestellte Capture-Affinity gilt auch bei offenem Menue.
Settings > Overlay zeigt Translucent dark / 88% opacity und Not required an.

## Verifikation

Der native D3D11/WARP-Check prueft alle 76 Modul-Tabs, echte Toggle-Aktionen,
Tastaturbedienung, Suche, Style-/Font-/Window-Stack, Drag, kompakte Darstellung,
Slider inklusive sichtbarer Ctrl-Eingabe, Keybinds, HUD und bestehende Renderer.
Offscreen-Vorschauen mit synthetischem Hintergrund beweisen die ImGui-Darstellung
und Alpha-Komposition; sie sind keine Live-Aufnahme aus Minecraft.
