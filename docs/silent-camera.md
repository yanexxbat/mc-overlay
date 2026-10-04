# Kill Aura: feste stille Rotation

Unter COMBAT → Kill Aura **Enable Kill Aura** einschalten, Menü schließen
und F5 nutzen. Die Kill Aura dreht und schlägt mit dem Spieler; die Maus steuert
immer die unabhängige Blickrichtung. Stille Rotation gehört fest zur Kill Aura,
auch direkt nach einem Reset. Es gibt keinen Silent-Kamera-Schalter und kein Preset.
Standardmäßig werden Spieler und Mobs in 3 Metern Reichweite berücksichtigt,
unabhängig von der Blickrichtung. Die nächste gültige lebende Hitbox wird gewählt;
ein näherer Kandidat löst das bisherige Ziel am nächsten Client-Tick ab.
Die getrennte Blickrichtung wirkt auch in der Egoansicht.
Normales View-Bob, Kamera-Kollisionen und Kameramods können den Blick weiterhin
bewegen.

## Zwei getrennte Winkel

`MinecraftCombat` schreibt Ziel-Yaw/Pitch auf dem Minecraft-Client-Tick in die
echte Entity. Das Spiel nutzt diese Werte für das Spielermodell, Hitbox-Raycasts
und seine normale Movement-Pipeline. Es gibt keine zusätzliche Packet-Queue für
gespoofte Winkel.

Der Combat-Hook läuft am Ende des normalen Client-Ticks. Zielpositionen und
Spielerposition stammen damit aus demselben abgeschlossenen Spielupdate.
`yHeadRot`, `yBodyRot` und deren alte Interpolationswinkel werden auf die
angewendete Spielerrotation synchronisiert; Vanilla kann sie danach vor dem
Rendern nicht mehr durch Bewegungswinkel ersetzen. Die normale Movement-Pipeline
überträgt die angewendete Rotation im folgenden Player-Tick.

Die Rotation Engine beginnt jeden Tick bei den tatsächlich gemessenen Winkeln,
statt verzögerte Queue-Posen als bereits angewendet zu behandeln. Beim Nachführen
eines bewegten Ziels darf die Bézier-Tangente nur in dessen neue Richtung zeigen.
Mit dem Standard-Overshoot/Jitter von null dreht die Kurve nicht zuerst vom Ziel
weg. Die Schrittgrenzen und die unabhängige Maus-Kamera bleiben erhalten.

Beim Aktivieren übernimmt `mcov.FrameSignal` die bisherige Blickrichtung und
eine Referenz auf den lokalen Spieler. Die instrumentierte Camera reicht
Aufrufe an `Entity.getViewYRot(float)` und `getViewXRot(float)` durch reine
Java-Wrapper. Für diesen Spieler liefern sie die unabhängigen View-Winkel;
für andere Entities bleibt das ursprüngliche Ergebnis erhalten. Das passiert
vor der F5-Abstandsberechnung, damit auch die Kameraposition stabil bleibt.
26.2 liest die Winkel in `Camera.alignWithEntity`; die Transformation prüft
deshalb alle Methoden der Camera, einschließlich privater Hilfsmethoden.

Der Hook an `Entity.turn(double,double)` leitet Mausdeltas des lokalen Spielers
in die View-Winkel und gibt null Deltas an die ursprüngliche Methode weiter.
Die Vanilla-Maussensitivität wird vorher berechnet; der abschließende
Entity-Faktor von 0,15 bleibt erhalten. Pitch ist auf ±90° begrenzt. Andere
Entities und der abgeschaltete Modus behalten ihre ursprünglichen Deltas.

## Thread, Pause und Lebensdauer

Die Kamera- und Mouse-Wrapper rufen kein JNI auf und erzeugen keine Objekte.
JNI läuft für Combat nur im Client-Tick. Die externe ImGui-Anwendung publiziert
Config/Lease und liest Diagnosen; der ESP-Sampler liest die fertig berechnete
Camera wie zuvor.

Beim Abschalten, Zielverlust, Menü, Fokusverlust oder abgelaufener Lease setzt
der Client-Tick die Spielerwinkel auf die zuletzt manuell gesteuerte
Blickrichtung zurück. Die alten Interpolationswinkel werden ebenfalls gesetzt;
danach wird die Camera-Override-Referenz entfernt. Ein Wechsel von Welt oder
Spieler entfernt zuerst die alte Referenz und erfasst dann die neue Blickrichtung.
Der reine VM-/Sampler-Close schreibt keine Entity von einem fremden Thread;
beim harten Agent-Unload kann deshalb einmal die letzte Spielerrotation sichtbar
werden. Das reguläre Beenden des Overlays publiziert vorher einen Stop.

Der native Adapter akzeptiert Combat-Aufträge nur, wenn Camera-Achsen und
Mouse-Hook für das ausgewählte Runtime-Profil instrumentiert wurden und die
View-/Interpolationsfelder gebunden sind. Andernfalls zeigt das Menü
**Camera hook missing - restart Minecraft**; Rotation und Angriffe bleiben gesperrt.
Es gibt keinen Rückfall auf sichtbares Drehen der Kamera.

## Prüfung und Grenzen

Die Signaturen wurden für 26.2, 1.21.11, 1.21.1 und 1.20.1 aus den lokal
checksum-geprüften offiziellen JARs abgeleitet. Mojmap-, Vanilla-, Intermediary-
und Forge-SRG-Profile nutzen ihre jeweiligen Namen. Die tatsächlichen
Camera-Getter-Aufrufstellen wurden zusätzlich per Bytecode geprüft.

Die JNI-Tests verwenden 22 synthetische Klassenwelten (elf Profile mit jeweils
Accessor- und Field-Camera). Sie prüfen automatische Rotation/Angriffe bei
stabiler View schon bei erster Aktivierung ohne Preset, eine aus dem View-Winkel
abgeleitete F5-Orbitposition, Maussteuerung,
Zielwechsel auf die gegenüberliegende Seite, fremde Entities, Partial-Ticks,
Menü-Pause/Rückkehr und Rückgabe beim Abschalten. Fehlende Camera-Hooks sperren
in einem negativen Test Rotation und Angriffe. Dynamisches Attach samt Camera-
und Mouse-Hooks wird in einer separaten Test-JVM an bereits geladenen Klassen
geprüft. Diese Tests unter Java
21 enthalten keinen Minecraft-Code; Live-F5-Rendering und Mods, die die Camera
ersetzen, müssen im laufenden Spiel geprüft werden. Neue DLL und beide Helper-
JARs benötigen einen vollständigen Minecraft-Neustart.
