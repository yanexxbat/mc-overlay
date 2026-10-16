# Verdeckung von Welt-Effekten

Für **Minecraft 1.21.11 / Fabric / normales OpenGL** besitzt der Client jetzt
einen separaten Tiefenkanal. Unter **Settings → Overlay → Occlude world effects**
ist die Verdeckung standardmäßig eingeschaltet. Minecraft nach diesem Update
vollständig neu starten und anschließend `build/client/Keim.exe` öffnen:
Agent, Java-Helfer und Overlay verwenden jetzt IPC **18**.

- **Active:** ein frischer Tiefenframe wird verwendet.
- **Waiting:** passende Tiefendaten fehlen noch; Welt-Effekte bleiben ausgeblendet.
- **Unavailable:** der aktuelle Renderer oder sein Tiefenformat ist inkompatibel.
- **Idle:** es sind keine entsprechenden Effekte sichtbar.
- **Off:** die Option ist ausgeschaltet.

Aura-Orbs und Schweife, Partikel, Trails, Sprung-/Zielringe, Wings, Halo,
Freund-/Self-Hats sowie der Target-Marker werden pixelweise mit der Spieltiefe
verglichen. Ein Orb hinter einem Spieler verschwindet; ein teilweise verdeckter
Ring bleibt im sichtbaren Bereich erhalten. HUD, Nametags, Glow-/Storage-ESP,
Wegpunktbeschriftungen und Schadenszahlen behalten ihre bisherige Sichtbarkeit.
Bei ausgeschalteter Option werden die Effekte wie bisher über die Welt gelegt.
Andere Minecraft-Versionen verwenden vorerst das bisherige Verhalten.

## Ablauf und Grenzen

Der Java-Hook sitzt im Weltpass unmittelbar **vor** `clearDepthTexture`, das die
Handdarstellung vorbereitet. Der native Agent liest Minecrafts OpenGL-Tiefentextur
über einen eigenen FBO und drei feste PBOs. Fence-Abfragen warten nicht auf die GPU.
Lesen, Schreiben und Wiederherstellen des GL-Zustands erfolgen ausschließlich auf
Minecrafts GL-Thread. Der externe Renderprozess führt weiterhin keine JNI-Aufrufe aus.
Ohne benötigte Welt-Effekte wird die native Render-Brücke deaktiviert.

Tiefenframe und Kamera-/Entity-Snapshot müssen dieselbe Render-Epoche besitzen.
Ein fester Cache ordnet beide auch dann zu, wenn der Sampler erst nach dem
GPU-Capture fertig wird. Uebersprungene Epochen blockieren die PBOs nicht.
Der Kanal enthält beide zusammen. Frames älter als 100 ms, Weltwechsel und falsche
Bildschirmproportionen werden verworfen. Das Overlay verwendet einen zusätzlichen
Vertex-Stream für inverse Sichttiefe und einen D3D11-Pixelshader zum Vergleich.
Nahe Clipping-Kanten interpolieren diese Tiefe perspektivisch korrekt. ImGui-Texturen,
Blend-Callbacks und der normale HUD-Renderzustand werden wiederhergestellt.

Die feste Tiefenauflösung **960 × 540** hält Transfer und GPU-Speicher begrenzt;
eine konservative Nachbarprobe kann an dünnen Kanten einen kleinen Rand ausblenden.
Transparente Materialien verdecken nur dann, wenn Minecraft sie in den Tiefenpuffer
schreibt. Shaderpacks, Vulkan, abweichende Mod-Projektionen und Renderziele werden
nicht als unterstützt zugesichert. Hurt-/Portal-Verzerrungen können von der
rekonstruierten Kamera abweichen. Diese Integration wurde mit synthetischen
GPU-/Java-Tests geprüft; der Live-Test in Minecraft steht noch aus.

Das Speedometer nutzt den interpolierten eigenen Entity-Record und den
Erfassungszeitpunkt, unabhängig vom Tiefenframe. Die langsamen HUD-Positionsfelder
und schwankende IPC-Ankunftszeiten bestimmen die Geschwindigkeitsanzeige nicht.

## Prüfungen

`depth_rendering` zeichnet mit ImGui/D3D11 WARP: teilweise verdeckter Effekt,
Effekt vor der Fläche, fehlende Tiefendaten, mehr als 64K Vertices, vorgeschaltete
DrawList sowie unabhängiges HUD. `depth_hook` lädt echte transformierte Bytecodes
für Vanilla-, Fabric- und Mojmap-Namen und prüft den Callback vor dem Depth-Clear
einschließlich unverändertem Operand-Stack. `control_panel` prüft die gespeicherte
Option; die vorhandenen Frame-/JNI-Profiltests prüfen die übrige Integration.

`depth_capture_gl` prueft den echten nativen OpenGL-Leseweg mit verstecktem
WGL-Kontext, PBOs und Fences: Minecrafts DEPTH_COMPONENT32 sowie 32F/24/16,
Float-Konvertierung, vertikale Umkehrung, spaeten Snapshot und wiederhergestellten
GL-Zustand. JNI liefert in dieser Pruefung feste Ressourcen; Minecraft wird nicht
gestartet oder veraendert. Ohne geeigneten GL-Treiber wird der Test als uebersprungen markiert.
