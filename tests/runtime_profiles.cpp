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
    TestFrameSynchronization(env,bindings,loader,client,camera);
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
