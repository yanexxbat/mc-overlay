#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace mcov::keys {
enum class Action { Menu, Quit, HudEditor, KillAura, AimAssist, Triggerbot, AutoTotem,
    BoxEsp, Skeleton, NameTags, BlockEsp, TargetAura, TargetMarker, Hud,
    Trails, HitParticles, AmbientParticles, HitBubbles, KillEffect, BreadCrumbs, ChinaHat,
    CrystalMacro, AnchorMacro, ChunkFinder,
    JumpCircles, Wings, Halo, GlowEsp, DamageMarkers, DamageInfo, TargetCircle, Crosshair, Arrows,
    NameProtect, HitSounds, ModuleSounds, MiddleFriend, DeathCoordinates, DetectPlayer, Notifications,
    ChestEsp, BeaconRadius, ChunkBorders, Waypoints,
    Radar, Keystrokes, Speedometer, Session, ArmorStrip, Indicators, Clock, Cps, Panic, Spinbot, StashFinder, SpawnerEsp, BaseEsp, BaseFinder, Freecam, Reserved59, Reserved60, AntiVanish, RegionMap, HoleEsp, AutoTrident, Count };
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
    "Radar","Keystrokes","Speedometer","Session statistics","Armor strip","Combat indicators","Clock","Click rate","Panic","Spinbot","Stash Finder","Spawner ESP","Base ESP","Chunkfinder (alternate)","Freecam","","","Anti Vanish","Region map","Hole ESP","Auto Trident"};
struct Bind { int key{}; unsigned modifiers{}; Mode mode=Mode::Toggle; };
using Bindings=std::array<Bind,kCount>;
using Input=std::array<bool,256>;
// Stable English labels, independent of the Windows display language.
inline std::string KeyName(int key) {
    if((key>=0x30&&key<=0x39)||(key>=0x41&&key<=0x5A)) return std::string(1,static_cast<char>(key));
    if(key>=0x70&&key<=0x87) return "F"+std::to_string(key-0x70+1);
    if(key>=0x60&&key<=0x69) return "Numpad "+std::to_string(key-0x60);
    switch(key) {
        case 0:return "None";
        case 0x01:return "Mouse 1"; case 0x02:return "Mouse 2"; case 0x03:return "Cancel";
        case 0x04:return "Mouse 3"; case 0x05:return "Mouse 4"; case 0x06:return "Mouse 5";
        case 0x08:return "Backspace"; case 0x09:return "Tab"; case 0x0C:return "Clear"; case 0x0D:return "Enter";
        case 0x10:return "Shift"; case 0x11:return "Ctrl"; case 0x12:return "Alt"; case 0x13:return "Pause";
        case 0x14:return "Caps Lock"; case 0x1B:return "Esc"; case 0x20:return "Space";
        case 0x21:return "Page Up"; case 0x22:return "Page Down"; case 0x23:return "End"; case 0x24:return "Home";
        case 0x25:return "Left"; case 0x26:return "Up"; case 0x27:return "Right"; case 0x28:return "Down";
        case 0x29:return "Select"; case 0x2A:return "Print"; case 0x2B:return "Execute";
        case 0x2C:return "Print Screen"; case 0x2D:return "Insert"; case 0x2E:return "Delete"; case 0x2F:return "Help";
        case 0x5B:return "Left Win"; case 0x5C:return "Right Win"; case 0x5D:return "Menu"; case 0x5F:return "Sleep";
        case 0x6A:return "Numpad *"; case 0x6B:return "Numpad +"; case 0x6C:return "Numpad separator";
        case 0x6D:return "Numpad -"; case 0x6E:return "Numpad ."; case 0x6F:return "Numpad /";
        case 0x90:return "Num Lock"; case 0x91:return "Scroll Lock";
        case 0xA0:return "Left Shift"; case 0xA1:return "Right Shift"; case 0xA2:return "Left Ctrl";
        case 0xA3:return "Right Ctrl"; case 0xA4:return "Left Alt"; case 0xA5:return "Right Alt";
        case 0xA6:return "Browser Back"; case 0xA7:return "Browser Forward"; case 0xA8:return "Browser Refresh";
        case 0xA9:return "Browser Stop"; case 0xAA:return "Browser Search"; case 0xAB:return "Browser Favorites";
        case 0xAC:return "Browser Home"; case 0xAD:return "Mute"; case 0xAE:return "Volume Down"; case 0xAF:return "Volume Up";
        case 0xB0:return "Next Track"; case 0xB1:return "Previous Track"; case 0xB2:return "Media Stop"; case 0xB3:return "Play/Pause";
        case 0xB4:return "Mail"; case 0xB5:return "Media Select"; case 0xB6:return "Launch App 1"; case 0xB7:return "Launch App 2";
        default:return "Key "+std::to_string(key);
    }
}
inline constexpr int Index(Action a) { return static_cast<int>(a); }
inline constexpr bool Reserved(Action a) { return a==Action::Reserved59||a==Action::Reserved60; }
inline constexpr bool System(Action a) { return Index(a)<=Index(Action::HudEditor)||a==Action::Panic||Reserved(a); }
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
