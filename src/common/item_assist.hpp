#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <type_traits>

namespace mcov::items {
enum class Item : std::uint8_t { Empty, Other, Totem, Crystal, Anchor, Glowstone };
enum class Target : std::uint8_t { None, OtherBlock, CrystalBase, Anchor, Crystal };
enum class Status : std::uint32_t { Off, Ready, HoldingTotem, NoTotem, Pending, WaitingBind, WaitingTarget,
    MissingItem, Paused, InventoryBusy, NoWorld, Unsupported, Error, Conflict, AnchorSafeDimension, OffhandFuel };
inline const char* Text(Status s) {
    switch(s) {
        case Status::Off:return "Off";
        case Status::Ready:return "Ready";
        case Status::HoldingTotem:return "Totem equipped";
        case Status::NoTotem:return "No totem in inventory";
        case Status::Pending:return "Action sent - waiting";
        case Status::WaitingBind:return "Hold Minecraft's Place / Use with the item selected";
        case Status::WaitingTarget:return "Aim at a valid crosshair target";
        case Status::MissingItem:return "Required item missing from hotbar";
        case Status::Paused:return "Paused - focus Minecraft / close menus";
        case Status::InventoryBusy:return "Container or carried item blocks inventory actions";
        case Status::NoWorld:return "Join a world";
        case Status::Unsupported:return "Required game binding unavailable";
        case Status::Error:return "Game action failed - see agent log";
        case Status::Conflict:return "Another macro cycle is active";
        case Status::AnchorSafeDimension:return "Anchors cannot explode in this dimension";
        case Status::OffhandFuel:return "Remove glowstone from offhand to detonate";
    }
    return "Unavailable";
}
struct TotemConfig { bool enabled=false; std::uint32_t retryMs=100; };
struct MacroConfig { bool enabled=false,restoreSlot=true; std::uint32_t intervalMs=150,confirmationMs=750; };
struct Request {
    std::uint64_t sentAtMs{};
    bool inputAllowed=false,combatActive=false;
    TotemConfig totem{}; MacroConfig crystal{},anchor{};
};
struct Report {
    std::uint64_t sampledAtMs{},swaps{},crystalActions{},anchorActions{};
    Status totem=Status::Unsupported,crystal=Status::Unsupported,anchor=Status::Unsupported;
};
struct Frame {
    bool world=false,alive=false,paused=false,screen=false,ownMenu=false,cursorEmpty=true;
    bool totemSupported=false,macroSupported=false,anchorSupported=false,anchorWorks=false;
    std::array<Item,36> inventory{};
    Item offhand=Item::Empty;
    int selected=0,charges=0;
    Target target=Target::None;
    // Diese Werte stammen ausschließlich aus dem Client-Tick, niemals aus Win32-Tasten.
    bool useDown=false,siteValid=false;
    Item cycle=Item::Empty;
    int crystalId=-1;
};
enum class Operation { None, SwapTotem, PlaceCrystal, AttackCrystal, PlaceAnchor, ChargeAnchor, DetonateAnchor, RestoreSlot };
struct Action { Operation operation=Operation::None; int slot=-1; bool anchor=false; };
inline bool Lease(const Request& r,std::uint64_t now) { return r.sentAtMs&&r.sentAtMs<=now&&now-r.sentAtMs<=250&&r.inputAllowed; }
inline int ContainerSlot(int inventorySlot) { return inventorySlot<9?inventorySlot+36:inventorySlot; }

// Deterministische Entscheidung ohne JNI. Höchstens eine Inventar-/Spielaktion pro Tick.
class Controller final {
    std::uint64_t nextTotem_=0,nextMacro_=0;
    int original_=-1,lastSelected_=-1;
    Item cycle_=Item::Empty;
    Operation waiting_=Operation::None;
    std::uint64_t confirmUntil_=0;
    int attackedId_=-1;
    Report report_{};
    static int Find(const Frame& f,Item item,int count) {
        for(int i=0;i<count;++i) if(f.inventory[i]==item) return i;
        return -1;
    }
public:
    void Reset() { nextTotem_=nextMacro_=confirmUntil_=0; original_=lastSelected_=-1; cycle_=Item::Empty; waiting_=Operation::None; }
    const Report& GetReport() const { return report_; }
    bool HasRestore() const { return original_>=0; }
    bool Active() const { return cycle_!=Item::Empty; }
    Action Tick(const Request& r,const Frame& f,std::uint64_t now) {
        report_.sampledAtMs=now;
        report_.totem=r.totem.enabled?Status::Ready:Status::Off;
        report_.crystal=r.crystal.enabled?(f.macroSupported?Status::WaitingBind:Status::Unsupported):Status::Off;
        report_.anchor=r.anchor.enabled?(f.anchorSupported?Status::WaitingBind:Status::Unsupported):Status::Off;
        const Item kind=Active()?cycle_:f.cycle;
        const bool crystal=kind==Item::Crystal,anchor=kind==Item::Anchor;
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
        const bool armed=crystal?r.crystal.enabled:anchor?r.anchor.enabled:false;
        const bool manualSlot=Active()&&lastSelected_>=0&&f.selected!=lastSelected_;
        if(!armed || !f.useDown || !macroAllowed || !f.siteValid || manualSlot) {
            if(crystal&&r.crystal.enabled) report_.crystal=!macroAllowed?Status::Paused:f.useDown?Status::WaitingTarget:Status::WaitingBind;
            if(anchor&&r.anchor.enabled) report_.anchor=!macroAllowed?Status::Paused:f.useDown?Status::WaitingTarget:Status::WaitingBind;
            cycle_=Item::Empty; waiting_=Operation::None; confirmUntil_=0;
            if(original_>=0&&f.ownMenu&&f.cursorEmpty&&!f.paused) {
                const int original=original_,expected=lastSelected_;
                original_=lastSelected_=-1; nextMacro_=0;
                if(f.selected==expected) return {Operation::RestoreSlot,original};
            }
            original_=lastSelected_=-1; nextMacro_=0; return {};
        }
        auto& status=anchor?report_.anchor:report_.crystal;
        if(!f.macroSupported||(anchor&&!f.anchorSupported)||f.selected<0||f.selected>=9) { status=Status::Unsupported; return {}; }
        if(anchor&&f.anchorWorks) { status=Status::AnchorSafeDimension; return {}; }
        cycle_=kind;
        // Keine Folgeaktion, bevor die Welt die vorige Aktion bestätigt.
        if(waiting_!=Operation::None) {
            const bool confirmed=waiting_==Operation::PlaceCrystal?f.target==Target::Crystal:
                waiting_==Operation::AttackCrystal?f.target!=Target::Crystal||f.crystalId!=attackedId_:
                waiting_==Operation::PlaceAnchor?f.target==Target::Anchor:
                waiting_==Operation::ChargeAnchor?f.target==Target::Anchor&&f.charges>0:
                f.target!=Target::Anchor;
            if(!confirmed&&now<confirmUntil_) { status=Status::Pending; return {}; }
            waiting_=Operation::None;
        }
        Action action;
        if(crystal&&f.target==Target::Crystal) action={Operation::AttackCrystal,f.selected,false};
        else if(crystal&&f.target==Target::CrystalBase) action={Operation::PlaceCrystal,Find(f,Item::Crystal,9),false};
        else if(anchor&&f.target==Target::Anchor) {
            if(f.charges<0||f.charges>4) { status=Status::WaitingTarget; return {}; }
            // Vanilla priorisiert Glowstone in der Offhand, solange Ladungen frei sind.
            if(f.charges>0&&f.charges<4&&f.offhand==Item::Glowstone) { status=Status::OffhandFuel; return {}; }
            int slot=-1;
            if(f.charges==0) slot=Find(f,Item::Glowstone,9);
            else {
                for(int i=0;i<9;++i) if(f.inventory[i]!=Item::Glowstone&&f.inventory[i]!=Item::Crystal&&f.inventory[i]!=Item::Empty) { slot=i; break; }
                if(slot<0&&f.offhand!=Item::Glowstone) slot=Find(f,Item::Empty,9);
            }
            action={f.charges==0?Operation::ChargeAnchor:Operation::DetonateAnchor,slot,true};
        } else if(anchor&&f.target==Target::OtherBlock) action={Operation::PlaceAnchor,Find(f,Item::Anchor,9),true};
        else { status=Status::WaitingTarget; return {}; }
        if(action.slot<0) { status=Status::MissingItem; return {}; }
        status=Status::Pending;
        if(now<nextMacro_) return {};
        const auto& config=anchor?r.anchor:r.crystal;
        if(config.restoreSlot&&original_<0) original_=f.selected;
        lastSelected_=action.slot; nextMacro_=now+std::clamp(config.intervalMs,100u,500u);
        waiting_=action.operation; confirmUntil_=now+std::clamp(config.confirmationMs,100u,2000u);
        attackedId_=f.crystalId;
        return action;
    }
    void Complete(Action action,bool success) {
        if(action.operation==Operation::None||action.operation==Operation::RestoreSlot) return;
        auto& status=action.operation==Operation::SwapTotem?report_.totem:action.anchor?report_.anchor:report_.crystal;
        if(!success) { status=Status::Error; if(action.operation!=Operation::SwapTotem) waiting_=Operation::None; return; }
        if(action.operation==Operation::SwapTotem) ++report_.swaps;
        else if(action.anchor) ++report_.anchorActions;
        else ++report_.crystalActions;
    }
};
static_assert(std::is_trivially_copyable_v<Request>&&std::is_trivially_copyable_v<Report>);
} // namespace mcov::items
