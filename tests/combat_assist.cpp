#include "combat/simulated_game.hpp"
#include <atomic>
#include <cstdio>
#include <stdexcept>
#include <thread>

using namespace mcov::combat;
namespace {
int checks=0;
void Require(bool condition,const char* message) { ++checks; if(!condition) throw std::runtime_error(message); }
bool Near(double a,double b,double epsilon=1e-8) { return std::abs(a-b)<=epsilon; }
Config Enabled() { Config c; c.enabled=c.attack.enabled=true; c.target.coneDegrees=60; return c; }
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
        Require(Finite(a) && std::abs(Wrap(a.yaw-179))<5,"yaw wrapped the long way"); highestPitch=std::max(highestPitch,a.pitch);
    }
    Require(highestPitch>10.1,"overshoot absent");
    const auto settled=engine.Step(f,t,c,random); Require(Near(settled.pitch,10),"overshoot correction absent");
    engine.Reset(); random.Seed(23); f={}; f.dt=.010; f.tick=50; t.desired={80,70};
    c.maxYawPerTick=5; c.maxPitchPerTick=2;
    Angles previous{}; double yawSum=0,pitchSum=0;
    for(int i=0;i<50;++i) {
        f.now=i*.010; const auto a=engine.Step(f,t,c,random);
        yawSum+=std::abs(Wrap(a.yaw-previous.yaw)); pitchSum+=std::abs(a.pitch-previous.pitch); previous=a;
    }
    Require(yawSum<=5+1e-8 && pitchSum<=2+1e-8,"per-tick budget exceeded by multiple render frames");
    f.tick=51; f.now+=.01; const auto next=engine.Step(f,t,c,random); Require(next.yaw>previous.yaw,"tick budget never renewed");
    c={}; c.reactionDelay={.010,.010}; c.jitterDegrees={.04,.04}; c.settleDegrees=.08;
    t.desired={}; f={}; f.dt=.02; engine.Reset(); random.Seed(1); double noise=0;
    for(int i=0;i<100;++i) { f.now=i*.02; f.tick=static_cast<std::uint64_t>(f.now/.05); const auto a=engine.Step(f,t,c,random); noise=std::max(noise,std::abs(a.yaw)); }
    Require(noise>0 && noise<=.04+1e-8,"micro jitter ignored or out of range");
}
struct Recorder final : Transport {
    std::array<Command,kQueueCapacity> commands{}; std::size_t count{}; bool fail{};
    bool Send(const Command& c) override { if(fail || count==commands.size()) return false; commands[count++]=c; return true; }
};
void SyncAndAttackChecks() {
    Config c=Enabled(); c.attack.firstClickDelay={0,0}; c.sync.interval={.04,.04}; c.sync.attackMaxAge=.2;
    Target t; t.id=1; t.uuid={1,1}; t.range=2;
    Frame f; f.now=f.sampledAt=1; f.world=1; f.inputAllowed=true; f.crosshairId=f.attackHitId=1; f.attackCooldown=1; f.gameAttackReach=3;
    AttackController attack; Random random(1);
    Require(attack.Prepare(f,t,c,random),"ready attack rejected"); attack.Sent();
    Require(!attack.Prepare(f,t,c,random),"attack reused old cooldown"); f.attackCooldown=.5;
    Require(!attack.Prepare(f,t,c,random),"attack before cooldown ready"); f.attackCooldown=1;
    Require(attack.Prepare(f,t,c,random),"attack not rearmed after real cooldown reset");
    f.attackCooldown.reset(); Require(!CanAttack(f,t,c),"unknown cooldown attacked"); f.attackCooldown=1;
    f.gameAttackReach=1; Require(!CanAttack(f,t,c),"game reach ignored"); f.gameAttackReach=3;
    c.attack.silentRotation=true; Require(!CanAttack(f,t,c),"unsupported silent rotation accepted"); c.attack.silentRotation=false;
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
    Config config=Enabled(); config.attack.silentRotation=true;
    ConfigStore a,b; a.Publish(config); b.Publish(config);
    FriendEnemyBook relations; SimulatedGame first,second;
    relations.Set(first.Entities()[2].uuid,Relation::Friend);
    CombatAssist one(a,relations,first),two(b,relations,second);
    DebugSnapshot state;
    for(int i=0;i<250;++i) { state=one.Step(first.Capture(i*.02,.02,true)); two.Step(second.Capture(i*.02,.02,true)); }
    Require(first.Attacks()>0 && first.Attacks()==second.Attacks(),"simulation produced no reproducible attacks");
    Require(first.Entities()[2].health==20 && Near(first.Camera().yaw,0) && std::abs(first.ServerRotation().yaw)>1,"friend or silent camera behavior failed");
    const auto x=first.Commands(),y=second.Commands(); Require(x.size()==y.size(),"seed changed command count");
    for(std::size_t i=0;i<x.size();++i) Require(x[i].sequence==y[i].sequence && x[i].kind==y[i].kind && x[i].due==y[i].due && x[i].pose.rotation.yaw==y[i].pose.rotation.yaw && x[i].pose.rotation.pitch==y[i].pose.rotation.pitch,"seed not reproducible");
    Require(state.traceCount>0 && state.packets.sent==x.size(),"debug mailbox statistics wrong");
    const auto before=x.size(); config.enabled=false; a.Publish(config);
    for(int i=250;i<270;++i) one.Step(first.Capture(i*.02,.02,true));
    Require(first.Commands().size()==before && one.Debug().packets.buffered==0,"hot disable emitted input");
    config.enabled=true; a.Publish(config); auto unsupported=first.Capture(5.5,.02,true); unsupported.silentSupported=false;
    const auto blocked=one.Step(unsupported); Require(blocked.silentUnavailable && first.Commands().size()==before,"unsupported silent fallback emitted normal camera input");
    auto stale=first.Capture(5.6,.02,true); stale.sampledAt=5;
    Require(!one.Step(stale).enabled,"stale frame remained active");
    auto missing=first.Capture(5.7,.02,true); missing.attackCooldown.reset();
    Require(one.Step(missing).cooldownUnavailable,"unknown cooldown hidden from HUD");
    for(auto& entity:first.Entities()) entity.visibility.reset();
    auto noSight=first.Capture(5.8,.02,true);
    const auto noTarget=one.Step(noSight);
    Require(!noTarget.target && noTarget.packets.buffered==0,"visibility loss left a queued attack");
}
}
int main() {
    try { ConfigChecks(); TargetChecks(); RotationChecks(); SyncAndAttackChecks(); IntegrationChecks(); }
    catch(const std::exception& error) { std::printf("FAIL Combat Assist: %s\n",error.what()); return 1; }
    std::printf("PASS Combat Assist: %d checks (selection, curves, cooldown, FIFO, threads, reload, seed, simulator)\n",checks); return 0;
}
