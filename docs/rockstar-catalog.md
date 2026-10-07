# Zuordnung der 122 Rockstar-Module

Referenz ist [dieser feste Quellstand](https://github.com/serbioZ/Rockstar-Client-src-deobf/tree/f9ae03e4fe85bb8214527b2eeb2ea07548eadd98/smirnov/ru/rockstar/client/features/impl).
Die Statusangaben beschreiben die mcov-Entsprechung; „neu“ bedeutet eine eigene C++-Umsetzung,
keinen identischen Java-Port. Die Bedienung aller 29 neuen Funktionen steht in
[rockstar-features.md](rockstar-features.md). Bereits vorhandene Funktionen werden nicht als neu gezählt.
Nicht portierte Module haben keine vorgetäuschten Schalter im Menü.

## combat (21)

| Rockstar-Datei | mcov-Status / Entsprechung |
|---|---|
| AimBot | **Bereits vorhanden:** Aim assist; bisherige Kamera-/Combat-Integration. |
| AntiAim | Nicht portiert. Benötigt weitere geprüfte Client-Tick-Aktionen, Inventar-/Kampfzustände oder Netzwerk-Hooks pro Version. |
| AntiBot | Nicht portiert. Benötigt weitere geprüfte Client-Tick-Aktionen, Inventar-/Kampfzustände oder Netzwerk-Hooks pro Version. |
| AntiCrystal | Nicht portiert. Benötigt weitere geprüfte Client-Tick-Aktionen, Inventar-/Kampfzustände oder Netzwerk-Hooks pro Version. |
| AutoArmor | Nicht portiert. Benötigt weitere geprüfte Client-Tick-Aktionen, Inventar-/Kampfzustände oder Netzwerk-Hooks pro Version. |
| AutoGapple | Nicht portiert. Benötigt weitere geprüfte Client-Tick-Aktionen, Inventar-/Kampfzustände oder Netzwerk-Hooks pro Version. |
| AutoPotion | Nicht portiert. Benötigt weitere geprüfte Client-Tick-Aktionen, Inventar-/Kampfzustände oder Netzwerk-Hooks pro Version. |
| AutoShield | Nicht portiert. Benötigt weitere geprüfte Client-Tick-Aktionen, Inventar-/Kampfzustände oder Netzwerk-Hooks pro Version. |
| AutoTotem | **Bereits vorhanden:** Auto totem mit Client-Tick-Adapter. |
| CrystalAura | Nicht portiert. Crystal macro bleibt eine Place/Use-Schleife; automatische Crystal-Ziel-/Positionsauswahl fehlt. |
| FastBow | Nicht portiert. Benötigt weitere geprüfte Client-Tick-Aktionen, Inventar-/Kampfzustände oder Netzwerk-Hooks pro Version. |
| HitBox | Nicht portiert. Benötigt weitere geprüfte Client-Tick-Aktionen, Inventar-/Kampfzustände oder Netzwerk-Hooks pro Version. |
| HitSounds | **Neu:** Combat → Hit sounds. |
| KillAura | **Bereits vorhanden:** Kill Aura mit dauerhaftem Ziel-Lock und entkoppelter Kamera. |
| NoFriendDamage | **Bereits vorhanden:** Friends-Filter für Combat Assist; kein globaler Vanilla-Angriffsfilter. |
| PushAttack | Nicht portiert. Benötigt weitere geprüfte Client-Tick-Aktionen, Inventar-/Kampfzustände oder Netzwerk-Hooks pro Version. |
| Reach | Nicht portiert. Benötigt weitere geprüfte Client-Tick-Aktionen, Inventar-/Kampfzustände oder Netzwerk-Hooks pro Version. |
| ShieldDesync | Nicht portiert. Benötigt weitere geprüfte Client-Tick-Aktionen, Inventar-/Kampfzustände oder Netzwerk-Hooks pro Version. |
| SuperKnockBack | Nicht portiert. Benötigt weitere geprüfte Client-Tick-Aktionen, Inventar-/Kampfzustände oder Netzwerk-Hooks pro Version. |
| TriggerBot | **Bereits vorhanden:** Triggerbot. |
| Velocity | Nicht portiert. Benötigt weitere geprüfte Client-Tick-Aktionen, Inventar-/Kampfzustände oder Netzwerk-Hooks pro Version. |

## display (14)

| Rockstar-Datei | mcov-Status / Entsprechung |
|---|---|
| ArreyList | **Bereits vorhanden:** Module-HUD; feste Modulreihenfolge. |
| ClickGUI | **Bereits vorhanden:** Bestehendes Classic-Menü mit eigenen Funktionstabs. |
| ClientFont | Nicht portiert. Bestehende eingebettete Fonts bleiben fest; kein freier Font-Selector. |
| DamageFlyIndicator | Nicht portiert. Eigene Movement-/DamageFly-Zustandsmaschine fehlt. |
| DamageInfo | **Neu:** Visuals → Damage vignette. |
| Hotbar | Nicht portiert. Armor strip / Equipment-HUD ersetzen keine Hotbar. |
| HUD | **Bereits vorhanden:** Modulares HUD. |
| Indicators | **Neu:** HUD → Combat indicators; HP-Vergleich als eigene Umsetzung. |
| InventoryPreview | Nicht portiert. Vollständiger Inventar-Snapshot und Item-Daten fehlen. |
| KeyBinds | **Bereits vorhanden:** Toggle/Hold, Maus, Tastatur und Modifier. |
| Keystrokes | **Neu:** HUD → Keystrokes. |
| Notifications | **Neu:** Misc → Notifications; bewegliches HUD. |
| PotionStatus | **Bereits vorhanden:** Effects-HUD. |
| Watermark | **Bereits vorhanden:** Watermark-HUD. |

## misc (13)

| Rockstar-Datei | mcov-Status / Entsprechung |
|---|---|
| AutoAccept | Nicht portiert. Benötigt zusätzliche Server-, Command-, Movement-, Chat- oder Netzwerk-Anbindung. |
| Disabler | Nicht portiert. Benötigt zusätzliche Server-, Command-, Movement-, Chat- oder Netzwerk-Anbindung. |
| DiscordRPC | Nicht portiert. Discord-Integration fehlt. |
| DuelAccept | Nicht portiert. Benötigt zusätzliche Server-, Command-, Movement-, Chat- oder Netzwerk-Anbindung. |
| FastWorldLoad | Nicht portiert. Benötigt zusätzliche Server-, Command-, Movement-, Chat- oder Netzwerk-Anbindung. |
| FreeCam | Nicht portiert. Benötigt zusätzliche Server-, Command-, Movement-, Chat- oder Netzwerk-Anbindung. |
| MCF | **Neu:** Misc → Middle click friend; UUID-Freundesliste. |
| ModuleSoundAlert | **Neu:** Misc → Module sounds. |
| Panic | **Neu:** Misc → Panic; eigener Keybind und Hold-Schutz. |
| SelfDamage | Nicht portiert. Benötigt zusätzliche Server-, Command-, Movement-, Chat- oder Netzwerk-Anbindung. |
| StaffAlert | Nicht portiert. Keine Staff-Datenquelle; Detect players zeigt geladene Spieler. |
| Teleport | Nicht portiert. Benötigt zusätzliche Server-, Command-, Movement-, Chat- oder Netzwerk-Anbindung. |
| TimerIndicator | Nicht portiert. Benötigt zusätzliche Server-, Command-, Movement-, Chat- oder Netzwerk-Anbindung. |

## movement (20)

| Rockstar-Datei | mcov-Status / Entsprechung |
|---|---|
| AirJump | Nicht portiert. Benötigt eigene Client-Tick-Hooks für Bewegung, Kollision und gegebenenfalls Netzwerk-Sync pro Version. |
| AutoSprint | Nicht portiert. Benötigt eigene Client-Tick-Hooks für Bewegung, Kollision und gegebenenfalls Netzwerk-Sync pro Version. |
| BedrockClip | Nicht portiert. Benötigt eigene Client-Tick-Hooks für Bewegung, Kollision und gegebenenfalls Netzwerk-Sync pro Version. |
| DamageFly | Nicht portiert. Benötigt eigene Client-Tick-Hooks für Bewegung, Kollision und gegebenenfalls Netzwerk-Sync pro Version. |
| FastClimb | Nicht portiert. Benötigt eigene Client-Tick-Hooks für Bewegung, Kollision und gegebenenfalls Netzwerk-Sync pro Version. |
| Flight | Nicht portiert. Benötigt eigene Client-Tick-Hooks für Bewegung, Kollision und gegebenenfalls Netzwerk-Sync pro Version. |
| HighJump | Nicht portiert. Benötigt eigene Client-Tick-Hooks für Bewegung, Kollision und gegebenenfalls Netzwerk-Sync pro Version. |
| Jesus | Nicht portiert. Benötigt eigene Client-Tick-Hooks für Bewegung, Kollision und gegebenenfalls Netzwerk-Sync pro Version. |
| LongJump | Nicht portiert. Benötigt eigene Client-Tick-Hooks für Bewegung, Kollision und gegebenenfalls Netzwerk-Sync pro Version. |
| NoFall | Nicht portiert. Benötigt eigene Client-Tick-Hooks für Bewegung, Kollision und gegebenenfalls Netzwerk-Sync pro Version. |
| NoSlowDown | Nicht portiert. Benötigt eigene Client-Tick-Hooks für Bewegung, Kollision und gegebenenfalls Netzwerk-Sync pro Version. |
| NoWeb | Nicht portiert. Benötigt eigene Client-Tick-Hooks für Bewegung, Kollision und gegebenenfalls Netzwerk-Sync pro Version. |
| SafeWalk | Nicht portiert. Benötigt eigene Client-Tick-Hooks für Bewegung, Kollision und gegebenenfalls Netzwerk-Sync pro Version. |
| Speed | Nicht portiert. Benötigt eigene Client-Tick-Hooks für Bewegung, Kollision und gegebenenfalls Netzwerk-Sync pro Version. |
| Step | Nicht portiert. Benötigt eigene Client-Tick-Hooks für Bewegung, Kollision und gegebenenfalls Netzwerk-Sync pro Version. |
| Strafe | Nicht portiert. Benötigt eigene Client-Tick-Hooks für Bewegung, Kollision und gegebenenfalls Netzwerk-Sync pro Version. |
| TargetStrafe | Nicht portiert. Benötigt eigene Client-Tick-Hooks für Bewegung, Kollision und gegebenenfalls Netzwerk-Sync pro Version. |
| Timer | Nicht portiert. Benötigt eigene Client-Tick-Hooks für Bewegung, Kollision und gegebenenfalls Netzwerk-Sync pro Version. |
| WaterLeave | Nicht portiert. Benötigt eigene Client-Tick-Hooks für Bewegung, Kollision und gegebenenfalls Netzwerk-Sync pro Version. |
| WaterSpeed | Nicht portiert. Benötigt eigene Client-Tick-Hooks für Bewegung, Kollision und gegebenenfalls Netzwerk-Sync pro Version. |

## player (22)

| Rockstar-Datei | mcov-Status / Entsprechung |
|---|---|
| AntiBlocker | Nicht portiert. Benötigt zusätzliche Inventar-, Interaktions-, Bewegungs- oder Netzwerk-Aktionen auf dem Client-Tick. |
| AutoAuth | Nicht portiert. Benötigt zusätzliche Inventar-, Interaktions-, Bewegungs- oder Netzwerk-Aktionen auf dem Client-Tick. |
| AutoFarm | Nicht portiert. Benötigt zusätzliche Inventar-, Interaktions-, Bewegungs- oder Netzwerk-Aktionen auf dem Client-Tick. |
| BedBreaker | Nicht portiert. Benötigt zusätzliche Inventar-, Interaktions-, Bewegungs- oder Netzwerk-Aktionen auf dem Client-Tick. |
| ChestStealer | Nicht portiert. Benötigt zusätzliche Inventar-, Interaktions-, Bewegungs- oder Netzwerk-Aktionen auf dem Client-Tick. |
| DeathCoordinates | **Neu:** Misc → Death coordinates. |
| DetectPlayer | **Neu:** Misc → Detect players; Reichweitenliste und Eintrittsmeldungen. |
| FastEat | Nicht portiert. Benötigt zusätzliche Inventar-, Interaktions-, Bewegungs- oder Netzwerk-Aktionen auf dem Client-Tick. |
| GuiMove | Nicht portiert. Benötigt zusätzliche Inventar-, Interaktions-, Bewegungs- oder Netzwerk-Aktionen auf dem Client-Tick. |
| InventoryManager | Nicht portiert. Benötigt zusätzliche Inventar-, Interaktions-, Bewegungs- oder Netzwerk-Aktionen auf dem Client-Tick. |
| ItemScroller | Nicht portiert. Benötigt zusätzliche Inventar-, Interaktions-, Bewegungs- oder Netzwerk-Aktionen auf dem Client-Tick. |
| KeepSprint | Nicht portiert. Benötigt zusätzliche Inventar-, Interaktions-, Bewegungs- oder Netzwerk-Aktionen auf dem Client-Tick. |
| MiddleClickPearl | Nicht portiert. Benötigt zusätzliche Inventar-, Interaktions-, Bewegungs- oder Netzwerk-Aktionen auf dem Client-Tick. |
| NoClip | Nicht portiert. Benötigt zusätzliche Inventar-, Interaktions-, Bewegungs- oder Netzwerk-Aktionen auf dem Client-Tick. |
| NoDelay | Nicht portiert. Benötigt zusätzliche Inventar-, Interaktions-, Bewegungs- oder Netzwerk-Aktionen auf dem Client-Tick. |
| NoInteract | Nicht portiert. Benötigt zusätzliche Inventar-, Interaktions-, Bewegungs- oder Netzwerk-Aktionen auf dem Client-Tick. |
| NoPush | Nicht portiert. Benötigt zusätzliche Inventar-, Interaktions-, Bewegungs- oder Netzwerk-Aktionen auf dem Client-Tick. |
| NoServerRotation | Nicht portiert. Benötigt zusätzliche Inventar-, Interaktions-, Bewegungs- oder Netzwerk-Aktionen auf dem Client-Tick. |
| Scaffold | Nicht portiert. Benötigt zusätzliche Inventar-, Interaktions-, Bewegungs- oder Netzwerk-Aktionen auf dem Client-Tick. |
| SpeedMine | Nicht portiert. Benötigt zusätzliche Inventar-, Interaktions-, Bewegungs- oder Netzwerk-Aktionen auf dem Client-Tick. |
| Spider | Nicht portiert. Benötigt zusätzliche Inventar-, Interaktions-, Bewegungs- oder Netzwerk-Aktionen auf dem Client-Tick. |
| XCarry | Nicht portiert. Benötigt zusätzliche Inventar-, Interaktions-, Bewegungs- oder Netzwerk-Aktionen auf dem Client-Tick. |

## visuals (32)

| Rockstar-Datei | mcov-Status / Entsprechung |
|---|---|
| ArmorHUD | **Bereits vorhanden:** Equipment-HUD mit Icons; zusätzlich neue Armor strip. |
| BeaconRadius | **Neu:** Blocks → Beacon radius; Radius-Hilfe ohne Pyramidenlevel. |
| Breadcrumbs | **Bereits vorhanden:** BreadCrumbs. |
| Chams | Nicht portiert. Benötigt Minecraft-Render-Hooks oder zusätzliche Model-, Item-, Welt- oder HUD-Daten. |
| ChestEsp | **Neu:** Blocks → Storage ESP; Truhen plus weitere Container. |
| ChinaHat | **Bereits vorhanden:** Eigener F5-Hat und automatischer Friend-Hat. |
| Crosshair | **Neu:** Visuals → Crosshair; ergänzt Vanilla. |
| CustomModel | Nicht portiert. Benötigt Minecraft-Render-Hooks oder zusätzliche Model-, Item-, Welt- oder HUD-Daten. |
| DamageMarkers | **Neu:** Visuals → Damage markers; beobachteter HP-Verlust. |
| DamageParticles | **Bereits vorhanden:** Hit particles / Hit bubbles aus beobachtetem HP-Verlust. |
| EnchantmentColor | Nicht portiert. Benötigt Minecraft-Render-Hooks oder zusätzliche Model-, Item-, Welt- oder HUD-Daten. |
| ESP | **Bereits vorhanden:** Box ESP / Skeleton / Health bars. |
| FogColor | Nicht portiert. Benötigt Minecraft-Render-Hooks oder zusätzliche Model-, Item-, Welt- oder HUD-Daten. |
| FullBright | Nicht portiert. Benötigt Minecraft-Render-Hooks oder zusätzliche Model-, Item-, Welt- oder HUD-Daten. |
| GlowESP | **Neu:** Visuals → Glow ESP; externe Umrisse ohne Tiefenpuffer. |
| ItemESP | Nicht portiert. Nichtlebende Item-Entitäten werden bisher nicht vollständig erfasst. |
| JumpCircle | **Neu:** Visuals → Jump circles; Aufwärtsbewegung aus Snapshots. |
| NameProtect | **Neu:** Misc → Name protect; externe Anzeigen. |
| NameTags | **Bereits vorhanden:** Name tags mit HP, Ping und Equipment. |
| NightMode | Nicht portiert. Benötigt Minecraft-Render-Hooks oder zusätzliche Model-, Item-, Welt- oder HUD-Daten. |
| Nimb | **Neu:** Visuals → Halo / Nimb. |
| NoRender | Nicht portiert. Benötigt Minecraft-Render-Hooks oder zusätzliche Model-, Item-, Welt- oder HUD-Daten. |
| PearlESP | Nicht portiert. Projektil-/Pearl-Zustände werden bisher nicht vollständig erfasst. |
| PenisESP | Nicht portiert. Benötigt Minecraft-Render-Hooks oder zusätzliche Model-, Item-, Welt- oder HUD-Daten. |
| ScoreBoard | Nicht portiert. Scoreboard-Zeilen werden bisher nicht erfasst. |
| SwingAnimations | Nicht portiert. Benötigt Minecraft-Render-Hooks oder zusätzliche Model-, Item-, Welt- oder HUD-Daten. |
| TargetESP | **Bereits vorhanden:** Target aura / Target marker; zusätzlich neuer Target circle. |
| Tracers | **Bereits vorhanden:** Entity- und Block-Tracers. |
| ViewModel | Nicht portiert. Benötigt Minecraft-Render-Hooks oder zusätzliche Model-, Item-, Welt- oder HUD-Daten. |
| Weather | Nicht portiert. Benötigt Minecraft-Render-Hooks oder zusätzliche Model-, Item-, Welt- oder HUD-Daten. |
| Wings | **Neu:** Visuals → Wings. |
| XRay | Nicht portiert. Bestehendes Block ESP zeigt geladene Scan-Treffer; Minecrafts Block-Renderer wird nicht verändert. |

## Zusätzlich ergänzt

Chunk borders, Waypoints, Radar, Speedometer, Session statistics, Armor strip,
Clock und Click rate erweitern die Bedienung dieses Pakets. Sie sind eigenständige
mcov-Funktionen und werden nicht als identische Rockstar-Module ausgegeben.

## Nächste technische Schritte

1. Vollständige Inventar-/Item-/Projektildaten im separaten Sampler eröffnen Inventory preview, Hotbar, Item ESP und Pearl ESP.
2. Anschließend versionierte Client-Tick-Aktionen für Auto armor, Auto gapple, Auto potion und Middle click pearl ergänzen und einzeln prüfen.
3. Render-Hooks für Full bright, Fog color, View model und Swing animations benötigen eine eigene Integration in die jeweilige Minecraft-/Forge-Pipeline.
4. Movement- und Netzwerkmodule benötigen getrennte Adapter, Zustandsmaschinen und Tests; die bestehenden externen DrawLists können diese Funktionen nicht ersetzen.

