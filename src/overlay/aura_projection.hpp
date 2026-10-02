#pragma once
#include "aura_renderer.hpp"
#include "esp_geometry.hpp"

namespace mcov {
// Weltposition: tatsächliche AABB-Mitte statt Kopf-/Fußposition schätzen.
inline bool MakeAuraAnchor(const EntitySnapshot& entity, AuraAnchor& out) noexcept {
    const auto bounds = EntityBounds(entity);
    const double x = bounds.max.x - bounds.min.x, y = bounds.max.y - bounds.min.y, z = bounds.max.z - bounds.min.z;
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || x <= 0 || y <= 0 || z <= 0) return false;
    out = {entity.entityId, (bounds.min.x + bounds.max.x) * .5, (bounds.min.y + bounds.max.y) * .5,
           (bounds.min.z + bounds.max.z) * .5, float(std::max({x, y, z}) * .5)};
    return std::isfinite(out.x) && std::isfinite(out.y) && std::isfinite(out.z) && std::isfinite(out.worldRadius);
}
// Bildschirmposition: dieselbe Kamera wie ESP; kein gecachtes Pixelzentrum.
inline bool ProjectAura(const CameraProjection& projection, const AuraAnchor& anchor,
                        float viewportWidth, float viewportHeight, AuraScreen& out) noexcept {
    ScreenPoint center;
    float radius;
    if (!projection.Billboard({anchor.x, anchor.y, anchor.z}, anchor.worldRadius, center, radius)) return false;
    const float limit = std::min(viewportWidth, viewportHeight) * .35f;
    if (!std::isfinite(limit) || limit < 1 || radius < .5f) return false;
    // Schutz vor enormen Dreiecken direkt an der Near Plane.
    out = {{center.x, center.y}, std::min(radius, limit)};
    return true;
}
} // namespace mcov
