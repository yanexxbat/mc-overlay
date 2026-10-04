#include "combat/simulated_game.hpp"
#include "common/combat_bridge_state.hpp"
#include <atomic>
#include <cstdio>
#include <stdexcept>
#include <thread>

using namespace mcov::combat;
namespace {
int checks=0;
void Require(bool condition,const char* message) { ++checks; if(!condition) throw std::runtime_error(message); }
bool Near(double a,double b,double epsilon=1e-8) { return std::abs(a-b)<=epsilon; }
// Zusätzliche Auswahlmodi bleiben als Bibliotheksfälle getestet; Live-Kill-Aura nutzt NearestEntity.
Config Enabled() {
    Config c; c.enabled=c.attack.enabled=true; c.target.coneDegrees=60; c.target.range=6;
    c.target.mode=TargetMode::AngleDistance; c.target.playersOnly=c.target.prioritizeEnemies=true; c.target.multiTarget=false;
    return c;
}
void ConfigChecks() {
    ConfigStore store; Config c=Enabled(); Require(store.Publish(c),"valid config rejected");
    const auto version=store.Read(); c.rotation.reactionDelay={0.040,0.010};
    Require(!store.Publish(c) && store.Read().revision==version.revision,"partial hot reload accepted");
    c=Enabled(); c.target.range=std::numeric_limits<double>::quiet_NaN(); Require(!Validate(c),"NaN config accepted");
    c=Enabled(); c.target.mode=static_cast<TargetMode>(99); Require(!Validate(c),"unknown enum accepted");
    std::atomic<bool> done=false,broken=false;
    std::jthread writer([&] { for(int i=0;i<2000;++i) { Config v=Enabled(); v.seed=static_cast<std::uint64_t>(i); v.target.range=i%2 ? 4 : 6; store.Publish(v); } done=true; });
    std::jthread reader([&] { while(!done) { const auto v=store.Read(); if(v.revision>version.revision && v.values.target.range!=(v.values.seed%2 ? 4 : 6)) broken=true; } });
    writer.join(); reader.join(); Require(!broken,"config torn between threads");
    mcov::SharedMailbox<mcov::CombatRequest> mailbox;
    done=false; broken=false;
    std::jthread ipcWriter([&] { for(std::uint64_t i=1;i<=2000;++i) {
        mcov::CombatRequest request; request.revision=i; request.sentAtMs=i;
        request.config.seed=i; request.relations[0].uuid={i,i}; mailbox.Publish(request);
    } done=true; });
    std::jthread ipcReader([&] { while(!done) { mcov::CombatRequest request;
        if(mailbox.Read(request) && request.revision && (request.sentAtMs!=request.revision || request.config.seed!=request.revision || request.relations[0].uuid.high!=request.revision)) broken=true;
    } });
    ipcWriter.join(); ipcReader.join(); Require(!broken,"IPC combat config/relations tore");
    c=Enabled(); c.target.coneDegrees=180; Require(Validate(c),"full Kill Aura cone rejected");
    Frame full; full.viewportHeight=1080; full.verticalFovDeg=70;
    Require(FovRadius(full,c.target)==0,"full sphere projected a misleading FOV circle");
    mcov::CombatRequest lease; lease.inputAllowed=true; lease.sentAtMs=1000;
    Require(mcov::CombatLeaseValid(lease,1250) && !mcov::CombatLeaseValid(lease,1251) && !mcov::CombatLeaseValid(lease,999),"combat lease accepted stale/future data");
}
void TargetChecks() {
    SimulatedGame game; auto f=game.Capture(1,0.020); Config c=Enabled();
    FriendEnemyBook book; Require(book.Set(game.Entities()[2].uuid,Relation::Friend),"friend insertion failed");
    Random random(13); TargetSelector selector;
    auto selected=selector.Select(f,c.target,book.Read(),random); Require(selected && selected->id==1,"angle-distance selection failed");
    Require(Near(selected->range,std::hypot(0.2,2.3)),"range was measured to center, not hitbox");
    c.target.mode=TargetMode::LowestHealth; selector.Reset(); selected=selector.Select(f,c.target,book.Read(),random);
    Require(selected && selected->id==2,"lowest health selection failed");
    c.target.mode=TargetMode::NearestPlayer; selector.Reset(); selected=selector.Select(f,c.target,book.Read(),random);
    Require(selected && selected->id==1,"nearest player selection failed");
    c.target.mode=TargetMode::Crosshair; f.crosshairId=2; selector.Reset(); selected=selector.Select(f,c.target,book.Read(),random);
    Require(selected && selected->id==2,"crosshair selection failed");
    f.crosshairId=3; selector.Reset(); Require(!selector.Select(f,c.target,book.Read(),random),"friend selected under crosshair");
    c.target.mode=TargetMode::AngleDistance; book.Set(game.Entities()[1].uuid,Relation::Enemy); selector.Reset();
    Require(selector.Select(f,c.target,book.Read(),random)->id==2,"enemy priority failed");
    book.Set(game.Entities()[1].uuid,Relation::Neutral); selector.Reset();
    auto invisible=game.Entities()[0]; invisible.visibility=Visibility{false,f.now};
    Require(!Evaluate(invisible,f,c.target,book.Read()),"invisible target accepted");
    invisible.visibility.reset(); Require(!Evaluate(invisible,f,c.target,book.Read()),"unknown visibility accepted");
    invisible.visibility=Visibility{true,f.now-0.101}; Require(!Evaluate(invisible,f,c.target,book.Read()),"old visibility accepted");
    invisible=game.Entities()[0]; invisible.self=true; Require(!Evaluate(invisible,f,c.target,book.Read()),"self selected");
    invisible=game.Entities()[0]; invisible.health=0; Require(!Evaluate(invisible,f,c.target,book.Read()),"dead target selected");
    auto silentView=f; silentView.cameraRotation={180,0}; silentView.observedRotation=selected->desired;
    Require(!Evaluate(game.Entities()[0],silentView,c.target,book.Read()),"silent selection ignored visible camera FOV");
    const double radius=FovRadius(f,c.target); auto large=f; large.viewportHeight*=2;
    Require(Near(FovRadius(large,c.target),radius*2),"dynamic FOV not resolution-scaled");
    c.target.fovMode=FovMode::StaticPixels;
    Require(Near(FovRadius(large,c.target),FovRadius(f,c.target)) && ConeDegrees(large,c.target)<ConeDegrees(f,c.target),"static FOV not static");
    c.target.fovMode=FovMode::DynamicDegrees; c.target.multiTarget=true; c.target.switchDelay={0.12,0.12};
    selector.Reset(); selected=selector.Select(f,c.target,book.Read(),random); Require(selected->id==1,"initial target failed");
    book.Set(game.Entities()[1].uuid,Relation::Enemy);
    Require(selector.Select(f,c.target,book.Read(),random)->id==1,"target switched without delay");
    f=game.Capture(1.08,0.02); Require(selector.Select(f,c.target,book.Read(),random)->id==1,"target switch too early");
    f=game.Capture(1.13,0.02); Require(selector.Select(f,c.target,book.Read(),random)->id==2,"delayed target switch failed");
    book.Set(game.Entities()[1].uuid,Relation::Friend);
    Require(selector.Select(f,c.target,book.Read(),random)->id==1,"new friend retained as target");
}
void NearestEntityChecks() {
    Config defaults;
    Require(defaults.target.mode==TargetMode::NearestEntity && !defaults.target.playersOnly &&
            defaults.target.range==defaults.attack.range && !defaults.hud.enabled,"Kill Aura defaults are not simple nearest-entity settings");
    SimulatedGame game;
    game.PutEntity(0,1,"far_player",{0,0,2.7},20);
    game.PutEntity(1,2,"near_mob",{0,0,-1.6},20); game.Entities()[1].player=false;
    game.PutEntity(2,3,"side_player",{1.9,0,0},1);
    auto f=game.Capture(1,.02); auto c=defaults.target;
    // FOV und Priority ändern das Distanzranking nicht; Multi-Target bleibt als Bibliotheksfall verfügbar.
    c.coneDegrees=1; c.fovMode=FovMode::StaticPixels; c.radiusPixels=10;
    c.prioritizeEnemies=true; c.multiTarget=true; c.switchImprovement=1; c.switchDelay={1,1};
    FriendEnemyBook book; book.Set(game.Entities()[0].uuid,Relation::Enemy);
    Random random(9); TargetSelector selector;
    auto chosen=selector.Select(f,c,book.Read(),random);
    Require(chosen && chosen->id==2 && chosen->angle>170,"nearest mob behind camera lost to FOV, HP or enemy priority");
    c.playersOnly=true; chosen=selector.Select(f,c,book.Read(),random);
    Require(chosen && chosen->id==3,"players-only filter selected a mob"); c.playersOnly=false;
    game.PutEntity(2,3,"closer_player",{.8,0,0},1); f=game.Capture(1.02,.02);
    chosen=selector.Select(f,c,book.Read(),random);
    Require(chosen && chosen->id==3,"nearest target retained previous lock or switch delay");
    game.PutEntity(1,2,"equal_mob",{-.8,0,0},20); game.Entities()[1].player=false; f=game.Capture(1.04,.02);
    Require(selector.Select(f,c,book.Read(),random)->id==2,"equal-distance tie was not deterministic");
    std::swap(game.Entities()[1],game.Entities()[2]); f=game.Capture(1.06,.02);
    Require(selector.Select(f,c,book.Read(),random)->id==2,"entity iteration order changed equal-distance target");
    book.Set(game.Entities()[2].uuid,Relation::Friend);
    Require(selector.Select(f,c,book.Read(),random)->id==3,"nearest friend was not skipped");
    game.Entities()[1].visibility=Visibility{false,f.now};
    Require(selector.Select(f,c,book.Read(),random)->id==1,"occluded nearest target was not skipped");
    game.Entities()[0].health=0;
    game.PutEntity(3,4,"dropped_item",{0,0,.4},20); game.Entities()[3].living=false;
    f=game.Capture(1.08,.02);
    Require(!selector.Select(f,c,book.Read(),random),"dead entity or nonliving item was selected");
    game.Entities()[1].visibility=Visibility{true,f.now}; c.range=.1;
    Require(!selector.Select(f,c,book.Read(),random),"nearest entity beyond configured reach was selected");
}
void ContinuousKillAuraChecks() {
    auto requested=Enabled();
    requested.target.enabled=requested.rotation.enabled=requested.attack.enabled=requested.sync.enabled=false;
    requested.rotation.smoothing=4; requested.rotation.yawDuration=requested.rotation.pitchDuration={2,2};
    requested.rotation.maxYawPerTick=requested.rotation.maxPitchPerTick=.1;
    requested.attack.firstClickDelay={.5,.5}; requested.sync.interval={.2,.2};
    const auto live=MakeKillAuraConfig(requested);
    Require(Validate(live) && live.enabled && live.target.enabled && live.rotation.enabled && live.attack.enabled && live.sync.enabled &&
            live.target.mode==TargetMode::NearestEntity && !live.target.multiTarget && live.attack.firstClickDelay.max==0,
            "old controls prevented automatic live Kill Aura");
    ConfigStore store; store.Publish(live);
    FriendEnemyBook relations; SimulatedGame game;
    game.PutEntity(0,1,"locked",{0,0,-2},12);
    game.PutEntity(1,2,"next",{2.6,0,0},20); game.Entities()[2].health=0;
    CombatAssist assist(store,relations,game,ExecutionMode::Continuous);
    auto state=assist.Step(game.Capture(1,.05));
    constexpr double angularTolerance=1e-4; // acos-Skalarprodukt hat auch bei identischen Winkeln Rundungsfehler.
    Require(state.target && state.target->id==1 && game.Attacks()==1 && state.error<angularTolerance && state.packets.buffered==0,
            "target behind camera was not aimed and attacked on the first client tick");
    Require(game.Commands().size()==2 && game.Commands()[0].kind==CommandKind::Pose && game.Commands()[1].kind==CommandKind::Attack &&
            game.Commands()[0].due==1 && game.Commands()[1].due==1,"direct pose/attack were delayed or reordered");
    game.PutEntity(1,2,"closer",{0,0,.8},20);
    int tick=0;
    while(game.Entities()[0].health>0 && tick<64) {
        ++tick;
        const double health=game.Entities()[0].health;
        game.PutEntity(0,1,"moving_lock",{tick%2 ? 1.0 : -1.0,0,-1.8},health);
        state=assist.Step(game.Capture(1+tick*.05,.05));
        Require(state.target && state.target->id==1 && state.error<angularTolerance && state.packets.buffered==0,
                "moving lock lagged, waited or switched to a closer candidate");
        Require(Near(Wrap(game.ServerRotation().yaw-state.target->desired.yaw),0) && Near(game.ServerRotation().pitch,state.target->desired.pitch) &&
                Near(game.Camera().yaw,0) && Near(game.Camera().pitch,0),
                "continuous rotation missed current hitbox or moved camera");
    }
    Require(game.Entities()[0].health==0 && game.Attacks()==3,"continuous moving-target attacks stopped before death");
    state=assist.Step(game.Capture(1+(++tick)*.05,.05));
    Require(state.target && state.target->id==2 && state.error<angularTolerance,"dead lock did not release to the next nearest entity");
    relations.Set(game.Entities()[1].uuid,Relation::Friend);
    state=assist.Step(game.Capture(1+(++tick)*.05,.05));
    Require(!state.target && state.packets.buffered==0,"newly added friend retained continuous lock");
    relations.Set(game.Entities()[1].uuid,Relation::Neutral);
    state=assist.Step(game.Capture(1+(++tick)*.05,.05));
    Require(state.target && state.target->id==2,"valid target was not reacquired");
    game.Entities()[1].visibility=Visibility{false,0};
    state=assist.Step(game.Capture(1+(++tick)*.05,.05));
    Require(!state.target && state.packets.buffered==0,"visibility loss retained continuous lock");
    game.PutEntity(1,2,"out_of_range",{0,0,4},20);
    state=assist.Step(game.Capture(1+(++tick)*.05,.05));
    Require(!state.target,"continuous tracking ignored range");
}
void RotationChecks() {
    RotationConfig c; c.reactionDelay={.010,.010}; c.yawDuration=c.pitchDuration={.050,.050};
    c.overshootFraction={.08,.08}; c.jitterDegrees={0,0}; c.settleDegrees=0;
    Frame f; f.dt=.010; f.tick=1; f.observedRotation={179,0};
    Target t; t.id=1; t.uuid={1,1}; t.desired={-179,10};
    Random random(23); RotationEngine engine;
    const auto first=engine.Step(f,t,c,random); Require(Near(first.yaw,179),"reaction delay ignored");
    double highestPitch=0;
    for(int i=1;i<40;++i) {
        f.now=i*.010; f.tick=static_cast<std::uint64_t>(f.now/.050);
        const auto a=engine.Step(f,t,c,random);
        f.observedRotation=a;
        Require(Finite(a) && std::abs(Wrap(a.yaw-179))<5,"yaw wrapped the long way"); highestPitch=std::max(highestPitch,a.pitch);
    }
    Require(highestPitch>10.1,"overshoot absent");
    const auto settled=engine.Step(f,t,c,random); Require(Near(settled.pitch,10),"overshoot correction absent");
    engine.Reset(); random.Seed(23); f={}; f.dt=.010; f.tick=50; t.desired={80,70};
    c.maxYawPerTick=5; c.maxPitchPerTick=2;
    Angles previous{}; double yawSum=0,pitchSum=0;
    for(int i=0;i<50;++i) {
        f.now=i*.010; const auto a=engine.Step(f,t,c,random);
        f.observedRotation=a;
        yawSum+=std::abs(Wrap(a.yaw-previous.yaw)); pitchSum+=std::abs(a.pitch-previous.pitch); previous=a;
    }
    Require(yawSum<=5+1e-8 && pitchSum<=2+1e-8,"per-tick budget exceeded by multiple render frames");
    f.tick=51; f.now+=.01; const auto next=engine.Step(f,t,c,random); Require(next.yaw>previous.yaw,"tick budget never renewed");
    c={}; c.reactionDelay={.010,.010}; c.jitterDegrees={.04,.04}; c.settleDegrees=.08;
    t.desired={}; f={}; f.dt=.02; engine.Reset(); random.Seed(1); double noise=0;
    for(int i=0;i<100;++i) { f.now=i*.02; f.tick=static_cast<std::uint64_t>(f.now/.05); const auto a=engine.Step(f,t,c,random); f.observedRotation=a; noise=std::max(noise,std::abs(a.yaw)); }
    Require(noise>0 && noise<=.04+1e-8,"micro jitter ignored or out of range");
}
void ClosedLoopRotationChecks() {
    RotationConfig c; c.yawDuration=c.pitchDuration={.5,.5}; c.reactionDelay={.01,.01}; c.settleDegrees=0;
    Frame f; f.dt=.05;
    Target target; target.id=1; target.uuid={1,1}; target.desired={80,0};
    RotationEngine engine; Random random(7);
    for(int i=0;i<5;++i) {
        f.tick=i; f.now=i*.05;
        f.observedRotation=engine.Step(f,target,c,random);
    }
    target.desired={-80,0}; f.tick=5; f.now=.25;
    f.observedRotation=engine.Step(f,target,c,random);
    const auto before=f.observedRotation;
    f.tick=6; f.now=.30;
    const auto after=engine.Step(f,target,c,random);
    Require(AngularDistance(after,target.desired)<=AngularDistance(before,target.desired),
            "target replan curved away from its new position");
    // Ein realer Client kann eine geplante Pose verzögern oder korrigieren.
    for(int i=7;i<80;++i) {
        f.tick=i; f.now=i*.05; f.observedRotation=engine.Step(f,target,c,random);
    }
    f.tick=80; f.now=4; f.observedRotation={60,20};
    const auto rebased=engine.Step(f,target,c,random);
    Require(std::abs(Wrap(rebased.yaw-60))<=c.maxYawPerTick+1e-8 &&
            std::abs(rebased.pitch-20)<=c.maxPitchPerTick+1e-8,
            "rotation kept an unapplied angle instead of the observed client state");
}
struct Recorder final : Transport {
    std::array<Command,kQueueCapacity> commands{}; std::size_t count{}; bool fail{};
    bool Send(const Command& c) override { if(fail || count==commands.size()) return false; commands[count++]=c; return true; }
};
void SyncAndAttackChecks() {
    Config c=Enabled(); c.attack.firstClickDelay={0,0}; c.sync.interval={.04,.04}; c.sync.attackMaxAge=.2;
    Target t; t.id=1; t.uuid={1,1}; t.range=2;
    Frame f; f.now=f.sampledAt=1; f.world=1; f.inputAllowed=f.silentSupported=true; f.crosshairId=f.attackHitId=1; f.attackCooldown=1; f.gameAttackReach=3;
    AttackController attack; Random random(1);
    Require(attack.Prepare(f,t,c,random),"ready attack rejected"); attack.Sent();
    Require(!attack.Prepare(f,t,c,random),"attack reused old cooldown"); f.attackCooldown=.5;
    Require(!attack.Prepare(f,t,c,random),"attack before cooldown ready"); f.attackCooldown=1;
    Require(attack.Prepare(f,t,c,random),"attack not rearmed after real cooldown reset");
    f.attackCooldown.reset(); Require(!CanAttack(f,t,c),"unknown cooldown attacked"); f.attackCooldown=1;
    f.gameAttackReach=1; Require(!CanAttack(f,t,c),"game reach ignored"); f.gameAttackReach=3;
    f.silentSupported=false; Require(!CanAttack(f,t,c),"unsupported silent rotation accepted"); f.silentSupported=true;
    SyncLayer sync; sync.Configure(c.sync,1); Recorder transport;
    Command command; command.world=1; command.target=1; command.identity=t.uuid; command.sampledAt=1;
    Require(sync.Enqueue(command,1),"pose enqueue failed"); command.pose.rotation.yaw=1;
    sync.Enqueue(command,1); Require(sync.Stats().buffered==1 && sync.Stats().coalesced==1,"pose not coalesced");
    command.kind=CommandKind::Attack; sync.Enqueue(command,1);
    command.kind=CommandKind::Pose; sync.Enqueue(command,1); f.now=f.sampledAt=1.10;
    const auto pumped=sync.Pump(f,t,c,transport);
    Require(pumped.attackSent && transport.count==3,"pose/attack batch failed");
    Require(transport.commands[0].sequence<transport.commands[1].sequence && transport.commands[1].sequence<transport.commands[2].sequence,"FIFO sequence reordered");
    sync.Enqueue(command,1.10); f.now=f.sampledAt=1.50;
    sync.Pump(f,t,c,transport); Require(transport.count==3 && sync.Stats().discarded==1,"expired command sent");
    command.sampledAt=1.50; sync.Enqueue(command,1.50); f.now=f.sampledAt=1.56;
    sync.Pump(f,std::nullopt,c,transport); Require(transport.count==3,"lost target received queued pose");
    command.sampledAt=1.50; sync.Enqueue(command,1.50); f.now=f.sampledAt=1.56;
    auto reused=t; reused.uuid={2,2}; sync.Pump(f,reused,c,transport); Require(transport.count==3,"command sent to reused entity ID");
    sync.Enqueue(command,1.56); sync.Cancel(); Require(sync.Stats().buffered==0,"cancel retained buffered command");
    command.sampledAt=1.56; sync.Enqueue(command,1.56); transport.fail=true; f.now=f.sampledAt=1.62;
    sync.Pump(f,t,c,transport); Require(sync.Stats().failed==1,"failed transport counted as sent"); transport.fail=false;
    command.sampledAt=f.now; sync.Enqueue(command,f.now);
    ServerCorrection correction; correction.world=1; correction.serial=1; correction.sampledAt=f.now; correction.server.position.x=2;
    f.correction=correction; sync.Observe(f);
    Require(sync.Recovering(f.now) && sync.Stats().buffered==0 && sync.Stats().corrections==1,"desync recovery failed");
    sync.Observe(f); Require(sync.Stats().corrections==1,"duplicate correction counted again");
    sync.ResetWorld(); Require(!sync.Recovering(f.now),"recovery leaked across worlds");
    sync.Configure(c.sync,2);
    std::array<std::jthread,3> producers;
    command.kind=CommandKind::Attack; command.sampledAt=2;
    for(auto& producer:producers) producer=std::jthread([&] { for(int i=0;i<80;++i) sync.Enqueue(command,2); });
    for(auto& producer:producers) producer.join();
    Require(sync.Stats().buffered==kQueueCapacity && sync.Stats().discarded>=240-kQueueCapacity,"bounded multi-producer queue failed");
    SyncLayer limited; Recorder limitedTransport;
    c.rotation.maxYawPerTick=5; c.rotation.maxPitchPerTick=2; c.sync.interval={.010,.010}; limited.Configure(c.sync,4);
    command.kind=CommandKind::Pose; command.pose.rotation={80,50}; command.sampledAt=3;
    limited.Enqueue(command,3); f.now=f.sampledAt=3.02; f.tick=100;
    limited.Pump(f,t,c,limitedTransport);
    Require(limitedTransport.count==1 && Near(limitedTransport.commands[0].pose.rotation.yaw,5) && Near(limitedTransport.commands[0].pose.rotation.pitch,2),"coalesced pose bypassed actual tick limit");
    limited.Cancel(); command.sampledAt=3.02; limited.Enqueue(command,3.02); f.now=f.sampledAt=3.04;
    limited.Pump(f,t,c,limitedTransport);
    Require(limitedTransport.count==2 && Near(limitedTransport.commands[1].pose.rotation.yaw,5),"cancel renewed actual rotation budget within tick");
    command.sampledAt=3.04; limited.Enqueue(command,3.04); f.now=f.sampledAt=3.06; f.tick=101; f.observedRotation={5,2};
    limited.Pump(f,t,c,limitedTransport);
    Require(limitedTransport.count==3 && Near(limitedTransport.commands[2].pose.rotation.yaw,10),"next tick actual budget not renewed");
    f.now=f.sampledAt=3.07; f.observedRotation={30,2};
    correction.sampledAt=f.now; correction.server.rotation={30,2}; correction.expectedAtSameTime.rotation={10,2};
    f.correction=correction; limited.Observe(f);
    command.sampledAt=3.07; limited.Enqueue(command,3.07); f.now=f.sampledAt=3.09;
    limited.Pump(f,t,c,limitedTransport);
    Require(limitedTransport.count==4 && Near(limitedTransport.commands[3].pose.rotation.yaw,30),"correction reused old rotation origin or reset spent tick budget");
    command.sampledAt=3.09; limited.Enqueue(command,3.09); f.now=f.sampledAt=3.11; f.tick=102;
    limited.Pump(f,t,c,limitedTransport);
    Require(limitedTransport.count==5 && Near(limitedTransport.commands[4].pose.rotation.yaw,31.75),"recovery failed to slow actual dispatched rotation");
}
void IntegrationChecks() {
    Config config=Enabled();
    ConfigStore a,b; a.Publish(config); b.Publish(config);
    FriendEnemyBook relations; SimulatedGame first,second;
    relations.Set(first.Entities()[2].uuid,Relation::Friend);
    CombatAssist one(a,relations,first),two(b,relations,second);
    DebugSnapshot state;
    for(int i=0;i<250;++i) { state=one.Step(first.Capture(i*.02,.02)); two.Step(second.Capture(i*.02,.02)); }
    Require(first.Attacks()>0 && first.Attacks()==second.Attacks(),"simulation produced no reproducible attacks");
    Require(first.Entities()[2].health==20 && Near(first.Camera().yaw,0) && std::abs(first.ServerRotation().yaw)>1,"friend or silent camera behavior failed");
    const auto x=first.Commands(),y=second.Commands(); Require(x.size()==y.size(),"seed changed command count");
    for(std::size_t i=0;i<x.size();++i) Require(x[i].silent && y[i].silent && x[i].sequence==y[i].sequence && x[i].kind==y[i].kind && x[i].due==y[i].due && x[i].pose.rotation.yaw==y[i].pose.rotation.yaw && x[i].pose.rotation.pitch==y[i].pose.rotation.pitch,"default combat changed camera or seed was not reproducible");
    Require(state.traceCount>0 && state.packets.sent==x.size(),"debug mailbox statistics wrong");
    const auto before=x.size(); config.enabled=false; a.Publish(config);
    for(int i=250;i<270;++i) one.Step(first.Capture(i*.02,.02));
    Require(first.Commands().size()==before && one.Debug().packets.buffered==0,"hot disable emitted input");
    config.enabled=true; a.Publish(config); auto unsupported=first.Capture(5.5,.02); unsupported.silentSupported=false;
    const auto blocked=one.Step(unsupported); Require(blocked.silentUnavailable && first.Commands().size()==before,"unsupported silent fallback emitted normal camera input");
    auto stale=first.Capture(5.6,.02); stale.sampledAt=5;
    Require(!one.Step(stale).enabled,"stale frame remained active");
    auto missing=first.Capture(5.7,.02); missing.attackCooldown.reset();
    Require(one.Step(missing).cooldownUnavailable,"unknown cooldown hidden from HUD");
    for(auto& entity:first.Entities()) entity.visibility.reset();
    auto noSight=first.Capture(5.8,.02);
    const auto noTarget=one.Step(noSight);
    Require(!noTarget.target && noTarget.packets.buffered==0,"visibility loss left a queued attack");
}
}
int main() {
    try { ConfigChecks(); TargetChecks(); NearestEntityChecks(); ContinuousKillAuraChecks(); RotationChecks(); ClosedLoopRotationChecks(); SyncAndAttackChecks(); IntegrationChecks(); }
    catch(const std::exception& error) { std::printf("FAIL Combat Assist: %s\n",error.what()); return 1; }
    std::printf("PASS Combat Assist: %d checks (selection, curves, cooldown, FIFO, threads, reload, seed, simulator)\n",checks); return 0;
}
