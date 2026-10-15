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

### Erweiterte Crystal-/Anchor-Einstellungen

Die beiden Untertabs speichern ihre Einstellungen automatisch. Module bleiben
beim nächsten Clientstart ausgeschaltet; die bisherigen Keybinds gelten weiter.

- **Separate action delays:** Place, Break/Detonate und beim Anchor Charge
  jeweils 50–1.000 ms. Die Zeit zählt seit der letzten Aktion; die erste Aktion
  startet sofort. Ohne diese Option bleibt der gemeinsame Abstand 100–500 ms.
  Ausführung am Client-Tick: Zeiten unterhalb eines Ticks erhöhen die Tickrate nicht.
- **Maximum range:** 1–6 Blöcke, Standard 4,5. Gemessen vom Spielerauge zum
  gespeicherten Platzierungstreffer. Angriffe prüfen zusätzlich den Abstand
  zur Crystal-Hitbox. Das Limit verkürzt nur die jeweilige Vanilla-Reichweite.
- **Minimum health:** 0 deaktiviert den Check, sonst 1–20 HP. Verwendet aktuelle
  Lebenspunkte ohne Absorptionsherzen. Ein unterschrittener Wert pausiert den
  Ablauf; bei Erholung läuft er weiter. Dies ist keine Explosionsschadensberechnung.
- **Require offhand totem:** pausiert bis ein Totem getragen wird. Auto Totem
  kann es unabhängig davon aus dem Hauptinventar nachlegen.
- **Strict world confirmation:** keine Wiederholung nach Timeout. Eine später
  beobachtete Weltänderung setzt den Ablauf fort; Loslassen der Use-Taste
  beendet ihn und setzt den Wartezustand zurück. Status nennt den Grund.
- **Charge to 4 with offhand glowstone:** Anchor bei Glowstone in der Offhand
  bis vier Ladungen laden, dann detonieren. Jede einzelne Erhöhung muss in der
  Welt beobachtet werden. Glowstone in der Hotbar wird weiterhin benötigt.
  Ohne diese Option bleibt die bisherige Offhand-Fuel-Pause erhalten.
- **Detonation slot:** 0 automatisch, 1–9 bevorzugte Hotbarposition. Glowstone
  und End Crystals sind ungeeignet und führen zur automatischen Alternativwahl.
- **Server conservative:** setzt Place/Break auf 250 ms, Charge auf 200 ms,
  Reichweite auf 3, Mindest-HP auf 6 und striktes Warten mit 1.500-ms-Timeout.
  Weitere Einstellungen und der Modul-Schalter bleiben erhalten.
- **GrimAC conservative:** setzt zusätzlich **Require real crosshair hit**.
  Crystal-Angriffe benötigen die tatsächlich von Minecraft gepickte Entität;
  Place benötigt den gespeicherten Untergrund im Fadenkreuz, Charge/Detonate
  den Anchor selbst. Blockaktionen verwenden den aktuellen Minecraft-HitResult
  mit seiner tatsächlichen Seite und Trefferposition. Ohne passenden Treffer
  pausiert der gespeicherte Ablauf; dies kann erneutes Anvisieren erfordern.

Das GrimAC-Profil orientiert sich an den offiziellen Prüfungen für
[Reach/Hitbox](https://github.com/GrimAnticheat/Grim/blob/2.0/common/src/main/java/ac/grim/grimac/checks/impl/combat/Reach.java),
[Blockseite](https://github.com/GrimAnticheat/Grim/blob/2.0/common/src/main/java/ac/grim/grimac/checks/impl/scaffolding/PositionPlace.java)
und [Platzierungsdistanz](https://github.com/GrimAnticheat/Grim/blob/2.0/common/src/main/java/ac/grim/grimac/checks/impl/scaffolding/FarPlace.java).
Es verändert Crystal-/Anchor-Interaktionen, nicht die Kill-Aura-Rotation.

Weltbestätigungen kommen aus dem Clientzustand, einschließlich möglicher
Vanilla-Vorhersage. Sie sind keine separate Bestätigung des Servers. Das
konservative Profil ist kein getesteter Anti-Cheat-Bypass und verspricht
keine serverübergreifende Akzeptanz.

**Blocks → Chunkfinder:** sucht nach horizontalem rohem Deepslate, hohem Kelp
und großen Amethystknospen/Clustern. Die Filter haben getrennte Mindestwerte pro
Chunk. Der frühere Base Finder steckt im selben Modul. Standardmäßig markieren
3D-Rahmen den vollständigen Chunk und Würfelkanten jeden verdächtigen Block.
Beide Markierungen sind unabhängig einstellbar. Nur abgeschlossene Scans lokal
geladener Blöcke werden gewertet. Details zu Grenzwerten, gespeicherten Funden,
Dimensionshöhen und Scanlimits: [Chunkfinder](smp-features.md#chunkfinder-deepslate-kelp-und-amethyst).

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
Zusätzliche Tests prüfen getrennte Aktionszeiten, native HP-/Totem-/Reichweiten-
Gates für alle 22 Profile, verspätete Bestätigungen ohne Wiederholung, jede
einzelne Anchor-Ladung, bevorzugte Detonations-Slots und Settings-Persistenz.
Native Fixtures prüfen außerdem unter allen Profilen, dass ungepickte Crystals
nicht angegriffen und reale Anchor-HitResults unverändert benutzt werden.

**IPC v19:** Minecraft vollständig beenden und mit den neu gebauten Agent-/Overlay-
Dateien erneut starten. Ein bereits geladener alter Agent kann nicht aktualisiert
werden, indem nur das Overlay neu gestartet wird. Der einzige Build liegt unter
`build`, die portable Startdatei liegt unter `build/client/mcov.exe`.
