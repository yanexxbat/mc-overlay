# Orb-/Fog-Aura

Die Aura folgt der Screenshot-Referenz: vier leuchtende Orbs mit weißen Kernen,
violetten Halos und türkisfarbenen, gebogenen Nebelschweifen. Es gibt keinen
radialen Stern mehr. Alle Orb- und Trailpositionen liegen auf geneigten 3D-Bahnen
um die tatsächliche AABB-Mitte der ausgewählten Entität.

Aktivierung: **Visuals → Target aura → Enable aura**. Das Ziel stammt aus der
vorhandenen Entity-ID des Minecraft-Fadenkreuzziels. Die Klassen-API kann auch
eine andere ausgewählte Entity verwenden. Es entstehen keine zusätzlichen
JNI-Aufrufe und keine Änderungen an Spieleingaben.

## Aufbau

- [aura_renderer.hpp](../src/overlay/aura_renderer.hpp) und
  [aura_renderer.cpp](../src/overlay/aura_renderer.cpp): Konfiguration, Ziel-Fade,
  analytische Weltbahnen und gebatchte ImDrawList-Sprites.
- [aura_projection.hpp](../src/overlay/aura_projection.hpp): separate Projektion
  jedes einzelnen Orb-/Schweifpunkts mit derselben Kamera wie ESP.
- [aura_blend_dx11.hpp](../src/overlay/aura_blend_dx11.hpp) und
  [aura_blend_dx11.cpp](../src/overlay/aura_blend_dx11.cpp): RAII-Besitzer für den
  Blendzustand und eine einmalig erzeugte 128×128-Gauß-Textur.

Die transparente Textur wird mathematisch berechnet und einmal hochgeladen.
Der weiche Nebel entsteht durch überlappende `AddImage()`-Quads. Es gibt keine
Desktop-Capture-, Shader-Blur- oder zusätzlichen RenderTarget-Pässe. Der Kern
wird mit kleineren, helleren Sprites ergänzt; der Schweif verjüngt sich und
blendet bis Alpha null aus. Ein gemeinsamer Textur-/Blendzustand batcht den
Effekt, statt für jeden Orb einen eigenen Renderpass zu öffnen.

## Konfiguration

| Parameter | Standard | Bedeutung |
|---|---:|---|
| enabled | false | Opt-in im Menü |
| animate | true | Aus friert die Umlauf- und Pulsphasen ein |
| orbCount | 4 | 1–8 Orbs |
| orbitRadius | 0.88 | Umlaufradius relativ zum Entitätsradius |
| orbSize | 0.20 | Orb-Radius relativ zum Entitätsradius |
| trailDegrees | 100 | Gebogene Schweiflänge auf der Umlaufbahn |
| rotationDegreesPerSecond | 75 | Grad/s, negative Werte drehen rückwärts |
| pulseAmplitude | 0.035 | Relativer Größenhub, 0 deaktiviert ihn |
| pulseHz | 0.4 | Pulse/s |
| opacity | 0.9 | Gesamtintensität |
| fadeSeconds | 0.18 | Ziel-Fade, 0 wechselt sofort |
| startColor | (0.66, 0.42, 1, 1) | Violetter Orb-Halo |
| endColor | (0.05, 1, 0.88, 0.85) | Türkiser Schweif |

Der eigene Schalter **Target aura → Animation** steuert Rotation und Pulsieren;
Ziel-Fades bleiben beim Ausschalten erhalten. „Reduced motion“ und die
Windows-Einstellung für Animationen gelten für das Menü und halten eine explizit
aktivierte Aura-Animation nicht an. Bei **Rotation = 0 deg/s** steht die Umlaufbahn;
positive Werte drehen vorwärts, negative rückwärts. Der Schweif wird aus
den vorherigen Orb-Winkeln um den **aktuellen** Weltanker berechnet. Damit zieht
eine bewegte oder teleportierende Entität keinen veralteten Pixel-Schweif hinter
sich her. Auch das ausblendende Ziel wird in Weltkoordinaten nachgeführt.

## Vollständiger Aufruf im Render-Loop

`Initialize()` einmal nach der D3D11-Geräteinitialisierung aufrufen. `Frame()`
liegt zwischen `ImGui::NewFrame()` und `ImGui::Render()`, vor ESP, HUD und Menü.
Der Besitzer bleibt bis nach `RenderDrawData()` am Leben.

```cpp
#include "overlay/aura_projection.hpp"
#include "overlay/aura_blend_dx11.hpp"
#include <algorithm>

class AuraExample {
    mcov::AuraRenderer renderer_;
    mcov::AuraBlendDX11 resources_;
    bool prepared_ = false;
public:
    mcov::AuraConfig config;

    bool Initialize(ID3D11Device* device) {
        config.enabled = true;
        return resources_.Initialize(device);
    }

    void Frame(const mcov::WorldSnapshot& snapshot, float fovTrim = 0.f) {
        auto& io = ImGui::GetIO();
        auto& draw = *ImGui::GetBackgroundDrawList();
        if (!prepared_) {
            mcov::AuraRenderer::Prepare(draw);
            prepared_ = true;
        }
        const mcov::CameraProjection camera(snapshot, io.DisplaySize.x, io.DisplaySize.y, fovTrim);
        mcov::AuraAnchor selected{};
        const mcov::AuraAnchor* target = nullptr;
        const auto count = std::min(snapshot.entityCount, mcov::kMaxEntities);
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto& entity = snapshot.entities[i];
            if (entity.flags & mcov::kEntityIsSelf) continue;
            mcov::AuraAnchor anchor;
            if (!mcov::MakeAuraAnchor(entity, anchor)) continue;
            renderer_.RefreshAnchor(anchor);
            if (snapshot.hasTarget && entity.entityId == snapshot.targetEntityId) {
                selected = anchor;
                target = &selected;
            }
        }
        renderer_.UpdateTarget(target, io.DeltaTime, config);
        for (const auto& item : renderer_.Targets()) {
            if (!item.valid || item.alpha <= 0) continue;
            mcov::AuraWorldFog world;
            mcov::AuraScreenFog screen;
            renderer_.BuildWorldFog(item.anchor, config, world);
            if (!mcov::ProjectAuraFog(camera, world, io.DisplaySize.x, io.DisplaySize.y,
                                      screen, ImGui::GetMainViewport()->Pos)) continue;
            renderer_.Draw(draw, screen, item.alpha, config, resources_.Callbacks(), resources_.GlowTexture());
        }
    }
};
```

Bei fehlender Verbindung bzw. Welt `renderer_.Reset()` aufrufen. Das integrierte
`Overlay::DrawAura()` übernimmt dies. Bei einem Geräte-/ImGui-Neustart zusätzlich
Ressourcen und Vorwärmzustand neu initialisieren.

## Blending und Lebensdauer

Der Effekt nutzt additives RGB: **Cs × As + Cd**. Alpha berechnet weiterhin die
Fensterabdeckung als **As + Ad × (1 − As)**. RGB-/Alpha-Faktoren werden separat
im [D3D11-Blendzustand](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_render_target_blend_desc)
gesetzt. `AddCallback()` aktiviert den Zustand erst innerhalb des ImGui-
Renderpasses. Ein RAII-Scope restauriert die Textur und hängt anschließend
`ImDrawCallback_ResetRenderState` an, damit ESP, HUD und Menü normal rendern.
Ressourcen und Callback-Daten müssen bis zum tatsächlichen Rendern gültig bleiben.

Ein externes Fenster addiert seine eigenen Glow-Schichten auf seinem Renderziel;
Minecrafts Framebuffer ist nicht Teil dieses Blendpasses. Der Desktop-Compositor
legt das Ergebnis später über das Spiel. Unter 1.21.11 Fabric / normalem OpenGL
verdeckt der neue [Tiefenvergleich](world-depth.md) hinter Spielern oder Blöcken
liegende Orbs und Schweifteile. Andere Versionen behalten die bisherige Darstellung.

Für OpenGL benötigt derselbe `AuraRenderer` eine einmal hochgeladene weiße RGBA-
Gauß-Textur und einen Callback mit `glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD)`
und `glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE_MINUS_SRC_ALPHA)`.
Die Textur-ID wird über `ImTextureID` übergeben; das OpenGL3-ImGui-Backend verarbeitet
anschließend denselben Reset-Callback. Das Projekt selbst verwendet Win32/D3D11.

## Performance und Fallstricke

- Feste Arrays für maximal acht Orbs, 32 Schweifpunkte und zwei Ziel-Slots;
  nach `Prepare()` keine Aura-Heap-Allokationen im Renderpfad. Vorberechnete
  Winkelvorlagen werden nur bei einer geänderten Schweiflänge neu berechnet.
- Maximal 4.288 Vertices und 6.432 Indices für beide Auren, etwa 96 KiB bei
  20-Byte-Vertices/16-Bit-Indices; die GPU-Textur benötigt 64 KiB. ImGui kann seine
  allgemeinen UI-/GPU-Puffer beim erstmaligen Wachstum weiterhin vergrößern.
- Kamera, Entity und alle Schweifpunkte aus demselben Snapshot projizieren.
  Weltkoordinaten bleiben bis zur Subtraktion des Kameraursprungs in double.
  Jede Kamerabewegung aktualisiert die komplette Projektion, keine alte Pixelspur.
- Größen sind perspektivisch und relativ zur Entität. Client-/ImGui-Viewportgröße
  verwenden und `DisplayFramebufferScale` nicht zweimal anwenden.
- Bestehenden ClipRect respektieren; Punkte hinter der Near Plane verwerfen.
  Sehr große Orb-Radien direkt an der Kamera werden begrenzt.
- Aura zuerst in die BackgroundDrawList, dann Boxen/Skelett und HUD; Menüs darüber.
  Große Draw-Listen benötigen die vorhandene Backend-Unterstützung für VtxOffset.
- Additive Schichten können saturieren. Bei vielen großen Orbs Intensität senken.

## Build und Prüfung

Es gibt einen aktuellen Build unter **build/**. Overlay, Injector und Agent
liegen in **build/bin/**, die gerenderten Vorschauen in **build/ui-preview/**.
`scripts/build.ps1` verwendet ebenfalls ausschließlich diesen Buildordner.
Mit `-Clean` werden alte Binärdateien, Backups und Diagnosen entfernt; die
vorhandenen ImGui-/Mapping-Quellcaches bleiben für Offline-Builds erhalten.
`-Checks` baut und startet die acht Regressionstests.

`aura_rendering` prüft echte GPU-Pixel, Alpha-Abdeckung und Blend-Reset sowie
3D-Umlaufbahnen, Rotationsgeschwindigkeit/-richtung, Pause/Fortsetzen,
gebogene Trails, Entity-/Kamerabewegungen, FPS-Unabhängigkeit,
Ziel-Fades, Clipping, große Vertex-Offsets und null Aura-Allokationen nach dem
Vorwärmen. `control_panel` prüft die Rotation im integrierten ESP/Fog-Pfad bei
reduzierten Menüanimationen sowie den eigenen Animation-Schalter und rendert
nach `build/ui-preview/aura.png`. Dies ist eine Vorschau ohne Live-Spielszene.
