#include "client_config.hpp"
#include "../common/client_paths.hpp"
#include "../common/compact_theme.hpp"
#include "../common/win_handle.hpp"
#include <fstream>
#include <algorithm>
#include <cstdio>

namespace mcov {
namespace {
std::filesystem::path ConfigDirectory(const std::filesystem::path& preferences) {
    return (preferences.empty()?ClientDirectory():preferences.parent_path())/L"configs";
}
bool PlainDirectory(const std::filesystem::path& directory,bool create) {
    std::error_code error;
    if(create) std::filesystem::create_directories(directory,error);
    const DWORD attributes=GetFileAttributesW(directory.c_str());
    return !error&&attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_DIRECTORY)&&!(attributes&FILE_ATTRIBUTE_REPARSE_POINT);
}
bool PlainFile(const std::filesystem::path& file,bool allowMissing=false) {
    const DWORD attributes=GetFileAttributesW(file.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES)
        return allowMissing&&(GetLastError()==ERROR_FILE_NOT_FOUND);
    return !(attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
}
bool SameName(const std::string& a,const std::string& b) { return _stricmp(a.c_str(),b.c_str())==0; }
std::string PathText(const std::filesystem::path& path) {
    const auto bytes=path.u8string(); return {reinterpret_cast<const char*>(bytes.data()),bytes.size()};
}
}

client_configs::Snapshot Overlay::CaptureConfig() const {
    client_configs::Snapshot c;
    c.visuals=visuals_; c.overlay=settings_; c.combat=combat_;
    c.killAura=combatMenu_.Store().Read().values; c.relations=combatMenu_.Relations().Read();
    c.aura=auraConfig_; c.marker=targetCubeConfig_; c.world=worldVisualsConfig_;
    c.friendHat=friendHatConfig_; c.hat=localHatConfig_; c.blocks=blocks_; c.chunks=chunkFinder_;
    c.smp=smpConfig_; c.crystal=crystalMacro_; c.anchor=anchorMacro_; c.hud=hud_; c.binds=bindings_;
    c.knife=knifeConfig_;
    c.extra=rockstarConfig_; c.waypoints=rockstarPack_.Markers(); c.reducedMotion=menu_.reduceMotion;
    return c;
}
void Overlay::ApplyConfig(const client_configs::Snapshot& c) {
    // The complete file has already passed schema/range checks before this point.
    visuals_=c.visuals; settings_=c.overlay; combat_=c.combat;
    combatMenu_.Store().Publish(c.killAura);
    auto& book=combatMenu_.Relations();
    for(const auto& r:book.Read()) if(r.used) book.Set(r.uuid,combat::Relation::Neutral);
    for(const auto& r:c.relations) if(r.used) book.Set(r.uuid,r.relation);
    auraConfig_=c.aura; targetCubeConfig_=c.marker; worldVisualsConfig_=c.world;
    friendHatConfig_=c.friendHat; localHatConfig_=c.hat; blocks_=c.blocks; chunkFinder_=c.chunks;
    smpConfig_=c.smp; crystalMacro_=c.crystal; anchorMacro_=c.anchor; hud_=c.hud; bindings_=c.binds;
    NormalizeChunkFinder();
    knifeConfig_=c.knife; knifeInputAllowed_=false; ++knifeInspectSerial_;
    rockstarConfig_=c.extra; rockstarPack_.Markers()=c.waypoints; menu_.reduceMotion=c.reducedMotion;
    // Discard old Hold-key leases and visual history on explicit profile changes.
    keyEngine_={}; captureAction_=-1; captureArmed_=false; captureModifier_=0;
    nextClickTick_=0; panicActive_=false; moduleObservationReady_=false;
    worldVisuals_.Reset(); auraRenderer_.Reset(); targetCube_.Reset();
    rockstarPack_.ResetWorld(); stashFinder_.Reset(); baseFinder_.Reset();
    if(hwnd_) { ApplyCaptureAffinity(); ApplyExtendedStyles(!menuVisible_&&!hudEditing_&&settings_.clickThroughWhenIdle); }
    PushScanRequest(); PushCombatRequest();
}
void Overlay::RefreshConfigs() {
    configListReady_=true; configNames_.clear();
    const auto directory=ConfigDirectory(preferencesPath_);
    std::error_code error;
    const bool exists=std::filesystem::exists(directory,error);
    if(error) { configError_=true; configStatus_="Cannot read configs. Check folder permissions, then Refresh."; return; }
    if(!exists) { selectedConfig_.clear(); return; }
    if(!PlainDirectory(directory,false)) { configError_=true; configStatus_="Configs must be a regular folder beside the client. Check it, then Refresh."; return; }
    auto iterator=std::filesystem::directory_iterator(directory,error);
    for(const auto end=std::filesystem::directory_iterator();!error&&iterator!=end;iterator.increment(error)) {
        const auto& path=iterator->path();
        if(_wcsicmp(path.extension().c_str(),L".ini")==0&&PlainFile(path)) {
            const auto name=PathText(path.stem());
            if(client_configs::ValidName(name)) configNames_.push_back(name);
        }
        if(configNames_.size()>=256) break;
    }
    std::sort(configNames_.begin(),configNames_.end(),[](const auto& a,const auto& b) { return _stricmp(a.c_str(),b.c_str())<0; });
    if(error) { configError_=true; configStatus_="Cannot list configs. Check folder permissions, then Refresh."; }
    if(std::none_of(configNames_.begin(),configNames_.end(),[&](const auto& name){return SameName(name,selectedConfig_);})) selectedConfig_.clear();
}
bool Overlay::SaveConfig(const std::string& name,bool overwrite) {
    const auto fail=[&](const char* message) { configError_=true; configStatus_=message; return false; };
    if(!client_configs::ValidName(name)) return fail("Use 1-48 letters, numbers, spaces, - or _. Avoid reserved Windows names.");
    std::string text;
    if(!client_configs::Encode(CaptureConfig(),text)) return fail("Settings contain an invalid value or duplicate keybind. Correct it, then Save again.");
    const auto directory=ConfigDirectory(preferencesPath_);
    if(!PlainDirectory(directory,true)) return fail("Cannot create configs beside the client. Check folder permissions, then Save again.");
    const auto file=directory/(name+".ini");
    if(!PlainFile(file,true)) return fail("This config name belongs to a folder or linked file. Choose another name.");
    const auto collision=[&] {
        RefreshConfigs();
        return fail("This config already exists. Use Overwrite config to replace it, or choose a new name.");
    };
    if(!overwrite&&GetFileAttributesW(file.c_str())!=INVALID_FILE_ATTRIBUTES) return collision();
    const auto temporary=directory/(name+".ini.tmp-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    struct Cleanup { std::filesystem::path file; bool owned=false; ~Cleanup() { if(owned) { std::error_code ignored; std::filesystem::remove(file,ignored); } } } cleanup{temporary};
    {
        WinHandle handle(CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));
        if(!handle) return fail("Cannot save config. Check folder permissions and free space, then retry.");
        cleanup.owned=true;
        DWORD written=0;
        if(!WriteFile(handle.get(),text.data(),DWORD(text.size()),&written,nullptr)||written!=text.size()||!FlushFileBuffers(handle.get()))
            return fail("Config could not be written completely. Free space and retry; the previous file is kept.");
    }
    if(!MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_WRITE_THROUGH|(overwrite?MOVEFILE_REPLACE_EXISTING:0))) {
        const auto error=GetLastError();
        if(!overwrite&&(error==ERROR_ALREADY_EXISTS||error==ERROR_FILE_EXISTS)) return collision();
        return fail("Config is locked or cannot be replaced. Close other editors, then retry.");
    }
    selectedConfig_=name; std::snprintf(configName_,sizeof(configName_),"%s",name.c_str());
    configStatus_="Saved \""+name+"\"."; configError_=false; RefreshConfigs();
    return !configError_;
}
bool Overlay::LoadConfig(const std::string& name) {
    const auto fail=[&](const char* message) { configError_=true; configStatus_=message; return false; };
    if(!client_configs::ValidName(name)) return fail("Choose a config from the list, then Load.");
    const auto directory=ConfigDirectory(preferencesPath_); const auto file=directory/(name+".ini");
    if(!PlainDirectory(directory,false)||!PlainFile(file)) return fail("Config is missing or linked. Refresh the list and choose a regular config file.");
    std::error_code error; const auto size=std::filesystem::file_size(file,error);
    if(error||size>256*1024) return fail("Config cannot be read or is too large. Choose another file.");
    std::ifstream in(file,std::ios::binary); std::string text(std::size_t(size),'\0');
    if(!in.read(text.data(),std::streamsize(text.size()))||in.peek()!=std::char_traits<char>::eof())
        return fail("Config could not be read completely. Close other editors and retry.");
    client_configs::Snapshot candidate;
    if(!client_configs::Decode(text,candidate)) return fail("Invalid or unsupported config. Current settings are kept; choose another file.");
    ApplyConfig(candidate); preferencesDirty_=true; SavePreferences();
    selectedConfig_=name; std::snprintf(configName_,sizeof(configName_),"%s",name.c_str());
    configError_=false; configStatus_="Loaded \""+name+"\".";
    if(!preferenceStatus_.empty()) { configError_=true; configStatus_+=" Automatic settings could not be saved. Check client folder permissions."; }
    return true;
}
void Overlay::DrawConfigSettings() {
    if(!configListReady_) RefreshConfigs();
    ImGui::TextWrapped("Save modules, settings, keybinds and HUD layout. Loading restores the saved module states.");
    ImGui::Spacing(); ImGui::TextUnformatted("Saved configs");
    if(configNames_.empty()) ImGui::TextWrapped("No configs yet. Enter a name below and save your current setup.");
    else {
        const float height=ImGui::GetTextLineHeightWithSpacing()*std::min(5,int(configNames_.size()))+8;
        if(ImGui::BeginListBox("##config_list",{-1,height})) {
            for(const auto& name:configNames_) {
                ImGui::PushID(name.c_str());
                if(ImGui::Selectable(name.c_str(),SameName(name,selectedConfig_))) {
                    selectedConfig_=name; std::snprintf(configName_,sizeof(configName_),"%s",name.c_str());
                    configStatus_.clear(); configError_=false;
                }
                if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",name.c_str());
                ImGui::PopID();
            }
            ImGui::EndListBox();
        }
    }
    ImGui::Spacing(); ImGui::TextUnformatted("Config name"); ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##config_name","e.g. My setup",configName_,sizeof(configName_));
    const bool validName=client_configs::ValidName(configName_);
    const bool overwrite=std::any_of(configNames_.begin(),configNames_.end(),[&](const auto& name){return SameName(name,configName_);});
    ImGui::BeginDisabled(!validName);
    if(ImGui::Button(overwrite?"Overwrite config":"Save config",{-1,0})) SaveConfig(configName_,overwrite);
    ImGui::EndDisabled();
    const float width=std::max(1.f,(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x)*.5f);
    ImGui::BeginDisabled(selectedConfig_.empty());
    if(ImGui::Button("Load selected",{width,0})) LoadConfig(selectedConfig_);
    ImGui::EndDisabled(); ImGui::SameLine();
    if(ImGui::Button("Refresh",{width,0})) { configStatus_.clear(); configError_=false; RefreshConfigs(); }
    if(!validName&&configName_[0]) ImGui::TextWrapped("Use letters, numbers, spaces, - or _. No leading/trailing spaces or reserved Windows names.");
    if(!configStatus_.empty()) {
        ImGui::Spacing(); ImGui::PushStyleColor(ImGuiCol_Text,configError_?style::compact::Danger:style::compact::Positive);
        ImGui::TextWrapped("%s",configStatus_.c_str()); ImGui::PopStyleColor();
    }
    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    ImGui::TextWrapped("Stored in configs beside mcov.exe. Copy this folder with the client to keep your profiles.");
    ImGui::TextUnformatted("Folder");
    std::string path=PathText(ConfigDirectory(preferencesPath_)); ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##config_folder",path.data(),path.size()+1,ImGuiInputTextFlags_ReadOnly);
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",path.c_str());
}
} // namespace mcov
