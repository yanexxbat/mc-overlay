#include "overlay.hpp"
#include <algorithm>
#include <cstdio>

namespace mcov {
namespace {
using Page=control_panel::Page;
using Mode=control_panel::TileMode;
using Action=keys::Action;
enum class Flag { None, Health, Distance, Tracers, TargetInfo, BlockTracers };
struct Definition { const char* name; Mode mode=Mode::Settings; Action action=Action::Count; Flag flag=Flag::None; };
constexpr Definition Module(const char* name,Action a) { return {name,Mode::Toggle,a}; }
constexpr Definition Option(const char* name,Flag flag) { return {name,Mode::Toggle,Action::Count,flag}; }
constexpr Definition combat[]={Module("Kill Aura",Action::KillAura),Module("Aim Assist",Action::AimAssist),
    Module("Triggerbot",Action::Triggerbot),Module("Auto Totem",Action::AutoTotem),{"Friends"},
    Module("Crystal Macro",Action::CrystalMacro),Module("Anchor Macro",Action::AnchorMacro),Module("Hit sounds",Action::HitSounds),
    Module("Spinbot",Action::Spinbot)};
constexpr Definition visuals[]={Module("Box ESP",Action::BoxEsp),Module("Skeleton",Action::Skeleton),Module("Name tags",Action::NameTags),
    Option("Health bars",Flag::Health),Option("Distance labels",Flag::Distance),Option("Tracers",Flag::Tracers),Option("Target info",Flag::TargetInfo),
    Module("Target aura",Action::TargetAura),Module("Target marker",Action::TargetMarker),Module("Trails",Action::Trails),
    Module("Hit particles",Action::HitParticles),Module("Ambient particles",Action::AmbientParticles),Module("Hit bubbles",Action::HitBubbles),
    Module("Kill effect",Action::KillEffect),Module("BreadCrumbs",Action::BreadCrumbs),Module("China hat",Action::ChinaHat),{"Entity filters"},
    Module("Jump circles",Action::JumpCircles),Module("Wings",Action::Wings),Module("Halo / Nimb",Action::Halo),Module("Glow ESP",Action::GlowEsp),
    Module("Damage markers",Action::DamageMarkers),Module("Damage vignette",Action::DamageInfo),Module("Target circle",Action::TargetCircle),
    Module("Crosshair",Action::Crosshair),Module("Offscreen arrows",Action::Arrows),Module("CS Knife",Action::CsKnife)};
constexpr Definition blocks[]={Module("Block ESP",Action::BlockEsp),{"Block types"},Option("Tracers",Flag::BlockTracers),
    Module("Chunkfinder",Action::ChunkFinder),Module("Storage ESP",Action::ChestEsp),Module("Beacon radius",Action::BeaconRadius),Module("Chunk borders",Action::ChunkBorders),
    Module("Stash Finder",Action::StashFinder),Module("Spawner ESP",Action::SpawnerEsp),Module("Base ESP",Action::BaseEsp)};
constexpr Definition stats[]={{"Local player"},{"Client"},{"Entities"}};
constexpr Definition settings[]={{"Overlay"},{"Focus"},{"Keybinds"},{"Agent"},{"Configs"}};
constexpr Definition misc[]={Module("Middle click friend",Action::MiddleFriend),Module("Death coordinates",Action::DeathCoordinates),
    Module("Detect players",Action::DetectPlayer),Module("Waypoints",Action::Waypoints),Module("Name protect",Action::NameProtect),
    Module("Module sounds",Action::ModuleSounds),Module("Notifications",Action::Notifications),{"Panic",Mode::Action,Action::Panic},Module("Freecam",Action::Freecam)};
std::span<const Definition> Definitions(Page page) {
    switch(page) {
        case Page::Combat:return combat; case Page::Visuals:return visuals; case Page::Blocks:return blocks;
        case Page::Stats:return stats; case Page::Settings:return settings; case Page::Misc:return misc;
        default:return {};
    }
}
}
int Overlay::ReadModules(Page page,std::span<control_panel::ModuleView> output) const {
    const auto definitions=Definitions(page);
    const int count=std::min(int(output.size()),page==Page::Hud?kHudModules+1:int(definitions.size()));
    for(int i=0;i<count;++i) {
        auto& view=output[i]; view={};
        if(page==Page::Hud) {
            view.name=i?kHudNames[i-1]:"Editor"; view.mode=i?Mode::Toggle:Mode::Action;
            view.enabled=i&&HudEnabled(i-1);
            if(!i) std::snprintf(view.binding,sizeof(view.binding),"%s",BindName(bindings_[keys::Index(Action::HudEditor)]).c_str());
            continue;
        }
        const auto& d=definitions[i]; view.name=d.name; view.mode=d.mode;
        if(d.action!=Action::Count) {
            view.enabled=ModuleEnabled(d.action);
            std::snprintf(view.binding,sizeof(view.binding),"Key: %s",BindName(bindings_[keys::Index(d.action)]).c_str());
        }
        switch(d.flag) {
            case Flag::Health:view.enabled=visuals_.healthBars;break;
            case Flag::Distance:view.enabled=visuals_.distanceText;break;
            case Flag::Tracers:view.enabled=visuals_.tracers;break;
            case Flag::TargetInfo:view.enabled=visuals_.targetInfo;break;
            case Flag::BlockTracers:view.enabled=blocks_.tracers;break;
            default:break;
        }
    }
    return count;
}
void Overlay::ActivateModule(Page page,int index) {
    if(page==Page::Hud) {
        if(index==0) SetHudEditing(true);
        else if(index<=kHudModules&&index>0) SetHudEnabled(index-1,!HudEnabled(index-1));
        return;
    }
    const auto definitions=Definitions(page);
    if(index<0||index>=int(definitions.size())) return;
    const auto& d=definitions[index];
    if(d.action==Action::Panic) { Panic(); return; }
    if(d.action!=Action::Count&&d.mode==Mode::Toggle) SetModuleEnabled(d.action,!ModuleEnabled(d.action));
    bool* flag=nullptr;
    switch(d.flag) {
        case Flag::Health:flag=&visuals_.healthBars;break;
        case Flag::Distance:flag=&visuals_.distanceText;break;
        case Flag::Tracers:flag=&visuals_.tracers;break;
        case Flag::TargetInfo:flag=&visuals_.targetInfo;break;
        case Flag::BlockTracers:flag=&blocks_.tracers;break;
        default:break;
    }
    if(flag) { *flag=!*flag; if(*flag) panicActive_=false; }
}
} // namespace mcov
