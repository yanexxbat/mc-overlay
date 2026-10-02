#pragma once
#include "geometry_profile.hpp"
#include "jvm_interface.hpp"

struct GeometryBindings {
    jclass bounds=nullptr,pose=nullptr,walk=nullptr,dimensions=nullptr,avatarOwner=nullptr;
    jclass humanoids[5]{};
    jmethodID getBounds=nullptr,headPitch=nullptr,getPose=nullptr,isPassenger=nullptr;
    jmethodID walkSpeed=nullptr,walkPosition=nullptr,getDimensions=nullptr,partialTick=nullptr,getDelta=nullptr,avatarState=nullptr;
    jmethodID runFrame=nullptr,updateCamera=nullptr;
    jfieldID min[3]{},max[3]{},old[3]{};
    jfieldID bodyYaw=nullptr,bodyYawOld=nullptr,headPitchOld=nullptr,walkState=nullptr,dimensionHeight=nullptr;
    jfieldID cameraEntity=nullptr,walkDistance=nullptr,walkDistanceOld=nullptr,bob=nullptr,bobOld=nullptr;
    jobject poses[5]{};
    void Release(JNIEnv* env) {
        for (auto& p:poses) if (p) { env->DeleteGlobalRef(p); p=nullptr; }
    }
};

inline GeometryBindings BindGeometry(JNIEnv* env,const mcov::mappings::RuntimeProfile& profile,
                                    jclass client,jclass entity,jclass living,jclass player,jclass camera) {
    GeometryBindings b;
    if (!profile.geometry) return b;
    const auto& g=*profile.geometry;
    auto cls=[&](const char* name) -> jclass {
        if (!name||!*name) return nullptr;
        const std::string key=std::string(profile.set->id)+".geometry."+name;
        jvm::Error ignored;
        return jvm::Runtime::Instance().ResolveClass(env,key.c_str(),{name},&ignored);
    };
    auto field=[&](jclass owner,mcov::mappings::Member m) { return jvm::TryField(env,owner,m.name,m.signature,false); };
    auto method=[&](jclass owner,mcov::mappings::Member m) { return jvm::TryMethod(env,owner,m.name,m.signature,false); };
    b.bounds=cls(g.boundsClass); b.pose=cls(g.poseClass); b.walk=cls(g.walkClass);
    b.dimensions=cls(g.dimensionsClass); b.avatarOwner=cls(g.avatarOwnerClass);
    auto avatar=cls(g.avatarStateClass);
    for (int i=0;i<5;++i) b.humanoids[i]=cls(g.humanoids[i]);
    b.getBounds=method(entity,g.getBounds);
    b.min[0]=field(b.bounds,g.minX); b.min[1]=field(b.bounds,g.minY); b.min[2]=field(b.bounds,g.minZ);
    b.max[0]=field(b.bounds,g.maxX); b.max[1]=field(b.bounds,g.maxY); b.max[2]=field(b.bounds,g.maxZ);
    b.old[0]=field(entity,g.oldX); b.old[1]=field(entity,g.oldY); b.old[2]=field(entity,g.oldZ);
    b.bodyYaw=field(living,g.bodyYaw); b.bodyYawOld=field(living,g.bodyYawOld);
    b.headPitch=method(entity,g.headPitch); b.headPitchOld=field(entity,g.headPitchOld);
    b.getPose=method(entity,g.getPose); b.isPassenger=method(entity,g.isPassenger);
    b.walkState=field(living,g.walkState); b.walkSpeed=method(b.walk,g.walkSpeed); b.walkPosition=method(b.walk,g.walkPosition);
    b.getDimensions=method(entity,g.getDimensions); b.dimensionHeight=field(b.dimensions,g.dimensionHeight);
    b.partialTick=method(g.partialSource==mcov::mappings::PartialTickSource::Client?client:camera,g.partialTick);
    b.getDelta=method(client,g.getDelta); b.cameraEntity=field(camera,g.cameraEntity);
    b.avatarState=method(b.avatarOwner,g.avatarState);
    b.runFrame=method(client,g.runFrame); b.updateCamera=method(camera,g.updateCamera);
    b.walkDistance=field(avatar?avatar:entity,g.walkDistance);
    b.walkDistanceOld=field(avatar?avatar:entity,g.walkDistanceOld);
    b.bob=field(avatar?avatar:player,g.bob); b.bobOld=field(avatar?avatar:player,g.bobOld);
    const mcov::mappings::Member poses[]={g.poseStanding,g.poseCrouch,g.poseSwim,g.poseGlide,g.poseSleep};
    for (int i=0;i<5;++i) {
        auto f=jvm::TryField(env,b.pose,poses[i].name,poses[i].signature,true);
        if (!f) continue;
        auto p=env->GetStaticObjectField(b.pose,f);
        if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
        if (p) { b.poses[i]=env->NewGlobalRef(p); env->DeleteLocalRef(p); }
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
    return b;
}
