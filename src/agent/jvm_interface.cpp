#include "jvm_interface.hpp"

#include <windows.h>

#include <chrono>
#include <algorithm>
#include <thread>
#include <vector>

#include "../common/log.hpp"

namespace jvm {
namespace {

// Signature of the exported jvm.dll entry point. On Windows x64 there is no
// name decoration, so GetProcAddress takes the plain symbol.
using PFN_JNI_GetCreatedJavaVMs = jint(JNICALL*)(JavaVM**, jsize, jsize*);

std::string LastErrorText(const char* what) {
    const DWORD code = ::GetLastError();
    char        buffer[512] = {};
    ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                     nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                     buffer, static_cast<DWORD>(sizeof(buffer)), nullptr);
    std::string out = what;
    out += ": ";
    out += buffer;
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return out;
}

void SetError(Error* error, Status status, std::string message) {
    if (!error) return;
    error->status  = status;
    error->message = std::move(message);
}

} // namespace

// ---------------------------------------------------------------------------
// JNI version selection
// ---------------------------------------------------------------------------
jint PreferredJniVersion() noexcept {
#ifdef JNI_VERSION_21
    // JDK 21's jni.h; value is 0x00150000.
    return JNI_VERSION_21;
#elif defined(JNI_VERSION_10)
    return JNI_VERSION_10;
#else
    return JNI_VERSION_1_8;
#endif
}

jint FallbackJniVersion() noexcept { return JNI_VERSION_1_8; }

// ---------------------------------------------------------------------------
// Runtime
// ---------------------------------------------------------------------------
Runtime& Runtime::Instance() {
    static Runtime instance;
    return instance;
}

bool Runtime::ready() const noexcept {
    return vm() != nullptr && !shuttingDown();
}

void Runtime::AdoptVm(JavaVM* vm) noexcept {
    vm_.store(vm, std::memory_order_release);
    shutdown_.store(false, std::memory_order_release);
    MCOV_LOG_INFO("adopted JavaVM* %p from the agent entry point", static_cast<void*>(vm));
}

bool Runtime::DiscoverVm(const std::wstring& jvmDllPath, Error* error) {
    if (vm_.load(std::memory_order_acquire)) return true; // already adopted

    // Inside javaw.exe, jvm.dll is guaranteed to be loaded already, so prefer
    // GetModuleHandleW: it does not bump the refcount, which means we must not
    // (and do not) call FreeLibrary on it later.
    HMODULE module = ::GetModuleHandleW(L"jvm.dll");
    bool    owns   = false;

    if (!module && !jvmDllPath.empty()) {
        // LOAD_WITH_ALTERED_SEARCH_PATH makes the loader resolve jvm.dll's own
        // dependencies (vcruntime, jimage, etc.) relative to *its* directory
        // rather than ours. Without it the load fails with a cryptic 0x7E on
        // most JDK layouts.
        module = ::LoadLibraryExW(jvmDllPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        owns   = (module != nullptr);
    }

    if (!module) {
        SetError(error, Status::JvmModuleNotFound,
                 LastErrorText("could not obtain a handle to jvm.dll"));
        return false;
    }

    auto getCreatedVms = reinterpret_cast<PFN_JNI_GetCreatedJavaVMs>(
        ::GetProcAddress(module, "JNI_GetCreatedJavaVMs"));
    if (!getCreatedVms) {
        SetError(error, Status::EntryPointMissing,
                 LastErrorText("jvm.dll has no JNI_GetCreatedJavaVMs export"));
        if (owns) ::FreeLibrary(module);
        return false;
    }

    // HotSpot only ever supports one VM per process, but the API is written for
    // a list, so we ask for a small array and take the first entry.
    JavaVM* vms[4] = {};
    jsize   count  = 0;
    const jint rc = getCreatedVms(vms, 4, &count);

    if (rc != JNI_OK || count == 0 || vms[0] == nullptr) {
        SetError(error, Status::NoVmCreated,
                 "JNI_GetCreatedJavaVMs reported no live VM. This is the expected "
                 "result when the calling process is not itself the JVM - the API "
                 "only sees VMs in the current address space.");
        if (owns) ::FreeLibrary(module);
        return false;
    }

    jvmModule_     = module;
    ownsJvmModule_ = owns;
    vm_.store(vms[0], std::memory_order_release);

    MCOV_LOG_INFO("JNI_GetCreatedJavaVMs -> %d VM(s), using %p",
                  static_cast<int>(count), static_cast<void*>(vms[0]));
    return true;
}

void Runtime::BeginShutdown() noexcept {
    shutdown_.store(true, std::memory_order_release);
}

bool Runtime::WaitForThreadsDetached(unsigned timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (attached_.load(std::memory_order_acquire) > 0) {
        if (std::chrono::steady_clock::now() >= deadline) {
            MCOV_LOG_ERROR("timed out with %d thread(s) still attached to the JVM; "
                           "unloading now would crash the VM",
                           attached_.load(std::memory_order_acquire));
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return true;
}

void Runtime::SetFallbackClassResolver(ClassResolver resolver) {
    std::lock_guard<std::mutex> guard(classMutex_);
    fallbackResolver_ = std::move(resolver);
}

bool Runtime::SetGameClassLoader(JNIEnv* env, jclass clientClass) {
    if (!env || !clientClass) return false;
    LocalFrame frame(env, 8);
    if (!frame.ok()) return false;
    jclass classType = env->FindClass("java/lang/Class");
    if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
    jmethodID getLoader = TryMethod(env, classType, "getClassLoader", "()Ljava/lang/ClassLoader;", false);
    if (!getLoader) return false;
    jobject loader = env->CallObjectMethod(clientClass, getLoader);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
    if (!loader) return false;
    jclass loaderType = env->FindClass("java/lang/ClassLoader");
    if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
    jmethodID load = TryMethod(env, loaderType, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;", false);
    if (!load) return false;
    jobject global = env->NewGlobalRef(loader);
    if (!global) { if (env->ExceptionCheck()) env->ExceptionClear(); return false; }
    std::lock_guard<std::mutex> guard(classMutex_);
    if (gameClassLoader_) env->DeleteGlobalRef(gameClassLoader_);
    gameClassLoader_ = global;
    loadClass_ = load;
    return true;
}

jclass Runtime::ResolveClass(JNIEnv*                            env,
                             const char*                        cacheKey,
                             std::initializer_list<const char*> candidates,
                             Error*                             error) {
    if (!env || !cacheKey) {
        SetError(error, Status::ClassNotFound, "ResolveClass called with null arguments");
        return nullptr;
    }

    LocalFrame frame(env, 16);
    if (!frame.ok()) return nullptr;
    ClassResolver fallback;
    jobject loader = nullptr;
    jmethodID load = nullptr;
    {
        std::lock_guard<std::mutex> guard(classMutex_);
        if (auto it = classCache_.find(cacheKey); it != classCache_.end()) return it->second;
        fallback = fallbackResolver_;
        if (gameClassLoader_) loader = env->NewLocalRef(gameClassLoader_);
        load = loadClass_;
    }
    if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }

    std::string attempted;

    for (const char* name : candidates) {
        if (!name || !*name) continue;
        if (!attempted.empty()) attempted += ", ";
        attempted += name;

        // Attempt 1: plain FindClass. Works for bootstrap classes (java/lang/*)
        // and for anything visible to the system loader. On a natively attached
        // thread it will NOT find Minecraft's classes under a mod loader.
        jclass local = nullptr;
        // Prefer the defining game loader once known. loadClass accepts dots
        // and does not initialize classes; GetMethodID/FieldID handles that.
        if (loader && load) {
            std::string dotted(name);
            std::replace(dotted.begin(), dotted.end(), '/', '.');
            jstring className = env->NewStringUTF(dotted.c_str());
            if (!className) { if (env->ExceptionCheck()) env->ExceptionClear(); continue; }
            local = static_cast<jclass>(env->CallObjectMethod(loader, load, className));
            env->DeleteLocalRef(className);
            if (env->ExceptionCheck()) { env->ExceptionClear(); local = nullptr; }
        }
        if (!local) local = env->FindClass(name);
        if (env->ExceptionCheck()) {
            env->ExceptionClear(); // NoClassDefFoundError is the expected miss
            local = nullptr;
        }

        // Attempt 2: the JVMTI-backed resolver the agent installed, which scans
        // classes already loaded by *any* loader.
        if (!local && fallback) {
            local = fallback(env, name);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                local = nullptr;
            }
        }

        if (!local) continue;

        // A local ref dies when the current JNI frame unwinds; the cache has to
        // hold a global ref to stay valid across sampler iterations.
        auto global = static_cast<jclass>(env->NewGlobalRef(local));
        env->DeleteLocalRef(local);
        if (!global) { if (env->ExceptionCheck()) env->ExceptionClear(); continue; }

        {
            std::lock_guard<std::mutex> guard(classMutex_);
            // Another thread may have won the race while we were unlocked.
            if (auto it = classCache_.find(cacheKey); it != classCache_.end()) {
                env->DeleteGlobalRef(global);
                return it->second;
            }
            classCache_.emplace(cacheKey, global);
        }

        MCOV_LOG_INFO("resolved class '%s' -> %s", cacheKey, name);
        return global;
    }

    SetError(error, Status::ClassNotFound,
             std::string("none of these class names resolved: ") + attempted);
    return nullptr;
}

void Runtime::ReleaseCachedRefs(JNIEnv* env) {
    std::lock_guard<std::mutex> guard(classMutex_);
    if (env) {
        for (auto& [key, klass] : classCache_) {
            if (klass) env->DeleteGlobalRef(klass);
        }
        if (gameClassLoader_) env->DeleteGlobalRef(gameClassLoader_);
    } else if (!classCache_.empty()) {
        // Leaking is strictly better than deleting a global ref without a valid
        // env - the latter corrupts the VM's ref table.
        MCOV_LOG_WARN("ReleaseCachedRefs called without a JNIEnv; leaking %zu global ref(s)",
                      classCache_.size());
    }
    classCache_.clear();
    gameClassLoader_ = nullptr;
    loadClass_ = nullptr;
    fallbackResolver_ = nullptr;
}

// ---------------------------------------------------------------------------
// ScopedEnv
// ---------------------------------------------------------------------------
ScopedEnv::ScopedEnv(const char* threadName) {
    JavaVM* vm = Runtime::Instance().vm();
    if (!vm) return;

    void* raw = nullptr;

    // Step 1: are we already attached? If GetEnv succeeds the thread belongs to
    // someone else (the JVM created it, or an outer ScopedEnv owns it) and we
    // must borrow it without ever detaching.
    jint rc = vm->GetEnv(&raw, PreferredJniVersion());
    if (rc == JNI_EVERSION) {
        rc = vm->GetEnv(&raw, FallbackJniVersion());
    }

    if (rc == JNI_OK && raw) {
        env_  = static_cast<JNIEnv*>(raw);
        owns_ = false;
        return;
    }

    if (rc != JNI_EDETACHED) {
        MCOV_LOG_ERROR("GetEnv failed with %d (not JNI_EDETACHED); refusing to attach", rc);
        return;
    }

    // Step 2: attach as a *daemon*. A plain AttachCurrentThread would register a
    // non-daemon thread, and the JVM refuses to exit while one exists - the game
    // would hang at the "Stopping!" stage until we detached.
    JavaVMAttachArgs args{};
    args.version = PreferredJniVersion();
    args.name    = const_cast<char*>(threadName); // JNI takes a non-const modified-UTF-8 name
    args.group   = nullptr;                       // null = the main ThreadGroup

    jint attachRc = vm->AttachCurrentThreadAsDaemon(&raw, &args);
    if (attachRc == JNI_EVERSION) {
        args.version = FallbackJniVersion();
        attachRc     = vm->AttachCurrentThreadAsDaemon(&raw, &args);
    }

    if (attachRc != JNI_OK || !raw) {
        MCOV_LOG_ERROR("AttachCurrentThreadAsDaemon failed with %d", attachRc);
        return;
    }

    env_  = static_cast<JNIEnv*>(raw);
    owns_ = true;
    Runtime::Instance().NoteAttach();
}

ScopedEnv::~ScopedEnv() {
    if (!owns_ || !env_) return;

    JavaVM* vm = Runtime::Instance().vm();
    if (!vm) return;

    // Any exception still pending at detach time is reported by HotSpot as an
    // "exception pending on thread detach" abort, so swallow it here.
    if (env_->ExceptionCheck()) env_->ExceptionClear();

    vm->DetachCurrentThread();
    Runtime::Instance().NoteDetach();
}

// ---------------------------------------------------------------------------
// LocalFrame
// ---------------------------------------------------------------------------
LocalFrame::LocalFrame(JNIEnv* env, jint capacity) : env_(env) {
    if (!env_) return;
    if (env_->PushLocalFrame(capacity) == JNI_OK) {
        pushed_ = true;
    } else {
        // PushLocalFrame throws OutOfMemoryError on failure.
        if (env_->ExceptionCheck()) env_->ExceptionClear();
    }
}

LocalFrame::~LocalFrame() {
    if (pushed_ && env_) {
        // PopLocalFrame(result) frees every ref made inside the frame and
        // re-creates `result` in the enclosing frame. Passing null frees all.
        env_->PopLocalFrame(escaped_);
    }
}

// ---------------------------------------------------------------------------
// Exceptions
// ---------------------------------------------------------------------------
bool CheckAndClearException(JNIEnv* env, const char* context, std::string* description) {
    if (!env || !env->ExceptionCheck()) return false;

    jthrowable thrown = env->ExceptionOccurred();

    // Clear immediately: with an exception pending, calling FindClass /
    // GetMethodID / CallObjectMethod below would be undefined behaviour.
    env->ExceptionClear();

    std::string text = "<no detail available>";
    if (thrown) {
        // java/lang/Throwable is a bootstrap class, so FindClass reaches it even
        // from a natively attached thread with no application loader in scope.
        if (jclass throwableCls = env->FindClass("java/lang/Throwable")) {
            if (jmethodID toString = env->GetMethodID(throwableCls, "toString", "()Ljava/lang/String;")) {
                auto message = static_cast<jstring>(env->CallObjectMethod(thrown, toString));
                if (env->ExceptionCheck()) {
                    env->ExceptionClear(); // toString() itself threw; give up on detail
                } else if (message) {
                    text = JStringToUtf8(env, message);
                    env->DeleteLocalRef(message);
                }
            } else if (env->ExceptionCheck()) {
                env->ExceptionClear();
            }
            env->DeleteLocalRef(throwableCls);
        } else if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        env->DeleteLocalRef(thrown);
    }

    MCOV_LOG_WARN("java exception during %s: %s", context ? context : "?", text.c_str());
    if (description) *description = text;
    return true;
}

bool CaptureException(JNIEnv* env, const char* context, Error* error) {
    std::string description;
    if (!CheckAndClearException(env, context, &description)) return false;
    SetError(error, Status::JavaException,
             std::string(context ? context : "jni call") + ": " + description);
    return true;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
std::string JStringToUtf8(JNIEnv* env, jstring value) {
    if (!env || !value) return {};

    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return {};
    }
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars); // never skip this - it pins memory
    return result;
}

jmethodID FindMethod(JNIEnv* env, jclass owner, const char* name, const char* sig, bool isStatic, Error* error) {
    if (!env || !owner) {
        SetError(error, Status::MemberNotFound, "FindMethod called with a null class");
        return nullptr;
    }
    jmethodID id = isStatic ? env->GetStaticMethodID(owner, name, sig)
                            : env->GetMethodID(owner, name, sig);
    if (!id || env->ExceptionCheck()) {
        // GetMethodID throws NoSuchMethodError rather than returning null-only.
        env->ExceptionClear();
        SetError(error, Status::MemberNotFound,
                 std::string("no ") + (isStatic ? "static " : "") + "method " + name + sig);
        return nullptr;
    }
    return id;
}

jfieldID FindField(JNIEnv* env, jclass owner, const char* name, const char* sig, bool isStatic, Error* error) {
    if (!env || !owner) {
        SetError(error, Status::MemberNotFound, "FindField called with a null class");
        return nullptr;
    }
    jfieldID id = isStatic ? env->GetStaticFieldID(owner, name, sig)
                           : env->GetFieldID(owner, name, sig);
    if (!id || env->ExceptionCheck()) {
        env->ExceptionClear();
        SetError(error, Status::MemberNotFound,
                 std::string("no ") + (isStatic ? "static " : "") + "field " + name + " " + sig);
        return nullptr;
    }
    return id;
}

jmethodID TryMethod(JNIEnv* env, jclass owner, const char* name, const char* sig, bool isStatic) {
    if (!env || !owner || !name || !*name || !sig || !*sig) return nullptr;
    jmethodID id = isStatic ? env->GetStaticMethodID(owner, name, sig)
                            : env->GetMethodID(owner, name, sig);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return nullptr;
    }
    return id;
}

jfieldID TryField(JNIEnv* env, jclass owner, const char* name, const char* sig, bool isStatic) {
    if (!env || !owner || !name || !*name || !sig || !*sig) return nullptr;
    jfieldID id = isStatic ? env->GetStaticFieldID(owner, name, sig)
                           : env->GetFieldID(owner, name, sig);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return nullptr;
    }
    return id;
}

// ---------------------------------------------------------------------------
// demo
// ---------------------------------------------------------------------------
namespace demo {

bool ResolveMinecraftClass(JNIEnv* env, MinecraftHandles& out, Error* error) {
    if (out.resolved) return true;

    // JNI class names use '/' separators and no ".class" suffix:
    //   java source     net.minecraft.client.Minecraft
    //   JNI FindClass   net/minecraft/client/Minecraft
    //   JVM descriptor  Lnet/minecraft/client/Minecraft;
    //
    // Which of these actually exists at runtime depends entirely on the mapping
    // set in play - see mappings.hpp for why we try more than one.
    static constexpr const char* kCandidates[] = {
        "net/minecraft/client/Minecraft",       // Mojang official mappings (NeoForge et al.)
        "net/minecraft/class_310",              // intermediary - a SHIPPED Fabric client
        "net/minecraft/client/MinecraftClient", // Yarn - Fabric dev environment only
    };

    jclass klass = Runtime::Instance().ResolveClass(
        env, "mc.client", {kCandidates[0], kCandidates[1], kCandidates[2]}, error);
    if (!klass) return false;

    // Work out which name won, so getInstance()'s return descriptor matches.
    // A wrong descriptor is the single most common cause of a silent
    // NoSuchMethodError here: JNI matches on the full signature, not just name.
    const char* descriptor = nullptr;
    for (const char* candidate : kCandidates) {
        jclass probe = env->FindClass(candidate);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (probe) {
            const bool same = env->IsSameObject(probe, klass);
            env->DeleteLocalRef(probe);
            if (same) { descriptor = candidate; break; }
        }
    }

    // If FindClass could not see the class at all (the usual mod-loader case),
    // fall back to trying each descriptor until GetStaticMethodID accepts one.
    for (const char* candidate : kCandidates) {
        if (descriptor && candidate != descriptor) continue;

        std::string sig = std::string("()L") + candidate + ";";
        if (jmethodID id = TryMethod(env, klass, "getInstance", sig.c_str(), /*isStatic=*/true)) {
            out.minecraftClass = klass;
            out.getInstance    = id;
            out.mappingId      = candidate;
            out.resolved       = true;
            MCOV_LOG_INFO("bound %s.getInstance%s", candidate, sig.c_str());
            return true;
        }
        if (descriptor) break;
    }

    SetError(error, Status::MemberNotFound,
             "found the client class but no matching static getInstance() - verify the "
             "return descriptor against your mapping set");
    return false;
}

jobject GetClientInstance(JNIEnv* env, const MinecraftHandles& handles, Error* error) {
    if (!handles.resolved || !env) {
        SetError(error, Status::ClassNotFound, "Minecraft class handles are not resolved");
        return nullptr;
    }

    jobject instance = env->CallStaticObjectMethod(handles.minecraftClass, handles.getInstance);

    // Always ExceptionCheck after a Call*Method: the return value is undefined
    // if the callee threw, and continuing with a pending exception is UB.
    if (CaptureException(env, "Minecraft.getInstance()", error)) return nullptr;

    // A null return is legitimate during early startup, before the client
    // singleton is constructed. The caller retries rather than treating it as
    // a hard failure.
    return instance;
}

} // namespace demo
} // namespace jvm
