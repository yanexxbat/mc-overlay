// Compile the actual binder and camera sampler in this test translation unit.
// No agent is started or attached to a user's game; the test owns its JVM.
#include "../src/agent/agent_main.cpp"
#include "../src/overlay/java_runtime.hpp"
#include "../src/overlay/process.hpp"
#include "../src/common/agent_health.hpp"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <iostream>

namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void Healthy(JNIEnv* env);
jobject NewObject(JNIEnv* env,jclass klass);
jclass LoadClass(JNIEnv* env,jobject loader,const char* name);
void SetStatic(JNIEnv* env,jclass owner,const char* name,const char* desc,jobject value);
mcov::agent::MinecraftCombat* testCombat=nullptr;
mcov::agent::InventoryAssist<Bindings>* testInventory=nullptr;
jboolean JNICALL TestMacroUse(JNIEnv* env,jclass,jobject client) { return testInventory&&testInventory->CaptureUse(env,client,false)?JNI_TRUE:JNI_FALSE; }
DWORD combatCallbackThread=0;
bool combatAfterVanillaTick=false;
void JNICALL TestCombatTick(JNIEnv* env,jclass,jobject client) {
    combatCallbackThread=GetCurrentThreadId();
    auto owner=env->GetObjectClass(client);
    auto done=env->GetStaticFieldID(owner,"testVanillaTickDone","Z");
    combatAfterVanillaTick=done && env->GetStaticBooleanField(owner,done)==JNI_TRUE;
    if(env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(owner);
    if(testCombat) testCombat->Tick(env,client,false);
    if(testInventory) testInventory->Tick(env,client,false);
}
void TestLiveCombat(JNIEnv* env,const Bindings& b,jobject loader,jobject client) {
    const auto& set=*b.set; const auto& g=*b.profile->geometry;
    auto block=std::make_unique<mcov::SharedBlock>();
    mcov::agent::MinecraftCombat bridge;
    Require(bridge.Install(env,b,block.get(),reinterpret_cast<void*>(&TestCombatTick)),"combat JNI methods or native tick did not bind");
    testCombat=&bridge;
    auto initialTick=jvm::TryMethod(env,b.client.minecraftClass,g.clientTick.name,g.clientTick.signature,false);
    env->CallVoidMethod(client,initialTick); Healthy(env);
    mcov::CombatReport initialReport;
    Require(block->combatReport.Read(initialReport) && initialReport.status==mcov::CombatBridgeStatus::Disabled,
            "empty IPC configuration raised an error before overlay connection");
    auto field=[&](jclass c,const char* name,const char* desc,bool stat=false) {
        const auto f=jvm::TryField(env,c,name,desc,stat); Require(f!=nullptr,"combat fixture field missing"); return f;
    };
    auto real=[&](jobject o,jclass c,const char* name,double value) { env->SetDoubleField(o,field(c,name,"D"),value); Healthy(env); };
    auto floating=[&](jobject o,jclass c,const char* name,float value) { env->SetFloatField(o,field(c,name,"F"),value); Healthy(env); };
    auto staticBoolean=[&](jclass c,const char* name,bool value) { env->SetStaticBooleanField(c,field(c,name,"Z",true),value); Healthy(env); };
    auto player=NewObject(env,LoadClass(env,loader,set.classes.localPlayer));
    auto entity=NewObject(env,b.clsPlayer),level=NewObject(env,b.clsLevel),mode=NewObject(env,b.clsInteractionMgr);
    env->SetObjectField(client,b.fPlayer,player); Healthy(env);
    env->SetObjectField(client,b.fLevel,level); Healthy(env);
    env->SetObjectField(client,b.fInteractionMgr,mode); Healthy(env);
    jobject screenOwner=client;
    if(b.fGui) {
        screenOwner=NewObject(env,LoadClass(env,loader,b.profile->guiClass));
        env->SetObjectField(client,b.fGui,screenOwner); Healthy(env);
    }
    auto alive=field(b.clsEntity,"testAlive","Z");
    env->SetBooleanField(player,alive,JNI_TRUE); Healthy(env);
    env->SetBooleanField(entity,alive,JNI_TRUE); Healthy(env);
    env->SetIntField(entity,field(b.clsEntity,"testId","I"),42); Healthy(env);
    real(entity,b.clsEntity,"testX",1); real(entity,b.clsEntity,"testZ",2);
    floating(player,b.clsEntity,"testPitch",0);
    floating(entity,b.clsLiving,"testHealth",20); floating(player,b.clsPlayer,"testCooldown",1);
    auto bounds=NewObject(env,b.geometry.bounds);
    const double lows[]={.7,0,1.7},highs[]={1.3,1.8,2.3};
    for(int i=0;i<3;++i) {
        env->SetDoubleField(bounds,b.geometry.min[i],lows[i]); Healthy(env);
        env->SetDoubleField(bounds,b.geometry.max[i],highs[i]); Healthy(env);
    }
    SetStatic(env,b.clsEntity,"testBounds",g.getBounds.signature+2,bounds);
    auto eye=NewObject(env,b.clsVec3); real(eye,b.clsVec3,set.fieldVecY.name,1.62);
    SetStatic(env,b.clsEntity,"testEye",g.eyePosition.signature+2,eye);
    auto hitClass=LoadClass(env,loader,g.hitClass);
    auto hit=NewObject(env,hitClass),location=NewObject(env,b.clsVec3);
    real(location,b.clsVec3,set.fieldVecY.name,1.62); real(location,b.clsVec3,set.fieldVecZ.name,3);
    SetStatic(env,b.clsEntity,"testHit",std::strchr(g.pickBlocks.signature,')')+1,hit);
    SetStatic(env,hitClass,"testLocation",g.hitLocation.signature+2,location);
    staticBoolean(b.clsLiving,"testVisible",true);
    if(g.attackReach.name && *g.attackReach.name) { env->SetStaticDoubleField(b.clsPlayer,field(b.clsPlayer,"testReach","D",true),3); Healthy(env); }
    staticBoolean(b.client.minecraftClass,"testPaused",false);
    auto listClass=env->FindClass("java/util/ArrayList"); Healthy(env);
    auto list=NewObject(env,listClass);
    auto add=jvm::TryMethod(env,listClass,"add","(Ljava/lang/Object;)Z",false);
    env->CallBooleanMethod(list,add,entity); Healthy(env);
    SetStatic(env,b.clsLevel,"testIterable","Ljava/lang/Iterable;",list);
    mcov::CombatRequest request; request.revision=1; request.inputAllowed=true;
    request.config.enabled=request.config.attack.enabled=true;
    request.config.target.range=6; request.config.target.coneDegrees=1;
    request.config.target.mode=mcov::combat::TargetMode::Crosshair; // Live-Policy muss alte widersprüchliche Werte korrigieren.
    request.config.rotation.overshootFraction={0,0}; request.config.rotation.jitterDegrees={0,0};
    request.config.rotation.smoothing=4;
    request.config.rotation.yawDuration=request.config.rotation.pitchDuration={2,2};
    request.config.rotation.maxYawPerTick=request.config.rotation.maxPitchPerTick=.1;
    request.config.attack.firstClickDelay={.5,.5}; request.config.sync.interval={.2,.2};
    auto tick=jvm::TryMethod(env,b.client.minecraftClass,g.clientTick.name,g.clientTick.signature,false);
    mcov::CombatReport report;
    auto step=[&](int count=1,bool renew=true) {
        for(int i=0;i<count;++i) {
            if(renew) request.sentAtMs=GetTickCount64();
            block->combatRequest.Publish(request);
            staticBoolean(b.client.minecraftClass,"testVanillaTickDone",false);
            env->CallVoidMethod(client,tick); Healthy(env);
            Require(combatAfterVanillaTick,"combat ran before Vanilla movement/model updates");
            Require(block->combatReport.Read(report),"combat report tore");
            Require(report.status!=mcov::CombatBridgeStatus::Error,"client-tick bridge hit a JNI error");
            if(count>1) Sleep(25);
        }
    };
    // Ohne Preset/Option: schon die erste Aktivierung muss den Blick unverändert lassen.
    auto camera=NewObject(env,b.clsCamera);
    const std::string viewDesc="(L"+std::string(set.classes.entity)+";F)F";
    const std::string orbitDesc="(L"+std::string(set.classes.entity)+";F)D";
    auto cameraYaw=jvm::TryMethod(env,b.clsCamera,"testCameraYaw",viewDesc.c_str(),false);
    auto cameraPitch=jvm::TryMethod(env,b.clsCamera,"testCameraPitch",viewDesc.c_str(),false);
    auto orbitX=jvm::TryMethod(env,b.clsCamera,"testOrbitX",orbitDesc.c_str(),false);
    auto orbitZ=jvm::TryMethod(env,b.clsCamera,"testOrbitZ",orbitDesc.c_str(),false);
    auto view=[&](jmethodID method,float partial=.5f) { const auto v=env->CallFloatMethod(camera,method,player,partial); Healthy(env); return v; };
    auto orbit=[&](jmethodID method) { const auto v=env->CallDoubleMethod(camera,method,player,.5f); Healthy(env); return v; };
    step();
    Require(report.attacks==1 && report.debug.target && report.debug.target->id==42 &&
            report.debug.error<.001 && report.debug.packets.buffered==0,
            "old slow settings delayed first-tick live rotation/attack");
    // Ein bewegtes Ziel muss schon im nächsten Tick ohne neu startende Kurve
    // nachgeführt werden, auch wenn der alte Blick die Hitbox verfehlt.
    for(int i=0;i<4;++i) {
        const double x=i%2 ? 1.0 : -1.0;
        real(entity,b.clsEntity,"testX",x);
        env->SetDoubleField(bounds,b.geometry.min[0],x-.3); Healthy(env);
        env->SetDoubleField(bounds,b.geometry.max[0],x+.3); Healthy(env);
        step(); Require(report.debug.target && report.debug.target->id==42 && report.debug.error<.001,
                        "live moving-target rotation lagged behind hitbox");
    }
    step(13);
    Require(combatCallbackThread==GetCurrentThreadId() && report.tick>=18,"combat was not invoked synchronously by instrumented client tick");
    Require(report.attacks==1 && report.debug.target && report.debug.target->id==42,"Kill Aura did not select, rotate and attack automatically");
    const auto aimedYaw=env->CallFloatMethod(player,b.mEntityYaw); Healthy(env);
    const auto headYaw=env->GetFloatField(player,b.geometry.headYaw); Healthy(env);
    const auto modelYaw=env->GetFloatField(player,b.geometry.bodyYaw); Healthy(env);
    Require(std::abs(mcov::combat::Wrap(headYaw-aimedYaw))<.001 && std::abs(mcov::combat::Wrap(modelYaw-aimedYaw))<.001 &&
            report.debug.error<request.config.attack.alignmentDegrees,"F5 head/body angles did not face the selected target");
    Require(std::abs(view(cameraYaw))<.001f && std::abs(view(cameraPitch))<.001f &&
            std::abs(orbit(orbitX))<.001 && std::abs(orbit(orbitZ)-4)<.001,
            "Kill Aura default activation changed the camera without a preset");
    Require(!mcov::agent::RayBox({0,1.62,0},{0,0},combat::Entity{.boundsMin={.7,0,1.7},.boundsMax={1.3,1.8,2.3}}),"ray hit unrelated crosshair entity");
    const auto cooldown=env->GetFloatField(player,field(b.clsPlayer,"testCooldown","F")); Healthy(env);
    Require(cooldown==0,"real attack did not reset game cooldown");
    auto nearerTie=NewObject(env,b.clsPlayer);
    env->SetBooleanField(nearerTie,alive,JNI_TRUE); Healthy(env);
    env->SetIntField(nearerTie,field(b.clsEntity,"testId","I"),41); Healthy(env);
    floating(nearerTie,b.clsLiving,"testHealth",20);
    env->CallBooleanMethod(list,add,nearerTie); Healthy(env); step();
    Require(report.debug.target && report.debug.target->id==42,"valid live lock switched to a new ranked candidate");
    floating(entity,b.clsLiving,"testHealth",0); step();
    Require(report.debug.target && report.debug.target->id==41,"dead live lock was not released");
    floating(entity,b.clsLiving,"testHealth",20);
    auto clear=jvm::TryMethod(env,listClass,"clear","()V",false);
    env->CallVoidMethod(list,clear); Healthy(env);
    env->CallBooleanMethod(list,add,entity); Healthy(env); step();
    floating(player,b.clsPlayer,"testCooldown",1); step(4);
    Require(report.attacks==2,"game cooldown recovery did not allow another hit");
    const auto attacks=report.attacks;
    request.inputAllowed=false; floating(player,b.clsPlayer,"testCooldown",1); step(2);
    Require(report.attacks==attacks && report.status==mcov::CombatBridgeStatus::Paused,"input/menu pause still attacked");
    request.inputAllowed=true; request.sentAtMs=GetTickCount64()-mcov::kCombatLeaseMs-1; step(1,false);
    Require(report.attacks==attacks && report.status==mcov::CombatBridgeStatus::Paused,"expired overlay lease still attacked");
    auto screenName=std::string(set.fieldCurrentScreen.signature+1); screenName.pop_back();
    auto screen=NewObject(env,LoadClass(env,loader,screenName.c_str()));
    env->SetObjectField(screenOwner,b.fCurrentScreen,screen); Healthy(env); step(2);
    Require(report.attacks==attacks && report.status==mcov::CombatBridgeStatus::Paused,"Minecraft GUI still attacked");
    env->SetObjectField(screenOwner,b.fCurrentScreen,nullptr); Healthy(env);
    auto uuid=env->CallObjectMethod(entity,b.mEntityUuid); Healthy(env);
    auto uuidClass=env->FindClass("java/util/UUID"); Healthy(env);
    auto high=jvm::TryMethod(env,uuidClass,"getMostSignificantBits","()J",false);
    auto low=jvm::TryMethod(env,uuidClass,"getLeastSignificantBits","()J",false);
    const auto uuidHigh=env->CallLongMethod(uuid,high); Healthy(env);
    const auto uuidLow=env->CallLongMethod(uuid,low); Healthy(env);
    request.relations[0]={{static_cast<std::uint64_t>(uuidHigh),static_cast<std::uint64_t>(uuidLow)},mcov::combat::Relation::Friend,true};
    step(4); Require(report.attacks==attacks && !report.debug.target,"UUID friend was targeted");
    request.relations={}; staticBoolean(b.clsLiving,"testVisible",false); step(4);
    Require(report.attacks==attacks && !report.debug.target,"invisible/occluded target was attacked");
    request.config.target.requireVisibility=false; ++request.revision; step(12);
    Require(report.attacks==attacks,"disabling selector LOS bypassed the final attack sight check");
    staticBoolean(b.clsLiving,"testVisible",true);
    real(location,b.clsVec3,set.fieldVecZ.name,.3); step(6);
    Require(report.attacks==attacks,"block raycast allowed an attack through a wall");
    real(location,b.clsVec3,set.fieldVecZ.name,3);
    if(g.attackReach.name && *g.attackReach.name) {
        env->SetStaticDoubleField(b.clsPlayer,field(b.clsPlayer,"testReach","D",true),1); Healthy(env); step(4);
        Require(report.attacks==attacks,"game reach was exceeded");
        env->SetStaticDoubleField(b.clsPlayer,field(b.clsPlayer,"testReach","D",true),3); Healthy(env);
    }
    // Hot-Reload ändert das feste Silent-Verhalten nicht.
    floating(player,b.clsEntity,"testYaw",0); floating(player,b.clsEntity,"testPitch",0);
    floating(player,b.clsPlayer,"testCooldown",1);
    ++request.revision; step(18);
    Require(!report.debug.silentUnavailable && report.attacks==attacks+1,"silent camera blocked automatic rotation/attacks");
    Require(std::abs(view(cameraYaw))<.001f && std::abs(view(cameraPitch))<.001f,"combat rotation leaked into camera angles");
    Require(std::abs(orbit(orbitX))<.001 && std::abs(orbit(orbitZ)-4)<.001,"F5 camera position orbited with combat rotation");
    const auto bodyYaw=env->CallFloatMethod(player,b.mEntityYaw); Healthy(env);
    Require(std::abs(bodyYaw)>10,"silent mode did not rotate the real player/model/movement state");
    auto turn=jvm::TryMethod(env,b.clsEntity,g.turn.name,g.turn.signature,false);
    env->CallVoidMethod(player,turn,40.0,20.0); Healthy(env);
    const auto afterMouse=env->CallFloatMethod(player,b.mEntityYaw); Healthy(env);
    Require(std::abs(afterMouse-bodyYaw)<.001f && std::abs(view(cameraYaw)-6)<.001f && std::abs(view(cameraPitch)-3)<.001f,
            "mouse look changed combat rotation or failed to move independent camera");
    step(3);
    Require(std::abs(view(cameraYaw,0)-6)<.001f && std::abs(view(cameraYaw,1)-6)<.001f,"partial ticks caused silent-camera jitter");
    // Anderes Ziel auf der gegenüberliegenden Seite: View und F5-Orbit bleiben bei der Maus.
    auto nextEntity=NewObject(env,b.clsPlayer);
    env->SetBooleanField(nextEntity,alive,JNI_TRUE); Healthy(env);
    env->SetIntField(nextEntity,field(b.clsEntity,"testId","I"),43); Healthy(env);
    real(nextEntity,b.clsEntity,"testX",-1); real(nextEntity,b.clsEntity,"testZ",2);
    floating(nextEntity,b.clsLiving,"testHealth",20);
    env->SetDoubleField(bounds,b.geometry.min[0],-1.3); Healthy(env);
    env->SetDoubleField(bounds,b.geometry.max[0],-.7); Healthy(env);
    env->CallVoidMethod(list,clear); Healthy(env);
    env->CallBooleanMethod(list,add,nextEntity); Healthy(env);
    floating(player,b.clsPlayer,"testCooldown",1); step();
    const auto switchedYaw=env->CallFloatMethod(player,b.mEntityYaw); Healthy(env);
    const bool switched=report.debug.target && report.debug.target->id==43 && report.attacks==attacks+2 && switchedYaw>10 &&
        report.debug.error<request.config.attack.alignmentDegrees;
    if(!switched) std::cerr<<"switch diagnostics: yaw="<<switchedYaw<<" error="<<report.debug.error<<" target="
        <<(report.debug.target?report.debug.target->id:0)<<" attacks="<<report.attacks<<" expected="<<attacks+2<<'\n';
    Require(switched,"silent camera prevented aiming/attacking a new target");
    Require(std::abs(view(cameraYaw)-6)<.001f && std::abs(view(cameraPitch)-3)<.001f &&
            std::abs(orbit(orbitX)-4*std::sin(6/combat::kDegrees))<.001,
            "target switch moved the independent F5 camera");
    // Die globale Java-Bridge darf keine Kamera/Mouse-Deltas fremder Entities verändern.
    env->CallVoidMethod(nextEntity,turn,20.0,10.0); Healthy(env);
    const auto foreignYaw=env->CallFloatMethod(camera,cameraYaw,nextEntity,.5f); Healthy(env);
    Require(std::abs(foreignYaw-3)<.001f && std::abs(view(cameraYaw)-6)<.001f,"silent camera affected another entity");
    env->SetObjectField(screenOwner,b.fCurrentScreen,screen); Healthy(env); step();
    const auto pausedYaw=env->CallFloatMethod(player,b.mEntityYaw); Healthy(env);
    Require(report.status==mcov::CombatBridgeStatus::Paused && std::abs(pausedYaw-6)<.001f,
            "opening Minecraft GUI retained detached combat rotation");
    env->SetObjectField(screenOwner,b.fCurrentScreen,nullptr); Healthy(env); step(12);
    Require(std::abs(view(cameraYaw)-6)<.001f && std::abs(view(cameraPitch)-3)<.001f,"resuming after GUI changed the view");
    request.config.enabled=false; ++request.revision; step();
    const auto restoredYaw=env->CallFloatMethod(player,b.mEntityYaw); Healthy(env);
    Require(std::abs(restoredYaw-6)<.001f && std::abs(view(cameraPitch)-3)<.001f,"disabling silent camera snapped back to combat direction");
    const auto oldYaw=jvm::TryField(env,b.clsEntity,g.yawOld.name,g.yawOld.signature,false);
    const auto previousYaw=env->GetFloatField(player,oldYaw); Healthy(env);
    Require(std::abs(previousYaw-restoredYaw)<.001f,"restored rotation retained an old combat interpolation angle");
    const auto restoredHead=env->GetFloatField(player,b.geometry.headYaw); Healthy(env);
    const auto restoredBody=env->GetFloatField(player,b.geometry.bodyYaw); Healthy(env);
    Require(std::abs(mcov::combat::Wrap(restoredHead-restoredYaw))<.001 &&
            std::abs(mcov::combat::Wrap(restoredBody-restoredYaw))<.001,"stopping combat left stale model rotations");
    // Die normale Entity.turn-Methode arbeitet nach Abschalten wieder unverändert.
    env->CallVoidMethod(player,turn,20.0,0.0); Healthy(env);
    Require(std::abs(view(cameraYaw)-9)<.001f,"normal mouse look was not restored");
    // Spin nutzt dieselbe Kamera-Trennung, ohne Zielsuche oder Angriffe.
    const auto beforeSpinAttacks=report.attacks;
    request.spin.enabled=true; request.spin.direction=0;
    step(8);
    const auto spinningYaw=env->CallFloatMethod(player,b.mEntityYaw); Healthy(env);
    Require(report.spinStatus==mcov::combat::SpinStatus::Active&&report.attacks==beforeSpinAttacks&&
        std::abs(mcov::combat::Wrap(spinningYaw-9))>1&&std::abs(view(cameraYaw)-9)<.001f,
        "Spinbot failed to rotate independently without attacking");
    request.inputAllowed=false; step();
    const auto spinPausedYaw=env->CallFloatMethod(player,b.mEntityYaw); Healthy(env);
    Require(report.spinStatus==mcov::combat::SpinStatus::Paused&&std::abs(spinPausedYaw-9)<.001f,
        "Spinbot retained rotation after input pause");
    request.inputAllowed=true; step(2);
    request.config.enabled=true; ++request.revision;
    floating(player,b.clsPlayer,"testCooldown",0); step();
    Require(report.debug.target&&report.spinStatus==mcov::combat::SpinStatus::CombatPriority,
        "Spinbot overrode Kill Aura target rotation");
    request.config.enabled=false; request.spin.enabled=false; ++request.revision; step();
    Require(report.spinStatus==mcov::combat::SpinStatus::Disabled&&std::abs(view(cameraYaw)-9)<.001f,
        "Disabling Spinbot disturbed camera restoration");
    const auto afterSilentAttacks=report.attacks;
    request.config.enabled=true; ++request.revision;
    request.inputAllowed=false;
    env->SetObjectField(client,b.fPlayer,player); Healthy(env); step();
    request.inputAllowed=true;
    auto respawn=NewObject(env,LoadClass(env,loader,set.classes.localPlayer));
    env->SetBooleanField(respawn,alive,JNI_TRUE); Healthy(env);
    floating(respawn,b.clsPlayer,"testCooldown",0);
    env->SetObjectField(client,b.fPlayer,respawn); Healthy(env); step();
    Require(report.attacks==afterSilentAttacks && report.debug.cooldown==0,"respawn retained the previous attack cooldown");
    env->SetObjectField(client,b.fPlayer,nullptr); Healthy(env); step();
    Require(report.status==mcov::CombatBridgeStatus::NoWorld && report.attacks==afterSilentAttacks,"world exit retained attack target");
    Require(!bridge.Send({}),"JNI transport retained a borrowed tick context");
    bridge.Close(env); testCombat=nullptr;
    env->CallVoidMethod(client,tick); Healthy(env);
    // Fehlende Camera-Hooks müssen Silent sperren, statt heimlich sichtbar zu drehen.
    auto signal=env->FindClass("mcov/FrameSignal"); Healthy(env);
    const auto cameraBits=field(signal,"cameraProfiles","J",true);
    const auto savedBits=env->GetStaticLongField(signal,cameraBits); Healthy(env);
    env->SetStaticLongField(signal,cameraBits,0); Healthy(env);
    mcov::agent::MinecraftCombat unsupported;
    Require(unsupported.Install(env,b,block.get(),reinterpret_cast<void*>(&TestCombatTick)),"missing camera hook prevented diagnostic bridge installation");
    env->SetStaticLongField(signal,cameraBits,savedBits); Healthy(env);
    testCombat=&unsupported;
    env->SetObjectField(client,b.fPlayer,player); Healthy(env);
    ++request.revision;
    const auto beforeUnsupported=env->CallFloatMethod(player,b.mEntityYaw); Healthy(env);
    floating(player,b.clsPlayer,"testCooldown",1); step(4);
    const auto afterUnsupported=env->CallFloatMethod(player,b.mEntityYaw); Healthy(env);
    Require(report.status==mcov::CombatBridgeStatus::SilentUnavailable && report.debug.silentUnavailable &&
            report.attacks==0 && std::abs(afterUnsupported-beforeUnsupported)<.001f,
            "missing silent hooks allowed visible rotation/attacks");
    request.config.enabled=false; request.spin.enabled=true; ++request.revision; step(2);
    Require(report.spinStatus==mcov::combat::SpinStatus::SilentUnavailable&&
        std::abs(env->CallFloatMethod(player,b.mEntityYaw)-beforeUnsupported)<.001f,
        "Spinbot ignored a missing independent camera hook");
    Healthy(env);
    unsupported.Close(env); testCombat=nullptr;
    std::cout<<"PASS live Kill Aura "<<set.id<<" (client tick, cooldown, filters, silent camera, target switch, GUI, hook guard)"<<std::endl;
}
void TestInventoryAssist(JNIEnv* env,Bindings& b,jobject loader,jobject client) {
    jvm::LocalFrame testFrame(env,192); Require(testFrame.ok(),"inventory test local frame failed");
    using namespace mcov::items;
    const auto& set=*b.set; const auto& g=*b.profile->geometry;
    auto channel=std::make_unique<mcov::SharedBlock>();
    auto field=[&](jclass owner,const char* name,const char* desc,bool stat=false) {
        auto id=jvm::TryField(env,owner,name,desc,stat); Require(id!=nullptr,"inventory fixture field missing"); return id;
    };
    auto stack=[&](const char* key) {
        auto value=NewObject(env,b.clsItemStack);
        if(!key) env->SetBooleanField(value,field(b.clsItemStack,"testEmpty","Z"),JNI_TRUE);
        else {
            auto item=NewObject(env,b.clsItem);
            env->SetObjectField(item,field(b.clsItem,"testTranslation","Ljava/lang/String;"),env->NewStringUTF(key));
            Healthy(env);
            env->SetObjectField(value,field(b.clsItemStack,"testItem",set.methodStackGetItem.signature+2),item);
        }
        Healthy(env); return value;
    };
    auto player=NewObject(env,LoadClass(env,loader,set.classes.localPlayer));
    auto level=NewObject(env,b.clsLevel),manager=NewObject(env,b.clsInteractionMgr),inventory=NewObject(env,b.clsPlayerInv);
    env->SetObjectField(client,b.fPlayer,player); env->SetObjectField(client,b.fLevel,level);
    env->SetObjectField(client,b.fInteractionMgr,manager); env->SetObjectField(player,b.fInventory,inventory); Healthy(env);
    auto ownClass=LoadClass(env,loader,std::string(g.inventoryMenu.signature).substr(1,std::strlen(g.inventoryMenu.signature)-2).c_str());
    auto own=NewObject(env,ownClass);
    env->SetObjectField(player,field(b.clsPlayer,g.inventoryMenu.name,g.inventoryMenu.signature),own);
    env->SetObjectField(player,b.fScreenHandler,own); env->SetIntField(own,b.fSyncId,7); Healthy(env);
    env->SetFloatField(player,field(b.clsLiving,"testHealth","F"),20);
    env->SetStaticBooleanField(b.client.minecraftClass,field(b.client.minecraftClass,"testPaused","Z",true),JNI_FALSE);
    jobject screenOwner=b.fGui?env->GetObjectField(client,b.fGui):client;
    env->SetObjectField(screenOwner,b.fCurrentScreen,nullptr); Healthy(env);
    auto empty=stack(nullptr),totem=stack("item.minecraft.totem_of_undying");
    auto listClass=env->FindClass("java/util/ArrayList");
    auto list=NewObject(env,listClass);
    auto add=env->GetMethodID(listClass,"add","(Ljava/lang/Object;)Z");
    auto replace=env->GetMethodID(listClass,"set","(ILjava/lang/Object;)Ljava/lang/Object;");
    for(int i=0;i<36;++i) { env->CallBooleanMethod(list,add,i==10?totem:empty); Healthy(env); }
    env->SetObjectField(inventory,field(b.clsPlayerInv,"testStacks","Ljava/util/List;"),list); Healthy(env);
    const auto equipmentDesc=std::strchr(set.methodGetEquipped.signature,')')+1;
    SetStatic(env,b.clsLiving,"testEquipment",equipmentDesc,empty);
    auto menuClass=LoadClass(env,loader,std::string(set.fieldScreenHandler.signature).substr(1,std::strlen(set.fieldScreenHandler.signature)-2).c_str());
    SetStatic(env,menuClass,"testCarried",equipmentDesc,nullptr);
    env->SetStaticIntField(b.clsInteractionMgr,field(b.clsInteractionMgr,"testSwaps","I",true),0); Healthy(env);
    auto late=b; late.mInvGetStack=nullptr; late.mInvSize=nullptr;
    mcov::agent::InventoryAssist<Bindings> bridge;
    Require(bridge.Install(env,late,channel.get(),reinterpret_cast<void*>(&TestCombatTick),reinterpret_cast<void*>(&TestMacroUse)),"inventory client tick hook did not install");
    testInventory=&bridge;
    Request request; request.inputAllowed=true; request.totem.enabled=true;
    auto tick=[&] {
        request.sentAtMs=GetTickCount64(); channel->itemRequest.Publish(request);
        env->CallVoidMethod(client,jvm::TryMethod(env,b.client.minecraftClass,g.clientTick.name,g.clientTick.signature,false)); Healthy(env);
        Report report; Require(channel->itemReport.Read(report)&&report.sampledAtMs,"inventory report missing"); return report;
    };
    auto report=tick();
    Require(report.swaps==1&&report.totem==Status::Pending,"Auto Totem did not execute through client tick after late inventory bindings");
    Require(env->GetStaticIntField(b.clsInteractionMgr,field(b.clsInteractionMgr,"testSwapId","I",true))==7&&
            env->GetStaticIntField(b.clsInteractionMgr,field(b.clsInteractionMgr,"testSwapSlot","I",true))==10&&
            env->GetStaticIntField(b.clsInteractionMgr,field(b.clsInteractionMgr,"testSwapButton","I",true))==40,"wrong container/source/offhand button"); Healthy(env);
    report=tick(); Require(report.swaps==1&&report.totem==Status::HoldingTotem,"equipped totem was swapped repeatedly");
    SetStatic(env,b.clsLiving,"testEquipment",equipmentDesc,empty);
    SetStatic(env,menuClass,"testCarried",equipmentDesc,totem);
    report=tick(); Require(report.swaps==1&&report.totem==Status::InventoryBusy,"carried stack did not stop inventory swap");
    SetStatic(env,menuClass,"testCarried",equipmentDesc,nullptr);
    auto otherMenu=NewObject(env,menuClass); env->SetObjectField(player,b.fScreenHandler,otherMenu); Healthy(env);
    report=tick(); Require(report.swaps==1&&report.totem==Status::InventoryBusy,"external container allowed inventory swap");
    env->SetObjectField(player,b.fScreenHandler,own); Healthy(env);
    env->CallObjectMethod(list,replace,10,empty); Healthy(env);
    report=tick(); Require(report.totem==Status::NoTotem,"missing totem was not reported");
    request.totem.enabled=false; request.crystal.enabled=true; request.crystal.intervalMs=100;
    auto selected=field(b.clsPlayerInv,g.selectedSlot.name,g.selectedSlot.signature);
    auto options=NewObject(env,b.clsOptions); auto keyClass=LoadClass(env,loader,g.keyMappingClass);
    auto key=NewObject(env,keyClass);
    env->SetObjectField(client,b.fOptions,options);
    env->SetObjectField(options,field(b.clsOptions,g.useKey.name,g.useKey.signature),key); Healthy(env);
    auto down=[&](bool value) { env->SetBooleanField(key,field(keyClass,"testDown","Z"),value?JNI_TRUE:JNI_FALSE); Healthy(env); };
    auto useId=jvm::TryMethod(env,b.client.minecraftClass,g.useItem.name,g.useItem.signature,false);
    auto use=[&] { request.sentAtMs=GetTickCount64(); channel->itemRequest.Publish(request); env->CallVoidMethod(client,useId); Healthy(env); };
    auto usesId=field(b.client.minecraftClass,"testUses","I",true);
    auto siteUsesId=field(b.clsInteractionMgr,"testSiteUses","I",true);
    auto resultClass=LoadClass(env,loader,g.resultClass);
    env->SetStaticBooleanField(resultClass,field(resultClass,"testConsumes","Z",true),JNI_TRUE);
    if(g.blockReach.name&&*g.blockReach.name) env->SetStaticDoubleField(b.clsPlayer,field(b.clsPlayer,"testBlockReach","D",true),5);
    else env->SetStaticFloatField(b.clsInteractionMgr,field(b.clsInteractionMgr,"testBlockReach","F",true),5);
    if(g.attackReach.name&&*g.attackReach.name) env->SetStaticDoubleField(b.clsPlayer,field(b.clsPlayer,"testReach","D",true),3); Healthy(env);
    auto vec=[&](double x,double y,double z) { auto v=env->NewObject(b.clsVec3,jvm::TryMethod(env,b.clsVec3,g.vecCtor.name,g.vecCtor.signature,false),x,y,z); Healthy(env); return v; };
    SetStatic(env,b.clsEntity,"testEye",g.eyePosition.signature+2,vec(.5,65.6,2.5));
    auto posClass=LoadClass(env,loader,g.blockPosClass),hitClass=LoadClass(env,loader,g.blockHitClass);
    auto directionClass=LoadClass(env,loader,g.directionClass);
    auto up=env->GetStaticObjectField(directionClass,field(directionClass,g.upDirection.name,g.upDirection.signature,true)); Healthy(env);
    auto position=[&](int x,int y,int z) { auto p=env->NewObject(posClass,jvm::TryMethod(env,posClass,g.blockPosCtor.name,g.blockPosCtor.signature,false),x,y,z); Healthy(env); return p; };
    auto hitAt=[&](int x,int y,int z) {
        auto h=env->NewObject(hitClass,jvm::TryMethod(env,hitClass,g.blockHitCtor.name,g.blockHitCtor.signature,false),vec(x+.5,y+1.,z+.5),up,position(x,y,z),JNI_FALSE); Healthy(env);
        env->SetObjectField(client,field(b.client.minecraftClass,g.hitResult.name,g.hitResult.signature),h); Healthy(env); return h;
    };
    auto hitOwner=LoadClass(env,loader,g.hitClass);
    const auto typeDesc=std::strchr(g.hitType.signature,')')+1;
    auto typeClass=LoadClass(env,loader,std::string(typeDesc).substr(1,std::strlen(typeDesc)-2).c_str());
    SetStatic(env,hitOwner,"testHitType",typeDesc,NewObject(env,typeClass));
    auto state=NewObject(env,b.clsBlockState),block=NewObject(env,b.clsBlock);
    SetStatic(env,b.clsLevel,"testBlockState",std::strchr(set.methodGetBlockState.signature,')')+1,state);
    SetStatic(env,b.clsBlockState,"testBlock",std::strchr(set.methodStateGetBlock.signature,')')+1,block);
    auto blockKey=field(b.clsBlock,"testBlockKey","Ljava/lang/String;");
    env->SetObjectField(block,blockKey,env->NewStringUTF("block.minecraft.obsidian")); Healthy(env);
    auto makeState=[&](const char* name,bool replaceable) {
        auto st=NewObject(env,b.clsBlockState),bl=NewObject(env,b.clsBlock);
        env->SetObjectField(bl,blockKey,env->NewStringUTF(name));
        const std::string blockDesc="L"+std::string(set.classes.block)+";";
        env->SetObjectField(st,field(b.clsBlockState,"testOwnBlock",blockDesc.c_str()),bl);
        env->SetBooleanField(st,field(b.clsBlockState,"testReplaceable","Z"),replaceable?JNI_TRUE:JNI_FALSE); Healthy(env); return st;
    };
    auto air=makeState("block.minecraft.air",true),anchorState=makeState("block.minecraft.respawn_anchor",false);
    auto integer=env->FindClass("java/lang/Integer"); auto box=env->GetStaticMethodID(integer,"valueOf","(I)Ljava/lang/Integer;");
    auto mapClass=env->FindClass("java/util/HashMap"); auto states=NewObject(env,mapClass);
    auto put=env->GetMethodID(mapClass,"put","(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
    auto worldState=[&](int y,jobject st) { auto index=env->CallStaticObjectMethod(integer,box,y); Healthy(env); env->CallObjectMethod(states,put,index,st); Healthy(env); };
    worldState(64,state); worldState(65,air); SetStatic(env,b.clsLevel,"testStates","Ljava/util/Map;",states);
    auto entities=NewObject(env,listClass); auto clear=env->GetMethodID(listClass,"clear","()V");
    SetStatic(env,b.clsLevel,"testIterable","Ljava/lang/Iterable;",entities);
    env->SetObjectField(client,b.fTargetedEntity,nullptr); hitAt(0,64,0);
    env->CallObjectMethod(list,replace,2,stack("item.minecraft.end_crystal")); Healthy(env);
    env->SetIntField(inventory,selected,2); down(false);
    report=tick(); Require(report.crystal==Status::WaitingBind&&report.crystalActions==0,"armed macro ran without Minecraft Use");
    // Dieselben Schutz-Gates müssen im echten JNI-Hook alle Profile schützen.
    request.crystal.requireTotem=true;
    down(true); use(); report=tick();
    Require(report.crystal==Status::NeedTotem&&report.crystalActions==0&&env->GetStaticIntField(b.client.minecraftClass,usesId)==0,"totem guard leaked macro/Vanilla use"); Healthy(env);
    request.crystal.requireTotem=false; request.crystal.minHealth=6;
    env->SetFloatField(player,field(b.clsLiving,"testHealth","F"),5); Healthy(env);
    use(); report=tick(); Require(report.crystal==Status::LowHealth&&report.crystalActions==0,"native health guard ignored");
    env->SetFloatField(player,field(b.clsLiving,"testHealth","F"),20); Healthy(env);
    request.crystal.maxRange=1;
    use(); report=tick(); Require(report.crystal==Status::OutOfRange&&report.crystalActions==0,"native range limit ignored");
    request.crystal.maxRange=4.5f;
    request.crystal.requireCrosshairHit=true;
    down(true); use(); report=tick();
    Require(report.crystalActions==1&&env->GetStaticIntField(b.clsInteractionMgr,siteUsesId)==1,"captured crystal placement did not run"); Healthy(env);
    Require(env->GetStaticIntField(b.client.minecraftClass,usesId)==0,"macro placement leaked into vanilla held Use"); Healthy(env);
    Sleep(110); report=tick(); Require(report.crystalActions==1,"unconfirmed crystal spawn duplicated placement");
    auto crystal=NewObject(env,LoadClass(env,loader,g.crystalClass));
    env->SetDoubleField(crystal,field(b.clsEntity,"testX","D"),.5);
    env->SetDoubleField(crystal,field(b.clsEntity,"testY","D"),65);
    env->SetDoubleField(crystal,field(b.clsEntity,"testZ","D"),.5);
    env->SetIntField(crystal,field(b.clsEntity,"testId","I"),123);
    env->SetBooleanField(crystal,field(b.clsEntity,"testAlive","Z"),JNI_TRUE); Healthy(env);
    auto boundsClass=LoadClass(env,loader,g.boundsClass); auto bounds=NewObject(env,boundsClass);
    for(int axis=0;axis<3;++axis) {
        env->SetDoubleField(bounds,b.geometry.min[axis],axis==1?65:-.5);
        env->SetDoubleField(bounds,b.geometry.max[axis],axis==1?67:1.5);
    } Healthy(env);
    SetStatic(env,b.clsEntity,"testBounds",g.getBounds.signature+2,bounds);
    env->CallBooleanMethod(entities,add,crystal); Healthy(env);
    const int attacksBefore=env->GetStaticIntField(b.clsInteractionMgr,field(b.clsInteractionMgr,"testAttacks","I",true)); Healthy(env);
    use(); report=tick();
    Require(report.crystalActions==1&&report.crystal==Status::CrosshairMismatch,"strict crosshair check attacked an unpicked crystal");
    env->SetObjectField(client,b.fTargetedEntity,crystal); Healthy(env); use(); report=tick();
    Require(report.crystalActions==2&&env->GetStaticIntField(b.clsInteractionMgr,field(b.clsInteractionMgr,"testAttacks","I",true))==attacksBefore+1,"crystal at saved base not attacked without crosshair entity"); Healthy(env);
    Sleep(110); report=tick(); Require(report.crystalActions==2,"unconfirmed removal repeated attack");
    env->CallVoidMethod(entities,clear); env->SetObjectField(client,b.fTargetedEntity,nullptr); Healthy(env); report=tick();
    Require(report.crystalActions==3,"removed crystal not replaced at saved base");
    down(false); report=tick(); Require(env->GetIntField(inventory,selected)==2&&report.crystalActions==3,"Use release failed to stop crystal cycle"); Healthy(env);
    Require(combatCallbackThread==GetCurrentThreadId()&&combatAfterVanillaTick,"macro actions not after vanilla client tick");
    request.crystal.requireCrosshairHit=false;

    // Ablehnung durch das Spiel ist keine bestätigte Platzierung.
    down(true); env->SetStaticBooleanField(resultClass,field(resultClass,"testConsumes","Z",true),JNI_FALSE); Healthy(env);
    use(); report=tick(); Require(report.crystal==Status::Error&&report.crystalActions==3,"rejected use counted as placement");
    env->SetStaticBooleanField(resultClass,field(resultClass,"testConsumes","Z",true),JNI_TRUE); Healthy(env);
    Sleep(110); report=tick(); Require(report.crystalActions==4,"rejected placement could not retry");
    // Manuelle Slotwahl muss bleiben; ein fremdes Item wird Vanilla überlassen.
    env->SetIntField(inventory,selected,4); Healthy(env); report=tick();
    Require(env->GetIntField(inventory,selected)==4,"manual slot change was overwritten"); Healthy(env);
    const int vanillaBefore=env->GetStaticIntField(b.client.minecraftClass,usesId); Healthy(env);
    use(); Require(env->GetStaticIntField(b.client.minecraftClass,usesId)==vanillaBefore+1,"unrelated item use was intercepted"); Healthy(env);

    request.crystal.enabled=false; request.anchor.enabled=true; request.anchor.intervalMs=100;
    request.anchor.requireCrosshairHit=true;
    env->CallObjectMethod(list,replace,1,stack("block.minecraft.respawn_anchor")); Healthy(env);
    env->CallObjectMethod(list,replace,3,stack("block.minecraft.glowstone")); Healthy(env);
    env->CallObjectMethod(list,replace,4,stack("item.minecraft.stick")); Healthy(env);
    if(g.dimensionType.name&&*g.dimensionType.name) {
        const auto desc=std::strchr(g.dimensionType.signature,')')+1;
        SetStatic(env,b.clsLevel,"testDimension",desc,NewObject(env,LoadClass(env,loader,g.dimensionTypeClass)));
    } else {
        auto environmentClass=LoadClass(env,loader,g.environmentClass);
        SetStatic(env,b.clsLevel,"testEnvironment",std::strchr(g.environment.signature,')')+1,NewObject(env,environmentClass));
        auto boolean=env->FindClass("java/lang/Boolean");
        auto value=env->CallStaticObjectMethod(boolean,env->GetStaticMethodID(boolean,"valueOf","(Z)Ljava/lang/Boolean;"),JNI_FALSE); Healthy(env);
        SetStatic(env,environmentClass,"testAttribute","Ljava/lang/Object;",value);
    }
    auto charge=[&](int n) { auto value=env->CallStaticObjectMethod(integer,box,n); Healthy(env); SetStatic(env,b.clsBlockState,"testStateValue","Ljava/lang/Comparable;",value); };
    env->SetIntField(inventory,selected,1); down(true); hitAt(0,64,0); report=tick(); use(); report=tick();
    Require(report.anchorActions==1,"anchor placement JNI path failed");
    worldState(65,anchorState); charge(0); hitAt(0,65,0); Sleep(110); use(); report=tick();
    Require(report.anchorActions==2&&env->GetIntField(inventory,selected)==3,"anchor charge without changing crosshair failed"); Healthy(env);
    auto lastHit=env->GetStaticObjectField(b.clsInteractionMgr,field(b.clsInteractionMgr,"testLastHit",("L"+std::string(g.blockHitClass)+";").c_str(),true)); Healthy(env);
    auto actualHit=env->GetObjectField(client,field(b.client.minecraftClass,g.hitResult.name,g.hitResult.signature)); Healthy(env);
    Require(env->IsSameObject(lastHit,actualHit),"strict anchor interaction did not preserve actual Minecraft hit/face");
    auto lastPos=env->CallObjectMethod(lastHit,jvm::TryMethod(env,hitClass,g.blockHitPos.name,g.blockHitPos.signature,false)); Healthy(env);
    Require(env->CallIntMethod(lastPos,jvm::TryMethod(env,posClass,g.posY.name,g.posY.signature,false))==65,"anchor interaction used placement support instead of saved anchor cell"); Healthy(env);
    charge(1); Sleep(110); use(); report=tick();
    Require(report.anchorActions==3&&env->GetIntField(inventory,selected)!=3,"anchor detonation without changing crosshair failed"); Healthy(env);
    Sleep(110); report=tick(); Require(report.anchorActions==3,"unconfirmed anchor removal repeated detonation");
    worldState(65,air); hitAt(0,64,0); report=tick(); Require(report.anchorActions==4,"exploded anchor not replaced automatically");
    down(false); report=tick(); Require(env->GetIntField(inventory,selected)==1,"anchor release did not restore selected anchor"); Healthy(env);
    request.anchor.requireCrosshairHit=false;

    // Vorhandene Anchor werden geladen; in Respawn-Dimensionen bleiben normale Uses erhalten.
    worldState(65,anchorState); charge(0); hitAt(0,65,0); down(true); use(); report=tick();
    Require(report.anchorActions==5&&env->GetIntField(inventory,selected)==3,"existing anchor could not start cycle"); Healthy(env);
    env->SetStaticBooleanField(b.client.minecraftClass,field(b.client.minecraftClass,"testPaused","Z",true),JNI_TRUE); Healthy(env);
    report=tick(); Require(report.anchor==Status::Paused&&env->GetIntField(inventory,selected)==3,"pause must defer restore without game action"); Healthy(env);
    env->SetStaticBooleanField(b.client.minecraftClass,field(b.client.minecraftClass,"testPaused","Z",true),JNI_FALSE); Healthy(env);
    use(); report=tick(); Require(report.anchorActions==5&&env->GetIntField(inventory,selected)==1,"closing pause must restore before allowing held Vanilla Use"); Healthy(env);
    use(); report=tick(); Require(report.anchorActions==6,"anchor cycle could not restart after deferred restore");
    if(g.dimensionType.name&&*g.dimensionType.name) {
        auto dimensionClass=LoadClass(env,loader,g.dimensionTypeClass);
        auto dimension=env->GetStaticObjectField(b.clsLevel,field(b.clsLevel,"testDimension",std::strchr(g.dimensionType.signature,')')+1,true)); Healthy(env);
        env->SetBooleanField(dimension,field(dimensionClass,"testAnchorWorks","Z"),JNI_TRUE); Healthy(env);
    } else {
        auto boolean=env->FindClass("java/lang/Boolean");
        auto value=env->CallStaticObjectMethod(boolean,env->GetStaticMethodID(boolean,"valueOf","(Z)Ljava/lang/Boolean;"),JNI_TRUE); Healthy(env);
        SetStatic(env,LoadClass(env,loader,g.environmentClass),"testAttribute","Ljava/lang/Object;",value);
    }
    charge(1); report=tick(); Require(report.anchor==Status::AnchorSafeDimension&&report.anchorActions==6,"native dimension guard allowed detonation");
    down(false); tick(); Require(env->GetIntField(inventory,selected)==1,"paused anchor release failed to restore"); Healthy(env);
    down(true); const int normal=env->GetStaticIntField(b.client.minecraftClass,usesId); Healthy(env);
    use(); Require(env->GetStaticIntField(b.client.minecraftClass,usesId)==normal+1,"respawn anchor ordinary Use was intercepted"); Healthy(env);
    SetStatic(env,b.clsLevel,"testStates","Ljava/util/Map;",nullptr);
    bridge.Close(env); testInventory=nullptr;
    Require(EnsureChunkBindings(env,b),"deepslate axis property did not bind");
    SetStatic(env,b.clsBlockState,"testStateValue","Ljava/lang/Comparable;",env->NewStringUTF("x"));
    Require(IsRotatedDeepslate(env,b,state),"horizontal deepslate axis JNI read failed");
    SetStatic(env,b.clsBlockState,"testStateValue","Ljava/lang/Comparable;",env->NewStringUTF("y"));
    Require(!IsRotatedDeepslate(env,b,state),"vertical deepslate axis became a hint"); Healthy(env);
    env->SetObjectField(block,blockKey,env->NewStringUTF("block.minecraft.deepslate")); Healthy(env);
    BlockKindCache cache; BlockScanner scanner; mcov::WorldSnapshot center{}; center.posY=64;
    SetStatic(env,b.clsBlockState,"testStateValue","Ljava/lang/Comparable;",env->NewStringUTF("x"));
    scanner.Begin(4,1u<<mcov::kBlockRotatedDeepslate,center);
    for(int step=0;step<1000&&scanner.running;++step) scanner.Step(env,b,client,cache,1);
    Require(!scanner.running&&scanner.accum.size()==729,"resumable scanner lost horizontal deepslate results");
    SetStatic(env,b.clsBlockState,"testStateValue","Ljava/lang/Comparable;",env->NewStringUTF("y"));
    scanner.Begin(4,1u<<mcov::kBlockRotatedDeepslate,center);
    for(int step=0;step<1000&&scanner.running;++step) scanner.Step(env,b,client,cache,1);
    Require(!scanner.running&&scanner.accum.empty(),"scanner retained vertical deepslate hints");
    cache.Release(env); Healthy(env);
    std::cout<<"PASS inventory assist "<<set.id<<" (client tick, late binding, SWAP, cursor/container guard, game Use, fixed crystal/anchor cycles, world confirmation/dimension, deepslate axis)"<<std::endl;
}
void TestDetailsCache() {
    mcov::EntityDetailsCache cache;
    mcov::EntityDetailsSnapshot data; data.ping=87;
    cache.Put(42,1000,data);
    Require(cache.Find(42,1100) && cache.Find(42,1100)->ping==87,"entity detail cache lost its ID binding");
    Require(!cache.Find(43,1100) && !cache.Find(42,999) && !cache.Find(42,6001),"invalid or stale entity details survived");
    cache.Clear(); Require(!cache.Find(42,1100),"world reset retained entity details");
}
void TestEntityDetails(JNIEnv* env,const Bindings& b,jobject loader,jobject client) {
    const auto& set=*b.set;
    Require(b.mConnection && b.mEntityUuid && b.mUuidHigh && b.mUuidLow && b.mPlayerInfo && b.mLatency && b.mEntityById,
            "entity ping/detail methods did not bind");
    auto connectionClass=LoadClass(env,loader,set.classes.connection);
    auto infoClass=LoadClass(env,loader,set.classes.playerInfo);
    auto connection=NewObject(env,connectionClass),info=NewObject(env,infoClass);
    auto entity=NewObject(env,b.clsPlayer),world=NewObject(env,b.clsLevel);
    auto stack=NewObject(env,b.clsItemStack),item=NewObject(env,b.clsItem);
    SetStatic(env,b.client.minecraftClass,"testConnection",set.methodConnection.signature+2,connection);
    SetStatic(env,connectionClass,"testPlayerInfo",(std::string("L")+set.classes.playerInfo+";").c_str(),info);
    SetStatic(env,b.clsLevel,"testEntity",(std::string("L")+set.classes.entity+";").c_str(),entity);
    SetStatic(env,b.clsLiving,"testEquipment",std::strchr(set.methodGetEquipped.signature,')')+1,stack);
    env->SetObjectField(stack,env->GetFieldID(b.clsItemStack,"testItem",set.methodStackGetItem.signature+2),item); Healthy(env);
    for (auto slot:b.slotConstants) Require(slot!=nullptr,"equipment enum singleton was not mapped");
    const auto data=SampleDetails(env,b,entity,connection); Healthy(env);
    Require(data.ping==87 && data.armorValue==18,"per-player ping or armor value incorrect");
    for (const auto& gear:data.equipment) {
        Require(gear.present && gear.damage==17 && gear.maxDamage==100 && gear.count==3,"equipment slot/durability/count incorrect");
        Require(gear.iconIndex && gear.iconIndex==mcov::equipment_icons::Find("item.minecraft.diamond_helmet"),"equipment icon identity incorrect");
        Require(std::strcmp(gear.name,"Diamond Helmet")==0,"equipment display name incorrect");
    }
    Require(ReadPlayerPing(env,b,entity,nullptr)==-1,"missing player info produced a fabricated ping");
    EntityDetailsState state; state.ObserveWorld(env,world);
    const auto first=state.identities.Read(env,entity,42,b.mEntityUuid,b.mUuidHigh,b.mUuidLow); Healthy(env);
    Require(first.high||first.low,"player UUID was not sampled");
    auto replacement=NewObject(env,b.clsPlayer);
    auto otherUuid=env->CallObjectMethod(replacement,b.mEntityUuid); Healthy(env);
    auto uuidField=jvm::TryField(env,b.clsEntity,"testUuid","Ljava/util/UUID;",false);
    Require(uuidField!=nullptr,"fixture UUID field missing");
    env->SetObjectField(entity,uuidField,otherUuid); Healthy(env);
    Require(state.identities.Read(env,entity,42,b.mEntityUuid,b.mUuidHigh,b.mUuidLow)==first,
            "unchanged Java entity re-read its immutable UUID"); Healthy(env);
    const auto second=state.identities.Read(env,replacement,42,b.mEntityUuid,b.mUuidHigh,b.mUuidLow); Healthy(env);
    Require(second!=first && (second.high||second.low),"reused entity ID kept another player's friend UUID");
    env->DeleteLocalRef(otherUuid);
    auto listClass=env->FindClass("java/util/ArrayList"); Healthy(env);
    auto list=NewObject(env,listClass);
    auto add=jvm::TryMethod(env,listClass,"add","(Ljava/lang/Object;)Z",false);
    env->CallBooleanMethod(list,add,entity); Healthy(env);
    SetStatic(env,b.clsLevel,"testIterable","Ljava/lang/Iterable;",list);
    env->SetObjectField(client,b.fLevel,world); Healthy(env);
    mcov::WorldSnapshot geometry{};
    SampleEntities(env,b,client,nullptr,geometry,state); Healthy(env);
    Require(geometry.entityCount==1 && geometry.entities[0].uuid==second,
            "geometry IPC snapshot did not carry the player's real UUID");
    SampleEntities(env,b,client,entity,geometry,state); Healthy(env);
    Require(geometry.entityCount==1 && (geometry.entities[0].flags&mcov::kEntityIsSelf) &&
            geometry.entities[0].rigKind==mcov::kRigHumanoid,
            "own body was omitted, duplicated or lost its sampled head geometry");
    mcov::WorldSnapshot snapshot{}; snapshot.entityCount=1; snapshot.hasTarget=1; snapshot.targetEntityId=42;
    snapshot.entities[0].entityId=42; snapshot.entities[0].flags=mcov::kEntityIsLiving|mcov::kEntityIsPlayer;
    snapshot.entities[0].distance=5;
    SampleEntityDetails(env,b,client,entity,snapshot,state,true); Healthy(env);
    Require(snapshot.entities[0].details.ping==87 && state.cache.Find(42,GetTickCount64()),"slow detail pass did not publish/cache the target");
    auto nextWorld=NewObject(env,b.clsLevel); state.ObserveWorld(env,nextWorld);
    Require(!state.cache.Find(42,GetTickCount64()),"world change attached old items to a reused entity ID");
    Require(state.identities.Read(env,entity,42,b.mEntityUuid,b.mUuidHigh,b.mUuidLow)==second,
            "world change did not refresh cached UUIDs"); Healthy(env);
    state.Clear(env);
}
void TestConnectionLiveness() {
    mcov::AgentHeartbeat clock;
    Require(!clock.Alive(0), "unobserved channel reported alive");
    clock.Observe(1, 1000);
    // 5000 overlay frames without a geometry publication are normal while a
    // world loads. Worker heartbeat changes every 100 ms keep it connected.
    for (std::uint64_t ms = 1000; ms < 6000; ++ms) {
        clock.Observe(static_cast<std::uint32_t>(1 + (ms - 1000) / 100), ms);
        Require(clock.Alive(ms), "world loading or high overlay FPS disconnected a live worker");
    }
    Require(clock.Alive(7900) && !clock.Alive(7901), "heartbeat timeout is not based on elapsed milliseconds");
    clock.Observe(0xffffffffu, 8000);
    clock.Observe(0, 8100);
    Require(clock.Alive(8100) && clock.AgeMs(8100) == 0, "heartbeat wraparound was mistaken for a stall");
    std::cout << "PASS connection liveness across world loading/high FPS and actual timeout\n";
}
void Healthy(JNIEnv* env) {
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        throw std::runtime_error("unexpected Java exception");
    }
}
jobject NewObject(JNIEnv* env, jclass klass) {
    const auto ctor = jvm::TryMethod(env, klass, "<init>", "()V", false);
    Require(ctor != nullptr, "missing fixture constructor");
    jobject object = env->NewObject(klass, ctor);
    Healthy(env);
    Require(object != nullptr, "fixture allocation failed");
    return object;
}
jobject NewLoader(JNIEnv* env, const std::filesystem::path& jar) {
    jclass fileClass = env->FindClass("java/io/File"); Healthy(env);
    jmethodID fileCtor = jvm::TryMethod(env, fileClass, "<init>", "(Ljava/lang/String;)V", false);
    jstring path = env->NewStringUTF(jar.string().c_str()); Healthy(env);
    jobject file = env->NewObject(fileClass, fileCtor, path); Healthy(env);
    jmethodID toUri = jvm::TryMethod(env, fileClass, "toURI", "()Ljava/net/URI;", false);
    jobject uri = env->CallObjectMethod(file, toUri); Healthy(env);
    jclass uriClass = env->FindClass("java/net/URI"); Healthy(env);
    jmethodID toUrl = jvm::TryMethod(env, uriClass, "toURL", "()Ljava/net/URL;", false);
    jobject url = env->CallObjectMethod(uri, toUrl); Healthy(env);
    jclass urlClass = env->FindClass("java/net/URL"); Healthy(env);
    jobjectArray urls = env->NewObjectArray(1, urlClass, url); Healthy(env);
    jclass loaderClass = env->FindClass("java/net/URLClassLoader"); Healthy(env);
    jmethodID ctor = jvm::TryMethod(env, loaderClass, "<init>", "([Ljava/net/URL;Ljava/lang/ClassLoader;)V", false);
    jobject loader = env->NewObject(loaderClass, ctor, urls, nullptr); Healthy(env);
    return loader;
}
jclass LoadClass(JNIEnv* env, jobject loader, const char* name) {
    std::string dotted(name);
    std::replace(dotted.begin(), dotted.end(), '/', '.');
    jclass type = env->FindClass("java/lang/ClassLoader"); Healthy(env);
    jmethodID method = jvm::TryMethod(env, type, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;", false);
    jstring text = env->NewStringUTF(dotted.c_str()); Healthy(env);
    auto klass = static_cast<jclass>(env->CallObjectMethod(loader, method, text)); Healthy(env);
    return klass;
}
void SetStatic(JNIEnv* env, jclass owner, const char* name, const char* desc, jobject value) {
    jfieldID field = jvm::TryField(env, owner, name, desc, true);
    Require(field != nullptr, "missing fixture static field");
    env->SetStaticObjectField(owner, field, value); Healthy(env);
}
void TestGeometry(JNIEnv* env,const Bindings& b,jobject loader,jobject client,jobject camera,mcov::WorldSnapshot& snapshot) {
    const auto& g=b.geometry;
    const auto& profile=*b.profile->geometry;
    Require(g.getBounds&&g.min[0]&&g.min[1]&&g.min[2]&&g.max[0]&&g.max[1]&&g.max[2]&&
            g.old[0]&&g.old[1]&&g.old[2]&&g.bodyYaw&&g.bodyYawOld&&g.headPitch&&g.headPitchOld&&
            g.getPose&&g.isPassenger&&g.walkState&&g.walkSpeed&&g.walkPosition&&g.getDimensions&&g.dimensionHeight&&g.partialTick&&
            g.cameraEntity&&g.walkDistance&&g.walkDistanceOld&&g.bob&&g.bobOld,"geometry bindings incomplete");
    for (int i=0;i<5;++i) Require(g.poses[i]&&g.humanoids[i],"pose singleton or humanoid classifier missing");
    Require(snapshot.partialTick==.5f,"camera interpolation fraction incorrect");
    jobject entity=NewObject(env,LoadClass(env,loader,b.set->classes.localPlayer));
    jobject bounds=NewObject(env,g.bounds),dimensions=NewObject(env,g.dimensions),walk=NewObject(env,g.walk);
    const double current[]={10,64,20};
    const double old[]={8,64,18},lo[]={9.7,64,19.7},hi[]={10.3,65.5,20.3};
    for (int i=0;i<3;++i) {
        env->SetDoubleField(entity,g.old[i],old[i]); Healthy(env);
        env->SetDoubleField(bounds,g.min[i],lo[i]); Healthy(env);
        env->SetDoubleField(bounds,g.max[i],hi[i]); Healthy(env);
    }
    env->SetFloatField(entity,g.bodyYawOld,179); Healthy(env);
    env->SetFloatField(entity,g.bodyYaw,-179); Healthy(env);
    env->SetFloatField(entity,g.headPitchOld,0); Healthy(env);
    env->SetFloatField(dimensions,g.dimensionHeight,1.8f); Healthy(env);
    env->SetObjectField(entity,g.walkState,walk); Healthy(env);
    SetStatic(env,b.clsEntity,"testBounds",profile.getBounds.signature+2,bounds);
    SetStatic(env,b.clsEntity,"testPose",profile.getPose.signature+2,g.poses[mcov::kPoseCrouching]);
    SetStatic(env,b.clsEntity,"testDimensions",(std::string("L")+profile.dimensionsClass+";").c_str(),dimensions);
    mcov::EntitySnapshot record{};
    record.x=current[0];record.y=current[1];record.z=current[2];record.height=1.5f;record.width=.6f;
    record.flags=mcov::kEntityIsLiving|mcov::kEntityIsPlayer;
    SampleEntityGeometry(env,b,entity,snapshot.partialTick,record); Healthy(env);
    Require(record.x==9&&record.y==64&&record.z==19,"entity did not interpolate xOld/yOld/zOld");
    Require((record.flags&mcov::kEntityHasBounds)&&std::abs(record.boundsMinX+.3f)<.001f&&record.boundsMaxY==1.5f,
            "actual hitbox not relative to interpolated entity anchor");
    Require(record.yaw==180&&record.headPitch==5&&record.modelHeight==1.8f&&record.pose==mcov::kPoseCrouching&&
            record.walkPhase==1.5f&&record.walkSpeed==.5f&&record.rigKind==mcov::kRigHumanoid,"humanoid pose/animation sample incorrect");
    auto options=NewObject(env,b.clsOptions),option=NewObject(env,b.clsOptVal);
    env->SetObjectField(client,b.fOptions,options); Healthy(env);
    env->SetObjectField(options,b.fBobView,option); Healthy(env);
    auto trueField=jvm::TryField(env,b.clsBoolean,"TRUE","Ljava/lang/Boolean;",true);
    auto enabled=env->GetStaticObjectField(b.clsBoolean,trueField); Healthy(env);
    SetStatic(env,b.clsOptVal,"testValue","Ljava/lang/Object;",enabled);
    env->SetObjectField(camera,g.cameraEntity,entity); Healthy(env);
    jobject state=entity;
    if (g.avatarState) {
        auto klass=LoadClass(env,loader,profile.avatarStateClass);
        state=NewObject(env,klass);
        SetStatic(env,g.avatarOwner,"testAvatar",profile.avatarState.signature+2,state);
    }
    env->SetFloatField(state,g.walkDistance,2); Healthy(env);
    env->SetFloatField(state,g.walkDistanceOld,1); Healthy(env);
    env->SetFloatField(state,g.bob,.2f); Healthy(env);
    env->SetFloatField(state,g.bobOld,.1f); Healthy(env);
    SampleCamera(env,b,client,snapshot); Healthy(env);
    Require(snapshot.viewBobbing&&snapshot.bobPhase==-2.5f&&std::abs(snapshot.bobAmplitude-.15f)<.001f,
            "view bob interpolation or backwards walk phase incorrect");
    record={};record.flags=mcov::kEntityIsLiving;record.height=1.8f;
    auto plain=NewObject(env,b.clsLiving);
    SampleEntityGeometry(env,b,plain,.5f,record); Healthy(env);
    Require(record.rigKind==mcov::kRigNone,"non-humanoid received a humanoid rig");
}
void TestFrameSynchronization(JNIEnv* env,const Bindings& b,jobject loader,jobject client,jobject camera) {
    Require(b.geometry.runFrame&&b.geometry.updateCamera,"frame boundary methods did not bind");
    Require(g_frameSync.Install(env),"could not install Java frame markers");
    env->CallVoidMethod(client,b.geometry.runFrame,JNI_TRUE); Healthy(env);
    Require(!g_frameSync.Read(env).sequence,"entity tick was sampled against the previous camera");
    auto readyCamera=[&] {
        if (b.profile->geometry->partialSource==mcov::mappings::PartialTickSource::CameraDelta) {
            auto delta=NewObject(env,LoadClass(env,loader,b.profile->geometry->deltaClass));
            env->CallVoidMethod(camera,b.geometry.updateCamera,delta); Healthy(env);
        } else { env->CallVoidMethod(camera,b.geometry.updateCamera,nullptr,nullptr,JNI_FALSE,JNI_FALSE,.5f); Healthy(env); }
    };
    readyCamera();
    const auto first=g_frameSync.Read(env);
    Require(first.sequence&&first.cameraQpc>0&&g_frameSync.StillCurrent(env,first),"camera completion did not release the sampler");
    env->CallVoidMethod(client,b.geometry.runFrame,JNI_TRUE); Healthy(env);
    Require(!g_frameSync.StillCurrent(env,first),"a mixed snapshot survived a new game frame");
    readyCamera();
    const auto next=g_frameSync.Read(env);
    Require(next.sequence>first.sequence&&!g_frameSync.StillCurrent(env,first),"stale camera epoch was accepted");
    g_frameSync.Close(env);
}
void TestProfile(JNIEnv* env, const mcov::mappings::RuntimeProfile& profile,
                 const std::filesystem::path& fixtureDir, bool fieldOnlyCamera = false) {
    jvm::LocalFrame frame(env, 128);
    auto& runtime = jvm::Runtime::Instance();
    const std::string suffix = fieldOnlyCamera ? "-field-camera" : "";
    jobject loader = NewLoader(env, fixtureDir / (std::string(profile.set->id) + suffix + ".jar"));
    jclass clientClass = LoadClass(env, loader, profile.set->classes.client);
    int scans = 0;
    // Only the already-loaded client is exposed by the JVMTI stand-in. Every
    // other class must be resolved through its defining URLClassLoader.
    runtime.SetFallbackClassResolver([&](JNIEnv* e, const char* name) -> jclass {
        ++scans;
        return std::strcmp(name, profile.set->classes.client) == 0
            ? static_cast<jclass>(e->NewLocalRef(clientClass)) : nullptr;
    });
    auto wrongVersion = profile;
    wrongVersion.version = "unsupported-test-version";
    Bindings rejected;
    Require(!BindMappingSet(env, wrongVersion, rejected) && !rejected.ready,
            "binder accepted a different game version");
    Bindings bindings;
    Require(BindMappingSet(env, profile, bindings) && bindings.ready, "runtime profile did not bind");
    if (fieldOnlyCamera) Require(!bindings.mCamPos && bindings.fCamPos,
                                "field-only camera fixture did not exercise the accessor fallback");
    Require(scans == 1, "binder scanned for classes instead of using the game loader");
    Require(bindings.fCurrentScreen && (!*profile.guiClass || bindings.fGui), "screen owner mismatch");
    Require(bindings.mClickSlot && bindings.fInventory && bindings.fSyncId && bindings.mGetEquipped && bindings.mEffDuration,
            "optional module bindings regressed");
    const auto& set = *profile.set;
    jobject client = NewObject(env, clientClass);
    jobject renderer = NewObject(env, bindings.clsRenderer);
    jobject camera = NewObject(env, bindings.clsCamera);
    jobject position = NewObject(env, bindings.clsVec3);
    env->SetDoubleField(position, bindings.fVecX, 5.0); Healthy(env);
    env->SetDoubleField(position, bindings.fVecY, 73.0); Healthy(env);
    env->SetDoubleField(position, bindings.fVecZ, -8.0); Healthy(env);
    env->SetObjectField(client, bindings.fRenderer, renderer); Healthy(env);
    env->SetObjectField(camera, bindings.fCamPos, position); Healthy(env);
    SetStatic(env, clientClass, "testInstance", set.methodGetInstance.signature + 2, client);
    SetStatic(env, bindings.clsRenderer, "testCamera", set.methodGetCamera.signature + 2, camera);
    SetStatic(env, bindings.clsCamera, "testPosition", set.methodCameraPosition.signature + 2, position);
    jvm::Error error;
    Require(env->IsSameObject(jvm::demo::GetClientInstance(env, bindings.client, &error), client),
            "getInstance used the wrong namespace");
    mcov::WorldSnapshot snapshot{};
    SampleCamera(env, bindings, client, snapshot); Healthy(env);
    Require(snapshot.fovDeg == 82.5f && snapshot.yawDeg == 29.25f && snapshot.pitchDeg == -11.5f,
            "camera FOV return type or rotations are incorrect");
    Require(snapshot.camX == 5.0f && snapshot.camY == 73.0f && snapshot.camZ == -8.0f,
            "camera accessor returned wrong position");
    TestGeometry(env,bindings,loader,client,camera,snapshot);
    TestEntityDetails(env,bindings,loader,client);
    TestFrameSynchronization(env,bindings,loader,client,camera);
    TestLiveCombat(env,bindings,loader,client);
    TestInventoryAssist(env,bindings,loader,client);
    bindings.mCamPos = nullptr;
    snapshot = {};
    SampleCamera(env, bindings, client, snapshot); Healthy(env);
    Require(snapshot.camY == 73.0f && snapshot.fovDeg == 82.5f, "camera field fallback failed");
    for (auto slot : bindings.slotConstants) if (slot) env->DeleteGlobalRef(slot);
    if (bindings.swapAction) env->DeleteGlobalRef(bindings.swapAction);
    if (bindings.axisProperty) env->DeleteGlobalRef(bindings.axisProperty);
    bindings.geometry.Release(env);
    runtime.ReleaseCachedRefs(env);
    std::cout << "PASS " << set.id << suffix << '\n';
}
void TestJavaSelection(const std::filesystem::path& fixtures) {
    for (int major : {17, 21, 25}) {
        auto home = fixtures / ("jdk-test-" + std::to_string(major));
        std::filesystem::create_directories(home / "bin");
        std::ofstream(home / "release") << "JAVA_VERSION=\"" << major << ".0.1\"\nOS_ARCH=\"amd64\"\n";
        std::ofstream(home / "bin/java.exe");
        std::ofstream(home / "bin/attach.dll");
        Require(mcov::ReadJavaMajor(home) == major, "Java release manifest parsed incorrectly");
    }
    mcov::TargetProcess target;
    target.javaHome = (fixtures / "jdk-test-25").wstring();
    SetEnvironmentVariableW(L"MCOV_JDK_HOME", (fixtures / "jdk-test-17").c_str());
    std::string diagnostic;
    auto selected = mcov::FindAttachCapableJava(target, &diagnostic);
    Require(std::filesystem::path(selected) == fixtures / "jdk-test-25" / "bin" / "java.exe",
            "a Java 17 helper was selected for a Java 25 target");
    auto wrongArch = fixtures / "jdk-test-x86";
    std::filesystem::create_directories(wrongArch / "bin");
    std::ofstream(wrongArch / "release") << "JAVA_VERSION=\"25.0.1\"\nOS_ARCH=\"x86\"\n";
    std::ofstream(wrongArch / "bin/java.exe");
    std::ofstream(wrongArch / "bin/attach.dll");
    SetEnvironmentVariableW(L"MCOV_JDK_HOME", wrongArch.c_str());
    Require(std::filesystem::path(mcov::FindAttachCapableJava(target, &diagnostic)) ==
            fixtures / "jdk-test-25" / "bin" / "java.exe", "x86 helper was selected for an x64 agent");
    SetEnvironmentVariableW(L"MCOV_JDK_HOME", (fixtures / "jdk-test-17").c_str());
    target.javaHome = (fixtures / "jdk-test-17").wstring();
    Require(std::filesystem::path(mcov::FindAttachCapableJava(target, &diagnostic)) ==
            fixtures / "jdk-test-17" / "bin" / "java.exe", "matching Java 17 override was rejected");
    target.javaHome = (fixtures / "missing-runtime").wstring();
    Require(mcov::FindAttachCapableJava(target, &diagnostic).empty() && !diagnostic.empty(),
            "missing runtime version did not produce a diagnostic");
    std::cout << "PASS Java 17/21/25 helper selection\n";
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 3) { std::cerr << "usage: mcov_compat_check <jdk-home> <fixture-dir>\n"; return 64; }
    JavaVM* vm = nullptr;
    JNIEnv* env = nullptr;
    try {
        auto home = std::filesystem::absolute(argv[1]);
        auto fixtures = std::filesystem::absolute(argv[2]);
        TestConnectionLiveness();
        TestDetailsCache();
        TestJavaSelection(fixtures);
        SetDllDirectoryW((home / "bin").c_str());
        HMODULE jvm = LoadLibraryW((home / "bin/server/jvm.dll").c_str());
        Require(jvm != nullptr, "could not load test JVM");
        using CreateVm = jint(JNICALL*)(JavaVM**, void**, void*);
        auto create = reinterpret_cast<CreateVm>(GetProcAddress(jvm, "JNI_CreateJavaVM"));
        Require(create != nullptr, "test JVM has no JNI_CreateJavaVM");
        const auto syncJar=std::filesystem::absolute(argv[0]).parent_path()/"tools/frame-sync-agent.jar";
        std::string agentOption="-javaagent:"+syncJar.string();
        JavaVMOption options[2]{};
        options[0].optionString = const_cast<char*>("-Xcheck:jni");
        options[1].optionString = agentOption.data();
        JavaVMInitArgs args{};
        args.version = JNI_VERSION_1_8;
        args.nOptions = 2;
        args.options = options;
        Require(create(&vm, reinterpret_cast<void**>(&env), &args) == JNI_OK, "test JVM creation failed");
        jvm::Runtime::Instance().AdoptVm(vm);
        for (const auto& profile : mcov::mappings::kRuntimeProfiles) {
            TestProfile(env, profile, fixtures);
            TestProfile(env, profile, fixtures, true);
        }
        g_frameSync.Close(env);
        Require(vm->DestroyJavaVM() == JNI_OK, "test JVM shutdown failed");
        std::cout << "All JNI compatibility checks passed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        g_frameSync.Close(env);
        if (env) jvm::Runtime::Instance().ReleaseCachedRefs(env);
        if (vm) vm->DestroyJavaVM();
        return 1;
    }
}
