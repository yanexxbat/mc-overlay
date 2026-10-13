#include "injector/portable_package.hpp"
#include <windows.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

void Require(bool value,const char* why) { if(!value) throw std::runtime_error(why); }
void Write(const std::filesystem::path& path,const std::vector<char>& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path,std::ios::binary); file.write(bytes.data(),bytes.size());
    Require(bool(file),"Could not write package fixture");
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) return 2;
        auto root=std::filesystem::path(argv[1])/("run-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
        auto current=root/"new cache",loaded=root/"old cache";
        std::vector<char> bytes(131073,'A');
        const wchar_t* files[]={L"mcov_agent.dll",L"tools/frame-signal.jar",L"tools/frame-sync-agent.jar"};
        for(const auto* file:files) { Write(current/file,bytes); Write(loaded/file,bytes); }
        const auto dll=loaded/L"mcov_agent.dll";
        Require(mcov::launcher::MatchingAgentRuntime(current,dll),"Identical build in another cache could not reconnect");
        for(const auto* file:files) {
            auto changed=bytes; changed.back()='B'; Write(loaded/file,changed);
            Require(!mcov::launcher::MatchingAgentRuntime(current,dll),"Same-sized outdated agent/helper was reused");
            Write(loaded/file,bytes);
        }
        Require(!mcov::launcher::MatchingAgentRuntime(current,{}),"Unknown loaded agent was treated as matching");
        Require(!mcov::launcher::MatchingAgentRuntime(current,root/"missing"/"mcov_agent.dll"),"Missing old package was accepted");
        Require(!mcov::launcher::MatchingAgentRuntime(root/"missing",dll),"Missing expected package was accepted");
        std::cout<<"PASS agent reuse: exact DLL + both helper bytes, different folders, trailing-byte changes and missing files\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
