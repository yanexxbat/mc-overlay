// render_math.hpp - minimal column-major 4x4 math for world -> screen projection.
//
// The agent publishes camera position, yaw, pitch and FOV; this header rebuilds
// the view-projection matrix on the overlay side. Doing it here instead of
// reading Minecraft's own matrix stack over JNI removes a whole class of
// mapping fragility and costs about twenty lines of trigonometry.
#pragma once

#include <cmath>

namespace mcov {

struct Mat4 {
    // Column-major, matching OpenGL/JOML: m[column * 4 + row].
    float m[16]{};

    static Mat4 Identity() {
        Mat4 result;
        result.m[0] = result.m[5] = result.m[10] = result.m[15] = 1.0f;
        return result;
    }
};

inline Mat4 Multiply(const Mat4& a, const Mat4& b) {
    Mat4 out;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += a.m[k * 4 + row] * b.m[column * 4 + k];
            out.m[column * 4 + row] = sum;
        }
    }
    return out;
}

inline Mat4 Perspective(float fovYDegrees, float aspect, float nearZ, float farZ) {
    const float rad   = fovYDegrees * 3.14159265358979323846f / 180.0f;
    const float focal = 1.0f / std::tan(rad * 0.5f);

    Mat4 out;
    out.m[0]  = focal / (aspect > 0.0001f ? aspect : 1.0f);
    out.m[5]  = focal;
    out.m[10] = (farZ + nearZ) / (nearZ - farZ);
    out.m[11] = -1.0f;
    out.m[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
    return out;
}

// Right-handed look-at, built from a forward vector rather than a target point.
inline Mat4 LookAlong(float eyeX, float eyeY, float eyeZ,
                      float dirX, float dirY, float dirZ) {
    // f = normalized forward
    float length = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
    if (length < 1e-6f) length = 1.0f;
    const float fx = dirX / length, fy = dirY / length, fz = dirZ / length;

    // s = normalize(cross(f, worldUp)), u = cross(s, f)
    const float upX = 0.0f, upY = 1.0f, upZ = 0.0f;
    float sx = fy * upZ - fz * upY;
    float sy = fz * upX - fx * upZ;
    float sz = fx * upY - fy * upX;
    float sLength = std::sqrt(sx * sx + sy * sy + sz * sz);
    if (sLength < 1e-6f) sLength = 1.0f;
    sx /= sLength; sy /= sLength; sz /= sLength;

    const float ux = sy * fz - sz * fy;
    const float uy = sz * fx - sx * fz;
    const float uz = sx * fy - sy * fx;

    Mat4 out = Mat4::Identity();
    out.m[0] = sx;  out.m[4] = sy;  out.m[8]  = sz;
    out.m[1] = ux;  out.m[5] = uy;  out.m[9]  = uz;
    out.m[2] = -fx; out.m[6] = -fy; out.m[10] = -fz;
    out.m[12] = -(sx * eyeX + sy * eyeY + sz * eyeZ);
    out.m[13] = -(ux * eyeX + uy * eyeY + uz * eyeZ);
    out.m[14] =  (fx * eyeX + fy * eyeY + fz * eyeZ);
    return out;
}

// Minecraft's angle convention, which is not the usual one:
//   yaw   0 = facing +Z (south), and it increases CLOCKWISE when viewed from above
//   pitch positive = looking DOWN
// Hence the negated sines below.
inline void DirectionFromMinecraftAngles(float yawDegrees, float pitchDegrees,
                                         float& outX, float& outY, float& outZ) {
    constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
    const float yaw   = yawDegrees   * kDegToRad;
    const float pitch = pitchDegrees * kDegToRad;

    const float cosPitch = std::cos(pitch);
    outX = -std::sin(yaw) * cosPitch;
    outY = -std::sin(pitch);
    outZ =  std::cos(yaw) * cosPitch;
}

inline Mat4 BuildViewProjection(float camX, float camY, float camZ,
                                float yawDegrees, float pitchDegrees,
                                float fovYDegrees, float aspect,
                                float nearZ, float farZ) {
    float dirX = 0.0f, dirY = 0.0f, dirZ = 1.0f;
    DirectionFromMinecraftAngles(yawDegrees, pitchDegrees, dirX, dirY, dirZ);

    const Mat4 view       = LookAlong(camX, camY, camZ, dirX, dirY, dirZ);
    const Mat4 projection = Perspective(fovYDegrees, aspect, nearZ, farZ);
    return Multiply(projection, view);
}

// Projects a world point to pixel coordinates. Returns false for anything at or
// behind the camera plane, which is what stops ESP boxes from mirroring onto
// the screen when a target is behind you.
inline bool WorldToScreen(const Mat4& viewProjection,
                          float worldX, float worldY, float worldZ,
                          float screenWidth, float screenHeight,
                          float& outX, float& outY) {
    const float* m = viewProjection.m;

    const float clipX = m[0] * worldX + m[4] * worldY + m[8]  * worldZ + m[12];
    const float clipY = m[1] * worldX + m[5] * worldY + m[9]  * worldZ + m[13];
    const float clipW = m[3] * worldX + m[7] * worldY + m[11] * worldZ + m[15];

    if (clipW < 0.05f) return false;

    const float inverseW = 1.0f / clipW;
    const float ndcX     = clipX * inverseW;
    const float ndcY     = clipY * inverseW;

    // NDC y points up, screen y points down.
    outX = (ndcX * 0.5f + 0.5f) * screenWidth;
    outY = (0.5f - ndcY * 0.5f) * screenHeight;
    return true;
}

} // namespace mcov
