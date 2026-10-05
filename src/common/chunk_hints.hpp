#pragma once
#include <string_view>
#include <cmath>
namespace mcov {
// Nur roher Deepslate; Erze, befallener und polierter Deepslate zählen nicht.
inline bool RotatedDeepslate(std::string_view blockKey,std::string_view axis) noexcept {
    return blockKey=="block.minecraft.deepslate"&&(axis=="x"||axis=="z"||axis=="X"||axis=="Z");
}
inline int ChunkCoordinate(double block) noexcept { return static_cast<int>(std::floor(block/16.)); }
}
