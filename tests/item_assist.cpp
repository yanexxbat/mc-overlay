#include "../src/common/item_assist.hpp"
#include "../src/common/chunk_hints.hpp"
#include "../src/overlay/keybinds.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>

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
    // Unabhängige Zeiten, Schutz und verspätete Weltbestätigungen.
    {
        Controller timed; Frame sample;
        sample.world=sample.alive=sample.ownMenu=sample.macroSupported=sample.useDown=sample.siteValid=true;
        sample.cycle=Item::Crystal; sample.inventory[2]=Item::Crystal; sample.selected=2;
        sample.target=Target::CrystalBase; sample.health=20; sample.distance=2;
        Request cfg; cfg.inputAllowed=true; cfg.crystal.enabled=true; cfg.crystal.separateDelays=true;
        cfg.crystal.placeMs=300; cfg.crystal.breakMs=50; cfg.crystal.minHealth=6; cfg.crystal.requireTotem=true;
        const auto step=[&](std::uint64_t now) { cfg.sentAtMs=now; return timed.Tick(cfg,sample,now); };
        Check(step(10000).operation==Operation::None&&timed.GetReport().crystal==Status::NeedTotem,"missing offhand totem did not pause");
        sample.offhand=Item::Totem; sample.health=5;
        Check(step(10001).operation==Operation::None&&timed.GetReport().crystal==Status::LowHealth,"low health did not pause");
        sample.health=20; sample.distance=5;
        Check(step(10002).operation==Operation::None&&timed.GetReport().crystal==Status::OutOfRange,"custom range ignored");
        sample.distance=2; a=step(10010); Check(a.operation==Operation::PlaceCrystal,"safety recovery must resume site"); timed.Complete(a,true);
        sample.target=Target::Crystal; sample.crystalId=17;
        Check(step(10059).operation==Operation::None,"break delay ran early");
        a=step(10060); Check(a.operation==Operation::AttackCrystal,"independent break delay ignored"); timed.Complete(a,true);
        sample.target=Target::CrystalBase;
        Check(step(10359).operation==Operation::None,"place delay ran early");
        a=step(10360); Check(a.operation==Operation::PlaceCrystal,"independent place delay ignored"); timed.Complete(a,true);
        cfg.crystal.strictConfirmation=true; cfg.crystal.confirmationMs=100;
        Check(step(11110).operation==Operation::None&&timed.GetReport().crystal==Status::ConfirmationTimeout,"strict timeout retransmitted placement");
        Check(step(14000).operation==Operation::None,"strict confirmation must continue suppressing retry");
        sample.target=Target::Crystal; sample.crystalId=18;
        a=step(14001); Check(a.operation==Operation::AttackCrystal,"late confirmation must resume without reset"); timed.Complete(a,true);
        sample.useDown=false; a=step(14002); Check(a.operation==Operation::RestoreSlot&&!timed.Active(),"release must restore after strict wait");
        cfg.crystal.maxRange=std::numeric_limits<float>::quiet_NaN(); cfg.crystal.placeMs=0; cfg.crystal.detonationSlot=99; cfg.crystal.Clamp();
        Check(cfg.crystal.maxRange==4.5f&&cfg.crystal.placeMs==50&&cfg.crystal.detonationSlot==9,"invalid config not sanitized");
    }
    {
        Controller fuel; Frame sample;
        sample.world=sample.alive=sample.ownMenu=sample.macroSupported=sample.anchorSupported=sample.useDown=sample.siteValid=true;
        sample.cycle=Item::Anchor; sample.target=Target::Anchor; sample.selected=3; sample.charges=1;
        sample.inventory[3]=Item::Glowstone; sample.inventory[7]=Item::Other; sample.offhand=Item::Glowstone;
        Request cfg; cfg.inputAllowed=true; cfg.anchor.enabled=true; cfg.anchor.chargeWithOffhandFuel=true; cfg.anchor.detonationSlot=8;
        const auto step=[&](std::uint64_t now) { cfg.sentAtMs=now; return fuel.Tick(cfg,sample,now); };
        a=step(15000); Check(a.operation==Operation::ChargeAnchor,"offhand fuel must charge before detonation"); fuel.Complete(a,true);
        Check(step(15150).operation==Operation::None,"unchanged positive charge must not confirm another charge");
        sample.charges=2; a=step(15150); Check(a.operation==Operation::ChargeAnchor,"second charge ignored"); fuel.Complete(a,true);
        sample.charges=3; a=step(15300); Check(a.operation==Operation::ChargeAnchor,"third charge ignored"); fuel.Complete(a,true);
        sample.charges=4; a=step(15450); Check(a.operation==Operation::DetonateAnchor&&a.slot==7,"full anchor did not use preferred detonation slot"); fuel.Complete(a,true);
        sample.selected=7; sample.useDown=false; a=step(15500); Check(a.operation==Operation::RestoreSlot&&a.slot==3,"fuel cycle restore failed");
        cfg.anchor.Conservative(); Check(cfg.anchor.strictConfirmation&&cfg.anchor.maxRange==3&&cfg.anchor.breakMs==250,"conservative profile incomplete");
    }
    {
        Controller direct; Frame sample;
        sample.world=sample.alive=sample.ownMenu=sample.macroSupported=sample.useDown=sample.siteValid=true;
        sample.cycle=Item::Crystal; sample.target=Target::CrystalBase; sample.inventory[1]=Item::Crystal; sample.selected=1;
        Request cfg; cfg.inputAllowed=true; cfg.crystal.enabled=true; cfg.crystal.GrimConservative();
        const auto step=[&](std::uint64_t now) { cfg.sentAtMs=now; return direct.Tick(cfg,sample,now); };
        Check(step(16000).operation==Operation::None&&direct.GetReport().crystal==Status::CrosshairMismatch,"Grim profile placed without actual block hit");
        sample.crosshairSupport=true; a=step(16001); Check(a.operation==Operation::PlaceCrystal,"actual support hit did not permit placement"); direct.Complete(a,true);
        sample.target=Target::Crystal; sample.crystalId=50;
        Check(step(16251).operation==Operation::None&&direct.GetReport().crystal==Status::CrosshairMismatch,"Grim profile attacked crystal outside crosshair");
        sample.crosshairCrystal=true; a=step(16252); Check(a.operation==Operation::AttackCrystal,"real crosshair crystal did not permit attack");
    }
    Check(mcov::RotatedDeepslate("block.minecraft.deepslate","x")&&mcov::RotatedDeepslate("block.minecraft.deepslate","z"),"horizontal raw deepslate must be a hint");
    Check(!mcov::RotatedDeepslate("block.minecraft.deepslate","y")&&!mcov::RotatedDeepslate("block.minecraft.deepslate_diamond_ore","x")&&!mcov::RotatedDeepslate("block.minecraft.infested_deepslate","z"),"vertical deepslate and ores must not be hints");
    Check(mcov::ChunkCoordinate(-.1)==-1&&mcov::ChunkCoordinate(-16)==-1&&mcov::ChunkCoordinate(-17)==-2&&mcov::ChunkCoordinate(16)==1,"negative chunk coordinates must floor correctly");
    {
        Request cfg; cfg.sentAtMs=20000; cfg.inputAllowed=cfg.trident.enabled=true;
        Frame sample; sample.world=sample.alive=sample.ownMenu=sample.useDown=true;
        TridentFrame held; held.supported=held.holding=held.riptide=held.wet=held.usingMain=true;
        held.ticks=9; Check(TridentDecision(cfg,sample,held,20000)==Status::ChargingTrident,"Riptide released before minimum charge");
        held.ticks=10; Check(TridentDecision(cfg,sample,held,20000)==Status::ReleaseTrident,"Charged Riptide did not release");
        cfg.trident.chargeTicks=20; Check(TridentDecision(cfg,sample,held,20000)==Status::ChargingTrident,"Custom Riptide charge ignored");
        cfg.trident.chargeTicks=-1; Check(TridentDecision(cfg,sample,held,20000)==Status::ReleaseTrident,"Charge minimum clamp failed");
        sample.useDown=false; Check(TridentDecision(cfg,sample,held,20000)==Status::WaitingBind,"Released Use still fired"); sample.useDown=true;
        held.riptide=false; Check(TridentDecision(cfg,sample,held,20000)==Status::NeedRiptide,"Normal trident auto-thrown"); held.riptide=true;
        held.wet=false; Check(TridentDecision(cfg,sample,held,20000)==Status::NeedWater,"Dry Riptide released"); held.wet=true;
        held.usingMain=false; Check(TridentDecision(cfg,sample,held,20000)==Status::Ready,"Offhand or changed active item released"); held.usingMain=true;
        held.busy=true; Check(TridentDecision(cfg,sample,held,20000)==Status::Conflict,"Simultaneous inventory action allowed"); held.busy=false;
        sample.screen=true; Check(TridentDecision(cfg,sample,held,20000)==Status::Paused,"Menu permitted Riptide release"); sample.screen=false;
        Check(TridentDecision(cfg,sample,held,20251)==Status::Paused,"Stale lease permitted Riptide release");
        Check(TridentDecision(cfg,sample,held,19999)==Status::Paused,"Future lease permitted Riptide release");
        cfg.trident.enabled=false; Check(TridentDecision(cfg,sample,held,20000)==Status::Off,"Disabled Auto Trident fired");
    }
    {
        Request cfg; cfg.sentAtMs=30000; cfg.inputAllowed=cfg.trident.enabled=true;
        Frame sample; sample.world=sample.alive=sample.ownMenu=sample.useDown=true;
        TridentFrame held; held.supported=held.holding=held.riptide=held.wet=held.usingMain=true;
        TridentController timing(42);
        unsigned seen=0;
        for(int cycle=0;cycle<128;++cycle) {
            held.usingMain=false; timing.Tick(cfg,sample,held,30000); held.usingMain=true;
            int release=0;
            for(int tick=0;tick<=13;++tick) {
                held.ticks=tick;
                const auto status=timing.Tick(cfg,sample,held,30000);
                if(status==Status::ReleaseTrident) { release=tick; break; }
                Check(status==Status::ChargingTrident,"Random charge stopped unexpectedly");
            }
            Check(release>=10&&release<=13,"Random charge outside default 10-13 ticks");
            seen|=1u<<(release-10);
            Check(timing.Tick(cfg,sample,held,30000)==Status::Ready,"Duplicate release in same use");
        }
        Check(seen==15,"Random charge did not cover all four durations");
        timing.Reset(); held.ticks=0; timing.Tick(cfg,sample,held,30000);
        const int target=timing.TargetTicks();
        for(int repeat=0;repeat<20;++repeat) timing.Tick(cfg,sample,held,30000);
        Check(timing.TargetTicks()==target,"Target rerolled during a charge");
        sample.screen=true; Check(timing.Tick(cfg,sample,held,30000)==Status::Paused&&timing.TargetTicks()==0,"Pause did not clear charge"); sample.screen=false;
        cfg.trident.randomizeCharge=false; cfg.trident.chargeTicks=12;
        held.ticks=11; Check(timing.Tick(cfg,sample,held,30000)==Status::ChargingTrident,"Fixed charge fired early");
        held.ticks=12; Check(timing.Tick(cfg,sample,held,30000)==Status::ReleaseTrident,"Disabling randomization did not restore exact ticks");
        timing.Reset(); cfg.trident.randomizeCharge=true; cfg.trident.chargeTicks=39; cfg.trident.extraChargeTicks=100;
        held.ticks=39; timing.Tick(cfg,sample,held,30000);
        Check(timing.TargetTicks()>=39&&timing.TargetTicks()<=40,"Random charge exceeded total cap");
        timing.Reset(); cfg.trident.chargeTicks=-100; cfg.trident.extraChargeTicks=-5; held.ticks=10;
        Check(timing.Tick(cfg,sample,held,30000)==Status::ReleaseTrident&&timing.TargetTicks()==10,"Invalid charge range was not clamped");
    }
    const auto binds=mcov::keys::Defaults();
    Check(!binds[21].key&&!binds[22].key&&binds[21].mode==mcov::keys::Mode::Toggle&&binds[22].mode==mcov::keys::Mode::Toggle,"macros must default to unbound toggle");
    std::cout<<"Item assist safety, timing, inventory decisions and chunk hints passed\n"; return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
