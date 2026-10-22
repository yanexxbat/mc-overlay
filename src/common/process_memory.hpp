#pragma once
#include <windows.h>
#include <psapi.h>
#include <cstdio>

namespace mcov {
class ProcessMemory {
    ULONGLONG nextSample_{};
    SIZE_T workingSet_{};
    bool available_{};
public:
    void Update() {
        const auto now=GetTickCount64();
        if(now<nextSample_) return;
        nextSample_=now+1000;
        PROCESS_MEMORY_COUNTERS counters{};
        counters.cb=sizeof(counters);
        available_=GetProcessMemoryInfo(GetCurrentProcess(),&counters,sizeof(counters))!=FALSE;
        if(available_) workingSet_=counters.WorkingSetSize;
    }
    float Megabytes() const { return static_cast<float>(workingSet_)/(1024.f*1024.f); }
    void Format(char* text,std::size_t size) const {
        if(available_) std::snprintf(text,size,"Keim: %.0f MB",Megabytes());
        else std::snprintf(text,size,"Keim: RAM unavailable");
    }
};
}
