# Chunkfinder, Crystal Macro, Anchor Macro und Auto Totem

## Bedienung

**Combat → Auto Totem:** aktivieren und Totems in Hotbar oder Hauptinventar
legen. Ein fehlendes Totem in der Offhand wird mit einem regulären Container-Swap
ersetzt. Ein bereits getragenes Totem wird nicht ständig erneut gewechselt.
Die Statuszeile nennt fehlende Totems, blockierte Inventaraktionen und fehlende
Bindings. Ein geöffnetes fremdes Inventar oder ein Item am Mauszeiger blockiert
den Swap. Pausiertes Spiel, Overlay-Menü, HUD-Editor und Fokusverlust pausieren
ebenfalls. Der Retry-Abstand ist 50–500 ms, standardmäßig 100 ms.

**Combat → Crystal Macro:** im Tastenfeld einen freien Bind vergeben. End Crystals
in die Hotbar legen, Menü schließen und die Taste halten. Auf Obsidian/Bedrock
zielen zum Platzieren; auf einen End Crystal zielen zum Zerstören. Das Macro
steuert den Hotbar-Slot und die normale Interaktion, nicht die Blickrichtung.
Platzierung braucht die üblichen freien Räume; eine fehlgeschlagene Interaktion
wird beim nächsten erlaubten Intervall erneut versucht.

**Combat → Anchor Macro:** Bind vergeben und Anchor, Glowstone sowie ein weiteres
Item in die Hotbar legen. Mit gehaltener Taste zuerst den Platzierungsblock und
danach den Anchor anvisieren. Ladestand 0 wählt Glowstone; ein geladener Anchor
wird mit einem anderen Item ausgelöst. Die Dimensionsregel kommt aus Minecraft:
funktionierende Respawn-Dimensionen erlauben keine Detonation durch das Macro.

Beide Macros bleiben zunächst ungebunden und verwenden ausschließlich **Hold**,
auch wenn eine alte Einstellungsdatei Toggle enthält. Der Schalter alleine führt
keine Aktion aus. Keine Minecraft-Taste für Use/Attack doppelt belegen. Beim
Loslassen wird standardmäßig der alte Hotbar-Slot wiederhergestellt; eine eigene
Slot-Änderung wird dabei respektiert. Ein offenes GUI, Fokusverlust, aktive Kill
Aura oder gleichzeitig gehaltene Macro-Binds blockieren neue Interaktionen.
Der Aktionsabstand ist 100–500 ms, standardmäßig 150 ms. „Actions sent“ zählt
ausgeführte Spielaufrufe; es bestätigt keine Platzierung oder Serverannahme.

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
Slot-Zuordnung und SWAP-Button 40 für die Offhand. Macros nutzen `startUseItem`,
den ausgewählten Hotbar-Slot sowie reguläre Attack-/Swing-Methoden. Die
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
Hold-only, Wiederherstellung, Macro-Konflikte, Anchor-Ladestände, Dimensionsregeln
und Chunk-Grenzen ab. Isolierte JVM-Fixtures prüfen die nativen Swap-/Attack-
Aufrufe, spät verfügbare Inventar-Bindings und die Achsen-Property. UI-Prüfungen
rendern die Untertabs auf Desktop und 480×360. Ein Live-Test in Minecraft ist
weiterhin erforderlich, insbesondere mit Mods, die Inventare/Interaktionen ändern.

**IPC v15:** Minecraft vollständig beenden und mit den neu gebauten Agent-/Overlay-
Dateien erneut starten. Ein bereits geladener alter Agent kann nicht aktualisiert
werden, indem nur das Overlay neu gestartet wird. Der einzige Build liegt unter
`build/bin`.
