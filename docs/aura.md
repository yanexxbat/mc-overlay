# AuraRenderer

Die vollständige Implementierung liegt in [aura_renderer.hpp](../src/overlay/aura_renderer.hpp)
und [aura_renderer.cpp](../src/overlay/aura_renderer.cpp). Die Klasse ist C++20,
verwendet ausschließlich ImDrawList und enthält weder JNI noch eine Zielauswahl.
[aura_projection.hpp](../src/overlay/aura_projection.hpp) trennt Weltanker und
Bildschirmprojektion. [aura_blend_dx11.hpp](../src/overlay/aura_blend_dx11.hpp) und
[aura_blend_dx11.cpp](../src/overlay/aura_blend_dx11.cpp) besitzen den einmalig
erzeugten D3D11-Blendzustand über RAII.

Im bestehenden Overlay: **Visuals → Target aura → Enable aura**.
Das aktuelle Minecraft-Fadenkreuzziel wird anhand seiner vorhandenen Entity-ID
im Snapshot gefunden. Minecraft liefert dieses Ziel nur innerhalb seiner
Interaktionsreichweite. Die Aura kann über die Klassen-API auch auf jede andere
ausgewählte Entity gesetzt werden; sie verändert keine Spieleingaben.

## Konfiguration

| Parameter | Standard | Einheit / Bedeutung |
|---|---:|---|
| enabled | false | Opt-in im Menü |
| animate | true | Rotation und Größenpuls; aus friert beide Phasen ein |
| rayCount | 24 | 3–64 gleichmäßig verteilte Strahlen |
| rayLength | 1.35 | Vielfaches des projizierten Entitätsradius |
| rayWidth | 0.11 | Maximale volle Strahlenbreite relativ zum Radius |
| rotationDegreesPerSecond | 12 | Grad/s; negative Werte drehen rückwärts |
| pulseAmplitude | 0.06 | Relativer Größenhub, maximal 0.3 |
| pulseHz | 0.6 | Pulse/s; 0 hält die aktuelle Pulsphase |
| opacity | 0.65 | Gesamtintensität |
| fadeSeconds | 0.18 | Ziel-Fade; 0 wechselt sofort |
| startColor | (0.78, 0.91, 1, 1) | RGBA im Kern |
| endColor | (0.18, 0.40, 1, 0) | RGBA außen |

Nicht endliche Konfigurationswerte erhalten sichere Ersatzwerte. Der Alpha-Wert
am Strahlende wird zusätzlich auf null ausgeblendet, auch bei einer deckenden
Endfarbe. Das Overlay berücksichtigt den vorhandenen Menüschalter „Reduced
motion“ und die Windows-Einstellung für Animationen. Deckkraftwechsel bleiben
erhalten; das Ziel selbst folgt ohne Positions-Lerp seiner aktuellen Projektion.

## Vollständiges Beispiel für den Render-Loop

Dieser Besitzer lebt so lange wie der Renderer. `Initialize()` erfolgt einmal
nach der D3D11-Geräteinitialisierung. `Frame()` wird zwischen `ImGui::NewFrame()`
und `ImGui::Render()` aufgerufen, vor ESP, HUD und Menü. Der Snapshot stammt aus
dem bestehenden IPC-Leser.

```cpp
#include "overlay/aura_projection.hpp"
#include "overlay/aura_blend_dx11.hpp"
#include <algorithm>

class AuraExample {
    mcov::AuraRenderer renderer_;
    mcov::AuraBlendDX11 blend_;
    bool prepared_ = false;
public:
    mcov::AuraConfig config;

    bool Initialize(ID3D11Device* device) {
        config.enabled = true;
        return blend_.Initialize(device);
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
        const ImVec2 origin = ImGui::GetMainViewport()->Pos;
        for (const auto& item : renderer_.Targets()) {
            if (!item.valid || item.alpha <= 0) continue;
            mcov::AuraScreen screen;
            if (!mcov::ProjectAura(camera, item.anchor, io.DisplaySize.x, io.DisplaySize.y, screen)) continue;
            screen.center.x += origin.x;
            screen.center.y += origin.y;
            renderer_.Draw(draw, screen, item.alpha, config, blend_.Callbacks());
        }
    }
};
```

Bei fehlender Verbindung bzw. Welt muss der Besitzer `renderer_.Reset()`
aufrufen. Das integrierte `Overlay::DrawAura()` übernimmt dies bereits. Bei
einem Geräte-/ImGui-Kontext-Neustart ebenfalls den Vorwärmzustand zurücksetzen.

## Blending

ImGui hat keinen globalen Schalter für additives Blending. `AddCallback()` stellt
einen GPU-Zustandswechsel in die Draw-Liste; ausgeführt wird er erst innerhalb
von `ImGui_ImplDX11_RenderDrawData()`. Den Blendzustand vorher am Context zu
setzen reicht nicht, weil das Backend seine Standardzustände setzt.

| Modus | RGB-Formel | Alpha-Formel |
|---|---|---|
| Normaler ImGui-Modus | Cs × As + Cd × (1 − As) | As + Ad × (1 − As) |
| Aura | Cs × As + Cd | As + Ad × (1 − As) |

RGB wird addiert, während Alpha die Abdeckung des transparenten Overlay-Fensters
beibehält. Die getrennten RGB-/Alpha-Faktoren entsprechen dem
[D3D11-Blendmodell](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_render_target_blend_desc).
Nach dem Effekt stellt `ImDrawCallback_ResetRenderState` das normale ImGui-Blending
wieder her. Die Klasse benutzt dafür einen RAII-Scope und restauriert auch die
DrawList-Flags. Der Backend-Callback verwendet den in ImGui 1.91.9b exponierten
[D3D11-RenderState](https://github.com/ocornut/imgui/blob/v1.91.9b/backends/imgui_impl_dx11.h).

Für OpenGL 3.3 bleibt `AuraRenderer` unverändert. Mit den über den vorhandenen
GL-Loader bereitgestellten Funktionen verwendet man folgenden Callback:

```cpp
static void BeginAuraOpenGL(const ImDrawList*, const ImDrawCmd*) {
    glEnable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}
// Am Draw-Aufruf: mcov::AuraBlend{BeginAuraOpenGL, nullptr} übergeben.
// Das OpenGL3-ImGui-Backend führt anschließend ResetRenderState aus.
```

Ein externes Fenster kann nur auf seinem eigenen Renderziel additiv mischen.
Minecrafts Framebuffer ist nicht der Destinationsbuffer dieser Berechnung; der
Desktop-Compositor legt das Ergebnis danach über das Spiel. Echtes additives
Mischen mit der Spielszene erfordert einen gemeinsamen Renderpass im Spiel.
Der Effekt hier erzeugt den Glow durch das Addieren seiner eigenen Schichten.

## Geometrie und Speicher

Ein weicher Ring, drei Strahlenbreiten und ein heller Kern werden als konvexe
Polygone mit `AddConvexPolyFilled()` ausgegeben. Danach erhalten deren Vertices
die inneren und äußeren Farben. Der Rasterizer interpoliert die Farben; acht
Strahlensegmente glätten das Längsprofil. Die Polygonpunkte laufen im Bildschirm-
Koordinatensystem im Uhrzeigersinn. AA-Fill ist innerhalb des Effekts deaktiviert,
damit es keine zusätzlichen AA-Vertices und temporären Puffer gibt.

Kreisrichtungen und normierte Strahlenprofile werden im Konstruktor berechnet;
Strahlenrichtungen nur bei einer geänderten Anzahl. Zwei feste Ziel-Slots erlauben
den Fade beim Zielwechsel, ohne Vektor/List-Allokationen. Rückwechsel übernehmen
die aktuelle Deckkraft. Das ausblendende Ziel wird in Weltkoordinaten nachgeführt
und jedes Frame erneut projiziert.

`Prepare()` reserviert Platz für beide Auren mit maximaler Strahlenzahl: 18.432
Vertices und 27.648 Indices, zusammen etwa 414 KiB bei 20-Byte-Vertices und
16-Bit-Indices. Danach allokiert der Aura-Drawpfad keinen Heap-Speicher.
ImGui und sein Backend besitzen eigene dynamische Frame-/GPU-Puffer, die beim
erstmaligen Wachstum oder bei zusätzlicher UI weiterhin wachsen können. Ein
statischer GPU-VBO lässt sich nicht gleichzeitig über `AddConvexPolyFilled()`
verwenden; hier werden normierte Vorlagen in ImGuis vorhandenen Stream kopiert.

## Fallstricke

- **Z-Order:** Aura zuerst in die BackgroundDrawList, dann Boxen/Skelett und HUD.
  Menüs bleiben darüber. Kein Zugriff auf Minecrafts Tiefenpuffer; der Effekt
  zeichnet wie das bisherige ESP durch Wände.
- **Clipping:** Den vorhandenen ClipRect respektieren. Hinter der Kamera oder
  Near Plane wird nichts gezeichnet. Sehr nahe Entitäten erhalten einen begrenzten
  Bildschirmradius. Außerhalb des Viewports wird nur die Ausgabe abgeschnitten;
  das Welt-/Bildschirmzentrum wird nicht aus einer abgeschnittenen Box berechnet.
- **Matrix:** Kamera und Entität aus demselben kohärenten Snapshot verwenden,
  dieselbe FOV-Korrektur und View-Bob-Matrix wie ESP. Niemals eine alte Pixelposition
  bei einer neuen Kameramatrix weiterverwenden. Weltkoordinaten vor der float-
  Matrix in double relativ zum Kameraursprung berechnen.
- **Auflösung:** `io.DisplaySize` bzw. im Win32-Overlay dessen Clientgröße für die
  Projektion verwenden. Strahlenabmessungen sind relativ zur perspektivischen
  Entitätsgröße. `DisplayFramebufferScale` berücksichtigt das passende Backend;
  nicht ein zweites Mal in die Aura multiplizieren. Menü-Font-/GUI-Skalierung
  gehört nicht in die Weltprojektion.
- **Lebensdauer:** Blendzustand und Callback-Daten bis zum tatsächlichen Rendern
  behalten. Keine kurzlebigen Lambda-Captures oder lokale GPU-Objekte übergeben.
- **Indexformat:** Bei großen Draw-Listen muss das Backend `VtxOffset` unterstützen.
  Die nachträgliche Farbzuweisung verwendet globale `VtxBuffer`-Offsets und
  funktioniert auch beim Überschreiten der 16-Bit-Indexgrenze.
- **Helligkeit:** Additive Schichten können in einem UNORM-Renderziel sättigen.
  Intensität reduzieren, bevor Strahlenzahl und Breite stark erhöht werden.

## Prüfung

`aura_rendering` prüft Perspektive, Weltgrenzen, Zielwechsel, echte D3D11-WARP-
Pixel, Alpha-Abdeckung, Blend-Reset, Clipping, große Vertex-Offsets und null
Aura-Heap-Allokationen nach dem Vorwärmen. `control_panel` rendert den integrierten
ESP/Aura-Pfad ohne Minecraft nach `ui-preview/aura.png`. Die Ausgabe liegt unter
dem jeweils verwendeten Build-Verzeichnis. Eine Live-Spielszene ist damit nicht
getestet.
