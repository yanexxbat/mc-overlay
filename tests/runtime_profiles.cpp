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
DWORD combatCallbackThread=0;
void JNICALL TestCombatTick(JNIEnv* env,jclass,jobject client) {
    combatCallbackThread=GetCurrentThreadId();
    if(testCombat) testCombat->Tick(env,client,false);
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
    request.config.target.range=3; request.config.target.coneDegrees=180;
    request.config.rotation.overshootFraction={0,0}; request.config.rotation.jitterDegrees={0,0};
    request.config.rotation.yawDuration=request.config.rotation.pitchDuration={.05,.05};
    request.config.attack.firstClickDelay={0,0}; request.config.sync.interval={.01,.01};
    auto tick=jvm::TryMethod(env,b.client.minecraftClass,g.clientTick.name,g.clientTick.signature,false);
    mcov::CombatReport report;
    auto step=[&](int count=1,bool renew=true) {
        for(int i=0;i<count;++i) {
            if(renew) request.sentAtMs=GetTickCount64();
            block->combatRequest.Publish(request);
            env->CallVoidMethod(client,tick); Healthy(env);
            Require(block->combatReport.Read(report),"combat report tore");
            Require(report.status!=mcov::CombatBridgeStatus::Error,"client-tick bridge hit a JNI error");
            if(count>1) Sleep(25);
        }
    };
    step(18);
    Require(combatCallbackThread==GetCurrentThreadId() && report.tick>=18,"combat was not invoked synchronously by instrumented client tick");
    Require(report.attacks==1 && report.debug.target && report.debug.target->id==42,"Kill Aura did not select, rotate and attack automatically");
    Require(!mcov::agent::RayBox({0,1.62,0},{0,0},combat::Entity{.boundsMin={.7,0,1.7},.boundsMax={1.3,1.8,2.3}}),"ray hit unrelated crosshair entity");
    const auto cooldown=env->GetFloatField(player,field(b.clsPlayer,"testCooldown","F")); Healthy(env);
    Require(cooldown==0,"real attack did not reset game cooldown");
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
    request.config.attack.silentRotation=true; ++request.revision; step(6);
    Require(report.attacks==attacks && report.debug.silentUnavailable,"unsupported silent rotation was used");
    request.config.attack.silentRotation=false; ++request.revision;
    request.inputAllowed=false;
    env->SetObjectField(client,b.fPlayer,player); Healthy(env); step();
    request.inputAllowed=true;
    auto respawn=NewObject(env,LoadClass(env,loader,set.classes.localPlayer));
    env->SetBooleanField(respawn,alive,JNI_TRUE); Healthy(env);
    floating(respawn,b.clsPlayer,"testCooldown",0);
    env->SetObjectField(client,b.fPlayer,respawn); Healthy(env); step();
    Require(report.attacks==attacks && report.debug.cooldown==0,"respawn retained the previous attack cooldown");
    env->SetObjectField(client,b.fPlayer,nullptr); Healthy(env); step();
    Require(report.status==mcov::CombatBridgeStatus::NoWorld && report.attacks==attacks,"world exit retained attack target");
    Require(!bridge.Send({}),"JNI transport retained a borrowed tick context");
    bridge.Close(env); testCombat=nullptr;
    env->CallVoidMethod(client,tick); Healthy(env);
    std::cout<<"PASS live Kill Aura "<<set.id<<" (client tick, cooldown, friends, LOS, GUI, lease, reach)"<<std::endl;
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
    Require(b.mConnection && b.mEntityUuid && b.mPlayerInfo && b.mLatency && b.mEntityById,
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
    SetStatic(env,b.clsItemStack,"testItem",set.methodStackGetItem.signature+2,item);
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
    mcov::WorldSnapshot snapshot{}; snapshot.entityCount=1; snapshot.hasTarget=1; snapshot.targetEntityId=42;
    snapshot.entities[0].entityId=42; snapshot.entities[0].flags=mcov::kEntityIsLiving|mcov::kEntityIsPlayer;
    snapshot.entities[0].distance=5;
    SampleEntityDetails(env,b,client,entity,snapshot,state,true); Healthy(env);
    Require(snapshot.entities[0].details.ping==87 && state.cache.Find(42,GetTickCount64()),"slow detail pass did not publish/cache the target");
    auto nextWorld=NewObject(env,b.clsLevel); state.ObserveWorld(env,nextWorld);
    Require(!state.cache.Find(42,GetTickCount64()),"world change attached old items to a reused entity ID");
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
    bindings.mCamPos = nullptr;
    snapshot = {};
    SampleCamera(env, bindings, client, snapshot); Healthy(env);
    Require(snapshot.camY == 73.0f && snapshot.fovDeg == 82.5f, "camera field fallback failed");
    for (auto slot : bindings.slotConstants) if (slot) env->DeleteGlobalRef(slot);
    if (bindings.swapAction) env->DeleteGlobalRef(bindings.swapAction);
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
