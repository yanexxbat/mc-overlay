#pragma once
#include "jvm_interface.hpp"
#include "../common/shared_state.hpp"
#include <memory>
namespace mcov::agent {
class ChunkFinderBridge {
    SharedBlock* block_{}; jclass bridge_{}; jmethodID update_{},disable_{};
    jfieldID view_{},status_{},world_{},revision_{},scanned_{},pending_{},limited_{},rows_{},blocks_{},alerts_{},scannedChunks_{};
    std::uint64_t next_=0;
    std::unique_ptr<chunks::Report> report_=std::make_unique<chunks::Report>();
    static bool Error(JNIEnv* e) {if(!e->ExceptionCheck())return false;e->ExceptionClear();return true;}
public:
    template<class Bindings> bool Install(JNIEnv* e,const Bindings& b,SharedBlock* block) {
        block_=block;jvm::LocalFrame frame(e,16);if(!frame.ok())return false;
        auto c=e->FindClass("mcov/ChunkFinderBridge");if(Error(e)||!c)return false;
        auto configure=e->GetStaticMethodID(c,"configure","(Ljava/lang/Class;Ljava/lang/String;)Z");
        update_=e->GetStaticMethodID(c,"update","(Z[IIIIZJJ)V");disable_=e->GetStaticMethodID(c,"disable","()V");
        view_=e->GetStaticFieldID(c,"view","Lmcov/ChunkFinderBridge$View;");
        auto v=e->FindClass("mcov/ChunkFinderBridge$View");if(Error(e)||!configure||!update_||!disable_||!view_||!v)return false;
        status_=e->GetFieldID(v,"status","I");world_=e->GetFieldID(v,"world","J");revision_=e->GetFieldID(v,"revision","J");
        scanned_=e->GetFieldID(v,"scanned","I");pending_=e->GetFieldID(v,"pending","I");limited_=e->GetFieldID(v,"limited","Z");
        rows_=e->GetFieldID(v,"rows","[I");blocks_=e->GetFieldID(v,"blocks","[I");alerts_=e->GetFieldID(v,"alerts","[J");
        scannedChunks_=e->GetFieldID(v,"scannedChunks","[J");
        if(Error(e)||!status_||!world_||!revision_||!scanned_||!pending_||!limited_||!rows_||!blocks_||!alerts_||!scannedChunks_)return false;
        auto profile=e->NewStringUTF(b.set->id);auto ok=e->CallStaticBooleanMethod(c,configure,b.client.minecraftClass,profile);
        if(Error(e)||!ok)return false;
        bridge_=static_cast<jclass>(e->NewGlobalRef(c));return bridge_&&!Error(e);
    }
    void Refresh(JNIEnv* e) {
        if(!block_)return;const auto now=GetTickCount64();if(now<next_)return;next_=now+100;
        auto& out=*report_;out.sampledAtMs=now;
        if(!bridge_) {out.status=chunks::Status::Unavailable;out.count=out.blockCount=0;block_->chunkReport.Publish(out);return;}
        jvm::LocalFrame scope(e,16);if(!scope.ok())return;
        chunks::Request r;bool fresh=block_->chunkRequest.Read(r)&&r.sentAtMs&&now>=r.sentAtMs&&now-r.sentAtMs<=500;
        const auto flags=block_->controlFlags.load(std::memory_order_acquire);
        const auto& d=r.detection;jint values[]={jint(d.signals),d.deepslateMin,d.rotatedMin,d.cobbledMin,d.endStoneMin,d.baseScore,d.farmChance,d.geodeSensitivity};
        auto settings=e->NewIntArray(8);if(!settings||Error(e))return;e->SetIntArrayRegion(settings,0,8,values);
        e->CallStaticVoidMethod(bridge_,update_,jboolean(fresh&&r.enabled&&!(flags&(kCtrlPauseSampling|kCtrlRequestUnload))),settings,
            jint(r.threads),jint(r.chunksPerTick),jint(r.alertsPerMinute),jboolean(r.chat),jlong(r.clearId),jlong(fresh?500-(now-r.sentAtMs):0));
        auto v=e->GetStaticObjectField(bridge_,view_);
        if(Error(e)||!v) {out.status=chunks::Status::Failed;out.count=out.blockCount=0;block_->chunkReport.Publish(out);return;}
        int status=e->GetIntField(v,status_);out.status=status>=0&&status<=4?chunks::Status(status):chunks::Status::Failed;
        const auto revision=std::uint64_t(e->GetLongField(v,revision_)),world=std::uint64_t(e->GetLongField(v,world_));
        out.scanned=e->GetIntField(v,scanned_);out.pending=e->GetIntField(v,pending_);out.limited=e->GetBooleanField(v,limited_);
        if(revision!=out.revision||world!=out.world||out.status!=chunks::Status::Scanning) {
            out.revision=revision;out.world=world;out.count=out.blockCount=out.scannedCount=0;
            auto rows=static_cast<jintArray>(e->GetObjectField(v,rows_));auto blocks=static_cast<jintArray>(e->GetObjectField(v,blocks_));auto alerts=static_cast<jlongArray>(e->GetObjectField(v,alerts_));
            if(rows&&blocks&&alerts&&!Error(e)) {
                int count=std::min({e->GetArrayLength(rows)/8,e->GetArrayLength(alerts),chunks::kMaxFinds});
                std::vector<jint> data(count*8);std::vector<jlong> ids(count);if(count){e->GetIntArrayRegion(rows,0,count*8,data.data());e->GetLongArrayRegion(alerts,0,count,ids.data());}
                if(!Error(e)) for(int i=0;i<count;++i) {auto* p=data.data()+i*8;auto& f=out.finds[out.count++];f={p[0],p[1],p[2],p[3],p[4],p[5],p[6],std::uint32_t(p[7]),std::uint64_t(ids[i])};}
                count=std::min(e->GetArrayLength(blocks)/4,chunks::kMaxEvidence);data.resize(count*4);if(count)e->GetIntArrayRegion(blocks,0,count*4,data.data());
                if(!Error(e)) for(int i=0;i<count;++i) {auto* p=data.data()+i*4;out.blocks[out.blockCount++]={p[0],p[1],p[2],p[3]};}
                auto scanned=static_cast<jlongArray>(e->GetObjectField(v,scannedChunks_));
                if(scanned&&!Error(e)){count=std::min(e->GetArrayLength(scanned),4096);ids.resize(count);if(count)e->GetLongArrayRegion(scanned,0,count,ids.data());if(!Error(e))for(auto id:ids)out.scannedChunks[out.scannedCount++]={int(id>>32),int(id&0xffffffffLL)};}
            }
        }
        if(Error(e)) {out.status=chunks::Status::Failed;out.count=out.blockCount=0;}
        block_->chunkReport.Publish(out);
    }
    void Close(JNIEnv* e) {if(bridge_) {e->CallStaticVoidMethod(bridge_,disable_);Error(e);e->DeleteGlobalRef(bridge_);}bridge_=nullptr;block_=nullptr;}
};
}
