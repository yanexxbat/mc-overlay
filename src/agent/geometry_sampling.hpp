#pragma once
// Included after Bindings and the scalar sampling helpers. This code runs only
// on the agent's sampler thread; it has no renderer/ImGui dependencies.
inline float GeometryFloat(JNIEnv* env,jobject object,jfieldID field,float fallback=0) {
    if (!object||!field) return fallback;
    const float value=env->GetFloatField(object,field);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return fallback; }
    return std::isfinite(value)?value:fallback;
}
inline double GeometryDouble(JNIEnv* env,jobject object,jfieldID field,double fallback=0) {
    if (!object||!field) return fallback;
    const double value=env->GetDoubleField(object,field);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return fallback; }
    return std::isfinite(value)?value:fallback;
}
inline float GeometryLerpAngle(float old,float current,float t) {
    return old+std::remainder(current-old,360.f)*t;
}
inline void SampleCameraGeometry(JNIEnv* env,const Bindings& b,jobject client,jobject camera,WorldSnapshot& s) {
    const auto& g=b.geometry;
    s.partialTick=1;
    if (g.partialTick) {
        float value=1;
        switch (b.profile->geometry->partialSource) {
            case mappings::PartialTickSource::Client: value=env->CallFloatMethod(client,g.partialTick); break;
            case mappings::PartialTickSource::Camera: value=env->CallFloatMethod(camera,g.partialTick); break;
            case mappings::PartialTickSource::CameraDelta: {
                jobject delta=g.getDelta?env->CallObjectMethod(client,g.getDelta):nullptr;
                if (env->ExceptionCheck()) { env->ExceptionClear(); break; }
                if (delta) { value=env->CallFloatMethod(camera,g.partialTick,delta); env->DeleteLocalRef(delta); }
                break;
            }
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
        else if (std::isfinite(value)) s.partialTick=std::clamp(value,0.f,1.f);
    }
    s.viewBobbing=0; s.bobPhase=s.bobAmplitude=0;
    if (!b.fOptions||!b.fBobView||!b.mOptGet) return;
    auto options=env->GetObjectField(client,b.fOptions);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    if (!options) return;
    auto option=env->GetObjectField(options,b.fBobView);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    if (!option) return;
    auto boxed=env->CallObjectMethod(option,b.mOptGet);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    if (!boxed||!b.clsBoolean||!b.mBooleanValue||!env->IsInstanceOf(boxed,b.clsBoolean)) return;
    const auto enabled=env->CallBooleanMethod(boxed,b.mBooleanValue);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    s.viewBobbing=enabled==JNI_TRUE;
    if (!s.viewBobbing||!g.cameraEntity) return;
    auto entity=env->GetObjectField(camera,g.cameraEntity);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    if (!entity) return;
    jobject state=entity;
    if (g.avatarState) {
        if (!g.avatarOwner||!env->IsInstanceOf(entity,g.avatarOwner)) return;
        state=env->CallObjectMethod(entity,g.avatarState);
        if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
        if (!state) return;
    } else if (!b.clsPlayer||!env->IsInstanceOf(entity,b.clsPlayer)) return;
    const float distance=GeometryFloat(env,state,g.walkDistance);
    const float previous=GeometryFloat(env,state,g.walkDistanceOld,distance);
    const float bob=GeometryFloat(env,state,g.bob);
    const float oldBob=GeometryFloat(env,state,g.bobOld,bob);
    s.bobPhase=-(distance+(distance-previous)*s.partialTick);
    s.bobAmplitude=oldBob+(bob-oldBob)*s.partialTick;
}
inline void SampleEntityGeometry(JNIEnv* env,const Bindings& b,jobject entity,float t,EntitySnapshot& e) {
    const auto& g=b.geometry;
    const double current[3]={e.x,e.y,e.z};
    double interpolated[3];
    for (int i=0;i<3;++i) {
        const double old=GeometryDouble(env,entity,g.old[i],current[i]);
        interpolated[i]=old+(current[i]-old)*t;
    }
    if (g.getBounds) {
        auto bounds=env->CallObjectMethod(entity,g.getBounds);
        if (env->ExceptionCheck()) env->ExceptionClear();
        else if (bounds) {
            double lo[3],hi[3]; bool valid=true;
            for (int i=0;i<3;++i) {
                lo[i]=GeometryDouble(env,bounds,g.min[i],current[i]);
                hi[i]=GeometryDouble(env,bounds,g.max[i],current[i]);
                valid=valid&&hi[i]>lo[i]&&hi[i]-lo[i]<128;
            }
            if (valid) {
                e.boundsMinX=static_cast<float>(lo[0]-current[0]); e.boundsMaxX=static_cast<float>(hi[0]-current[0]);
                e.boundsMinY=static_cast<float>(lo[1]-current[1]); e.boundsMaxY=static_cast<float>(hi[1]-current[1]);
                e.boundsMinZ=static_cast<float>(lo[2]-current[2]); e.boundsMaxZ=static_cast<float>(hi[2]-current[2]);
                e.flags|=kEntityHasBounds;
            }
            env->DeleteLocalRef(bounds);
        }
    }
    e.x=interpolated[0]; e.y=interpolated[1]; e.z=interpolated[2];
    e.modelHeight=e.height;
    if (!(e.flags&kEntityIsLiving)) return;
    if (e.flags&kEntityIsPlayer) e.rigKind=kRigHumanoid;
    else for (int i=0;i<5;++i) if (g.humanoids[i]&&env->IsInstanceOf(entity,g.humanoids[i])) {
        e.rigKind=i==3?kRigEnderman:kRigHumanoid; break;
    }
    if (!e.rigKind) return;
    const float yaw=GeometryFloat(env,entity,g.bodyYaw,e.yaw);
    e.yaw=GeometryLerpAngle(GeometryFloat(env,entity,g.bodyYawOld,yaw),yaw,t);
    const float pitch=CallFloat(env,entity,g.headPitch);
    e.headPitch=GeometryLerpAngle(GeometryFloat(env,entity,g.headPitchOld,pitch),pitch,t);
    if (g.getPose) {
        auto pose=env->CallObjectMethod(entity,g.getPose);
        if (env->ExceptionCheck()) env->ExceptionClear();
        else if (pose) {
            for (int i=1;i<5;++i) if (g.poses[i]&&env->IsSameObject(pose,g.poses[i])) { e.pose=static_cast<std::uint8_t>(i); break; }
            env->DeleteLocalRef(pose);
        }
    }
    if (g.getDimensions&&g.poses[0]) {
        auto dimensions=env->CallObjectMethod(entity,g.getDimensions,g.poses[0]);
        if (env->ExceptionCheck()) env->ExceptionClear();
        else if (dimensions) {
            const float h=GeometryFloat(env,dimensions,g.dimensionHeight,e.height);
            if (h>.05f&&h<32) e.modelHeight=h;
            env->DeleteLocalRef(dimensions);
        }
    }
    if (g.isPassenger) {
        const auto riding=env->CallBooleanMethod(entity,g.isPassenger);
        if (env->ExceptionCheck()) env->ExceptionClear();
        else if (riding==JNI_TRUE) e.rigFlags|=kRigRiding;
    }
    if (g.walkState&&g.walkSpeed&&g.walkPosition) {
        auto walk=env->GetObjectField(entity,g.walkState);
        if (env->ExceptionCheck()) env->ExceptionClear();
        else if (walk) {
            const float speed=env->CallFloatMethod(walk,g.walkSpeed,t);
            if (env->ExceptionCheck()) env->ExceptionClear();
            else if (std::isfinite(speed)) e.walkSpeed=std::clamp(speed,0.f,1.f);
            const float phase=env->CallFloatMethod(walk,g.walkPosition,t);
            if (env->ExceptionCheck()) env->ExceptionClear();
            else if (std::isfinite(phase)) e.walkPhase=phase;
            env->DeleteLocalRef(walk);
        }
    }
}
