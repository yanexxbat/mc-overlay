#include "../src/common/item_assist.hpp"
#include "../src/common/chunk_hints.hpp"
#include "../src/overlay/keybinds.hpp"
#include <iostream>
#include <stdexcept>

using namespace mcov::items;
void Check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
int main() try {
    Frame f; f.world=f.alive=f.ownMenu=f.totemSupported=f.macroSupported=f.anchorSupported=true;
    Request r; r.sentAtMs=1000; r.inputAllowed=true; r.totem.enabled=true;
    f.inventory[0]=Item::Totem;
    Controller c;
    auto a=c.Tick(r,f,1000);
    Check(a.operation==Operation::SwapTotem&&ContainerSlot(a.slot)==36,"hotbar totem must use container slot 36");
    c.Complete(a,true); f.offhand=Item::Totem;
    Check(c.Tick(r,f,1050).operation==Operation::None&&c.GetReport().totem==Status::HoldingTotem,"must not repeatedly swap equipped totem");
    f.offhand=Item::Empty; f.inventory[0]=Item::Empty; f.inventory[35]=Item::Totem;
    r.sentAtMs=1100; a=c.Tick(r,f,1100);
    Check(a.operation==Operation::SwapTotem&&ContainerSlot(a.slot)==35,"consumed totem must be replenished from main inventory");
    f.ownMenu=false; Check(c.Tick(r,f,1200).operation==Operation::None&&c.GetReport().totem==Status::InventoryBusy,"chest must block swaps");
    f.ownMenu=true; f.cursorEmpty=false; Check(c.Tick(r,f,1200).operation==Operation::None,"cursor stack must block swaps");
    f.cursorEmpty=true; Check(c.Tick(r,f,1500).operation==Operation::None&&c.GetReport().totem==Status::Paused,"stale overlay request must block actions");
    r.totem.enabled=false; r.crystal.enabled=true; r.sentAtMs=2000; f.selected=7; f.inventory[2]=Item::Crystal; f.target=Target::CrystalBase;
    Check(c.Tick(r,f,2000).operation==Operation::None,"enabled macro alone must not place");
    r.crystalHeld=true; a=c.Tick(r,f,2000);
    Check(a.operation==Operation::Use&&a.slot==2,"crystal macro must select hotbar crystal for valid base");
    f.selected=2; f.target=Target::Crystal;
    Check(c.Tick(r,f,2050).operation==Operation::None,"macro interval must limit repeated actions");
    r.sentAtMs=2150; Check(c.Tick(r,f,2150).operation==Operation::AttackCrystal,"crosshair crystal must be broken");
    r.crystalHeld=false; a=c.Tick(r,f,2200);
    Check(a.operation==Operation::RestoreSlot&&a.slot==7,"release must restore old slot");
    c.Reset(); r.crystalHeld=true; r.sentAtMs=2300; f.target=Target::CrystalBase; f.selected=7; c.Tick(r,f,2300);
    f.selected=5; r.crystalHeld=false;
    Check(c.Tick(r,f,2350).operation==Operation::None,"manual slot change must not be overwritten on release");
    c.Reset(); r.crystal.enabled=false; r.crystalHeld=false; r.anchor.enabled=r.anchorHeld=true; r.sentAtMs=3000;
    f.inventory[1]=Item::Anchor; f.inventory[3]=Item::Glowstone; f.inventory[4]=Item::Other; f.selected=8;
    f.target=Target::OtherBlock; a=c.Tick(r,f,3000);
    Check(a.operation==Operation::Use&&a.anchor&&a.slot==1,"anchor placement must select anchor");
    f.target=Target::Anchor; f.charges=0; f.selected=1; r.sentAtMs=3150; a=c.Tick(r,f,3150);
    Check(a.operation==Operation::Use&&a.slot==3,"uncharged anchor must select glowstone");
    f.charges=1; f.selected=3; r.sentAtMs=3300; a=c.Tick(r,f,3300);
    Check(a.operation==Operation::Use&&f.inventory[a.slot]!=Item::Glowstone&&f.inventory[a.slot]!=Item::Crystal,"charged anchor must not use glowstone or crystal");
    f.anchorWorks=true; r.sentAtMs=3450;
    Check(c.Tick(r,f,3450).operation==Operation::None&&c.GetReport().anchor==Status::AnchorSafeDimension,"respawn dimension must refuse detonation");
    f.anchorWorks=false; f.charges=-1;
    Check(c.Tick(r,f,3450).operation==Operation::None,"unknown anchor charge must refuse interaction");
    f.charges=1; r.crystal.enabled=r.crystalHeld=true;
    a=c.Tick(r,f,3450); Check(a.operation!=Operation::Use&&a.operation!=Operation::AttackCrystal,"simultaneous macros must conflict");
    c.Reset(); r.crystal.enabled=r.crystalHeld=false; r.combatActive=true;
    Check(c.Tick(r,f,3450).operation==Operation::None&&c.GetReport().anchor==Status::Paused,"Kill Aura must pause macro");
    Check(mcov::RotatedDeepslate("block.minecraft.deepslate","x")&&mcov::RotatedDeepslate("block.minecraft.deepslate","z"),"horizontal raw deepslate must be a hint");
    Check(!mcov::RotatedDeepslate("block.minecraft.deepslate","y")&&!mcov::RotatedDeepslate("block.minecraft.deepslate_diamond_ore","x")&&!mcov::RotatedDeepslate("block.minecraft.infested_deepslate","z"),"vertical deepslate and ores must not be hints");
    Check(mcov::ChunkCoordinate(-.1)==-1&&mcov::ChunkCoordinate(-16)==-1&&mcov::ChunkCoordinate(-17)==-2&&mcov::ChunkCoordinate(16)==1,"negative chunk coordinates must floor correctly");
    const auto binds=mcov::keys::Defaults();
    Check(!binds[21].key&&!binds[22].key&&binds[21].mode==mcov::keys::Mode::Hold&&binds[22].mode==mcov::keys::Mode::Hold,"macros must default to unbound hold");
    std::cout<<"Item assist safety, timing, inventory decisions and chunk hints passed\n"; return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
