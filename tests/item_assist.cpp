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
    r.totem.enabled=false; r.crystal.enabled=true; r.sentAtMs=2000;
    f.selected=2; f.inventory[2]=Item::Crystal; f.target=Target::CrystalBase;
    Check(c.Tick(r,f,2000).operation==Operation::None,"enabled macro alone must not place");
    f.useDown=f.siteValid=true; f.cycle=Item::Crystal;
    a=c.Tick(r,f,2000); Check(a.operation==Operation::PlaceCrystal&&a.slot==2,"captured crystal placement did not start");
    c.Complete(a,true);
    r.sentAtMs=2150; Check(c.Tick(r,f,2150).operation==Operation::None,"unconfirmed spawn must not duplicate placement");
    f.target=Target::Crystal; f.crystalId=42;
    a=c.Tick(r,f,2150); Check(a.operation==Operation::AttackCrystal,"spawn at saved base must be attacked");
    c.Complete(a,true);
    r.sentAtMs=2300; Check(c.Tick(r,f,2300).operation==Operation::None,"must wait for crystal removal");
    f.target=Target::CrystalBase;
    a=c.Tick(r,f,2300); Check(a.operation==Operation::PlaceCrystal,"removed crystal must be replaced without changing aim");
    c.Complete(a,true); f.useDown=false; a=c.Tick(r,f,2350);
    Check(a.operation==Operation::RestoreSlot&&a.slot==2&&!c.Active(),"release must stop and restore");
    f.useDown=true; r.sentAtMs=2360; a=c.Tick(r,f,2360);
    Check(a.operation==Operation::PlaceCrystal,"release must clear the previous cycle's timer");
    c.Complete(a,true); f.selected=5;
    Check(c.Tick(r,f,2400).operation==Operation::None&&!c.Active(),"manual slot must cancel and not be overwritten");

    c.Reset(); r.crystal.enabled=false; r.anchor.enabled=true; r.sentAtMs=3000;
    f.cycle=Item::Anchor; f.selected=1; f.target=Target::OtherBlock;
    f.inventory[1]=Item::Anchor; f.inventory[3]=Item::Glowstone; f.inventory[4]=Item::Other;
    a=c.Tick(r,f,3000); Check(a.operation==Operation::PlaceAnchor&&a.slot==1,"anchor placement failed");
    c.Complete(a,true); r.sentAtMs=3150;
    Check(c.Tick(r,f,3150).operation==Operation::None,"unconfirmed anchor must not be duplicated");
    f.target=Target::Anchor; f.charges=0;
    a=c.Tick(r,f,3150); Check(a.operation==Operation::ChargeAnchor&&a.slot==3,"anchor must be charged automatically");
    c.Complete(a,true); f.selected=3; r.sentAtMs=3300;
    Check(c.Tick(r,f,3300).operation==Operation::None,"must wait for charge confirmation");
    f.charges=1;
    a=c.Tick(r,f,3300); Check(a.operation==Operation::DetonateAnchor&&f.inventory[a.slot]!=Item::Glowstone,"anchor detonation must not add fuel");
    c.Complete(a,true); f.selected=a.slot; r.sentAtMs=3450;
    Check(c.Tick(r,f,3450).operation==Operation::None,"must wait for anchor disappearance");
    f.target=Target::OtherBlock; a=c.Tick(r,f,3450);
    Check(a.operation==Operation::PlaceAnchor,"exploded anchor must be replaced at saved site");
    c.Complete(a,true); f.selected=1; f.target=Target::Anchor; f.charges=0; r.sentAtMs=3600;
    a=c.Tick(r,f,3600); c.Complete(a,true); f.selected=3; f.charges=1; f.offhand=Item::Glowstone; r.sentAtMs=3750;
    Check(c.Tick(r,f,3750).operation==Operation::None&&c.GetReport().anchor==Status::OffhandFuel,"offhand fuel must not overcharge");
    f.offhand=Item::Empty; f.anchorWorks=true;
    Check(c.Tick(r,f,3750).operation==Operation::None&&c.GetReport().anchor==Status::AnchorSafeDimension,"respawn dimensions must pause detonation");
    f.anchorWorks=false; f.siteValid=false; a=c.Tick(r,f,3750);
    Check(!c.Active()&&a.operation==Operation::RestoreSlot,"invalid / distant site must stop");
    f.siteValid=true; f.selected=1; f.target=Target::OtherBlock; c.Reset(); r.sentAtMs=4000;
    a=c.Tick(r,f,4000); c.Complete(a,false); r.sentAtMs=4150;
    Check(c.Tick(r,f,4150).operation==Operation::PlaceAnchor,"rejected use must retry after action interval");
    c.Reset(); r.combatActive=true;
    Check(c.Tick(r,f,4150).operation==Operation::None&&c.GetReport().anchor==Status::Paused,"Kill Aura must pause macro");
    r.combatActive=false; c.Reset(); r.sentAtMs=5000; a=c.Tick(r,f,5000); c.Complete(a,true);
    r.sentAtMs=5600; Check(c.Tick(r,f,5600).operation==Operation::None,"confirmation wait ignored");
    r.sentAtMs=5750; Check(c.Tick(r,f,5750).operation==Operation::PlaceAnchor,"confirmation timeout must permit retry");
    r.sentAtMs=6000; r.anchor.enabled=false; c.Tick(r,f,6000); Check(!c.Active(),"disabling module must stop cycle");
    c.Reset(); r.anchor.enabled=true; r.sentAtMs=7000; f.selected=1; f.target=Target::Anchor; f.charges=0;
    a=c.Tick(r,f,7000); c.Complete(a,true); f.selected=a.slot; f.paused=true;
    Check(c.Tick(r,f,7050).operation==Operation::None&&!c.Active()&&c.HasRestore(),"pause must stop cycle and retain pending restore");
    f.paused=false; f.ownMenu=false;
    Check(c.Tick(r,f,7100).operation==Operation::None&&c.HasRestore(),"container must defer pending restore");
    f.ownMenu=true; a=c.Tick(r,f,7150);
    Check(a.operation==Operation::RestoreSlot&&a.slot==1&&!c.HasRestore(),"closing menu must complete pending restore");
    c.Reset(); f.selected=1; r.sentAtMs=7200; a=c.Tick(r,f,7200); c.Complete(a,true); f.selected=a.slot; f.paused=true;
    c.Tick(r,f,7250); f.selected=6; f.paused=false;
    Check(c.Tick(r,f,7300).operation==Operation::None&&!c.HasRestore(),"manual choice during pause must cancel pending restore");
    Check(mcov::RotatedDeepslate("block.minecraft.deepslate","x")&&mcov::RotatedDeepslate("block.minecraft.deepslate","z"),"horizontal raw deepslate must be a hint");
    Check(!mcov::RotatedDeepslate("block.minecraft.deepslate","y")&&!mcov::RotatedDeepslate("block.minecraft.deepslate_diamond_ore","x")&&!mcov::RotatedDeepslate("block.minecraft.infested_deepslate","z"),"vertical deepslate and ores must not be hints");
    Check(mcov::ChunkCoordinate(-.1)==-1&&mcov::ChunkCoordinate(-16)==-1&&mcov::ChunkCoordinate(-17)==-2&&mcov::ChunkCoordinate(16)==1,"negative chunk coordinates must floor correctly");
    const auto binds=mcov::keys::Defaults();
    Check(!binds[21].key&&!binds[22].key&&binds[21].mode==mcov::keys::Mode::Toggle&&binds[22].mode==mcov::keys::Mode::Toggle,"macros must default to unbound toggle");
    std::cout<<"Item assist safety, timing, inventory decisions and chunk hints passed\n"; return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
