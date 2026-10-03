# Combat Assist / Kill Aura: C++20-Kern und Minecraft-Brücke

## Lieferumfang

`src/combat/` enthält die ausführbare Referenzimplementierung für TargetSelector, RotationEngine, AttackController, SyncLayer, ConfigStore und CombatHud. Der konkrete SimulatedGame-Transport führt Hitbox-Raycasts, Cooldown, HP-Verlust und getrennte Kamera-/Serverrotation aus. Die Kernklassen verwenden feste Arrays, RAII und begrenzte Mutex-Abschnitte; sie benötigen keine eigenen Heap-Allokationen pro Tick.

**Die sichtbare Kill Aura ist mit Minecraft verbunden.** `src/agent/combat_live.hpp` erfasst UUIDs, aktuelle Hitboxen, Sichtbarkeit, Cooldown und Reichweite und führt Rotation sowie `MultiPlayerGameMode.attack`/Hand-Swing synchron auf dem instrumentierten Client-Tick aus. Die bisherigen Mausmodule pausieren während Kill Aura aktiv ist. `sent` zählt lokal ausgeführte Aufträge, keine Serverbestätigungen; das HUD zählt keine einzelnen Netzwerkpakete.

Der Renderprozess erhält keine JNI-Handles. IPC v11 überträgt Config/UUID-Relations und eine begrenzte Eingabe-Lease in atomaren Mailboxes; der Client-Tick publiziert Diagnosen zurück. Vor jedem Angriff prüft der Adapter Cooldown, UUID, Lebensstatus, Reichweite, Ausrichtung, LOS und den nächsten Entity-Treffer vor dem Block-Ray-Ende erneut. Für 1.20.1 gilt die Vanilla-Pick-Reichweite (3, im Creative-Fernmodus 6); neuere Profile lesen `entityInteractionRange`. Welt- und Player-Referenzwechsel erhöhen die Generation, auch beim Respawn. JNI-Fehler stoppen das Modul bis zu einer Config-Revision.

Silent Rotation und echte Serverkorrektur-Ereignisse sind weiterhin offen. Die Sync-Regler steuern die lokale Auftragsqueue; Vanilla sendet die normalen Spielpakete. Reaktions- und Intervallwerte sind an die tatsächliche Client-Tick-Frequenz gebunden. Auf einem 20-Hz-Client kann eine 10-ms-Verzögerung erst beim nächsten Tick wirken.

Benutzung: Minecraft nach dem Update vollständig neu starten, mit dem aktuellen Injector verbinden, INSERT → COMBAT → Kill Aura → **Use Kill Aura preset** → **Enable Kill Aura**, Menü schließen. Das Preset sucht rundherum (180° Halbwinkel), nutzt 3 Meter und respektiert den Spiel-Cooldown. **Players only** ausschalten, wenn auch Mobs Ziele sein sollen. Geöffnete Minecraft-/Overlay-Menüs, Fokusverlust, fehlende Welt und eine abgelaufene 250-ms-Lease pausieren sofort am nächsten Tick. Overlay-Shutdown publiziert einen expliziten Stop. Der visuelle Aura-Effekt folgt dem aktiven Combat-Ziel.

Es bleibt bei einem Build-Ordner. Die ausführbaren Beispiele liegen in `build/bin`:

```powershell
.\build\bin\mcov_combat_demo.exe
.\build\bin\mcov_combat_check.exe
.\build\bin\mcov_combat_hud_check.exe
```

## 1. TargetSelector

Klassen: `TargetSelector`, `FriendEnemyBook`; Datei `src/combat/target_selector.hpp`.

Die Auswahl filtert UUID-Freunde, eigene/tote Entities, ungültige Hitboxen, veraltete Sichtbarkeit, Reichweite und FOV. Feinde haben konfigurierbar Vorrang. Abstand wird zum nächsten Hitboxpunkt gemessen; die Rotations-Zielposition liegt separat standardmäßig bei 75 % der Hitboxhöhe.

| Modus | Ranking |
|---|---|
| Crosshair | Entity im Crosshair der sichtbaren Kamera, unter Beachtung aller Filter |
| NearestPlayer | Kürzester Hitbox-Abstand eines Spielers |
| LowestHealth | Kleinste HP, dann Distanz/Winkel bei Gleichstand |
| AngleDistance | Normalisierte Winkel-/Distanzkosten, standardmäßig 0,7 / 0,3 |

Ein gültiger Single-Target-Lock bleibt bestehen. Multi-Target wechselt nach einer min/max-Verzögerung und ausreichender Score-Verbesserung. Zielverlust hebt den alten Lock sofort auf. Relation-Änderungen greifen am nächsten Owner-Tick.

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
| `coneDegrees` | 20° (Kill-Aura-Preset 180°) | 1–180° |
| `radiusPixels` | 160 | 10–2000 |
| `range` | 6 Blöcke | 0,1–64 |
| `aimHeight` | 0,75 | 0,1–0,95 |
| `angleWeight`, `distanceWeight` | 0,7 / 0,3 | jeweils 0–1, Summe > 0 |
| `visibilityMaxAge` | 100 ms | 1–500 ms |
| `switchImprovement` | 0,08 | 0–1 |
| `switchDelay` | 80–180 ms | 0–1000 ms, min ≤ max |

Fallstricke: `isInvisible` ist kein Occlusion-Test. Unbekannte Sichtbarkeit wird bei aktiviertem Sichtcheck abgelehnt. Entity-IDs können wiederverwendet werden; Freunde werden per UUID verwaltet. Kamera-FOV und gesendete Rotation bleiben bei Silent getrennte Eingaben.

## 2. RotationEngine

Klasse: `RotationEngine`; Datei `src/combat/rotation_engine.hpp`.

Yaw und Pitch haben getrennte kubische Bézier-Verläufe und Laufzeiten. Das erste Segment endet am kleinen Overshoot-Punkt, das zweite korrigiert zum Ziel. Am Umkehrpunkt haben beide Nullgeschwindigkeit. Replanning eines bewegten Ziels übernimmt einen begrenzten Geschwindigkeitstangenten; nur ein neues Ziel löst die Reaktionspause aus.

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
| `overshootFraction` | 0–4 % | 0–20 % des Winkelfehlers |
| `maxOvershootDegrees` | 1° | 0–3° |
| `correctionDuration` | 90 ms | 10–1000 ms |
| `jitterDegrees` | 0–0,04° | 0–0,5° |
| `jitterPeriod` | 70–140 ms | 10–1000 ms |
| `maxYawPerTick` / `maxPitchPerTick` | 18° / 12° | jeweils 0,1–90° |
| `tickSeconds` | 50 ms | 10–200 ms; tatsächlichen Takt berücksichtigen |
| `tangentFraction` | 0,33 | 0–1 |
| `replanDegrees` | 1° | 0,01–20° |
| `settleDegrees` | 0,08° | 0–1°; Totzone nur ohne aktiven Jitter |

Fallstricke: Ein Renderframe ist kein Spieltick. Höheres Smoothing erhöht die Ausrichtungszeit. Der Zufall wird aus einem Seed erzeugt; er beweist kein menschliches Verhalten. Die mathematische Bézier-Zeitkurve steuert Rotationsachsen, keine freie Weltpositionsbahn.

## 3. AttackController

Klasse: `AttackController`; Datei `src/combat/attack_controller.hpp`.

Beim neuen Ziel startet eine erste Klickverzögerung. `Prepare` merkt erst dann einen Angriff vor, wenn der echte Spiel-Cooldown, Reichweite, Ausrichtung und der wandgeprüfte Angriffs-Raycaster passen. `Sent` wartet anschließend auf einen tatsächlich abgesunkenen Cooldown. `Discarded` gibt einen verworfenen Auftrag wieder frei. Ein alter, weiter bei 1,0 liegender Cooldown erzeugt keine Klickserie.

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
| `silentRotation` | false | bool; geprüfter Adapter erforderlich |

Fallstricke: CPS ersetzt keinen Attack-Cooldown. Fehlende Cooldown-Daten blockieren Angriffe. `attackHitId` stammt aus dem Raycast entlang der ausgegebenen Rotation; `crosshairId` gehört weiterhin zur sichtbaren Kamera. Der reale Adapter muss auf dem Game-Thread nochmals prüfen und tatsächliche Ausführung melden. Ein Win32-Adapter müsste den Rückgabewert von `SendInput` auswerten ([Microsoft-Dokumentation](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput)).

## 4. SyncLayer

Klasse: `SyncLayer`; Datei `src/combat/sync_layer.hpp`. Konkreter Referenztransport: `SimulatedGame` in `simulated_game.hpp`.

Ein FIFO mit 128 Slots hält Pose- und Angriffsaufträge samt Sequenz, Weltgeneration, Entity-ID, UUID, Erfassungs- und Fälligkeitszeit. Mehrere Producer und ein serialisierter Consumer sind möglich. Benachbarte Pose-Aufträge werden zusammengefasst; ein Angriff bildet eine Barriere. Pose-A / Angriff / Pose-B bleiben geordnet. Der Angriff folgt seiner Pose im selben Batch.

Konkretes Scheduling aus `Enqueue`, unter dem Queue-Mutex:

```cpp
command.sequence = ++sequence_;
command.due = std::max(now, lastDue_)
    + (command.kind == CommandKind::Pose ? rng_.Sample(config_.interval) : 0);
lastDue_ = command.due;
```

`Pump(frame, target, config, transport)` prüft Alter, Generation, UUID, Ziel und Angriffsvoraussetzungen. `Observe(frame)` verarbeitet echte Serverkorrekturen. `Cancel()` entfernt ausstehende Aufträge. `Transport::Send` hat eine konkrete Simulatorimplementierung und meldet Annahme, keine Serverbestätigung.

Auch bei der Ausgabe gilt ein kumuliertes Yaw-/Pitch-Budget pro Spieltick. Coalescing und verspätete Aufträge können dadurch mehrere geplante Schritte nicht zu einem einzigen großen Sprung bündeln. Cancel oder ein Zielwechsel erneuern dieses Budget innerhalb desselben Ticks nicht.

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

`ConfigStore::Publish` validiert min/max, Enum-Werte und NaN/Inf, ersetzt danach die ganze Config unter einem Mutex. Der Owner übernimmt eine Revision pro Step. Das Update wirkt am nächsten Step vollständig und verwirft alte Aufträge. Die Konfiguration und UUID-Relations sind unter COMBAT in Kill Aura, Targets, Rotation, Attacks, Sync, Friends und Debug editierbar und über IPC mit dem Client-Tick verbunden. Ein Dateiwatcher/JSON-Parser ist offen. Alle Zeitwerte im C++-API sind Sekunden.

`frameMaxAge=100 ms` (1–500 ms), `seed` ist uint64_t. Master-Schalter standardmäßig aus; Auswahl, Rotation, Angriff und Sync besitzen eigene Schalter. Getrennte RNG-Streams verhindern, dass zusätzliche Jitter-Samples den nächsten Klickdelay verschieben. Gleiches Seed plus gleiche Frame-/Config-/Relations-Folge erzeugt dieselben Ausgaben.

Vollständiger ausführbarer Ablauf:

```cpp
#include "combat/simulated_game.hpp"
#include <cstdio>

int main() {
    using namespace mcov::combat;
    ConfigStore store;
    FriendEnemyBook friends;
    SimulatedGame game;
    Config config;
    config.enabled = config.attack.enabled = true;
    config.target.coneDegrees = 40;
    if (!store.Publish(config)) return 1;
    if (!friends.Set(game.Entities()[2].uuid, Relation::Friend)) return 2;
    CombatAssist assist(store, friends, game);
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

Der Aufruf gehört zwischen NewFrame und Render. `HudConfig::{enabled,target,fov,trace,packetStats}` sind einzelne bool-Toggles, standardmäßig true. Der HUD verändert keine Style-Stacks und balanciert Clip-Stacks per RAII. ImGui kann eigene Draw-Buffers bei erstem Bedarf vergrößern; der Kern/Trace allokiert keine dynamischen Arrays pro Frame.

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
Friend-/Enemy-UI -> IPC CombatRequest -> nativer FriendEnemyBook
Serverkorrekturen -> Frame::correction -> SyncLayer::Observe [Live-Hook offen]
```

## Priorisierte Roadmap

1. **P0 – erledigt: Referenzkern und deterministische Tests.** Auswahl, UUID-Schutz, FOV, Wrap-Around, Overshoot, Jitter, Tick-Budget, Cooldown-Rearm, FIFO, TTL, Multi-Producer, Hot-Reload und Simulator stehen unabhängig von JNI.
2. **P1 – Daten und Thread-Bridge.** UUID, Spieltick, echter Cooldown, erlaubte Reichweite, LOS/Raycast und Serverkorrekturen für alle Profile auditieren. An einer Client-Tick-Grenze erfassen und über den Worker publizieren. Welt-/Respawn-Generation erhöhen, IPC-Version bei Layoutänderung anheben. Fehlende Bindings ausdrücklich im HUD melden. Diese Daten bestimmen die Angriffszulässigkeit.
3. **P2 – sichtbare Rotation und normale Angriffe.** Client-Thread-Ausgabe einbauen und bisherigen Aim Lock/Triggerbot ersetzen. Hold-Key, Fokus und geöffnete Spiel-/Overlay-Menüs berücksichtigen. Erfolg erst nach tatsächlicher Ausführung melden. Zuerst ohne Silent auf einem lokalen Testserver prüfen.
4. **P3 – teilweise erledigt: Menü und persistente Config.** Menüregler/UUID-Relations sind an die Stores angebunden. Dateiformat und Watcher ergänzen, den aktiven Game-Kern an dieselben Stores anschließen und die Debug-Mailbox mit Live-Daten speisen.
5. **P4 – Silent und Server-Resync.** Normale Movement-Pipeline versionsgenau hooken, aktuelle Position erhalten, Kamera-/gesendete Rotation trennen. Korrekturen mit zeitlich passendem Erwartungswert erfassen. Zuerst Reihenfolge, Teleports und Weltwechsel, dann Target-Wechsel/Recovery testen.
6. **P5 – Live-Matrix und Performance.** 26.2, 1.21.11, 1.21.1, 1.20.1 samt verfügbaren Loader-/Namespace-Profilen prüfen. Langsamer Server, Wand, Respawn, Inventar, Fokusverlust, High-DPI und 30/60/144 FPS testen. Sampling-Kosten und TTL messen, danach Bewegungsparameter abstimmen.

P1/P2 sowie die Live-Config-/HUD-Verbindung aus P3 sind für sichtbare Kill Aura implementiert. Serverkorrekturen, Silent Rotation, Dateipersistenz und die In-Game-Matrix bleiben offen. Alle elf Profile bestehen echte instrumentierte Client-Tick-/JNI-Fixtures unter Java 21 mit `-Xcheck:jni`, inklusive automatischem Angriff, Cooldown-Rearm, UUID-Freunden, LOS, Block-Ray, GUI, Lease und fehlender Welt. Simulator-/Fixture-Tests ersetzen keinen Test in einem laufenden Minecraft-Client.
