#pragma once

#include "esp_geometry.hpp"

// Fixed-size world/projection data. No game queries, state history or heap work.
namespace mcov::visual {

struct Cuboid {
    std::array<WorldPoint, 8> points{};
    inline static constexpr int edges[12][2] = {
        {0,1}, {2,3}, {4,5}, {6,7}, {0,2}, {1,3},
        {4,6}, {5,7}, {0,4}, {1,5}, {2,6}, {3,7}};
    inline static constexpr int faces[6][4] = {
        {0,4,6,2}, {1,3,7,5}, {0,1,5,4},
        {2,6,7,3}, {0,2,3,1}, {4,5,7,6}};
};
struct ProjectedFace {
    std::array<ScreenPoint, 12> points{};
    int count = 0;
};
inline bool Finite(WorldPoint p) noexcept {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
inline WorldPoint Subtract(WorldPoint a, WorldPoint b) noexcept {
    return {a.x-b.x, a.y-b.y, a.z-b.z};
}
inline WorldPoint Cross(WorldPoint a, WorldPoint b) noexcept {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
inline double Dot(WorldPoint a, WorldPoint b) noexcept { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Cuboid BoundsCuboid(WorldBounds bounds) noexcept {
    Cuboid result;
    for (int i = 0; i < 8; ++i)
        result.points[i] = {i&1 ? bounds.max.x : bounds.min.x,
                            i&2 ? bounds.max.y : bounds.min.y,
                            i&4 ? bounds.max.z : bounds.min.z};
    return result;
}
inline bool ValidBounds(WorldBounds b) noexcept {
    return Finite(b.min) && Finite(b.max) && b.min.x < b.max.x &&
           b.min.y < b.max.y && b.min.z < b.max.z;
}
inline bool FacingCamera(const Cuboid& cube, int face, WorldPoint camera) noexcept {
    if (face < 0 || face >= 6 || !Finite(camera)) return false;
    const auto& indices = Cuboid::faces[face];
    const auto a = cube.points[indices[0]];
    const auto normal = Cross(Subtract(cube.points[indices[1]], a),
                              Subtract(cube.points[indices[2]], a));
    return Dot(normal, Subtract(camera, a)) > 1e-10;
}
inline bool ProjectFace(const CameraProjection& projection, const Cuboid& cube,
                        int face, float width, float height, ProjectedFace& out) noexcept {
    out.count = 0;
    if (face < 0 || face >= 6 || !std::isfinite(width) || !std::isfinite(height) ||
        width < 1 || height < 1) return false;
    std::array<WorldPoint, 8> clipped{};
    int count = 0;
    // Clip in world space at the camera's near plane before perspective divide.
    // The epsilon keeps roundoff from rejecting an intersection in Point().
    const float nearDepth = projection.NearPlane() * 1.001f;
    const auto& faceIndices = Cuboid::faces[face];
    WorldPoint previous = cube.points[faceIndices[3]];
    float previousDepth = projection.Depth(previous);
    if (!Finite(previous) || !std::isfinite(previousDepth)) return false;
    for (int i = 0; i < 4; ++i) {
        const auto current = cube.points[faceIndices[i]];
        const float currentDepth = projection.Depth(current);
        if (!Finite(current) || !std::isfinite(currentDepth)) return false;
        const bool in = currentDepth >= nearDepth, wasIn = previousDepth >= nearDepth;
        if (in != wasIn) {
            const double t = double(nearDepth-previousDepth) / double(currentDepth-previousDepth);
            clipped[count++] = {previous.x+(current.x-previous.x)*t,
                                previous.y+(current.y-previous.y)*t,
                                previous.z+(current.z-previous.z)*t};
        }
        if (in) clipped[count++] = current;
        previous = current; previousDepth = currentDepth;
    }
    if (count < 3) return false;
    std::array<ScreenPoint, 12> points{}, next{};
    for (int i = 0; i < count; ++i)
        if (!projection.Point(clipped[i], points[i])) return false;
    // Sutherland-Hodgman viewport clipping bounds geometry and AA work even when
    // a limb or a face crosses the camera plane immediately beside the camera.
    for (int plane = 0; plane < 4; ++plane) {
        const auto distance = [=](ScreenPoint p) {
            switch (plane) {
                case 0: return p.x;
                case 1: return width-p.x;
                case 2: return p.y;
                default: return height-p.y;
            }
        };
        int nextCount = 0;
        auto prior = points[count-1];
        float priorDistance = distance(prior);
        for (int i = 0; i < count; ++i) {
            const auto p = points[i]; const float d = distance(p);
            if ((d >= 0) != (priorDistance >= 0)) {
                const float t = priorDistance/(priorDistance-d);
                if (nextCount >= int(next.size())) return false;
                next[nextCount++] = {prior.x+(p.x-prior.x)*t, prior.y+(p.y-prior.y)*t};
            }
            if (d >= 0) {
                if (nextCount >= int(next.size())) return false;
                next[nextCount++] = p;
            }
            prior = p; priorDistance = d;
        }
        if (nextCount < 3) return false;
        points = next; count = nextCount;
    }
    out.points = points; out.count = count;
    return true;
}
inline bool LimbCuboid(WorldPoint start, WorldPoint end, double halfWidth,
                       double halfDepth, Cuboid& out) noexcept {
    if (!Finite(start) || !Finite(end) || !std::isfinite(halfWidth) ||
        !std::isfinite(halfDepth) || halfWidth <= 0 || halfDepth <= 0) return false;
    auto axis = Subtract(end, start);
    const double length = std::sqrt(Dot(axis, axis));
    if (!std::isfinite(length) || length < 1e-5) return false;
    axis = {axis.x/length, axis.y/length, axis.z/length};
    const WorldPoint reference = std::abs(axis.y) < .95 ? WorldPoint{0,1,0} : WorldPoint{0,0,1};
    auto side = Cross(axis, reference);
    const double sideLength = std::sqrt(Dot(side, side));
    side = {side.x/sideLength, side.y/sideLength, side.z/sideLength};
    const auto front = Cross(side, axis);
    for (int i = 0; i < 8; ++i) {
        const auto anchor = (i&2) ? end : start;
        const double x = (i&1) ? halfWidth : -halfWidth;
        const double z = (i&4) ? halfDepth : -halfDepth;
        out.points[i] = {anchor.x+side.x*x+front.x*z,
                         anchor.y+side.y*x+front.y*z,
                         anchor.z+side.z*x+front.z*z};
    }
    return true;
}
struct HumanoidHighlight {
    // Torso, head, upper/lower arms, upper/lower legs. Approximate vanilla rig,
    // not sampled Minecraft mesh vertices or modded skin geometry.
    std::array<Cuboid, 10> parts{};
    int count = 0;
};
inline HumanoidHighlight BuildHighlight(const EntitySnapshot& entity,
                                        const HumanoidRig& rig) noexcept {
    HumanoidHighlight result;
    if (entity.rigKind == kRigNone) return result;
    const double h = entity.modelHeight > 0 ? entity.modelHeight : entity.height;
    if (!std::isfinite(h) || h <= 0 || h > 16) return result;
    const bool enderman = entity.rigKind == kRigEnderman;
    const auto part = [&](WorldPoint a, WorldPoint b, double width, double depth) {
        if (LimbCuboid(a,b,width,depth,result.parts[result.count])) ++result.count;
    };
    part(rig.joints[2], rig.joints[1], h*(enderman ? .10 : .15), h*.075);
    const auto headAxis = Subtract(rig.joints[0], rig.joints[1]);
    const double headLength = std::sqrt(Dot(headAxis, headAxis));
    if (headLength > 1e-5) {
        const double radius = h*(enderman ? .086 : .13);
        const WorldPoint a{rig.joints[0].x-headAxis.x/headLength*radius,
                           rig.joints[0].y-headAxis.y/headLength*radius,
                           rig.joints[0].z-headAxis.z/headLength*radius};
        const WorldPoint b{rig.joints[0].x+headAxis.x/headLength*radius,
                           rig.joints[0].y+headAxis.y/headLength*radius,
                           rig.joints[0].z+headAxis.z/headLength*radius};
        part(a,b,radius,radius);
    }
    const double limb = h*(enderman ? .037 : .061);
    for (const auto& edge : std::array<std::array<int,2>,8>{{
             {3,5}, {5,7}, {4,6}, {6,8}, {9,11}, {11,13}, {10,12}, {12,14}}})
        part(rig.joints[edge[0]],rig.joints[edge[1]],limb,limb);
    return result;
}

} // namespace mcov::visual
