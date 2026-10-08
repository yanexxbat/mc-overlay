// Ein Startpunkt: Paket vorbereiten, gewaehlte JVM verbinden und Overlay starten.
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <tlhelp32.h>
#include <wrl/client.h>
#include <atomic>
#include <future>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <stdexcept>
#include "launcher_ui.hpp"
#include "portable_package.hpp"
#include "../common/flat_app.hpp"
#include "../common/client_paths.hpp"
#include "../common/win_handle.hpp"
#include "../common/ipc.hpp"
#include "../common/log.hpp"
#include "../overlay/java_runtime.hpp"
#include "../overlay/process.hpp"

using namespace mcov;
namespace {
std::string Utf8(const std::wstring& value) {
    if(value.empty()) return {};
    const int n=WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),nullptr,0,nullptr,nullptr);
    std::string result(n,'\0'); WideCharToMultiByte(CP_UTF8,0,value.data(),int(value.size()),result.data(),n,nullptr,nullptr); return result;
}
std::wstring Wide(const std::string& value) {
    if(value.empty()) return {};
    const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);
    std::wstring result(n,L'\0'); if(n) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),result.data(),n); return result;
}
std::filesystem::path Storage() {
#ifdef MCOV_BUNDLED
    PWSTR path=nullptr;
    if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&path))) throw std::runtime_error("Der lokale Datenordner ist nicht verfuegbar.");
    const auto root=std::filesystem::path(path)/L"mcov"; CoTaskMemFree(path); return root;
#else
    return ExecutableDirectory();
#endif
}
bool ClientRunning(std::uint32_t pid) {
    WinHandle handle(OpenMutexW(SYNCHRONIZE,FALSE,OverlayMutexName(pid).c_str())); return bool(handle);
}
std::filesystem::path AgentPath(std::uint32_t pid) {
    WinHandle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid));
    if(!snapshot) return {};
    MODULEENTRY32W entry{}; entry.dwSize=sizeof(entry);
    if(Module32FirstW(snapshot.get(),&entry)) do {
        if(_wcsicmp(entry.szModule,L"mcov_agent.dll")==0) return entry.szExePath;
    } while(Module32NextW(snapshot.get(),&entry));
    return {};
}
enum class Phase { Idle, Package, Runtime, Attach, Agent, Overlay };
const char* PhaseText(Phase phase) {
    switch(phase) {
        case Phase::Package:return "Client-Dateien werden vorbereitet...";
        case Phase::Runtime:return "Minecraft und Java werden geprueft...";
        case Phase::Attach:return "Verbindung mit Minecraft wird hergestellt...";
        case Phase::Agent:return "Minecraft-Verbindung wird geprueft...";
        case Phase::Overlay:return "Menue und HUD werden gestartet...";
        default:return "Bereit";
    }
}
struct Result {
    bool success=false,needsJdk=false;
    std::string status,detail;
    std::uint32_t pid{};
    std::filesystem::path agentLog;
    WinHandle overlay;
};
void Cancel(std::stop_token stop) { if(stop.stop_requested()) throw std::runtime_error("Verbindung abgebrochen."); }
// Erst ein fortschreitender Heartbeat bestaetigt, dass ein vorhandener Agent lebt.
bool LiveAgent(std::uint32_t pid,std::stop_token stop,std::uint64_t timeout,bool existing) {
    SharedChannel probe;
    const auto until=GetTickCount64()+timeout;
    bool seen=false; std::uint32_t first=0;
    do {
        Cancel(stop);
        if(!probe.valid()) probe.OpenReader(pid,nullptr);
        if(const auto* block=probe.block()) {
            const auto state=static_cast<AgentStatus>(block->status.load(std::memory_order_acquire));
            if(state==AgentStatus::MappingsFailed) throw std::runtime_error("Minecraft-Version oder Mappings werden nicht erkannt. Details stehen im Agent-Protokoll.");
            const auto heartbeat=block->heartbeat.load(std::memory_order_acquire);
            if(!seen) { first=heartbeat; seen=true; }
            else if(heartbeat!=first&&state!=AgentStatus::Booting&&state!=AgentStatus::ResolvingMappings&&state!=AgentStatus::Detaching) return true;
        } else if(existing) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    } while(GetTickCount64()<until);
    return false;
}
Result Connect(std::uint32_t pid,const std::filesystem::path& root,std::atomic<Phase>& phase,std::stop_token stop) {
    Result result; result.pid=pid;
    try {
        phase=Phase::Package; Cancel(stop);
        const auto runtime=launcher::PreparePackage(root);
        phase=Phase::Runtime; Cancel(stop);
        TargetProcess target; std::string detail;
        if(!FindMinecraftProcess(target,&detail,pid)) { result.detail=detail; throw std::runtime_error("Diese Minecraft-Instanz ist nicht mehr verfuegbar. Liste aktualisieren und erneut waehlen."); }
        if(ClientRunning(pid)) { result.success=true; result.status="Der Client laeuft bereits. Zu Minecraft wechseln und das Menue oeffnen."; return result; }
        const auto loaded=AgentPath(pid);
        result.agentLog=(loaded.empty()?runtime:loaded.parent_path())/L"mcov_agent.log";
        if(!loaded.empty()) {
            if(!LiveAgent(pid,stop,2200,true)) throw std::runtime_error("Ein alter oder gestoppter Agent ist geladen. Minecraft vollstaendig beenden und erneut starten.");
            result.detail="Vorhandene Minecraft-Verbindung wiederverwendet.\n";
        } else {
            const auto java=FindAttachCapableJava(target,&detail);
            if(java.empty()) {
                result.needsJdk=true; result.detail=detail;
                throw std::runtime_error("Passendes JDK "+std::to_string(ReadJavaMajor(target.javaHome))+" fehlt. Den Ordner einer vollstaendigen JDK-Installation auswaehlen.");
            }
            result.detail="Minecraft PID "+std::to_string(pid)+"\nJava: "+Utf8(java)+"\n";
            phase=Phase::Attach; Cancel(stop);
            std::uint32_t code=0; std::string failure;
            if(!LaunchAgentLoader(target,(runtime/L"tools").wstring(),(runtime/L"mcov_agent.dll").wstring(),&code,&failure,stop)) {
                result.detail+=failure; throw std::runtime_error("Verbindung fehlgeschlagen. Diagnose zeigt den Grund; danach erneut versuchen.");
            }
            phase=Phase::Agent;
            if(!LiveAgent(pid,stop,10000,false)) throw std::runtime_error("Minecraft antwortet nicht auf den Client. Minecraft neu starten und erneut verbinden.");
        }
        phase=Phase::Overlay; Cancel(stop);
        const auto executable=runtime/L"mcov_overlay.exe";
        const auto eventName=L"Local\\mcov_ready_"+std::to_wstring(pid)+L"_"+std::to_wstring(GetCurrentProcessId());
        WinHandle ready(CreateEventW(nullptr,TRUE,FALSE,eventName.c_str()));
        if(!ready) throw std::runtime_error("Client-Start konnte nicht vorbereitet werden.");
        std::wstring command=L"\""+executable.wstring()+L"\" --no-agent --pid "+std::to_wstring(pid)+L" --ready-event "+eventName;
        STARTUPINFOW startup{}; startup.cb=sizeof(startup); startup.dwFlags=STARTF_USESHOWWINDOW; startup.wShowWindow=SW_HIDE;
        PROCESS_INFORMATION process{};
        if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,runtime.c_str(),&startup,&process))
            throw std::runtime_error("Client konnte nicht gestartet werden. Fehler "+std::to_string(GetLastError())+".");
        WinHandle thread(process.hThread); result.overlay=WinHandle(process.hProcess);
        HANDLE handles[]={ready.get(),result.overlay.get()};
        const auto until=GetTickCount64()+10000;
        while(true) {
            // Einen schon gestarteten Client bei Abbruch offen lassen.
            Cancel(stop);
            const auto signaled=WaitForMultipleObjects(2,handles,FALSE,100);
            if(signaled==WAIT_OBJECT_0) break;
            if(signaled==WAIT_OBJECT_0+1) {
                DWORD code=0; GetExitCodeProcess(result.overlay.get(),&code);
                result.detail+="Overlay exit code "+std::to_string(code)+"\n";
                throw std::runtime_error("Das Overlay konnte nicht oeffnen. Das Overlay-Protokoll unter Diagnose pruefen.");
            }
            if(signaled==WAIT_FAILED||GetTickCount64()>=until) throw std::runtime_error("Der Client-Start dauert zu lange. Diagnose pruefen und erneut versuchen.");
        }
        result.success=true; result.status="Client gestartet. Zu Minecraft wechseln; INSERT oeffnet das Menue (Standard).";
        result.detail+="Client bereit.\nRuntime: "+Utf8(runtime.wstring());
    } catch(const std::exception& e) { result.status=e.what(); }
    MCOV_LOG_INFO("%s\n%s",result.status.c_str(),result.detail.c_str()); return result;
}
std::string LogTail(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary);
    if(!file) return {};
    file.seekg(0,std::ios::end); const auto size=file.tellg();
    if(size>65536) file.seekg(size-std::streamoff(65536)); else file.seekg(0);
    std::string text((std::istreambuf_iterator<char>(file)),{}); return text;
}
std::filesystem::path ChooseJdk(HWND owner) {
    Microsoft::WRL::ComPtr<IFileDialog> dialog;
    if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))) return {};
    DWORD flags=0; dialog->GetOptions(&flags); dialog->SetOptions(flags|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST);
    dialog->SetTitle(L"JDK-Ordner mit bin und release auswaehlen");
    if(FAILED(dialog->Show(owner))) return {};
    Microsoft::WRL::ComPtr<IShellItem> item; if(FAILED(dialog->GetResult(&item))) return {};
    PWSTR path=nullptr; if(FAILED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))) return {};
    std::filesystem::path result(path); CoTaskMemFree(path); return result;
}
struct AppState {
    launcher::View view;
    std::vector<JvmProcess> processes;
    std::atomic<Phase> phase=Phase::Idle;
    std::stop_source stop;
    std::future<Result> job;
    std::uint64_t refreshAt=0,logsAt=0;
    std::filesystem::path agentLog;
    ~AppState() { stop.request_stop(); if(job.valid()) job.wait(); }
    void Refresh() {
        const auto previous=view.selected>=0&&view.selected<int(view.instances.size())?view.instances[view.selected].pid:0;
        processes.clear(); view.instances.clear();
        for(auto& p:EnumerateJvmProcesses()) if(p.looksLikeMinecraft&&p.window) {
            const auto major=ReadJavaMajor(p.javaHome);
            view.instances.push_back({p.pid,Utf8(p.windowTitle),major?"Java "+std::to_string(major):"Java-Version unbekannt",p.alreadyInjected,ClientRunning(p.pid)});
            processes.push_back(std::move(p));
        }
        view.selected=view.instances.empty()?-1:0;
        for(int i=0;i<int(view.instances.size());++i) if(view.instances[i].pid==previous) view.selected=i;
        refreshAt=GetTickCount64()+3000;
    }
};
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    int count=0; auto args=CommandLineToArgvW(GetCommandLineW(),&count);
    if(args&&count==3&&std::wstring(args[1])==L"--verify-package") {
        try { launcher::PreparePackage(args[2]); LocalFree(args); return 0; }
        catch(...) { LocalFree(args); return 1; }
    }
    if(args) LocalFree(args);
    WinHandle singleton(CreateMutexW(nullptr,FALSE,L"Local\\mcov_launcher"));
    if(!singleton) return 1;
    if(GetLastError()==ERROR_ALREADY_EXISTS) {
        if(auto existing=FindWindowW(L"mcov_flat_app",L"mcov - Client")) { ShowWindow(existing,SW_RESTORE); SetForegroundWindow(existing); }
        return 0;
    }
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    int exit=0;
    try {
        const auto root=Storage(); std::filesystem::create_directories(root);
        SetEnvironmentVariableW(L"MCOV_DATA_DIR",root.c_str());
        const auto jdkFile=root/L"launcher-jdk.txt";
        if(auto saved=LogTail(jdkFile); !saved.empty()) SetEnvironmentVariableW(L"MCOV_JDK_HOME",Wide(saved).c_str());
        log::Init("client",(root/L"mcov_client.log").wstring(),false);
#ifdef MCOV_BUNDLED
        // Bestehende lokale Binds/HUD beim ersten Start uebernehmen, nie ersetzen.
        const auto executableDir=std::filesystem::path(ExecutableDirectory());
        const auto savedSettings=root/L"client-settings.ini";
        std::error_code settingsError;
        if(!std::filesystem::exists(savedSettings,settingsError)) {
            auto legacy=executableDir/L"client-settings.ini";
            if(!std::filesystem::exists(legacy,settingsError)&&executableDir.filename()==L"client")
                legacy=executableDir.parent_path()/L"bin"/L"client-settings.ini";
            if(std::filesystem::is_regular_file(legacy,settingsError)) {
                std::filesystem::copy_file(legacy,savedSettings,std::filesystem::copy_options::none,settingsError);
                if(settingsError) MCOV_LOG_WARN("Existing keybind/HUD settings could not be imported: %s",settingsError.message().c_str());
            }
        }
#endif
        {
            AppState state; state.Refresh(); FlatApp app; std::string failure;
            if(!app.Initialize(L"mcov - Client",640,640,&failure)) throw std::runtime_error(failure);
            app.Run([&] {
                const auto now=GetTickCount64();
                if(state.view.busy&&state.job.valid()&&state.job.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
                    auto result=state.job.get(); state.view.busy=false; state.view.error=!result.success; state.view.needsJdk=result.needsJdk;
                    state.view.status=result.status; state.view.diagnostic=result.detail; state.agentLog=result.agentLog; state.Refresh();
                    if(result.success&&GetForegroundWindow()==app.hwnd()) {
                        ShowWindow(static_cast<HWND>(app.hwnd()),SW_MINIMIZE);
                        for(const auto& p:state.processes) if(p.pid==result.pid) SetForegroundWindow(static_cast<HWND>(p.window));
                    }
                }
                if(!state.view.busy&&now>=state.refreshAt) state.Refresh();
                if(state.view.tab==1&&now>=state.logsAt) {
                    state.view.log=LogTail(root/L"mcov_client.log")+"\n"+LogTail(root/L"mcov_overlay.log")+"\n"+LogTail(state.agentLog); state.logsAt=now+1000;
                }
                state.view.phase=PhaseText(state.phase.load());
                const auto a=launcher::Render(state.view,{float(app.width()),float(app.height())});
                app.SetNoDragRect(float(app.width()-74),22,float(app.width()-20),44);
                if(a.close) { state.stop.request_stop(); app.RequestClose(); }
                if(a.minimize) ShowWindow(static_cast<HWND>(app.hwnd()),SW_MINIMIZE);
                if(a.refresh&&!state.view.busy) state.Refresh();
                if(a.openLogs) ShellExecuteW(static_cast<HWND>(app.hwnd()),L"open",root.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
                if(a.chooseJdk&&!state.view.busy) {
                    const auto chosen=ChooseJdk(static_cast<HWND>(app.hwnd()));
                    if(!chosen.empty()) {
                        if(IsX64Java(chosen)&&ReadJavaMajor(chosen)>=17&&
                           std::filesystem::exists(chosen/L"bin"/L"java.exe")&&
                           std::filesystem::exists(chosen/L"bin"/L"javac.exe")&&
                           std::filesystem::exists(chosen/L"bin"/L"attach.dll")) {
                            SetEnvironmentVariableW(L"MCOV_JDK_HOME",chosen.c_str());
                            std::ofstream saved(jdkFile,std::ios::binary|std::ios::trunc); saved<<Utf8(chosen.wstring());
                            state.view.needsJdk=state.view.error=false; state.view.status="JDK gespeichert. Client starten, um die Verbindung erneut zu pruefen.";
                        } else state.view.status="Dieser Ordner ist kein vollstaendiges Windows-x64-JDK. Den JDK-Hauptordner mit bin und release auswaehlen.";
                    }
                }
                if(a.start&&!state.view.busy&&state.view.selected>=0&&state.view.selected<int(state.processes.size())) {
                    const auto target=state.processes[state.view.selected];
                    if(ClientRunning(target.pid)) SetForegroundWindow(static_cast<HWND>(target.window));
                    else {
                        state.view.busy=true; state.view.error=state.view.needsJdk=false; state.stop=std::stop_source{};
                        const auto token=state.stop.get_token();
                        state.job=std::async(std::launch::async,[&,pid=target.pid,token] { return Connect(pid,root,state.phase,token); });
                    }
                }
            });
        } // Worker wird vor Log-Shutdown beendet; Overlay/JVM bleiben unabhaengig.
        log::Shutdown();
    } catch(const std::exception& e) {
        MessageBoxW(nullptr,Wide(e.what()).c_str(),L"mcov konnte nicht starten",MB_OK|MB_ICONERROR); log::Shutdown(); exit=1;
    }
    CoUninitialize(); return exit;
}
