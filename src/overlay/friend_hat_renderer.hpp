#pragma once
#include "aura_renderer.hpp"
#include "esp_geometry.hpp"

namespace mcov {
struct FriendHatConfig {
    float opacity=.28f;
    float glow=.65f;
    float radiusScale=.95f;                 // Brim-Radius relativ zur Spielerbreite.
    float heightScale=.55f;                 // Kegelhöhe relativ zum Radius.
    float headGap=.025f;
};
struct LocalHatConfig {
    bool enabled=true;                     // Eigener Hut standardmäßig sichtbar in F5.
    bool firstPerson=false;
    bool auraColours=true;
    FriendHatConfig shape{};
    ImVec4 start{.66f,.42f,1.f,1.f},end{.05f,1.f,.88f,1.f};
};
inline constexpr int kFriendHatSegments=48;
struct FriendHatWorld {
    WorldPoint tip{};
    std::array<WorldPoint,kFriendHatSegments> brim{};
    float glowRadius{};
};
struct FriendHatScreen {
    ImVec2 tip{};
    std::array<ImVec2,kFriendHatSegments> brim{};
    std::array<float,kFriendHatSegments> glowRadius{};
    std::array<double,kFriendHatSegments> depth{};
    float tipGlowRadius{};
};
// Kopfgeometrie, Projektion und Zeichnen bleiben getrennt. Alle Eingaben
// stammen aus dem kohärenten Snapshot und der UUID-Freundeliste.
class FriendHatRenderer final {
public:
    FriendHatRenderer() noexcept;
    static void Prepare(ImDrawList& draw);
    static bool IsFriend(const EntitySnapshot& entity,const combat::Relations& relations) noexcept;
    bool BuildWorld(const EntitySnapshot& entity,const FriendHatConfig& config,FriendHatWorld& out) const noexcept;
    static bool LocalVisible(const EntitySnapshot& entity,WorldPoint camera,bool firstPerson) noexcept;
    void DrawLocal(ImDrawList& draw,const WorldSnapshot& snapshot,const CameraProjection& projection,
                   const LocalHatConfig& config,ImVec4 start,ImVec4 end,AuraBlend blend,
                   ImTextureID texture,ImVec2 viewport,ImVec2 origin={}) const;
    static bool Project(const CameraProjection& projection,const FriendHatWorld& world,
                        WorldPoint camera,ImVec2 viewport,FriendHatScreen& out,ImVec2 origin={}) noexcept;
    static void Draw(ImDrawList& draw,const FriendHatScreen& screen,const FriendHatConfig& config,
                     ImVec4 startColor,ImVec4 endColor,AuraBlend blend,ImTextureID glowTexture);
    void DrawFriends(ImDrawList& draw,const WorldSnapshot& snapshot,const CameraProjection& projection,
                     const combat::Relations& relations,float maxDistance,const FriendHatConfig& config,
                     ImVec4 startColor,ImVec4 endColor,AuraBlend blend,ImTextureID glowTexture,
                     ImVec2 viewport,ImVec2 origin={}) const;
private:
    std::array<ImVec2,kFriendHatSegments> circle_{};
};
} // namespace mcov
