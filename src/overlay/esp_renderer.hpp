#pragma once

#include "esp_geometry.hpp"
#include "imgui.h"

namespace mcov {

enum class EspBoxStyle { Corners2D, Full2D, Wire3D };
enum class EspBarPlacement { Vertical, Horizontal };
enum class EspTracerOrigin { Bottom, Center, Top, Custom };
enum class EspHighlight { Off, Bounds, Humanoid };

struct VisualSettings {
    bool boxEsp = true;
    bool skeleton = true;
    bool filledBox = false;
    bool tracers = false;
    bool nameTags = true;
    bool nameTagHealth = true;
    bool nameTagPing = true;
    bool nameTagEquipment = true;
    bool healthBars = true;
    bool distanceText = true;
    bool targetInfo = true;
    bool nameProtect = false; // Nur die externen Beschriftungen anonymisieren.

    bool showPlayers = true;
    bool showHostiles = true;
    bool showOther = false;

    float maxDistance = 128.f;
    float boxThickness = 1.6f;
    float boxRounding = 7.f;
    float skeletonThickness = 1.1f;
    float labelSize = 12.f;

    // Existing menu controls keep their meanings; the health accent is the
    // healthy endpoint of the optional red/yellow/green ramp.
    float playerColor[4] = {.94f, .94f, .96f, 1.f};
    float hostileColor[4] = {.94f, .94f, .96f, 1.f};
    float otherColor[4] = {.94f, .94f, .96f, 1.f};
    float healthColor[4] = {.30f, .91f, .55f, 1.f};
    float tracerColor[4] = {.66f, .42f, 1.f, .45f};

    // Style controls live in the existing Visuals sections of the menu.
    EspBoxStyle boxStyle = EspBoxStyle::Corners2D;
    float cornerFraction = .28f;
    float fillAlpha = .12f;
    float fillBottomAlpha = .025f;
    bool fillGradient = true;
    EspBarPlacement barPlacement = EspBarPlacement::Vertical;
    bool healthGradient = true;
    bool armorBars = true; // Shares the existing health-bars module switch.
    float barThickness = 3.f;
    float healthLow[4] = {.96f, .28f, .35f, 1.f};
    float healthMiddle[4] = {1.f, .77f, .30f, 1.f};
    float armorColor[4] = {.35f, .73f, 1.f, .95f};
    EspTracerOrigin tracerOrigin = EspTracerOrigin::Bottom;
    ImVec2 customTracerOrigin{.5f, 1.f}; // Normalized viewport coordinates.
    float tracerThickness = 1.25f;
    float tracerStartAlpha = .08f;
    float tracerEndColor[4] = {.43f, .82f, 1.f, .8f};
    bool textOutline = true;
    bool targetGlow = true; // Shares the existing box-ESP module switch.
    float glowRadius = 5.f;
    float glowOpacity = .45f;
    EspHighlight targetHighlight = EspHighlight::Humanoid;
    float highlightAlpha = .18f;
    float targetColor[4] = {.69f, .52f, 1.f, 1.f};
};

// Reiner ImDrawList-Renderer. Kamera und Entitäten kommen aus demselben
// unveränderten Snapshot; keine JNI-Abfragen und keine Positionshistorie.
class EspRenderer final {
public:
    static void Draw(ImDrawList& draw, const WorldSnapshot& snapshot,
                     const CameraProjection& projection, const VisualSettings& config,
                     ImVec2 viewportSize, ImVec2 viewportOrigin = {}, ImTextureID itemTexture = 0);
};

} // namespace mcov
