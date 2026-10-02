#pragma once
#include "mappings.hpp"
#include "geometry_profile.hpp"

namespace mcov::mappings {
enum class FovSource { RendererFloat, RendererDouble, CameraFloat };

struct RuntimeProfile {
    const MappingSet* set;
    const char* version;
    int javaMajor;
    FovSource fovSource;
    const char* versionClass;
    Member currentVersion;
    const char* worldVersionClass;
    Member versionId;
    // 26.2 moved the active screen from Minecraft to Minecraft.gui.
    const char* guiClass;
    Member clientGui;
    const GeometryProfile* geometry;
};
} // namespace mcov::mappings
