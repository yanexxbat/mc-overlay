#pragma once
#include <jni.h>
#include <windows.h>
#include <cstdint>

// Only the sampler reads the Java agent's volatile frame epoch through JNI.
class FrameSync {
    jclass signal_=nullptr;
    jfieldID epoch_=nullptr;
    HANDLE timer_=nullptr;
public:
    struct Stamp { std::uint64_t sequence{}; std::int64_t cameraQpc{}; std::uint64_t rawSequence{}; };
    bool Install(JNIEnv* env) {
        // A 1 ms Sleep can round up to a whole display frame on Windows.
        // A per-worker high-resolution timer avoids changing the system timer.
        if (!timer_) timer_=::CreateWaitableTimerExW(nullptr,nullptr,0x2,TIMER_ALL_ACCESS);
        if (!timer_) timer_=::CreateWaitableTimerW(nullptr,FALSE,nullptr);
        auto local=env->FindClass("mcov/FrameSignal");
        if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
        if (!local) return false;
        const auto ready=env->GetStaticFieldID(local,"enabled","Z");
        if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(local); return false; }
        const auto active=env->GetStaticBooleanField(local,ready);
        if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(local); return false; }
        epoch_=env->GetStaticFieldID(local,"epoch","J");
        if (env->ExceptionCheck()) { env->ExceptionClear(); epoch_=nullptr; }
        if (active==JNI_TRUE&&epoch_) signal_=static_cast<jclass>(env->NewGlobalRef(local));
        if (env->ExceptionCheck()) { env->ExceptionClear(); signal_=nullptr; }
        env->DeleteLocalRef(local);
        return signal_!=nullptr;
    }
    bool enabled() const { return signal_!=nullptr; }
    Stamp Read(JNIEnv* env) const {
        if (!signal_) return {};
        const auto seq=env->GetStaticLongField(signal_,epoch_);
        if (env->ExceptionCheck()) { env->ExceptionClear(); return {}; }
        if (seq<=0||(seq&1)) return {0,0,static_cast<std::uint64_t>(seq)};
        LARGE_INTEGER now{}; ::QueryPerformanceCounter(&now);
        return {static_cast<std::uint64_t>(seq),now.QuadPart,static_cast<std::uint64_t>(seq)};
    }
    bool StillCurrent(JNIEnv* env,Stamp stamp) const {
        if (!signal_||!stamp.sequence) return false;
        const auto seq=env->GetStaticLongField(signal_,epoch_);
        if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
        return static_cast<std::uint64_t>(seq)==stamp.sequence;
    }
    void Wait(DWORD milliseconds=1) const {
        LARGE_INTEGER due{}; due.QuadPart=-static_cast<LONGLONG>(milliseconds)*10000;
        if (timer_&&::SetWaitableTimer(timer_,&due,0,nullptr,nullptr,FALSE)) ::WaitForSingleObject(timer_,milliseconds+50);
        else ::Sleep(milliseconds);
    }
    void Close(JNIEnv* env) {
        if (signal_&&env) env->DeleteGlobalRef(signal_);
        signal_=nullptr;epoch_=nullptr;
        if (timer_) { ::CloseHandle(timer_);timer_=nullptr; }
    }
};
