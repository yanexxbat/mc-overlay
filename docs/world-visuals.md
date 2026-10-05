# Trails, Partikel und China Hat

Inspiration sind die visuellen Module [Trails, HitParticles und Particles von
ThunderHack-Recode](https://github.com/Pan4ur/ThunderHack-Recode/tree/main/src/main/java/thunder/hack/features/modules/render).
Die Umsetzung ist eigener C++-Code für den vorhandenen externen D3D11/ImGui-Renderer.
Java-Code, Texturen und Shader des Referenzprojekts wurden nicht übernommen.

Alle Effekt-Module starten ausgeschaltet und ungebunden. Der eigene **China hat**
ist standardmäßig in F5 aktiviert und ebenfalls ungebunden. Unter **VISUALS** erreicht
die zweite Tab-Leiste **Trails**, **Hit particles**, **Ambient particles**, **Hit bubbles**,
**Kill effect**, **BreadCrumbs** und **China hat**;
die Pfeile rechts blättern durch die Leiste. Der Schalter aktiviert das Modul,
das Tastenfeld daneben vergibt einen Toggle-/Hold-Bind. Alternativ sind alle
Belegungen unter **Settings → Keybinds** erreichbar. Bestehende Bind-Indizes
bleiben erhalten; die neuen Aktionen sind angehängt.

## Trails

**Follow** wählt den eigenen Spieler, alle Spieler einschließlich des eigenen,
das aktuelle Ziel oder alle lebenden Entitäten. Bei aktiviertem Kill Aura folgt
**Current target** dessen frischem aktiven Ziel; sonst dem Crosshair-Ziel.
**Self** ist in F5 am besten sichtbar.

**Style** bietet ein vertikales, nach oben weich auslaufendes Band (**Ribbon**),
eine leuchtende Linie oder eine Folge von Orbs. Die Spur speichert Weltpunkte,
keine Bildschirmpositionen, und bleibt beim Drehen der Kamera an ihrem Ort.
Lifetime: 0,2–3 s; Width / orb size: 0,01–0,15 m; Ribbon height: 0,1–2 m;
Opacity: 0–1. Im Stillstand läuft die vorhandene Spur aus.

## Hit particles

Ein beobachteter HP-Verlust einer lebenden Entität erzeugt einen Burst aus Orbs
oder Sternen. **Current target only** begrenzt ihn auf das aktuelle Ziel.
Ein Burst enthält 1–24 Partikel; Lifetime: 0,2–3 s; Size: 0,02–0,2 m;
Speed: 0,1–4 m/s; Gravity: 0–5 m/s²; Opacity: 0–1.

Der vorhandene Snapshot liefert HP, aber kein Angreifer- oder Hurt-Event.
Schaden durch andere Spieler oder die Umgebung erzeugt deshalb ebenfalls einen
Burst. Absorption ohne HP-Verlust wird nicht erkannt. Ein wiederholtes Rendern
desselben Snapshots erzeugt keinen zweiten Burst. Erste Beobachtung, Rückkehr
nach fehlenden Frames, ID-/UUID-Wechsel und Teleports beginnen mit einer neuen
HP-Basis. Partikel besitzen keine Block-Kollision.

## Ambient particles

**Fireflies**, **Stars** und **Snow** verteilen weiche Partikel um den eigenen
Spieler. Amount: 8–160; horizontaler Radius: 2–16 m; Size: 0,015–0,15 m;
Drift speed: 0–2 m/s; Opacity: 0–1. Stil und Drift-Geschwindigkeit wirken sofort.
Partikel blenden ein und aus und werden außerhalb des lokalen Volumens ersetzt.
Es gibt keine Welt-/Block-Abfragen für ihre Bewegung.

Alle Module übernehmen standardmäßig die beiden **Target Aura**-Farben,
auch wenn die Aura selbst ausgeschaltet ist. **Use Target Aura colours**
abschalten, um eigene Start-/Endfarben einzustellen. **Glow intensity** (0–2)
regelt den weichen farbigen Nebel um Partikel, Sterne, Schnee, Orbs und Ringe.
Ein heller Kern bleibt auch bei Glow 0 erhalten; Opacity 0 blendet den ganzen
Effekt aus. Trails, Treffer-, Todes- und Bodenspur-Effekte
teilen Effect distance: 4–128 m. Die üblichen Visuals-Einstellungen bleiben
sessionlokal; Keybinds werden mit dem HUD-Layout gespeichert.

## Hit bubbles und Kill effect

**Hit bubbles** erzeugt bei beobachtetem HP-Verlust einen kameraorientierten,
expandierenden Ring am Trefferort. Er bleibt als Weltpunkt gespeichert und
dreht beim Kameraschwenk keine Bildschirmspur mit. Lifetime: 0,2–2 s;
Ring radius: 0,15–1,5 m; Ring width: 0,005–0,08 m. Der Damage-Trigger besitzt
dieselben Grenzen wie Hit particles: kein Angreifer-/Hurt-Event, sondern HP-Differenz.

**Kill effect** erzeugt beim Übergang von positiven HP zu genau 0 HP einen
horizontalen Ring an den Füßen und einen aufsteigenden Burst aus Orbs oder
Sternen. Particles per death: 8–48; Lifetime: 0,3–3 s; Ground ring radius:
0,3–3 m; Particle size: 0,02–0,2 m; Burst speed: 0,1–4 m/s.
**Current target only** berücksichtigt im Todesframe auch das gerade freigegebene
Ziel. Der Burst bestätigt einen beobachteten Tod, keinen dem Nutzer zugeschriebenen
Kill. Ein Despawn oder eine nicht beobachtete 0-HP-Phase löst ihn nicht aus.
Neue Entitäten, Teleports und UUID-Wechsel erzeugen keine falschen Todes-Effekte.

## BreadCrumbs

**BreadCrumbs** hinterlässt leuchtende Punkte direkt an den eigenen Füßen.
Sie werden ab einem konfigurierbaren Abstand gespeichert und laufen im Stillstand
aus. Lifetime: 0,3–4 s; Point spacing: 0,1–1 m; Point size: 0,02–0,15 m.
Beim Springen folgen die Punkte den Füßen; es gibt keinen Boden-Raycast.

## Eigener China Hat

**China hat** ist standardmäßig aktiviert. Der transparente Kegel und sein
leuchtender Rand verwenden die gleichen Farben und den gleichen Renderer wie
die automatischen Freundehüte. Opacity: 0–0,65; Glow intensity: 0–1;
Brim radius: 0,5–2 × Spielerbreite; Cone height: 0,2–1 × Radius;
Head gap: 0–0,2 m. Die automatische Freundeliste bleibt unabhängig.

Der Agent publiziert jetzt den eigenen Körper mit `kEntityIsSelf` als ersten
Entity-Slot. Kopfpose und Position verwenden die gleiche Interpolation wie
die anderen Entitäten; Kamerarotation wird nicht als Kopfrotation eingesetzt.
Die zusätzlichen Geometrie-Abfragen laufen ausschließlich im bestehenden
JNI-Worker. IPC-Layout und Version bleiben unverändert. ESP und Zielauswahl
schließen den eigenen Körper weiterhin aus. Trails und BreadCrumbs verwenden
jetzt ebenfalls die präzise eigene Position, sofern der neue Agent geladen ist.

Der Hut wird in erster Person standardmäßig über den Abstand der Kamera zum
eigenen Kopf ausgeblendet. Bei einer sehr nahen F5-Kamera durch Wandkollision
kann er dadurch ebenfalls verschwinden. **Also draw in first person** hebt
diese Distanzprüfung auf; Near-Plane-Clipping bleibt erhalten.
Nach dem Update Minecraft neu starten und den aktuellen Injector erneut
anhängen, damit die neue Agent-DLL geladen wird.

## Rendering und Grenzen

`world_visuals.hpp/.cpp` trennt Snapshot-Auswertung, feste Welt-Historie und
Zeichnen. `overlay_world_visuals.cpp` enthält ausschließlich die Menüregler.
Es gibt bis zu 64 Tracks mit je 96 Samples, 384 Trefferpartikel, 160
Ambient-Partikel, 192 Todes-Partikel, 64 Treffer-/Todesringe und 96 BreadCrumbs.
Neue Bursts und Ringe werden bei vollem Pool verworfen; es gibt keine
unbegrenzte Queue. ImDrawList-Speicher wird beim Start reserviert. Die Gauß-Textur
und additive D3D11-Blending-Ressource stammen vom vorhandenen Aura-Renderer;
Callbacks stellen danach den normalen ImGui-Zustand wieder her.

Dimension-/Serverwechsel, zurückgesetzte Frame-Zähler, große Positionssprünge
und Snapshots ohne neue Frames für 350 ms löschen den Effektzustand. Weltpunkte
bleiben in double und verwenden die bestehende Kamera-Projektion. Ohne eigenen
Entity-Record fallen Bodenspuren auf die bisherigen float-Positionsfelder zurück;
ein eigener Hut wird in diesem Fall nicht gezeichnet. Es wurden keine JNI-Aufrufe
im Renderer, Agent-Hooks oder IPC-Felder ergänzt. Das externe Fenster hat
keinen Minecraft-Tiefenpuffer: Effekte sind nicht an Blockflächen verdeckt.

**Reduced motion** stoppt den dekorativen Drift, die Sternrotation und das Ringwachstum; Partikel
blenden weiterhin aus. Trails folgen weiterhin der tatsächlichen Bewegung.

`world_visuals` prüft HP-/Todes-Bursts, Zielverlust im Todesframe, Despawns, Wiederholungen, Teleports, UUID-Wechsel,
Weltwechsel, stale Frames, Poolgrenzen, langsame Bewegung und den aufgewärmten
allokationsfreien DrawList-Pfad. `control_panel` prüft echte Menüeingaben und
rendert D3D11-WARP-Vorschauen in `build/ui-preview/world-*.png`, einschließlich
kleiner Viewports. Ein Live-Test in Minecraft ist dadurch nicht ersetzt.
