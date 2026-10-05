# Kill-Aura-Zielwürfel

Am aktiven Kill-Aura-Ziel erscheint automatisch ein offener gelbgrüner
Würfelrahmen. Vier kräftige, abgerundete Eckklammern bilden die Vorderseite;
dezente hintere Ecken und Verbindungen geben Tiefe. Der Glow nutzt dieselbe
Gauß-Textur und additive RGB-Ausgabe wie Target Aura und Freund-Hüte.
Der solide Kern wird danach mit normalem Alpha-Blending gezeichnet.

`TargetCubeRenderer` trennt Animation, Weltgeometrie, Projektion und Zeichnen.
Der Mittelpunkt folgt der aktuellen interpolierten Hitbox; die Würfelgröße
folgt Höhe und Breite. Die Kamera zieht ihren Ursprung in Double-Präzision ab.
Die Vorderseite bleibt zur Kamera ausgerichtet und dreht um ihre Blickachse.
Eine analytische Sinusbewegung wechselt die Drehrichtung weich alle 3,5 Sekunden,
ohne Sprung in Winkel oder Geschwindigkeit. Die Anfangsdeckkraft ist bereits
65 % und erreicht in 160 ms den vollen Wert.

Unter **VISUALS → Target marker** ist der Marker unabhängig
von **Target aura** konfigurierbar. Die Regler wirken sofort; **Reset** auf der
Visuals-Seite setzt ESP, Aura und Marker auf ihre Standardwerte zurück. Die
Einstellungen gelten für die aktuelle Overlay-Sitzung.

| Regler | Bereich | Standard |
| --- | --- | --- |
| Enable marker | An / aus | An |
| Size | 0,30–1,50 × Hitboxhöhe; mindestens 1,25 × Hitboxbreite/-tiefe | 0,68 × |
| Line width | 0,5–8 % der Würfelkante | 5 % |
| Glow intensity | 0–2; 0 lässt nur den festen Kern stehen | 1 |
| Opacity | 0–1 | 1 |
| Animation | An / aus; aus hält den aktuellen Winkel | An |
| Rotation arc | 20–180° Amplitude der Links-/Rechtsbewegung | 105° |
| Direction switch | 1–10 s zwischen zwei Richtungswechseln | 3,5 s |
| Frame colour / Highlight colour | Zwei RGBA-Farben | Limette / Gelb |

**Shape & timing** enthält zusätzliche Formregler: **Corner length** (10–40 %,
Standard 28 %), **Corner rounding** (0,5–9 %, Standard 4,5 %), **Cube depth**
(hintere Ecken/Verbindungen, standardmäßig an) und **Target fade** (0–0,5 s,
Standard 0,16 s). Ein kürzeres Direction-switch-Intervall erhöht das Tempo;
die aktuelle Phase bleibt beim Ändern erhalten. Animation aus deaktiviert die
Timing-Regler. Die projizierte Linienbreite bleibt zwischen 1,25 und 14 Pixeln.

Der Marker erscheint auch bei ausgeschaltetem ESP und ausgeschalteten Aura-Orbs.
Bei Pause, fehlendem/ungültigem
Ziel, Zielverlust, Tod, Disconnect oder einem über 250 ms alten Combat-Report
wird er entfernt. Bekannte Snapshot-UUIDs müssen der Ziel-UUID entsprechen.
Der explizite **Reduced motion**-Schalter friert die Rotation ein und behält
den Marker sichtbar. Windows-Menüanimationen bremsen den Spieleffekt nicht.

Es gibt keine neuen JNI-Aufrufe, IPC-Felder, Texturen oder Shader. Alle Arrays
sind fest dimensioniert; ImGui-Kapazitäten und Pfadpuffer werden beim Start
vorgewärmt. Ein Würfel an/ hinter der Near Plane wird übersprungen; die
Bildschirmgrenzen begrenzen die Ausgabe. Glow-Callbacks restaurieren den
normalen Renderer-Zustand vor ESP und Menü.

Geprüft mit den vorhandenen WARP-/ImGui-Tests: tatsächliche Glow-Pixel,
Clipping, Blend-Rückgabe, reservierte Vertex-/Indexgrenzen, keine ImGui-
Allokationen im vorgewärmten Zeichnen, große Weltkoordinaten, Auflösung,
Entfernung, beide Drehrichtungen, FPS-Unabhängigkeit und reduzierte Bewegung.
Der Produktionspfad wird zusätzlich ohne ESP/Orbs sowie mit Pause, altem
Report, wiederverwendeter Entity-ID und totem Ziel geprüft. Menütests bedienen
den echten Marker-Reiter, den Aktivierungsschalter und den Größenregler;
Renderer-Tests prüfen freie Farben, Glow aus, Deckkraft null, Tiefenlinien,
Einblenden, Pause/Fortsetzen und Tempoänderung ohne Winkelsprung.

Vorschaubilder: `build/ui-preview/target-cube.png`, `target-cube-esp.png`
und `target-cube-small.png`; freie Farben zeigt `target-cube-custom.png`.
Die Regler zeigen `marker-settings.png`, `marker-settings-details.png` und
`marker-settings-small.png`. Diese Bilder zeigen die Overlay-Geometrie ohne
laufenden Minecraft-Renderer. Die Optik im Spiel wurde nicht live geprüft.
