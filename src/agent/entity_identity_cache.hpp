#pragma once
#include "../common/shared_state.hpp"
#include <jni.h>

namespace mcov {
// UUIDs sind unveränderlich. Schwache Referenzen erkennen wiederverwendete
// Entity-IDs, ohne despawnte Spieler in der JVM am Leben zu halten.
// Besitzer ist ausschließlich der Sampler; Clear gehört zu dessen Shutdown.
class EntityIdentityCache final {
    struct Entry {
        jweak object{};
        combat::Identity uuid{};
        std::uint32_t id{};
    };
    std::array<Entry,kMaxEntities> entries_{};
    std::size_t next_{};
public:
    EntityIdentityCache() = default;
    EntityIdentityCache(const EntityIdentityCache&) = delete;
    EntityIdentityCache& operator=(const EntityIdentityCache&) = delete;
    void Clear(JNIEnv* env) noexcept {
        for(auto& entry:entries_) {
            if(entry.object) env->DeleteWeakGlobalRef(entry.object);
            entry={};
        }
        next_=0;
    }
    combat::Identity Read(JNIEnv* env,jobject entity,std::uint32_t id,
                          jmethodID getUuid,jmethodID high,jmethodID low) noexcept {
        if(!entity || !getUuid || !high || !low) return {};
        Entry* slot=nullptr;
        for(auto& entry:entries_) if(entry.object && entry.id==id) {
            if(env->IsSameObject(entry.object,entity)) return entry.uuid;
            slot=&entry; break;
        }
        auto uuid=env->CallObjectMethod(entity,getUuid);
        if(env->ExceptionCheck()) { env->ExceptionClear(); return {}; }
        if(!uuid) return {};
        const auto a=env->CallLongMethod(uuid,high);
        if(env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(uuid); return {}; }
        const auto b=env->CallLongMethod(uuid,low);
        env->DeleteLocalRef(uuid);
        if(env->ExceptionCheck()) { env->ExceptionClear(); return {}; }
        const combat::Identity identity{static_cast<std::uint64_t>(a),static_cast<std::uint64_t>(b)};
        auto weak=env->NewWeakGlobalRef(entity);
        if(env->ExceptionCheck()) { env->ExceptionClear(); return identity; }
        if(!weak) return identity;
        if(!slot) { slot=&entries_[next_]; next_=(next_+1)%entries_.size(); }
        if(slot->object) env->DeleteWeakGlobalRef(slot->object);
        *slot={weak,identity,id};
        return identity;
    }
};
} // namespace mcov
