#include "portable_package.hpp"
#include "../common/win_handle.hpp"
#include "../overlay/process.hpp"
#include <bcrypt.h>
#include <fstream>
#include <array>
#include <vector>
#include <stdexcept>
#include <span>
#ifdef MCOV_BUNDLED
#include "portable_manifest.hpp"
#endif

namespace mcov::launcher {
namespace {
struct Sha256 final {
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    BCRYPT_HASH_HANDLE hash=nullptr;
    ~Sha256() { if(hash) BCryptDestroyHash(hash); if(algorithm) BCryptCloseAlgorithmProvider(algorithm,0); }
    Sha256() {
        if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)
            throw std::runtime_error("SHA-256 konnte nicht gestartet werden.");
        if(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0) {
            BCryptCloseAlgorithmProvider(algorithm,0); algorithm=nullptr;
            throw std::runtime_error("SHA-256 konnte nicht gestartet werden.");
        }
    }
    void Add(std::span<const unsigned char> bytes) {
        if(BCryptHashData(hash,const_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),0)<0) throw std::runtime_error("Paketpruefung fehlgeschlagen.");
    }
    std::string Finish() {
        std::array<unsigned char,32> bytes{};
        if(BCryptFinishHash(hash,bytes.data(),DWORD(bytes.size()),0)<0) throw std::runtime_error("Paketpruefung fehlgeschlagen.");
        constexpr char digits[]="0123456789abcdef"; std::string out;
        for(auto b:bytes) { out+=digits[b>>4]; out+=digits[b&15]; } return out;
    }
};
std::string FileHash(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary); if(!file) return {};
    Sha256 sha; std::array<unsigned char,65536> bytes{};
    while(file) { file.read(reinterpret_cast<char*>(bytes.data()),bytes.size()); sha.Add({bytes.data(),static_cast<std::size_t>(file.gcount())}); }
    return file.eof()?sha.Finish():std::string{};
}
}
bool MatchingAgentRuntime(const std::filesystem::path& expected,const std::filesystem::path& loadedAgent) {
    if(loadedAgent.empty()) return false;
    const auto loaded=loadedAgent.parent_path();
    for(const auto* name:{L"mcov_agent.dll",L"tools/frame-signal.jar",L"tools/frame-sync-agent.jar"}) {
        const auto current=FileHash(expected/name);
        const auto previous=FileHash(std::wcscmp(name,L"mcov_agent.dll")==0?loadedAgent:loaded/name);
        if(current.empty()||previous.empty()||current!=previous) return false;
    }
    return true;
}
std::filesystem::path PreparePackage(const std::filesystem::path& storage) {
#ifdef MCOV_BUNDLED
    const auto directory=storage/L"runtime"/kPackageId;
    std::filesystem::create_directories(directory/L"tools");
    const std::wstring name=L"Local\\mcov_package_"+std::wstring(kPackageId,kPackageId+std::char_traits<char>::length(kPackageId));
    WinHandle mutex(CreateMutexW(nullptr,FALSE,name.c_str()));
    if(!mutex) throw std::runtime_error("Paket-Cache konnte nicht gesperrt werden.");
    const auto waited=WaitForSingleObject(mutex.get(),10000);
    if(waited!=WAIT_OBJECT_0&&waited!=WAIT_ABANDONED) throw std::runtime_error("Eine andere mcov-Instanz bereitet das Paket vor. Bitte erneut versuchen.");
    struct Unlock { HANDLE value; ~Unlock() { ReleaseMutex(value); } } unlock{mutex.get()};
    for(const auto& file:kPayloads) {
        const auto resource=FindResourceW(nullptr,MAKEINTRESOURCEW(file.id),RT_RCDATA);
        const auto loaded=resource?LoadResource(nullptr,resource):nullptr;
        const auto size=resource?SizeofResource(nullptr,resource):0;
        auto bytes=loaded?static_cast<const unsigned char*>(LockResource(loaded)):nullptr;
        if(!bytes||!size) throw std::runtime_error("Die mcov.exe enthaelt ein unvollstaendiges Paket. Bitte neu bauen.");
        Sha256 sha; sha.Add({bytes,size});
        if(sha.Finish()!=file.sha) throw std::runtime_error("Das eingebettete Paket ist beschaedigt. Bitte mcov.exe ersetzen.");
        const auto target=directory/file.name;
        if(FileHash(target)==file.sha) continue;
        const auto temporary=std::filesystem::path(target.wstring()+L".part-"+std::to_wstring(GetCurrentProcessId()));
        struct RemoveTemporary { std::filesystem::path path; ~RemoveTemporary() { std::error_code ignored; std::filesystem::remove(path,ignored); } } cleanup{temporary};
        {
            std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
            out.write(reinterpret_cast<const char*>(bytes),size); out.flush();
            if(!out) throw std::runtime_error("mcov kann den Cache nicht schreiben. Freien Speicher und Zugriffsrechte pruefen.");
        }
        if(!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Eine Client-Datei ist gesperrt. Minecraft und mcov schliessen und erneut starten.");
    }
    return directory;
#else
    (void)storage;
    return ExecutableDirectory();
#endif
}
} // namespace mcov::launcher
