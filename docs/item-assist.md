# Chunkfinder, Crystal Macro, Anchor Macro und Auto Totem

## Bedienung

**Combat → Auto Totem:** aktivieren und Totems in Hotbar oder Hauptinventar
legen. Ein fehlendes Totem in der Offhand wird mit einem regulären Container-Swap
ersetzt. Ein bereits getragenes Totem wird nicht ständig erneut gewechselt.
Die Statuszeile nennt fehlende Totems, blockierte Inventaraktionen und fehlende
Bindings. Ein geöffnetes fremdes Inventar oder ein Item am Mauszeiger blockiert
den Swap. Pausiertes Spiel, Overlay-Menü, HUD-Editor und Fokusverlust pausieren
ebenfalls. Der Retry-Abstand ist 50–500 ms, standardmäßig 100 ms.

**Combat → Crystal Macro:** aktivieren, End Crystals auswählen und Minecrafts
normale Platzieren-/Benutzen-Taste auf Obsidian oder Bedrock halten. Das Macro
speichert diesen Platz: platzieren → Crystal automatisch zerstören → erneut
platzieren. Der Crystal muss zwischen den Schritten nicht neu anvisiert werden.
Er wird anhand seiner Position über dem gewählten Block gesucht. Die üblichen
Platzierungs- und Reichweitenregeln gelten weiterhin.

**Combat → Anchor Macro:** aktivieren, Anchor auswählen und Anchor, Glowstone
sowie ein weiteres Item in der Hotbar bereithalten. Minecrafts Platzieren-Taste
auf einem Platzierungsblock oder vorhandenen Anchor halten. Am gespeicherten
Platz läuft automatisch: platzieren → laden → detonieren → erneut platzieren.
Ladestand 0 wählt Glowstone; ein geladener Anchor wird mit einem anderen Item
ausgelöst. Glowstone in der Offhand blockiert diesen
Schritt bei Ladestand 1–3; die Statuszeile fordert dann zum Entfernen auf. Die Dimensionsregel kommt aus Minecraft:
funktionierende Respawn-Dimensionen erlauben keine Detonation durch das Macro.

Beide Modul-Binds sind anfangs ungebunden und unterstützen Toggle oder Hold.
Sie steuern die Bereitschaft. Der Schalter alleine führt keine Aktion aus.
Auslöser ist die echte Minecraft-Use-Taste, auch bei eigener Tastenbelegung.
Beim Loslassen stoppt die Schleife und der zuvor ausgewählte Slot wird
standardmäßig wiederhergestellt. Manuelle Slotwahl, Wegsehen, Weltwechsel,
Menus, Fokusverlust oder aktive Kill Aura beenden den Vorgang.
Bei blockierendem Inventar-/Pausemenü wartet die Slot-Wiederherstellung bis zum
Schließen; eine inzwischen manuell geänderte Auswahl bleibt erhalten.
Beide Module können bereit sein; das ausgewählte Item entscheidet über den Ablauf.
Der Aktionsabstand ist 100–500 ms, standardmäßig 150 ms. Jede Folgeaktion wartet
auf Spawn, Entfernung, Block- oder Ladestandsänderung. Der Bestätigungs-Timeout
ist 100–2.000 ms, standardmäßig 750 ms; erst danach darf eine unbeantwortete
Aktion wiederholt werden. „Actions sent“ zählt erfolgreiche Spielinteraktionen,
keine abschließende Serverbestätigung.

**Blocks → Chunkfinder:** aktivieren, Radius und Mindestzahl der Hinweise pro Chunk
wählen. Gesucht wird ausschließlich roher Deepslate mit horizontaler X/Z-Achse.
Vertikaler, polierter und befallener Deepslate sowie Deepslate-Erze zählen nicht.
Würfelkanten markieren die Blöcke; Labels zeigen Chunk-Koordinaten und Trefferzahl.
Negative Koordinaten verwenden Floor-Division. Die Hinweise sind kein Nachweis
einer Basis und können auch durch Weltgenerierung und andere Bauten entstehen.

Der Scan sieht nur lokal geladene Blöcke in einem Würfel um den Spieler. Es gibt
keine Erkennung entfernter/unbekannter Chunks. Radius 4–48, Standard 20; maximal
1.024 Treffer gemeinsam mit Block ESP. Größere Radien brauchen länger. Eine
Fortschrittsanzeige und frühe Treffer sind bereits während eines Durchlaufs
sichtbar. Welt- und Filterwechsel verwerfen alte Resultate.

## Architektur und Prüfung

`common/item_assist.hpp` entscheidet ohne JNI über genau eine Aktion pro Tick.
`agent/item_assist_live.hpp` liest Inventar/Crosshair und führt die Aktion am Ende
des instrumentierten Minecraft-Client-Ticks aus. Es gibt keine Inventaränderung
aus dem Sampler oder dem externen Renderthread. Deaktivierte Module fragen das
Inventar nicht ab. IPC-Anfragen haben eine 250-ms-Lease und einen zusätzlichen
Fokus-Check im Agent. Fehlende optionale Handles werden erneut aufgelöst.

Totem-Swaps nutzen die aktuelle Container-ID, die normale Inventory→Container-
Slot-Zuordnung und SWAP-Button 40 für die Offhand. Der Java-Hook in `startUseItem`
übernimmt ausschließlich passende Platzierungsversuche. Während des Vorgangs
verhindert er konkurrierende Vanilla-Uses beim automatischen Slotwechsel.
Die Folgeaktionen nutzen reguläres `MultiPlayerGameMode.useItemOn` mit festen
HitResults sowie Attack-/Swing-Methoden. Keine direkten Paketmanipulationen.
Die
Versionsprofile auditieren die zusätzlichen Signaturen anhand der
checksum-geprüften offiziellen Client-JARs. Moderne Versionen lesen die
Anchor-Regel aus EnvironmentAttributeSystem, ältere aus DimensionType.

Der Blockscanner bleibt im Sampler und setzt seinen Cursor nach spätestens
etwa 2 ms fort. Die Deadline wird alle 32 Zellen geprüft; ein einzelner langsamer
Spielaufruf kann dieses Budget überschreiten. `overlay_utilities.cpp` liest nur
Snapshots/Reports und zeichnet die Hinweise mit derselben Kameraprojektion wie
ESP. Configs ändern sich im Overlay-Thread, werden als POD kopiert und über die
vorhandenen Mailboxes veröffentlicht. Der Client-Tick-Controller besitzt einen
Mutex für Installation/Shutdown und eigene globale Item-/Property-Referenzen.

Die Tests decken Slot-Zuordnung, Verbrauch, Cursor-/Container-Gates, Lease,
Spiel-Use, Wiederherstellung, feste Platzierungszyklen, Bestätigungs-Timeouts,
abgelehnte Interaktionen, Anchor-Ladestände, Dimensionsregeln
und Chunk-Grenzen ab. Isolierte JVM-Fixtures prüfen die nativen Swap-/Attack-
Aufrufe, spät verfügbare Inventar-Bindings und die Achsen-Property. UI-Prüfungen
rendern die Untertabs auf Desktop und 480×360. Ein Live-Test in Minecraft ist
weiterhin erforderlich, insbesondere mit Mods, die Inventare/Interaktionen ändern.

**IPC v17:** Minecraft vollständig beenden und mit den neu gebauten Agent-/Overlay-
Dateien erneut starten. Ein bereits geladener alter Agent kann nicht aktualisiert
werden, indem nur das Overlay neu gestartet wird. Der einzige Build liegt unter
`build`, die portable Startdatei liegt unter `build/client/mcov.exe`.
