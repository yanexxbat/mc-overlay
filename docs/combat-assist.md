# Combat Assist / Kill Aura: C++20-Kern und Minecraft-Brücke

## Lieferumfang

`src/combat/` enthält die ausführbare Referenzimplementierung für TargetSelector, RotationEngine, AttackController, SyncLayer, ConfigStore und CombatHud. Der konkrete SimulatedGame-Transport führt Hitbox-Raycasts, Cooldown, HP-Verlust und getrennte Kamera-/Serverrotation aus. Die Kernklassen verwenden feste Arrays, RAII und begrenzte Mutex-Abschnitte; sie benötigen keine eigenen Heap-Allokationen pro Tick.

**Die Kill Aura ist mit Minecraft verbunden.** `src/agent/combat_live.hpp` erfasst UUIDs, aktuelle Hitboxen, Sichtbarkeit, Cooldown und Reichweite und führt Rotation sowie `MultiPlayerGameMode.attack`/Hand-Swing synchron auf dem instrumentierten Client-Tick aus. Die Rotation ist immer still: Die Maus-Blickrichtung bleibt unabhängig von der Combat-Rotation des Spielers, auch ohne Preset oder nach Reset. Es gibt keine umschaltbare Kamera-Einstellung. Auch die F5-Kameraposition wird aus dieser Blickrichtung berechnet. Die bisherigen Mausmodule pausieren während Kill Aura aktiv ist. `sent` zählt lokal ausgeführte Aufträge, keine Serverbestätigungen; das HUD zählt keine einzelnen Netzwerkpakete.

Der Renderprozess erhält keine JNI-Handles. IPC v20 überträgt Config/UUID-Relations, Spinbot-Werte und eine begrenzte Eingabe-Lease in atomaren Mailboxes; der Client-Tick publiziert Diagnosen zurück. Vor jedem Angriff prüft der Adapter Cooldown, UUID, Lebensstatus, Reichweite, Ausrichtung, LOS und den nächsten Entity-Treffer vor dem Block-Ray-Ende erneut. Für 1.20.1 gilt die Vanilla-Pick-Reichweite (3, im Creative-Fernmodus 6); neuere Profile lesen `entityInteractionRange`. Welt- und Player-Referenzwechsel erhöhen die Generation, auch beim Respawn. JNI-Fehler stoppen das Modul bis zu einer Config-Revision.

## Spinbot

Unter **Combat → Spinbot** aktiviert Linksklick das Modul; Rechtsklick öffnet
die Einstellungen. Standard: 720 Grad/s, wechselnde Drehrichtung alle drei
Sekunden, Kamera-Pitch. **Rotation speed** erlaubt 30–1800 Grad/s;
**Direction** bietet rechts, links und wechselnd mit weichem Übergang.
**Pitch** behält den Kamera-Pitch bei oder dreht das Modell nach oben/unten bzw.
in einer begrenzten Welle. Die Blickrichtung und F5-Kamera bleiben unabhängig.
Der Modul-Keybind ist zunächst ungebunden; Toggle/Hold ist unter Settings →
Keybinds verfügbar. Config-Profile speichern Parameter und Bind.

`src/combat/spinbot.hpp` berechnet die Winkel ohne Allokationen anhand der
Client-Tick-Zeit. `MinecraftCombat` schreibt sie nur auf diesem Thread über die
vorhandenen Yaw/Pitch-Setter und synchronisiert Kopf-/Körperinterpolation.
Spinbot allein sucht keine Ziele und greift nicht an. Ein gültiges Kill-Aura-Ziel
hat Vorrang; ohne Ziel kann Spinbot weiterdrehen. Aim Assist und Triggerbot
pausieren bei aktiviertem Spinbot, damit keine Mausrotation oder Kreuzhaar-
Angriffe gegen die getrennte Spielerrotation arbeiten.

Menü, HUD-Editor, Minecraft-GUI, Fokusverlust, fehlende Welt und abgelaufene
Eingabe-Lease pausieren am nächsten Client-Tick. Ausschalten oder Panic stellt
die Spielerrotation auf die unabhängige Blickrichtung zurück. Ohne passende
Kamera-Hooks bleibt Spinbot gesperrt und meldet dies im Statusfeld.
Nach diesem IPC-v20-Update Minecraft vollständig neu starten und erneut injecten.

Die Silent-Kamera ist implementiert; echte Serverkorrektur-Ereignisse bleiben offen. Die Entity behält die tatsächliche Zielrotation für ihr Modell und die normale Vanilla-Movement-Pipeline; Vanilla sendet die normalen Spielpakete. Die Live-Kill-Aura richtet sich direkt in jedem Client-Tick aus. Der erste Treffer ist bei freiem Ray und bereitem Cooldown im selben Tick möglich; danach bestimmt der Spiel-Cooldown das Tempo. Auf einem 20-Hz-Client werden Zielpositionen alle 50 ms nachgeführt. Details zu Hooks, Rückgabe und Grenzen stehen in [silent-camera.md](silent-camera.md).

Benutzung: Minecraft nach dem Update vollständig neu starten, mit dem aktuellen Injector verbinden, INSERT → COMBAT → Kill Aura → **Enable Kill Aura**, Menü schließen und F5 drücken. Das nächste gültige Ziel wird rundum in 3 Metern gesucht, sofort nachgeführt und bis Tod oder Zielverlust gehalten. **Targets** wechselt bei Bedarf von **Players and mobs** zu **Players only**; **Reach** begrenzt die Reichweite. Die Maus dreht weiter deinen Blick. Weitere Rotations-/Timing-Einstellungen sind nicht nötig; der Client normalisiert alte technische Werte automatisch. COMBAT bietet eigene Untertabs für Kill Aura, Aim Assist, Triggerbot, Auto Totem und Friends. **Diagnostics** enthält optionale HUD-Toggles und Zustandsanzeigen und bleibt standardmäßig geschlossen. Geöffnete Minecraft-/Overlay-Menüs, Fokusverlust, fehlende Welt und eine abgelaufene 250-ms-Lease pausieren sofort am nächsten Tick. Overlay-Shutdown publiziert einen expliziten Stop. Der visuelle Aura-Effekt folgt dem aktiven Combat-Ziel.

Es bleibt bei einem Build-Ordner. Die ausführbaren Beispiele liegen in `build/bin`:

```powershell
.\build\bin\mcov_combat_demo.exe
.\build\bin\mcov_combat_check.exe
.\build\bin\mcov_combat_hud_check.exe
```

## 1. TargetSelector

Klassen: `TargetSelector`, `FriendEnemyBook`; Datei `src/combat/target_selector.hpp`.

Die Live-Kill-Aura verwendet fest **NearestEntity** mit `multiTarget=false`: nächste gültige lebende Hitbox in Sicht und Reichweite, unabhängig von der Kamera. Ein gültiger Lock erhält pro Tick frische Position/Winkel und bleibt bis Tod, Entfernen, Freundstatus oder Sicht-/Reichweitenverlust bestehen. Dann wird sofort das nächste gültige Ziel gewählt. UUID-Freunde, eigene/tote Entities, Item-Drops, ungültige Hitboxen und veraltete Sichtbarkeit sind ausgeschlossen. Auswahl-Reichweite ist zusätzlich durch Attack-Reichweite und den Spielmodus begrenzt. Abstand wird zum nächsten Hitboxpunkt gemessen; die Rotations-Zielposition liegt bei 75 % der Hitboxhöhe. Die Bibliothek behält weitere Modi für Tests und andere Adapter; sie sind im Live-Menü nicht auswählbar.

| Modus | Ranking |
|---|---|
| NearestEntity (Live-Standard) | Kürzester Hitbox-Abstand beim Erwerb; gültiger Lock bleibt bis Zielverlust, stabile Entity-ID bei Gleichstand |
| Crosshair | Entity im Crosshair der sichtbaren Kamera, unter Beachtung aller Filter |
| NearestPlayer | Kürzester Hitbox-Abstand eines Spielers |
| LowestHealth | Kleinste HP, dann Distanz/Winkel bei Gleichstand |
| AngleDistance | Normalisierte Winkel-/Distanzkosten, standardmäßig 0,7 / 0,3 |

NearestEntity ignoriert FOV, Enemy-Priorität, HP-Ranking und Wechselverzögerung. Mit `multiTarget=false` hält es den gültigen Lock; mit `true` bleibt die sofortige Entfernungsauswahl für andere Adapter verfügbar. Die anderen Bibliotheksmodi halten ebenfalls einen Single-Target-Lock; Multi-Target wechselt dort nach einer min/max-Verzögerung und ausreichender Score-Verbesserung. Zielverlust hebt den alten Lock sofort auf. Relation-Änderungen greifen am nächsten Owner-Tick.

Konkreter Aufruf mit persistentem Selector:

```cpp
std::optional<mcov::combat::Target> SelectNow(
    mcov::combat::TargetSelector& selector,
    const mcov::combat::Frame& frame,
    const mcov::combat::Config& config,
    const mcov::combat::FriendEnemyBook& book,
    mcov::combat::Random& random) {
    return selector.Select(frame, config.target, book.Read(), random);
}
```

Der dynamische Radius folgt `height * tan(cone) / (2 * tan(cameraFov / 2))`. `coneDegrees` ist der halbe Öffnungswinkel. Dynamisch hält der Kreis den Winkel konstant; statisch hält er den Pixelradius konstant. Eine zusätzliche distanzabhängige Vergrößerung ist nicht implementiert.

| Parameter | Standard | Validierter Bereich |
|---|---:|---:|
| `coneDegrees` | 180°; für NearestEntity ohne Einfluss | 1–180° |
| `radiusPixels` | 160 | 10–2000 |
| `range` | 3 Blöcke | 0,1–64 im API; im Live-Menü mit Attack-Reichweite gekoppelt |
| `aimHeight` | 0,75 | 0,1–0,95 |
| `angleWeight`, `distanceWeight` | 0,7 / 0,3 | jeweils 0–1, Summe > 0 |
| `visibilityMaxAge` | 100 ms | 1–500 ms |
| `switchImprovement` | 0,08 | 0–1 |
| `switchDelay` | 80–180 ms | 0–1000 ms, min ≤ max |

Fallstricke: `isInvisible` ist kein Occlusion-Test. Unbekannte Sichtbarkeit wird bei aktiviertem Sichtcheck abgelehnt. Entity-IDs können wiederverwendet werden; Freunde werden per UUID verwaltet. Kamera-FOV und gesendete Rotation bleiben bei Silent getrennte Eingaben.

## 2. RotationEngine

Klasse: `RotationEngine`; Datei `src/combat/rotation_engine.hpp`.

Die Live-Kill-Aura nutzt `Track(target)` und `ExecutionMode::Continuous`: Ziel-Yaw/Pitch werden in jedem Client-Tick direkt angewendet. Kurven, Reaktionspause, Mikro-Jitter, Totzone und kleine Winkelbudgets entfallen. `Transport::RefreshAfterPose` liefert danach die tatsächlich angewendeten Winkel, den aktuellen Cooldown und den Hit-Ray, bevor der Kern einen Angriff vorbereitet. Dadurch funktionieren auch bewegte Ziele ohne den Blick des vorherigen Ticks. Die folgenden Kurvenparameter betreffen ausschließlich den geglätteten Bibliotheksmodus `ExecutionMode::Smoothed` und sind im Live-Menü nicht mehr einstellbar.

Yaw und Pitch haben getrennte kubische Bézier-Verläufe und Laufzeiten. Das erste Segment endet am kleinen Overshoot-Punkt, das zweite korrigiert zum Ziel. Am Umkehrpunkt haben beide Nullgeschwindigkeit. Jeder neue Spieltick beginnt bei der gemessenen Spielerrotation; verzögerte Queue-Posen werden nicht als angewendet vorausgesetzt. Replanning eines bewegten Ziels übernimmt nur eine in die neue Zielrichtung begrenzte Geschwindigkeitstangente; nur ein neues Ziel löst die Reaktionspause aus. Overshoot und Jitter sind standardmäßig null.

```cpp
double Bezier(double a, double b, double c, double d, double t) {
    const double u = 1.0 - t;
    return u*u*u*a + 3.0*u*u*t*b + 3.0*u*t*t*c + t*t*t*d;
}
```

`RotationEngine::Step(frame, target, config.rotation, rng, speedScale)` liefert geplante Angles. Der kürzeste Yaw-Weg und Pitch-Clamping vermeiden Wrap-Artefakte. Ein zeitabhängiger Schritt plus kumuliertes Budget pro tatsächlichem Spieltick begrenzen die Geschwindigkeit auch bei hohen Overlay-FPS. Mikro-Jitter wird zeitlich weich interpoliert.

| Parameter | Standard | Validierter Bereich |
|---|---:|---:|
| `reactionDelay` | 10–40 ms | 10–40 ms |
| `yawDuration` / `pitchDuration` | 80–220 / 100–260 ms | jeweils 10–2000 ms |
| `smoothing` | 1,0 | 0,25–4; multipliziert die Laufzeiten |
| `overshootFraction` | 0 % | 0–20 % des Winkelfehlers |
| `maxOvershootDegrees` | 1° | 0–3° |
| `correctionDuration` | 90 ms | 10–1000 ms |
| `jitterDegrees` | 0° | 0–0,5° |
| `jitterPeriod` | 70–140 ms | 10–1000 ms |
| `maxYawPerTick` / `maxPitchPerTick` | 18° / 12° | jeweils 0,1–90° |
| `tickSeconds` | 50 ms | 10–200 ms; tatsächlichen Takt berücksichtigen |
| `tangentFraction` | 0,33 | 0–1 |
| `replanDegrees` | 1° | 0,01–20° |
| `settleDegrees` | 0,08° | 0–1°; Totzone nur ohne aktiven Jitter |

Fallstricke: Ein Renderframe ist kein Spieltick. Höheres Smoothing erhöht die Ausrichtungszeit. Der Zufall wird aus einem Seed erzeugt; er beweist kein menschliches Verhalten. Die mathematische Bézier-Zeitkurve steuert Rotationsachsen, keine freie Weltpositionsbahn.

## 3. AttackController

Klasse: `AttackController`; Datei `src/combat/attack_controller.hpp`.

Beim neuen Ziel startet eine erste Klickverzögerung; im Live-Modus beträgt sie fest null. `Prepare` merkt erst dann einen Angriff vor, wenn der echte Spiel-Cooldown, Reichweite, Ausrichtung und der wandgeprüfte Angriffs-Raycaster passen. `Sent` wartet anschließend auf einen tatsächlich abgesunkenen Cooldown. `Discarded` gibt einen verworfenen Auftrag wieder frei. Ein alter, weiter bei 1,0 liegender Cooldown erzeugt keine Klickserie. Bei jedem neu bereiten Cooldown wird das gehaltene Ziel erneut angegriffen.

Konkreter Teil der Angriffsprüfung:

```cpp
bool ReachAndCooldownReady(const mcov::combat::Frame& f,
                          const mcov::combat::Target& t,
                          const mcov::combat::AttackConfig& c) {
    return f.attackCooldown && f.gameAttackReach
        && *f.attackCooldown >= c.cooldownThreshold
        && *f.attackCooldown <= 1.0
        && t.range <= std::min(c.range, *f.gameAttackReach)
        && f.attackHitId == t.id;
}
```

Der vollständige Kern prüft zusätzlich Config, Input-Freigabe, Datenalter, gültige Zahlen, Ausrichtung und Silent-Capability. SyncLayer prüft unmittelbar vor Ausgabe nochmals.

| Parameter | Standard | Validierter Bereich |
|---|---:|---:|
| `enabled` | false | bool |
| `range` | 3 Blöcke | 0,1–6, zusätzlich durch Spiel-Reichweite begrenzt |
| `cooldownThreshold` | 0,98 | 0,8–1 |
| `alignmentDegrees` | 1,5° | 0,1–5° |
| `firstClickDelay` | 35–90 ms | 0–500 ms |

Stille Rotation ist festes Verhalten des Kerns, kein Config-Parameter. Camera-/Mouse-Hooks sind für jede Ausgabe erforderlich.

Fallstricke: CPS ersetzt keinen Attack-Cooldown. Fehlende Cooldown-Daten blockieren Angriffe. `attackHitId` stammt aus dem Raycast entlang der ausgegebenen Rotation; `crosshairId` gehört weiterhin zur sichtbaren Kamera. Der reale Adapter muss auf dem Game-Thread nochmals prüfen und tatsächliche Ausführung melden. Ein Win32-Adapter müsste den Rückgabewert von `SendInput` auswerten ([Microsoft-Dokumentation](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput)).

## 4. SyncLayer

Klasse: `SyncLayer`; Datei `src/combat/sync_layer.hpp`. Konkreter Referenztransport: `SimulatedGame` in `simulated_game.hpp`.

Ein FIFO mit 128 Slots hält Pose- und Angriffsaufträge samt Sequenz, Weltgeneration, Entity-ID, UUID, Erfassungs- und Fälligkeitszeit. Mehrere Producer und ein serialisierter Consumer sind möglich. Benachbarte Pose-Aufträge werden zusammengefasst; ein Angriff bildet eine Barriere. Pose-A / Angriff / Pose-B bleiben geordnet. Der Angriff folgt seiner Pose im selben Batch.

`Configure(config.sync, seed, ExecutionMode::Continuous)` aktiviert die feste Live-Policy. Aufträge sind sofort im aktuellen Client-Tick fällig; die Winkel werden direkt ausgegeben. Alters-, Welt-, UUID-, Sicht-, Reichweiten- und Cooldown-Prüfungen bleiben bestehen. `ExecutionMode::Smoothed` behält für andere Adapter das folgende verzögerte Scheduling und die kumulierten Winkelbudgets.

Konkretes Scheduling aus `Enqueue`, unter dem Queue-Mutex:

```cpp
command.sequence = ++sequence_;
const double delay = command.kind == CommandKind::Pose && mode_ == ExecutionMode::Smoothed
    ? rng_.Sample(config_.interval) : 0;
command.due = std::max(now, lastDue_) + delay;
lastDue_ = command.due;
```

`Pump(frame, target, config, transport)` prüft Alter, Generation, UUID, Ziel und Angriffsvoraussetzungen. `Observe(frame)` verarbeitet echte Serverkorrekturen. `Cancel()` entfernt ausstehende Aufträge. `Transport::Send` hat eine konkrete Simulatorimplementierung und meldet Annahme, keine Serverbestätigung.

Im geglätteten Bibliotheksmodus gilt auch bei der Ausgabe ein kumuliertes Yaw-/Pitch-Budget pro Spieltick. Coalescing und verspätete Aufträge können dadurch mehrere geplante Schritte nicht zu einem einzigen großen Sprung bündeln. Cancel oder ein Zielwechsel erneuern dieses Budget innerhalb desselben Ticks nicht.

| Parameter | Standard | Validierter Bereich |
|---|---:|---:|
| `interval` | 40–60 ms | 10–200 ms |
| `poseMaxAge` | 150 ms | 10–500 ms |
| `attackMaxAge` | 75 ms | 10–200 ms |
| `positionDesync` | 0,5 Blöcke | 0,01–8 |
| `angleDesync` | 8° | 0,1–90° |
| `correctionMaxAge` | 250 ms | 10–1000 ms |
| `recoverySeconds` | 200 ms | 10–1000 ms |
| `recoverySpeedScale` | 0,35 | 0,05–1 |

Serverzustand und erwarteter Zustand müssen zum selben Zeitpunkt gehören. Nach einer echten Korrektur wird die Queue gelöscht; die Rotation startet aus der aktualisierten gemessenen Rotation und wird kurz langsamer. Die lokale Sequenznummer wird nicht als Minecraft-Protokollfeld erfunden.

Fallstricke: Keine alten Positionen blind als zusätzliche Movement-Pakete ausspielen. Der Live-Hook muss die aktuelle Vanilla-Position übernehmen und die Rotation in deren normale Pipeline integrieren. Jitter-Scheduling ersetzt nicht den Spieltakt. Ein alter lokaler Cache beweist keinen Server-Desync. `Send` liegt außerhalb des Queue-Mutex, aber innerhalb des Consumer-Mutex; synchrones Re-Entrancy in Pump/Cancel/Configure/Observe wäre ein Deadlock. Ein async-Adapter benötigt eine echte Ausführungsbestätigung und darf bloßes Queueing nicht als ausgeführten Angriff melden. Forge 1.20.x beschreibt Main-Thread-Scheduling über `enqueueWork`; neuere APIs sind separat zu prüfen ([Forge-Dokumentation](https://docs.minecraftforge.net/en/1.20.x/networking/simpleimpl/)).

## 5. Config und Integration

Klassen: `ConfigStore`, `CombatAssist`, `DebugMailbox`; Dateien `config.hpp` und `combat_assist.hpp`.

`ConfigStore::Publish` validiert min/max, Enum-Werte und NaN/Inf, ersetzt danach die ganze Config unter einem Mutex. Der Owner übernimmt eine Revision pro Step. Das Update wirkt am nächsten Step vollständig und verwirft alte Aufträge. `MakeKillAuraConfig` normalisiert die Live-Werte in Menü und Client: nur Aktivierung, Entity-Typ, Reichweite, Seed und HUD bleiben aus der Anfrage erhalten. COMBAT bietet eigene Untertabs für Kill Aura, Aim Assist, Triggerbot, Auto Totem und Friends. Direkt einstellbar sind Aktivierung, Entity-Typ und Reichweite; **Diagnostics** enthält optionale HUD-Toggles und Zustandsanzeigen. Config und UUID-Freunde sind über IPC mit dem Client-Tick verbunden. Ein Dateiwatcher/JSON-Parser ist offen. Alle Zeitwerte im C++-API sind Sekunden.

`frameMaxAge=100 ms` (1–500 ms), `seed` ist uint64_t. Kill Aura ist standardmäßig aus; der eine Aktivierungsschalter aktiviert Auswahl, Rotation, Angriff und Sync gemeinsam. Getrennte Subkomponenten-Schalter bleiben im Bibliotheks-API für Tests und andere Adapter. Getrennte RNG-Streams verhindern, dass zusätzliche Jitter-Samples den nächsten Klickdelay verschieben. Gleiches Seed plus gleiche Frame-/Config-/Relations-Folge erzeugt dieselben Ausgaben.

Vollständiger ausführbarer Ablauf:

```cpp
#include "combat/simulated_game.hpp"
#include <cstdio>

int main() {
    using namespace mcov::combat;
    ConfigStore store;
    FriendEnemyBook friends;
    SimulatedGame game;
    Config config = MakeKillAuraConfig({});
    config.enabled = true;
    if (!store.Publish(config)) return 1;
    if (!friends.Set(game.Entities()[2].uuid, Relation::Friend)) return 2;
    CombatAssist assist(store, friends, game, ExecutionMode::Continuous);
    for (int tick = 0; tick < 250; ++tick)
        assist.Step(game.Capture(tick * 0.020, 0.020));
    std::printf("Angriffe: %llu\n",
        static_cast<unsigned long long>(game.Attacks()));
    return game.Attacks() > 0 ? 0 : 3;
}
```

Fallstricke: Der Borrowed Span `Frame::entities` darf während Step nicht verändert werden. Step hat einen Owner-Thread; nur Config, Relations, Sync und DebugMailbox sind thread-safe. Engine-/Selector-Instanzen bleiben persistent. Zufällige Thread-Reihenfolge ist trotz gleichen Seeds keine reproduzierbare Eingabefolge.

## 6. Debug / HUD

Klasse: `CombatHud`; Dateien `debug_hud.hpp/.cpp`.

Targetname, Hitbox-Distanz, Kamera-Zielwinkel, gemessener Rotationsfehler, Cooldown, Betriebsstatus und Auftragsstatistik werden aus der DebugMailbox gezeichnet. 128 feste Trace-Samples zeigen Yaw/Pitch: gemessen hell, Ziel türkis; Pitch ist transparenter. Keine JNI-Abfragen im HUD.

```cpp
#include "combat/debug_hud.hpp"
#include "imgui.h"

void DrawCombatDiagnostics(const mcov::combat::CombatAssist& assist,
                           const mcov::combat::HudConfig& hud) {
    mcov::combat::CombatHud::Draw(
        *ImGui::GetBackgroundDrawList(), assist.Debug(), hud,
        ImGui::GetIO().DisplaySize);
}
```

Der Aufruf gehört zwischen NewFrame und Render. `HudConfig::{enabled,target,fov,trace,packetStats}` sind einzelne bool-Toggles. Standardmäßig ist nur `target` true; der gesamte Debug-HUD bleibt durch `enabled=false` verborgen. NearestEntity zeichnet keinen FOV-Kreis. Der HUD verändert keine Style-Stacks und balanciert Clip-Stacks per RAII. ImGui kann eigene Draw-Buffers bei erstem Bedarf vergrößern; der Kern/Trace allokiert keine dynamischen Arrays pro Frame.

Fallstricke: FOV-Radius und DisplaySize müssen dieselben Einheiten haben. High-DPI-Skalierung gehört in den Adapter. Yaw-Traces werden relativ zur ersten Probe über ±180° entrollt. Text/Plots werden an Panel und Viewport geclippt. Kohärente Kamera-/Entity-Daten verhindern falsche Winkel und Zittern.

## Gesamtintegration

```text
Minecraft-Client-Tick / normale Movement-Pipeline
       | UUID / LOS / Cooldown / Kamera / Korrekturen
       v
MinecraftCombat::Tick -> aktueller Frame (JNI auf dem Client-Thread)
       |
       v
CombatAssist [ein Owner-Thread]
       +-> TargetSelector -> optional<Target>
       +-> RotationEngine -> geplante Angles
       +-> AttackController -> vorgemerkter Angriff
       +-> SyncLayer [Mutex, FIFO, TTL, Coalescing]
                    |
                    v
              Transport::Send
              +-> SimulatedGame [implementiert]
              +-> MinecraftCombat [implementiert, synchron]
       |
       +-> DebugMailbox [Mutex, feste Snapshot-Kopie]
                    |
                    v
              CombatHud / ImGui-Renderthread

Menü -> ConfigStore::Publish -> IPC CombatRequest -> nativer ConfigStore
Friends-UI -> IPC CombatRequest -> nativer FriendEnemyBook
Serverkorrekturen -> Frame::correction -> SyncLayer::Observe [Live-Hook offen]
```

## Priorisierte Roadmap

1. **P0 – erledigt: Referenzkern und deterministische Tests.** Auswahl, UUID-Schutz, FOV, Wrap-Around, Overshoot, Jitter, Tick-Budget, Cooldown-Rearm, FIFO, TTL, Multi-Producer, Hot-Reload und Simulator stehen unabhängig von JNI.
2. **P1 – erledigt: Daten und Thread-Bridge.** UUID, Client-Tick, echter Cooldown, erlaubte Reichweite und LOS/Block-Ray sind für alle Profile auditiert. Erfassung und Ausgabe laufen am Tick-Ende nach Vanilla-Movement und Modell-Updates; Kopf-/Körperwinkel folgen der angewendeten Rotation. Welt-/Respawn-Generationen, IPC v19, Eingabe-Lease und Fehlerstatus sind integriert.
3. **P2 – implementiert: Spielerrotation, Silent-Kamera und normale Angriffe.** Der Client-Thread führt Aufträge synchron aus und bestätigt erst danach lokal. Die reine Java-Kamera-/Mouse-Bridge trennt F5-Blick und Combat-Winkel ohne JNI im Kameraframe. Legacy-Mausmodule pausieren; Fokus, Welt und beide Menüs sind Gates. Die Prüfung in einem laufenden lokalen Minecraft bleibt ausstehend.
4. **P3 – teilweise erledigt: Menü und persistente Config.** Menüregler, UUID-Relations und Live-HUD sind per IPC mit dem aktiven Kern verbunden. Dateiformat und Watcher ergänzen; derzeit bleiben Einstellungen innerhalb der Sitzung.
5. **P4 – Server-Resync.** Korrekturen aus der normalen Movement-Pipeline mit zeitlich passendem Erwartungswert erfassen. Reihenfolge, Teleports, Weltwechsel und Recovery testen. Die Kamera-Trennung ist bereits implementiert; aktuelle Position und Vanilla-Movement bleiben im Spiel verwaltet.
6. **P5 – Live-Matrix und Performance.** 26.2, 1.21.11, 1.21.1, 1.20.1 samt verfügbaren Loader-/Namespace-Profilen prüfen. Langsamer Server, Wand, Respawn, Inventar, Fokusverlust, High-DPI und 30/60/144 FPS testen. Sampling-Kosten und TTL messen, danach Bewegungsparameter abstimmen.

P1/P2 sowie die Live-Config-/HUD-Verbindung aus P3 sind für Kill Aura inklusive Silent-Kamera implementiert. Serverkorrekturen, Dateipersistenz und die In-Game-Matrix bleiben offen. Alle elf Profile bestehen instrumentierte Client-Tick-/JNI-Fixtures unter Java 21 mit `-Xcheck:jni`, inklusive automatischem Angriff, Cooldown-Rearm, UUID-Freunden, LOS, Block-Ray, GUI, Lease, fehlender Welt und unabhängigen Kamera-/Spielerwinkeln. Simulator-/Fixture-Tests ersetzen keinen Test in einem laufenden Minecraft-Client.
