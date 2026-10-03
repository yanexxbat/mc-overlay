#pragma once

#include "esp_geometry.hpp"
#include "imgui.h"

namespace mcov {

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

    bool showPlayers = true;
    bool showHostiles = true;
    bool showOther = false;

    float maxDistance = 128.f;
    float boxThickness = 1.6f;
    float boxRounding = 7.f;
    float skeletonThickness = 1.1f;
    float labelSize = 12.f;

    // Helle Konturen; der violette Lebensbalken verbindet ESP und Aura optisch.
    float playerColor[4] = {.94f, .94f, .96f, 1.f};
    float hostileColor[4] = {.94f, .94f, .96f, 1.f};
    float otherColor[4] = {.94f, .94f, .96f, 1.f};
    float healthColor[4] = {.66f, .42f, 1.f, 1.f};
    float tracerColor[4] = {.66f, .42f, 1.f, .45f};
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
