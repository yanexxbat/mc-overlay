#pragma once
#include "shared_state.hpp"
#include <windows.h>
#include <array>
#include <memory>
#include <string>

namespace mcov {
inline constexpr int kDepthWidth=960,kDepthHeight=540;
enum class DepthStatus : std::uint32_t { Waiting, Ready, Unsupported, Disabled };
// Separater Kanal: grosse Pixelarrays belasten den normalen Snapshot-Leser nicht.
struct DepthFrame {
    std::uint64_t epoch{},timeMs{};
    float nearPlane=.05f,farPlane=1000;
    int sourceWidth{},sourceHeight{};
    WorldSnapshot world{};
    std::array<float,kDepthWidth*kDepthHeight> pixels{};
};
struct DepthBlock {
    std::uint32_t magic=0x4450434d,version=1;
    std::atomic<std::uint32_t> sequence{},requested{},status{};
    DepthFrame frame;
};
class DepthChannel final {
    HANDLE handle_{}; DepthBlock* block_{};
public:
    ~DepthChannel() { Close(); }
    DepthChannel()=default;
    DepthChannel(const DepthChannel&)=delete;
    DepthChannel& operator=(const DepthChannel&)=delete;
    bool Open(std::uint32_t pid,bool writer) {
        Close(); const auto name=L"Local\\mcov_depth_v1_"+std::to_wstring(pid);
        handle_=writer?CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(DepthBlock),name.c_str()):
            OpenFileMappingW(FILE_MAP_READ|FILE_MAP_WRITE,FALSE,name.c_str());
        if(!handle_) return false;
        block_=static_cast<DepthBlock*>(MapViewOfFile(handle_,FILE_MAP_READ|FILE_MAP_WRITE,0,0,sizeof(DepthBlock)));
        if(!block_) { Close(); return false; }
        if(writer) { block_->magic=0x4450434d; block_->version=1; block_->sequence=0; block_->requested=0; block_->status=0; }
        else if(block_->magic!=0x4450434d||block_->version!=1) { Close(); return false; }
        return true;
    }
    void Close() { if(block_) UnmapViewOfFile(block_); if(handle_) CloseHandle(handle_); block_=nullptr; handle_=nullptr; }
    DepthBlock* Block() const { return block_; }
    bool Read(DepthFrame& out) const {
        if(!block_||block_->status.load(std::memory_order_acquire)!=std::uint32_t(DepthStatus::Ready)) return false;
        for(int i=0;i<3;++i) {
            const auto before=block_->sequence.load(std::memory_order_acquire); if(before&1) continue;
            std::memcpy(&out,&block_->frame,sizeof out);
            std::atomic_thread_fence(std::memory_order_acquire);
            if(block_->sequence.load(std::memory_order_acquire)==before) return out.epoch!=0;
        }
        return false;
    }
};
}
