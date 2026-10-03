#pragma once
#include "combat_assist.hpp"
struct ImDrawList;
struct ImVec2;
namespace mcov::combat {
class CombatHud {
public:
    // Nur POD-Diagnosen aus der Mailbox. Keine JNI-Abfragen und keine Style-Stacks.
    static void Draw(ImDrawList& draw,const DebugSnapshot& snapshot,const HudConfig& config,const ImVec2& viewport);
};
} // namespace mcov::combat
