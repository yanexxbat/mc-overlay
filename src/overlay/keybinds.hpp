#pragma once
#include <array>
#include <cstdint>

namespace mcov::keys {
enum class Action { Menu, Quit, HudEditor, KillAura, AimAssist, Triggerbot, AutoTotem,
    BoxEsp, Skeleton, NameTags, BlockEsp, TargetAura, TargetMarker, Hud,
    Trails, HitParticles, AmbientParticles, HitBubbles, KillEffect, BreadCrumbs, ChinaHat,
    CrystalMacro, AnchorMacro, ChunkFinder,
    JumpCircles, Wings, Halo, GlowEsp, DamageMarkers, DamageInfo, TargetCircle, Crosshair, Arrows,
    NameProtect, HitSounds, ModuleSounds, MiddleFriend, DeathCoordinates, DetectPlayer, Notifications,
    ChestEsp, BeaconRadius, ChunkBorders, Waypoints,
    Radar, Keystrokes, Speedometer, Session, ArmorStrip, Indicators, Clock, Cps, Panic, Spinbot, StashFinder, SpawnerEsp, Count };
enum class Mode { Toggle, Hold };
enum Modifier : unsigned { Ctrl=1, Shift=2, Alt=4 };
inline constexpr int kCount=static_cast<int>(Action::Count);
inline constexpr std::array<const char*,kCount> kNames={"Menu","Quit overlay","HUD editor","Kill Aura","Aim assist",
    "Triggerbot","Auto totem","Box ESP","Skeleton","Name tags","Block ESP","Target aura","Target marker","HUD",
    "Trails","Hit particles","Ambient particles","Hit bubbles","Kill effect","BreadCrumbs","China hat",
    "Crystal macro","Anchor macro","Chunkfinder",
    "Jump circles","Wings","Halo / Nimb","Glow ESP","Damage markers","Damage vignette","Target circle","Crosshair","Offscreen arrows",
    "Name protect","Hit sounds","Module sounds","Middle click friend","Death coordinates","Detect players","Notifications",
    "Storage ESP","Beacon radius","Chunk borders","Waypoints",
    "Radar","Keystrokes","Speedometer","Session statistics","Armor strip","Combat indicators","Clock","Click rate","Panic","Spinbot","Stash Finder","Spawner ESP"};
struct Bind { int key{}; unsigned modifiers{}; Mode mode=Mode::Toggle; };
using Bindings=std::array<Bind,kCount>;
using Input=std::array<bool,256>;
inline constexpr int Index(Action a) { return static_cast<int>(a); }
inline constexpr bool System(Action a) { return Index(a)<=Index(Action::HudEditor)||a==Action::Panic; }
inline Bindings Defaults() {
    Bindings b{}; b[Index(Action::Menu)].key=0x2D; b[Index(Action::Quit)].key=0x23;
    b[Index(Action::HudEditor)].key=0x75;
    return b; // INSERT, END, F6; Modul-Tasten bleiben ungebunden.
}
inline unsigned Modifiers(const Input& in) {
    return (in[0x11]||in[0xA2]||in[0xA3]?Ctrl:0)|(in[0x10]||in[0xA0]||in[0xA1]?Shift:0)|
           (in[0x12]||in[0xA4]||in[0xA5]?Alt:0);
}
inline bool ModifierKey(int k) { return k==0x10||k==0x11||k==0x12||(k>=0xA0&&k<=0xA5); }
inline unsigned ModifierForKey(int k) {
    return k==0x11||k==0xA2||k==0xA3?Ctrl:k==0x10||k==0xA0||k==0xA1?Shift:k==0x12||k==0xA4||k==0xA5?Alt:0;
}
inline bool Matches(Bind b,const Input& in) { return b.key>0&&b.key<256&&in[b.key]&&(Modifiers(in)&~ModifierForKey(b.key))==b.modifiers; }
inline bool Conflict(const Bindings& b,int index) {
    if(!b[index].key) return false;
    for(int i=0;i<kCount;++i) if(i!=index&&b[i].key==b[index].key&&b[i].modifiers==b[index].modifiers) return true;
    return false;
}
// Reiner Zustand: Win32-Abfrage und Moduländerung liegen im aufrufenden Overlay.
// Hold stellt beim Loslassen/Fokusverlust den Zustand vor dem Tastendruck wieder her.
class Engine final {
    std::array<bool,kCount> down_{},holding_{},restore_{};
    bool initialized_=false;
public:
    // Panic darf beim Loslassen einer Hold-Taste keinen alten Aktiv-Zustand restaurieren.
    void DiscardHoldRestoration() { for(int i=0;i<kCount;++i) if(holding_[i]) restore_[i]=false; }
    template<class Read,class Write,class Invoke>
    void Update(const Bindings& binds,const Input& input,bool focused,bool modulesAllowed,bool capturing,
                Read read,Write write,Invoke invoke) {
        for(int i=0;i<kCount;++i) {
            const auto a=static_cast<Action>(i); const bool active=Matches(binds[i],input);
            const auto mode=binds[i].mode;
            if(!initialized_) { down_[i]=active; continue; }
            const bool allowed=focused&&!capturing&&(System(a)||modulesAllowed);
            if(holding_[i]&&(!active||!allowed||mode!=Mode::Hold)) {
                write(a,restore_[i]); holding_[i]=false;
            }
            if(allowed&&active&&!down_[i]) {
                if(System(a)) invoke(a);
                else if(mode==Mode::Toggle) write(a,!read(a));
                else { restore_[i]=read(a); holding_[i]=true; write(a,true); }
            }
            down_[i]=active; // Fokuswechsel mit gehaltener Taste löst keinen neuen Toggle aus.
        }
        initialized_=true;
    }
};
} // namespace mcov::keys
