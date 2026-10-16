# Chunkfinder, Crystal Macro, Anchor Macro und Auto Totem

## Bedienung

**Combat â†’ Auto Totem:** aktivieren und Totems in Hotbar oder Hauptinventar
legen. Ein fehlendes Totem in der Offhand wird mit einem regulÃ¤ren Container-Swap
ersetzt. Ein bereits getragenes Totem wird nicht stÃ¤ndig erneut gewechselt.
Die Statuszeile nennt fehlende Totems, blockierte Inventaraktionen und fehlende
Bindings. Ein geÃ¶ffnetes fremdes Inventar oder ein Item am Mauszeiger blockiert
den Swap. Pausiertes Spiel, Overlay-MenÃ¼, HUD-Editor und Fokusverlust pausieren
ebenfalls. Der Retry-Abstand ist 50â€“500 ms, standardmÃ¤ÃŸig 100 ms.

**Combat â†’ Crystal Macro:** aktivieren, End Crystals auswÃ¤hlen und Minecrafts
normale Platzieren-/Benutzen-Taste auf Obsidian oder Bedrock halten. Das Macro
speichert diesen Platz: platzieren â†’ Crystal automatisch zerstÃ¶ren â†’ erneut
platzieren. Der Crystal muss zwischen den Schritten nicht neu anvisiert werden.
Er wird anhand seiner Position Ã¼ber dem gewÃ¤hlten Block gesucht. Die Ã¼blichen
Platzierungs- und Reichweitenregeln gelten weiterhin.

**Combat â†’ Anchor Macro:** aktivieren, Anchor auswÃ¤hlen und Anchor, Glowstone
sowie ein weiteres Item in der Hotbar bereithalten. Minecrafts Platzieren-Taste
auf einem Platzierungsblock oder vorhandenen Anchor halten. Am gespeicherten
Platz lÃ¤uft automatisch: platzieren â†’ laden â†’ detonieren â†’ erneut platzieren.
Ladestand 0 wÃ¤hlt Glowstone; ein geladener Anchor wird mit einem anderen Item
ausgelÃ¶st. Glowstone in der Offhand blockiert diesen
Schritt bei Ladestand 1â€“3; die Statuszeile fordert dann zum Entfernen auf. Die Dimensionsregel kommt aus Minecraft:
funktionierende Respawn-Dimensionen erlauben keine Detonation durch das Macro.

Beide Modul-Binds sind anfangs ungebunden und unterstÃ¼tzen Toggle oder Hold.
Sie steuern die Bereitschaft. Der Schalter alleine fÃ¼hrt keine Aktion aus.
AuslÃ¶ser ist die echte Minecraft-Use-Taste, auch bei eigener Tastenbelegung.
Beim Loslassen stoppt die Schleife und der zuvor ausgewÃ¤hlte Slot wird
standardmÃ¤ÃŸig wiederhergestellt. Manuelle Slotwahl, Wegsehen, Weltwechsel,
Menus, Fokusverlust oder aktive Kill Aura beenden den Vorgang.
Bei blockierendem Inventar-/PausemenÃ¼ wartet die Slot-Wiederherstellung bis zum
SchlieÃŸen; eine inzwischen manuell geÃ¤nderte Auswahl bleibt erhalten.
Beide Module kÃ¶nnen bereit sein; das ausgewÃ¤hlte Item entscheidet Ã¼ber den Ablauf.
Der Aktionsabstand ist 100â€“500 ms, standardmÃ¤ÃŸig 150 ms. Jede Folgeaktion wartet
auf Spawn, Entfernung, Block- oder LadestandsÃ¤nderung. Der BestÃ¤tigungs-Timeout
ist 100â€“2.000 ms, standardmÃ¤ÃŸig 750 ms; erst danach darf eine unbeantwortete
Aktion wiederholt werden. â€žActions sentâ€œ zÃ¤hlt erfolgreiche Spielinteraktionen,
keine abschlieÃŸende ServerbestÃ¤tigung.

### Erweiterte Crystal-/Anchor-Einstellungen

Die beiden Untertabs speichern ihre Einstellungen automatisch. Module bleiben
beim nÃ¤chsten Clientstart ausgeschaltet; die bisherigen Keybinds gelten weiter.

- **Separate action delays:** Place, Break/Detonate und beim Anchor Charge
  jeweils 50â€“1.000 ms. Die Zeit zÃ¤hlt seit der letzten Aktion; die erste Aktion
  startet sofort. Ohne diese Option bleibt der gemeinsame Abstand 100â€“500 ms.
  AusfÃ¼hrung am Client-Tick: Zeiten unterhalb eines Ticks erhÃ¶hen die Tickrate nicht.
- **Maximum range:** 1â€“6 BlÃ¶cke, Standard 4,5. Gemessen vom Spielerauge zum
  gespeicherten Platzierungstreffer. Angriffe prÃ¼fen zusÃ¤tzlich den Abstand
  zur Crystal-Hitbox. Das Limit verkÃ¼rzt nur die jeweilige Vanilla-Reichweite.
- **Minimum health:** 0 deaktiviert den Check, sonst 1â€“20 HP. Verwendet aktuelle
  Lebenspunkte ohne Absorptionsherzen. Ein unterschrittener Wert pausiert den
  Ablauf; bei Erholung lÃ¤uft er weiter. Dies ist keine Explosionsschadensberechnung.
- **Require offhand totem:** pausiert bis ein Totem getragen wird. Auto Totem
  kann es unabhÃ¤ngig davon aus dem Hauptinventar nachlegen.
- **Strict world confirmation:** keine Wiederholung nach Timeout. Eine spÃ¤ter
  beobachtete WeltÃ¤nderung setzt den Ablauf fort; Loslassen der Use-Taste
  beendet ihn und setzt den Wartezustand zurÃ¼ck. Status nennt den Grund.
- **Charge to 4 with offhand glowstone:** Anchor bei Glowstone in der Offhand
  bis vier Ladungen laden, dann detonieren. Jede einzelne ErhÃ¶hung muss in der
  Welt beobachtet werden. Glowstone in der Hotbar wird weiterhin benÃ¶tigt.
  Ohne diese Option bleibt die bisherige Offhand-Fuel-Pause erhalten.
- **Detonation slot:** 0 automatisch, 1â€“9 bevorzugte Hotbarposition. Glowstone
  und End Crystals sind ungeeignet und fÃ¼hren zur automatischen Alternativwahl.
- **Server conservative:** setzt Place/Break auf 250 ms, Charge auf 200 ms,
  Reichweite auf 3, Mindest-HP auf 6 und striktes Warten mit 1.500-ms-Timeout.
  Weitere Einstellungen und der Modul-Schalter bleiben erhalten.
- **GrimAC conservative:** setzt zusÃ¤tzlich **Require real crosshair hit**.
  Crystal-Angriffe benÃ¶tigen die tatsÃ¤chlich von Minecraft gepickte EntitÃ¤t;
  Place benÃ¶tigt den gespeicherten Untergrund im Fadenkreuz, Charge/Detonate
  den Anchor selbst. Blockaktionen verwenden den aktuellen Minecraft-HitResult
  mit seiner tatsÃ¤chlichen Seite und Trefferposition. Ohne passenden Treffer
  pausiert der gespeicherte Ablauf; dies kann erneutes Anvisieren erfordern.

Das GrimAC-Profil orientiert sich an den offiziellen PrÃ¼fungen fÃ¼r
[Reach/Hitbox](https://github.com/GrimAnticheat/Grim/blob/2.0/common/src/main/java/ac/grim/grimac/checks/impl/combat/Reach.java),
[Blockseite](https://github.com/GrimAnticheat/Grim/blob/2.0/common/src/main/java/ac/grim/grimac/checks/impl/scaffolding/PositionPlace.java)
und [Platzierungsdistanz](https://github.com/GrimAnticheat/Grim/blob/2.0/common/src/main/java/ac/grim/grimac/checks/impl/scaffolding/FarPlace.java).
Es verÃ¤ndert Crystal-/Anchor-Interaktionen, nicht die Kill-Aura-Rotation.

WeltbestÃ¤tigungen kommen aus dem Clientzustand, einschlieÃŸlich mÃ¶glicher
Vanilla-Vorhersage. Sie sind keine separate BestÃ¤tigung des Servers. Das
konservative Profil ist kein getesteter Anti-Cheat-Bypass und verspricht
keine serverÃ¼bergreifende Akzeptanz.

**Blocks â†’ Chunkfinder:** sucht nach horizontalem rohem Deepslate, hohem Kelp
und groÃŸen Amethystknospen/Clustern. Die Filter haben getrennte Mindestwerte pro
Chunk. Der frÃ¼here Base Finder steckt im selben Modul. StandardmÃ¤ÃŸig markieren
3D-Rahmen den vollstÃ¤ndigen Chunk und WÃ¼rfelkanten jeden verdÃ¤chtigen Block.
Beide Markierungen sind unabhÃ¤ngig einstellbar. Nur abgeschlossene Scans lokal
geladener BlÃ¶cke werden gewertet. Details zu Grenzwerten, gespeicherten Funden,
DimensionshÃ¶hen und Scanlimits: [Chunkfinder](smp-features.md#chunkfinder-deepslate-kelp-und-amethyst).

## Architektur und PrÃ¼fung

`common/item_assist.hpp` entscheidet ohne JNI Ã¼ber genau eine Aktion pro Tick.
`agent/item_assist_live.hpp` liest Inventar/Crosshair und fÃ¼hrt die Aktion am Ende
des instrumentierten Minecraft-Client-Ticks aus. Es gibt keine InventarÃ¤nderung
aus dem Sampler oder dem externen Renderthread. Deaktivierte Module fragen das
Inventar nicht ab. IPC-Anfragen haben eine 250-ms-Lease und einen zusÃ¤tzlichen
Fokus-Check im Agent. Fehlende optionale Handles werden erneut aufgelÃ¶st.

Totem-Swaps nutzen die aktuelle Container-ID, die normale Inventoryâ†’Container-
Slot-Zuordnung und SWAP-Button 40 fÃ¼r die Offhand. Der Java-Hook in `startUseItem`
Ã¼bernimmt ausschlieÃŸlich passende Platzierungsversuche. WÃ¤hrend des Vorgangs
verhindert er konkurrierende Vanilla-Uses beim automatischen Slotwechsel.
Die Folgeaktionen nutzen regulÃ¤res `MultiPlayerGameMode.useItemOn` mit festen
HitResults sowie Attack-/Swing-Methoden. Keine direkten Paketmanipulationen.
Die
Versionsprofile auditieren die zusÃ¤tzlichen Signaturen anhand der
checksum-geprÃ¼ften offiziellen Client-JARs. Moderne Versionen lesen die
Anchor-Regel aus EnvironmentAttributeSystem, Ã¤ltere aus DimensionType.

Der Blockscanner bleibt im Sampler und setzt seinen Cursor nach spÃ¤testens
etwa 2 ms fort. Die Deadline wird alle 32 Zellen geprÃ¼ft; ein einzelner langsamer
Spielaufruf kann dieses Budget Ã¼berschreiten. `overlay_utilities.cpp` liest nur
Snapshots/Reports und zeichnet die Hinweise mit derselben Kameraprojektion wie
ESP. Configs Ã¤ndern sich im Overlay-Thread, werden als POD kopiert und Ã¼ber die
vorhandenen Mailboxes verÃ¶ffentlicht. Der Client-Tick-Controller besitzt einen
Mutex fÃ¼r Installation/Shutdown und eigene globale Item-/Property-Referenzen.

Die Tests decken Slot-Zuordnung, Verbrauch, Cursor-/Container-Gates, Lease,
Spiel-Use, Wiederherstellung, feste Platzierungszyklen, BestÃ¤tigungs-Timeouts,
abgelehnte Interaktionen, Anchor-LadestÃ¤nde, Dimensionsregeln
und Chunk-Grenzen ab. Isolierte JVM-Fixtures prÃ¼fen die nativen Swap-/Attack-
Aufrufe, spÃ¤t verfÃ¼gbare Inventar-Bindings und die Achsen-Property. UI-PrÃ¼fungen
rendern die Untertabs auf Desktop und 480Ã—360. Ein Live-Test in Minecraft ist
weiterhin erforderlich, insbesondere mit Mods, die Inventare/Interaktionen Ã¤ndern.
ZusÃ¤tzliche Tests prÃ¼fen getrennte Aktionszeiten, native HP-/Totem-/Reichweiten-
Gates fÃ¼r alle 22 Profile, verspÃ¤tete BestÃ¤tigungen ohne Wiederholung, jede
einzelne Anchor-Ladung, bevorzugte Detonations-Slots und Settings-Persistenz.
Native Fixtures prÃ¼fen auÃŸerdem unter allen Profilen, dass ungepickte Crystals
nicht angegriffen und reale Anchor-HitResults unverÃ¤ndert benutzt werden.

**IPC v19:** Minecraft vollstÃ¤ndig beenden und mit den neu gebauten Agent-/Overlay-
Dateien erneut starten. Ein bereits geladener alter Agent kann nicht aktualisiert
werden, indem nur das Overlay neu gestartet wird. Der einzige Build liegt unter
`build`, die portable Startdatei liegt unter `build/client/Keim.exe`.

## Auto Trident

Combat → Auto Trident repeats Riptide while Minecraft's Place / Use key is held
(right mouse by default). Hold a Riptide trident in the main hand in water or rain.
Charge time defaults to the normal 10 game ticks and can be raised to 40 ticks.
Randomize charge is enabled by default and adds 0–3 ticks, for a 10–13 tick range.
The extra range is adjustable from 0–10 ticks and the total never exceeds 40.
A duration is sampled once per charge; disabling randomization uses the fixed
charge time. Preferences and named configs retain both randomization settings.
Automatic release also waits for the previous Riptide boost to finish, using
LivingEntity.isAutoSpinAttack from the game. A ready charge waits without choosing
a new duration. This prevents overlapping automated boosts; server behavior
still needs a live check and is not validated by the namespace fixtures.
Vanilla starts each following use and enforces its normal reuse and durability rules.
Unenchanted tridents and offhand use are never automatically released.

Menus, lost focus, expired control requests, other active inventory operations and
Kill Aura pause automatic release. Panic disables the module. Settings and its
optional Toggle/Hold binding are saved. Automatic preferences retain charge time
and the binding, with the module disabled at startup. Named configs also retain
the enabled state; old profiles default to disabled/unbound.
The implementation checks only the held/active stacks on the existing client-tick
callback and calls the mapped MultiPlayerGameMode.releaseUsingItem action.
Eleven checksum-audited namespace/version profiles cover 1.20.1, 1.21.1, 1.21.11
and 26.2. IPC v33 requires a full Minecraft restart after updating an older agent.
