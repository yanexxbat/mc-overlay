#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <type_traits>

namespace mcov::items {
enum class Item : std::uint8_t { Empty, Other, Totem, Crystal, Anchor, Glowstone };
enum class Target : std::uint8_t { None, OtherBlock, CrystalBase, Anchor, Crystal };
enum class Status : std::uint32_t { Off, Ready, HoldingTotem, NoTotem, Pending, WaitingBind, WaitingTarget,
    MissingItem, Paused, InventoryBusy, NoWorld, Unsupported, Error, Conflict, AnchorSafeDimension };
inline const char* Text(Status s) {
    switch(s) {
        case Status::Off:return "Off";
        case Status::Ready:return "Ready";
        case Status::HoldingTotem:return "Totem equipped";
        case Status::NoTotem:return "No totem in inventory";
        case Status::Pending:return "Action sent - waiting";
        case Status::WaitingBind:return "Hold the assigned key";
        case Status::WaitingTarget:return "Aim at a valid crosshair target";
        case Status::MissingItem:return "Required item missing from hotbar";
        case Status::Paused:return "Paused - focus game / close menu / stop Kill Aura";
        case Status::InventoryBusy:return "Container or carried item blocks inventory actions";
        case Status::NoWorld:return "Join a world";
        case Status::Unsupported:return "Required game binding unavailable";
        case Status::Error:return "Game action failed - see agent log";
        case Status::Conflict:return "Hold only one macro bind";
        case Status::AnchorSafeDimension:return "Anchors cannot explode in this dimension";
    }
    return "Unavailable";
}
struct TotemConfig { bool enabled=false; std::uint32_t retryMs=100; };
struct MacroConfig { bool enabled=false,restoreSlot=true; std::uint32_t intervalMs=150; };
struct Request {
    std::uint64_t sentAtMs{};
    bool inputAllowed=false,crystalHeld=false,anchorHeld=false,combatActive=false;
    TotemConfig totem{}; MacroConfig crystal{},anchor{};
};
struct Report {
    std::uint64_t sampledAtMs{},swaps{},crystalActions{},anchorActions{};
    Status totem=Status::Unsupported,crystal=Status::Unsupported,anchor=Status::Unsupported;
};
struct Frame {
    bool world=false,alive=false,paused=false,screen=false,ownMenu=false,cursorEmpty=true;
    bool totemSupported=false,macroSupported=false,anchorWorks=false;
    std::array<Item,36> inventory{};
    Item offhand=Item::Empty;
    int selected=0,charges=0;
    Target target=Target::None;
};
enum class Operation { None, SwapTotem, Use, AttackCrystal, RestoreSlot };
struct Action { Operation operation=Operation::None; int slot=-1; bool anchor=false; };
inline bool Lease(const Request& r,std::uint64_t now) { return r.sentAtMs&&r.sentAtMs<=now&&now-r.sentAtMs<=250&&r.inputAllowed; }
inline int ContainerSlot(int inventorySlot) { return inventorySlot<9?inventorySlot+36:inventorySlot; }

// Deterministische Entscheidung ohne JNI. Höchstens eine Inventar-/Spielaktion pro Tick.
class Controller final {
    std::uint64_t nextTotem_=0,nextMacro_=0;
    int original_=-1,lastSelected_=-1;
    Report report_{};
    static int Find(const Frame& f,Item item,int count) {
        for(int i=0;i<count;++i) if(f.inventory[i]==item) return i;
        return -1;
    }
public:
    void Reset() { nextTotem_=nextMacro_=0; original_=lastSelected_=-1; }
    const Report& GetReport() const { return report_; }
    Action Tick(const Request& r,const Frame& f,std::uint64_t now) {
        report_.sampledAtMs=now;
        report_.totem=r.totem.enabled?Status::Ready:Status::Off;
        report_.crystal=r.crystal.enabled?Status::WaitingBind:Status::Off;
        report_.anchor=r.anchor.enabled?Status::WaitingBind:Status::Off;
        const bool crystal=r.crystal.enabled&&r.crystalHeld,anchor=r.anchor.enabled&&r.anchorHeld;
        const bool live=f.world&&f.alive;
        const bool allowed=live&&Lease(r,now)&&!f.paused;
        if(!live) { Reset(); if(r.totem.enabled) report_.totem=Status::NoWorld; if(r.crystal.enabled) report_.crystal=Status::NoWorld; if(r.anchor.enabled) report_.anchor=Status::NoWorld; return {}; }
        if(r.totem.enabled) {
            if(!f.totemSupported) report_.totem=Status::Unsupported;
            else if(!allowed) report_.totem=Status::Paused;
            else if(!f.ownMenu||!f.cursorEmpty) report_.totem=Status::InventoryBusy;
            else if(f.offhand==Item::Totem) report_.totem=Status::HoldingTotem;
            else {
                const int slot=Find(f,Item::Totem,36);
                if(slot<0) report_.totem=Status::NoTotem;
                else {
                    report_.totem=Status::Pending;
                    if(now>=nextTotem_) { nextTotem_=now+std::clamp(r.totem.retryMs,50u,500u); return {Operation::SwapTotem,slot}; }
                }
            }
        }
        const bool macroAllowed=allowed&&!f.screen&&f.ownMenu&&f.cursorEmpty&&!r.combatActive;
        if(!crystal&&!anchor || !macroAllowed || crystal&&anchor) {
            if(crystal) report_.crystal=crystal&&anchor?Status::Conflict:Status::Paused;
            if(anchor) report_.anchor=crystal&&anchor?Status::Conflict:Status::Paused;
            if(original_>=0&&f.ownMenu&&f.cursorEmpty&&!f.paused) {
                const int original=original_; original_=-1;
                if(f.selected==lastSelected_) return {Operation::RestoreSlot,original};
            }
            original_=lastSelected_=-1; nextMacro_=0; return {};
        }
        auto& status=anchor?report_.anchor:report_.crystal;
        if(!f.macroSupported) { status=Status::Unsupported; return {}; }
        if(anchor&&f.anchorWorks) { status=Status::AnchorSafeDimension; return {}; }
        Action action;
        if(crystal&&f.target==Target::Crystal) action={Operation::AttackCrystal,f.selected,false};
        else if(crystal&&f.target==Target::CrystalBase) action={Operation::Use,Find(f,Item::Crystal,9),false};
        else if(anchor&&f.target==Target::Anchor) {
            int slot=-1;
            if(f.charges==0) slot=Find(f,Item::Glowstone,9);
            else {
                for(int i=0;i<9;++i) if(f.inventory[i]!=Item::Glowstone&&f.inventory[i]!=Item::Crystal) { slot=i; break; }
            }
            action={Operation::Use,slot,true};
        } else if(anchor&&f.target==Target::OtherBlock) action={Operation::Use,Find(f,Item::Anchor,9),true};
        else { status=Status::WaitingTarget; return {}; }
        if(action.slot<0) { status=Status::MissingItem; return {}; }
        status=Status::Pending;
        if(now<nextMacro_) return {};
        const auto& config=anchor?r.anchor:r.crystal;
        if(config.restoreSlot&&original_<0) original_=f.selected;
        lastSelected_=action.slot; nextMacro_=now+std::clamp(config.intervalMs,100u,500u);
        return action;
    }
    void Complete(Action action,bool success) {
        if(action.operation==Operation::None||action.operation==Operation::RestoreSlot) return;
        auto& status=action.operation==Operation::SwapTotem?report_.totem:action.anchor?report_.anchor:report_.crystal;
        if(!success) { status=Status::Error; return; }
        if(action.operation==Operation::SwapTotem) ++report_.swaps;
        else if(action.anchor) ++report_.anchorActions;
        else ++report_.crystalActions;
    }
};
static_assert(std::is_trivially_copyable_v<Request>&&std::is_trivially_copyable_v<Report>);
} // namespace mcov::items
