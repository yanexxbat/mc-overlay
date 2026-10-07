# Freund-Hüte

COMBAT → Friends → Spieler-UUID eingeben → Add friend. Freunde erhalten
automatisch einen leicht transparenten Kegelhut mit violettem/türkisem Leuchtrand.
Remove entfernt zugleich Freund-Markierung und Hut. Die Freundeliste gilt für
die laufende Sitzung; Kill Aura ignoriert diese UUIDs weiterhin.

Der Hut verwendet die beiden Farben der Target Aura und deren vorhandene
Gauß-Textur mit additivem RGB-Blending. Die Kegelfläche verwendet normales
Alpha-Blending mit 28 % Grunddeckkraft. Der Renderzustand wird nach dem Glow
wiederhergestellt. Target Aura und ESP müssen dafür nicht eingeschaltet sein.
Die Visuals-Reichweite begrenzt auch Freund-Hüte.

`FriendHatRenderer` trennt Kopfgeometrie, Welt-/Bildschirmprojektion und Zeichnen.
Die interpolierte Position, Körper-Yaw, Kopf-Pitch und Pose kommen aus demselben
Snapshot wie ESP. Ducken, Schwimmen und Fliegen verwenden die vorhandene
Humanoid-Geometrie; modifizierte Spieler-Modelle bleiben eine Näherung. Bei
Near-Plane-Schnitt wird der Hut verworfen, statt große Dreiecke zu zeichnen.
Für 1.21.11 Fabric / normales OpenGL verdeckt der neue
[Tiefenvergleich](world-depth.md) Hüte hinter Spielern und Blöcken.

Die UUID liegt in `EntitySnapshot`. Der Sampler liest sie einmal pro Java-Objekt
und speichert sie mit einer schwachen Referenz in `EntityIdentityCache`.
Wiederverwendete Entity-IDs werden per Objektidentität erkannt; Weltwechsel und
Shutdown geben die Referenzen frei. Das Rendering führt keine JNI-Aufrufe aus.
UUIDs fehlen bei erfolgloser Auflösung; solche Entities erhalten keinen Hut.
Arrays und Kreisgeometrie haben feste Kapazitäten; ImGui-Buffers werden einmal
vorgewärmt. Das neue Snapshot-Layout benötigt IPC v19 und aktualisierte Agent-,
Injector- und Overlay-Binaries. Minecraft nach dem Update vollständig neu starten.

Prüfungen: UUID-Cache und Snapshot-Publikation in allen JNI-Profil-Fixtures;
GPU-Pixel, Transparenz, Freund-Entfernung, Perspektiv-/Auflösungsskalierung,
Ducken, Bewegungsanker und allokationsfreies Zeichnen nach dem Vorwärmen.
Die Produktionsvorschauen liegen in `build/ui-preview/friend-hat.png` und
`friend-hat-crouched.png`. Ein Test in einem laufenden Minecraft bleibt offen.
