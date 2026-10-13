#include "common/ipc.hpp"
#include <windows.h>
#include <iostream>
#include <memory>
#include <cstdlib>
#include <cstring>
int main(int argc,char** argv) {
    if(argc<2) return 2;
    mcov::SharedChannel channel; std::string error;
    if(!channel.OpenReader(std::strtoul(argv[1],nullptr,10),&error)) { std::cerr<<error<<'\n'; return 1; }
    auto* block=channel.block();
    if(argc>2&&std::strcmp(argv[2],"--reset")==0) {
        auto& flags=block->controlFlags;
        const auto old=flags.fetch_or(mcov::kCtrlPauseSampling,std::memory_order_acq_rel);
        if(old&mcov::kCtrlPauseSampling) { std::cerr<<"Sampling already paused; preserving it.\n"; return 2; }
        struct Restore { std::atomic<std::uint32_t>& flags; ~Restore(){flags.fetch_and(~mcov::kCtrlPauseSampling,std::memory_order_release);} } restore{flags};
        Sleep(400);
    }
    const char* names[]={"unsupported","disabled","waiting","building","ready","empty","failed"};
    for(int i=0;i<10;++i) {
        mcov::base::Report report; mcov::base::Request request;
        if(block->baseReport.Read(report)&&block->baseRequest.Read(request)) {
            const auto index=std::uint32_t(report.status);
            std::cout<<"base="<<(index<7?names[index]:"unknown")<<" enabled="<<request.enabled<<" regions="<<request.count
                <<" scanned="<<report.scanned<<" blocks="<<report.blocks<<" vertices="<<report.vertices<<" skipped="<<report.skipped<<'\n';
        }
        Sleep(300);
    }
}
