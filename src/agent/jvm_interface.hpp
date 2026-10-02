// jvm_interface.hpp - JNI interop layer for Java 17/21/25 HotSpot VMs.
//
// ===========================================================================
// READ THIS FIRST: where this code runs
// ===========================================================================
// JNI_GetCreatedJavaVMs enumerates JavaVM instances that live in the address
// space of the *calling* process. There is no remote/cross-process form of it,
// and a JavaVM* is a pointer into the target's heap - it is meaningless when
// read from another process. Consequently every function in this header must
// execute inside javaw.exe.
//
// This file is therefore compiled into mcov_agent.dll, which the JVM itself
// loads through the standard JVMTI attach mechanism (see AgentLoader.java).
// The external overlay never links this translation unit; it consumes the
// results over shared memory instead.
//
// Two entry paths are supported:
//   * Runtime::AdoptVm()   - the normal path. Agent_OnAttach hands us a live
//                            JavaVM*, so no discovery is needed at all.
//   * Runtime::DiscoverVm()- the JNI_GetCreatedJavaVMs path, kept because it is
//                            what you need when the DLL arrives in the process
//                            by some route that does not hand you a JavaVM*.
// ===========================================================================
#pragma once

#include <jni.h>

#include <atomic>
#include <functional>
#include <initializer_list>
#include <mutex>
#include <string>
#include <unordered_map>

namespace jvm {

enum class Status {
    Ok = 0,
    JvmModuleNotFound,
    EntryPointMissing,
    NoVmCreated,
    UnsupportedJniVersion,
    AttachFailed,
    ClassNotFound,
    MemberNotFound,
    JavaException,
    ShuttingDown,
};

struct Error {
    Status      status = Status::Ok;
    std::string message;

    bool        failed() const noexcept { return status != Status::Ok; }
    explicit    operator bool() const noexcept { return failed(); }
    const char* what() const noexcept { return message.c_str(); }
};

// JDK 21's jni.h defines JNI_VERSION_21 (0x00150000). Requesting it gives access
// to the 21-era function table; we fall back to 1.8 because a VM launched with
// an older -version request will reject the newer constant from GetEnv.
jint PreferredJniVersion() noexcept;
jint FallbackJniVersion() noexcept;

// ---------------------------------------------------------------------------
// Runtime - process-wide handle on the JVM.
// ---------------------------------------------------------------------------
class Runtime {
public:
    static Runtime& Instance();

    // Preferred path: Agent_OnAttach / Agent_OnLoad already gave us the VM.
    void AdoptVm(JavaVM* vm) noexcept;

    // Fallback path: locate jvm.dll and ask it for the already-created VM.
    // jvmDllPath may be empty, in which case we only look for an already-loaded
    // module (which is always the case inside javaw.exe).
    bool DiscoverVm(const std::wstring& jvmDllPath, Error* error);

    JavaVM* vm() const noexcept { return vm_.load(std::memory_order_acquire); }
    bool    shuttingDown() const noexcept { return shutdown_.load(std::memory_order_acquire); }
    bool    ready() const noexcept;

    // --- lifecycle ---------------------------------------------------------
    // Flips the shutdown flag. Worker threads are expected to notice, unwind
    // their ScopedEnv (which detaches them) and exit.
    //
    // Note: DetachCurrentThread can ONLY detach the calling thread. There is no
    // API to detach someone else, so "detach every attached thread" necessarily
    // means "signal every worker and wait for each to detach itself".
    void BeginShutdown() noexcept;
    bool WaitForThreadsDetached(unsigned timeoutMs);
    int  attachedThreadCount() const noexcept { return attached_.load(std::memory_order_acquire); }

    // Frees cached global refs. MUST be called from an attached thread, before
    // that thread detaches, and before the DLL unloads.
    void ReleaseCachedRefs(JNIEnv* env);

    // --- class resolution --------------------------------------------------
    // FindClass() called on a natively-attached thread searches the *system*
    // class loader only. Minecraft's classes live under the launcher's (or a
    // mod loader's) own loader, so plain FindClass throws NoClassDefFoundError
    // for them. The agent injects a JVMTI-backed resolver here that scans
    // already-loaded classes instead. See agent_main.cpp.
    using ClassResolver = std::function<jclass(JNIEnv*, const char* /*slashed name*/)>;
    void SetFallbackClassResolver(ClassResolver resolver);
    // Pin Minecraft's defining loader after JVMTI discovers the client. Forge
    // and Fabric can then resolve classes that have not been loaded yet.
    bool SetGameClassLoader(JNIEnv* env, jclass clientClass);

    // Resolves and caches a class as a global ref, trying each candidate name
    // in order. Candidates let one call cover several mapping sets.
    jclass ResolveClass(JNIEnv*                            env,
                        const char*                        cacheKey,
                        std::initializer_list<const char*> candidates,
                        Error*                             error);

    void NoteAttach() noexcept { attached_.fetch_add(1, std::memory_order_acq_rel); }
    void NoteDetach() noexcept { attached_.fetch_sub(1, std::memory_order_acq_rel); }

private:
    Runtime() = default;

    std::atomic<JavaVM*> vm_{nullptr};
    std::atomic<bool>    shutdown_{false};
    std::atomic<int>     attached_{0};

    void* jvmModule_    = nullptr; // HMODULE
    bool  ownsJvmModule_ = false;  // true only if *we* called LoadLibraryExW

    std::mutex                             classMutex_;
    std::unordered_map<std::string, jclass> classCache_; // values are global refs
    ClassResolver                          fallbackResolver_;
    jobject                                gameClassLoader_ = nullptr; // global ref
    jmethodID                              loadClass_ = nullptr;
};

// ---------------------------------------------------------------------------
// ScopedEnv - RAII attach/detach for a native thread.
//
// Two JNI footguns this exists to prevent:
//   1. Calling DetachCurrentThread on a thread the JVM created (or that was
//      already attached by someone else) tears down state that isn't ours and
//      typically aborts the VM. So: GetEnv first, and only detach what we
//      ourselves attached.
//   2. AttachCurrentThread creates a *non-daemon* thread, which keeps the JVM
//      alive: Minecraft will appear to hang on exit until you detach. We use
//      AttachCurrentThreadAsDaemon, which does not participate in VM shutdown.
// ---------------------------------------------------------------------------
class ScopedEnv {
public:
    explicit ScopedEnv(const char* threadName = "mcov-worker");
    ~ScopedEnv();

    ScopedEnv(const ScopedEnv&)            = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;

    JNIEnv* get() const noexcept { return env_; }
    JNIEnv* operator->() const noexcept { return env_; }
    bool    ok() const noexcept { return env_ != nullptr; }
    bool    ownsAttachment() const noexcept { return owns_; }

private:
    JNIEnv* env_  = nullptr;
    bool    owns_ = false;
};

// ---------------------------------------------------------------------------
// LocalFrame - RAII PushLocalFrame/PopLocalFrame.
//
// The JVM only guarantees 16 local reference slots; HotSpot gives ~512 before
// it warns and then aborts. Any loop that touches objects (like walking the
// entity list) must scope its refs or it will blow the table. Wrapping each
// iteration in a LocalFrame is cheaper and far less error-prone than matching
// every ref with DeleteLocalRef by hand.
// ---------------------------------------------------------------------------
class LocalFrame {
public:
    LocalFrame(JNIEnv* env, jint capacity = 32);
    ~LocalFrame();

    LocalFrame(const LocalFrame&)            = delete;
    LocalFrame& operator=(const LocalFrame&) = delete;

    bool ok() const noexcept { return pushed_; }

    // Promote one ref out of the frame so it survives PopLocalFrame.
    jobject escape(jobject local) noexcept { escaped_ = local; return local; }

private:
    JNIEnv* env_     = nullptr;
    bool    pushed_  = false;
    jobject escaped_ = nullptr;
};

// ---------------------------------------------------------------------------
// Exception plumbing
// ---------------------------------------------------------------------------
// With an exception pending, almost every JNI function is undefined behaviour -
// only ExceptionOccurred/ExceptionDescribe/ExceptionClear, DeleteLocalRef,
// and a handful of others are legal. So we clear *first*, then inspect.
//
// Also note ExceptionDescribe() itself clears the exception as a side effect,
// which makes it useless for "log and rethrow" and is why we don't use it.
bool CheckAndClearException(JNIEnv* env, const char* context, std::string* description);

// Convenience: turns a pending exception into an Error and returns true.
bool CaptureException(JNIEnv* env, const char* context, Error* error);

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------
// GetStringUTFChars yields *modified* UTF-8 (NUL as 0xC0 0x80, surrogate pairs
// encoded separately). Fine for class/player names; do not feed the result to
// something that demands strict UTF-8 without converting.
std::string JStringToUtf8(JNIEnv* env, jstring value);

jmethodID FindMethod(JNIEnv* env, jclass owner, const char* name, const char* sig, bool isStatic, Error* error);
jfieldID  FindField (JNIEnv* env, jclass owner, const char* name, const char* sig, bool isStatic, Error* error);

// Soft variants: return nullptr and clear any exception instead of reporting an
// error. Used for members whose names vary between mapping sets, where a miss
// is expected and should degrade one value rather than fail the whole sample.
jmethodID TryMethod(JNIEnv* env, jclass owner, const char* name, const char* sig, bool isStatic);
jfieldID  TryField (JNIEnv* env, jclass owner, const char* name, const char* sig, bool isStatic);

// ---------------------------------------------------------------------------
// demo - the "look up a core client class and call a static method" walkthrough
// asked for in the brief, kept separate from the generic machinery above.
// ---------------------------------------------------------------------------
namespace demo {

struct MinecraftHandles {
    jclass    minecraftClass = nullptr; // global ref, owned by Runtime's cache
    jmethodID getInstance    = nullptr; // static ()Lnet/minecraft/client/Minecraft;
    bool      resolved       = false;
    std::string mappingId;              // which candidate name actually matched
};

// Resolves net.minecraft.client.Minecraft (Mojang mappings) or its Yarn
// equivalent, then locates the static getInstance() accessor.
bool ResolveMinecraftClass(JNIEnv* env, MinecraftHandles& out, Error* error);

// Calls Minecraft.getInstance(). Returns a *local* ref (may be null very early
// in startup, before the client singleton exists - that is not an error).
jobject GetClientInstance(JNIEnv* env, const MinecraftHandles& handles, Error* error);

} // namespace demo
} // namespace jvm
