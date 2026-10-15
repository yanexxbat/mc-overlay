#include "auth_handoff.hpp"
#include "win_handle.hpp"
#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace mcov::auth {
namespace {
constexpr std::uint32_t kHandoffMagic=0x31414B4D;
struct Packet {
    std::uint32_t magic=kHandoffMagic,length=0;
    std::int64_t expiry=0;
    std::array<char,kMaxSession> token{};
};
struct WipePacket { Packet& packet; ~WipePacket() { SecureZeroMemory(&packet,sizeof(packet)); } };
}
Session ReadHandoff(HANDLE raw) {
    WinHandle pipe(raw);
    if(!pipe||GetFileType(pipe.get())!=FILE_TYPE_PIPE) return {};
    DWORD available=0;
    if(!PeekNamedPipe(pipe.get(),nullptr,0,nullptr,&available,nullptr)||available!=sizeof(Packet)) return {};
    Packet packet; WipePacket wipe{packet}; DWORD read=0;
    if(!ReadFile(pipe.get(),&packet,sizeof(packet),&read,nullptr)||read!=sizeof(packet)||packet.magic!=kHandoffMagic||
       !packet.length||packet.length>kMaxSession) return {};
    Session session(std::string(packet.token.data(),packet.length),packet.expiry);
    if(!session.Valid(UnixTime())) return {};
    return session;
}
bool LaunchOverlay(const std::filesystem::path& executable,const std::filesystem::path& directory,
                   std::wstring command,const Session& session,PROCESS_INFORMATION& process,std::string& error) {
    if(!session.Valid(UnixTime())) { error="Verify an active license before starting the client."; return false; }
    SECURITY_ATTRIBUTES attributes{sizeof(attributes),nullptr,TRUE}; HANDLE rawRead=nullptr,rawWrite=nullptr;
    if(!CreatePipe(&rawRead,&rawWrite,&attributes,sizeof(Packet))) { error="Could not prepare the licensed client session."; return false; }
    WinHandle read(rawRead),write(rawWrite);
    if(!SetHandleInformation(write.get(),HANDLE_FLAG_INHERIT,0)) { error="Could not prepare the licensed client session."; return false; }
    Packet packet; WipePacket wipe{packet}; packet.length=std::uint32_t(session.id.size()); packet.expiry=session.expires;
    std::memcpy(packet.token.data(),session.id.data(),packet.length); DWORD written=0;
    if(!WriteFile(write.get(),&packet,sizeof(packet),&written,nullptr)||written!=sizeof(packet)) { error="Could not prepare the licensed client session."; return false; }
    write=WinHandle{};
    SIZE_T bytes=0; InitializeProcThreadAttributeList(nullptr,1,0,&bytes); std::vector<unsigned char> storage(bytes);
    auto* list=reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if(!InitializeProcThreadAttributeList(list,1,0,&bytes)) { error="Could not prepare client startup."; return false; }
    struct Cleanup { PPROC_THREAD_ATTRIBUTE_LIST list; ~Cleanup() { DeleteProcThreadAttributeList(list); } } cleanup{list};
    HANDLE inherited=read.get();
    if(!UpdateProcThreadAttribute(list,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,&inherited,sizeof(inherited),nullptr,nullptr)) { error="Could not prepare the licensed client session."; return false; }
    STARTUPINFOEXW startup{}; startup.StartupInfo.cb=sizeof(startup); startup.StartupInfo.dwFlags=STARTF_USESHOWWINDOW;
    startup.StartupInfo.wShowWindow=SW_HIDE; startup.lpAttributeList=list;
    command+=L" --auth-pipe "+std::to_wstring(reinterpret_cast<std::uintptr_t>(read.get()));
    if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,
        nullptr,directory.c_str(),&startup.StartupInfo,&process)) {
        error="Could not start the client. Windows error "+std::to_string(GetLastError())+"."; return false;
    }
    return true;
}
}
