#include "overlay.hpp"
#include <windows.h>
#include <cstdio>
#include <sstream>

namespace mcov {
namespace {
namespace panel=control_panel;
bool Types(std::uint32_t& mask,std::array<ImVec4,smp::kStorageKinds.size()>* colours=nullptr) {
    bool dirty=false;
    if(panel::BeginRows(colours?"storage_colours":"stash_types")) {
        for(std::size_t i=0;i<smp::kStorageKinds.size();++i) {
            const auto kind=smp::kStorageKinds[i]; const auto bit=1u<<kind;
            bool selected=(mask&bit)!=0; ImGui::PushID(kind);
            if(colours) {
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(BlockKindLabel(kind));
                ImGui::TableSetColumnIndex(1);
                if(ImGui::Checkbox("##visible",&selected)) { mask=selected?mask|bit:mask&~bit; dirty=true; }
                ImGui::SameLine(); dirty|=ImGui::ColorEdit4("##colour",&(*colours)[i].x,ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_AlphaPreview);
                if(ImGui::IsItemHovered()) ImGui::SetTooltip("Change %s colour",BlockKindLabel(kind));
            } else if(panel::CheckRow(BlockKindLabel(kind),&selected)) { mask=selected?mask|bit:mask&~bit; dirty=true; }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    return dirty;
}
}
void Overlay::DrawScanStatus() {
    if(!agentConnected_||!haveSnapshot_) { ImGui::TextWrapped("Connect to Minecraft and join a world to scan blocks."); return; }
    if(!lastSnapshot_.dimension[0]) { ImGui::TextWrapped("Join a world to start scanning."); return; }
    if(const auto* block=channel_.block()) {
        const auto state=static_cast<ScanState>(block->scanState.load(std::memory_order_acquire));
        if(state==ScanState::MissingBindings) { ImGui::TextWrapped("Block scanning is unavailable for this runtime. Check Settings > Agent."); return; }
        if(state==ScanState::Scanning) ImGui::TextDisabled("Scanning loaded blocks: %u%%",block->scanProgress.load(std::memory_order_acquire));
        else if(state==ScanState::Ready) ImGui::TextDisabled("Scan finished | %u matching blocks",lastSnapshot_.blockCount);
        else ImGui::TextDisabled("Scan idle");
    }
    if(lastSnapshot_.blockScanTruncated) ImGui::TextWrapped("Result limit reached (1024 blocks). Counts are lower bounds; reduce the radius or select fewer types.");
}
void Overlay::DrawSmpSettings(int section) {
    bool dirty=false;
    if(section==0) {
        auto& appearance=rockstarConfig_[rockstar::Feature::ChestEsp]; auto& c=smpConfig_.storage;
        if(panel::BeginRows("storage_settings")) {
            dirty|=panel::CheckRow("Enable Storage ESP",&appearance.enabled); DrawBind(keys::Action::ChestEsp,true);
            dirty|=panel::SliderRow("Scan radius",&appearance.range,4,48,"%.0f blocks");
            dirty|=panel::CheckRow("Block names and distance",&c.labels); dirty|=panel::CheckRow("Tracers",&c.tracers);
            dirty|=panel::SliderRow("Opacity",&appearance.opacity,0,1,"%.2f"); dirty|=panel::SliderRow("Glow",&appearance.glow,0,2,"%.2f");
            ImGui::EndTable();
        }
        panel::Section("Block types and colours"); dirty|=Types(c.mask,&c.colours);
        if(ImGui::Button("Reset storage colours")) { c.colours=smp::StorageSettings{}.colours; dirty=true; }
        if(!c.mask) ImGui::TextWrapped("Select at least one block type to show Storage ESP.");
        ImGui::TextWrapped("Each type uses its own colour. Only blocks available in the client world can be found.");
    } else if(section==1) {
        auto& c=smpConfig_.stash;
        if(panel::BeginRows("stash_settings")) {
            dirty|=panel::CheckRow("Enable Stash Finder",&c.enabled); DrawBind(keys::Action::StashFinder,true);
            dirty|=panel::SliderIntRow("Scan radius",&c.radius,4,48,"%d blocks");
            dirty|=panel::SliderIntRow("Scan interval",&c.interval,200,3000,"%d ms");
            dirty|=panel::SliderIntRow("Minimum containers / chunk",&c.minContainers,2,128,"%d");
            dirty|=panel::SliderRow("Marker distance",&c.maxDraw,8,512,"%.0f m");
            dirty|=panel::CheckRow("Labels",&c.labels); dirty|=panel::CheckRow("Tracers",&c.tracers);
            dirty|=panel::CheckRow("Notify on new stash",&c.notifications); dirty|=panel::ColorRow("Marker colour",&c.colour.x);
            ImGui::EndTable();
        }
        panel::Section("Count these containers"); dirty|=Types(c.mask);
        if(!c.mask) ImGui::TextWrapped("Select container types to start finding stashes.");
        ImGui::TextWrapped("Finds chunks with enough selected container blocks. A double chest counts as two. Ender chests are excluded by default. Counts cover the scanned part of a chunk and do not reveal inventory contents.");
        if(c.notifications&&(!hud_.visible||!HudEnabled(int(HudModule::Notifications))))
            ImGui::TextWrapped("Enable HUD > Notifications to see stash messages.");
        panel::Section("Discoveries this session");
        const auto finds=stashFinder_.Finds();
        if(finds.empty()) ImGui::TextWrapped(c.enabled?"No stash found yet. Let a scan finish or lower the minimum.":"Enable Stash Finder to search for container clusters.");
        else {
            ImGui::TextDisabled("%zu / %d discoveries",finds.size(),smp::StashFinder::kMaxFinds);
            if(ImGui::Button("Copy all coordinates")) {
                std::ostringstream text;
                for(const auto& f:finds) text<<static_cast<int>(std::floor(f.centre.x))<<' '<<static_cast<int>(std::floor(f.centre.y))<<' '<<static_cast<int>(std::floor(f.centre.z))<<" | "<<f.count<<" containers\n";
                ImGui::SetClipboardText(text.str().c_str());
            }
            ImGui::SameLine(); const bool clear=ImGui::Button("Clear discoveries");
            for(std::size_t i=0;i<finds.size();++i) {
                const auto& f=finds[i]; ImGui::PushID(static_cast<int>(i)); ImGui::Separator();
                char coordinates[64]; std::snprintf(coordinates,sizeof(coordinates),"%.0f %.0f %.0f",std::floor(f.centre.x),std::floor(f.centre.y),std::floor(f.centre.z));
                ImGui::TextWrapped("Chunk %d, %d | %d containers",f.chunkX,f.chunkZ,f.count); ImGui::TextUnformatted(coordinates);
                if(ImGui::SmallButton("Copy coordinates")) ImGui::SetClipboardText(coordinates);
                ImGui::SameLine();
                if(ImGui::SmallButton("Save waypoint")) {
                    auto position=lastSnapshot_; position.posX=static_cast<float>(f.centre.x); position.posY=static_cast<float>(f.centre.y); position.posZ=static_cast<float>(f.centre.z);
                    char name[kNameLen]; std::snprintf(name,sizeof(name),"Stash %d, %d",f.chunkX,f.chunkZ);
                    if(rockstarPack_.AddMarker(position,name)) { rockstarConfig_[rockstar::Feature::Waypoints].enabled=true; dirty=true; }
                    else rockstarPack_.Notify("Waypoint list is full. Remove a waypoint first.",double(GetTickCount64())/1000);
                }
                ImGui::PopID();
            }
            if(clear) stashFinder_.Clear();
        }
        if(stashFinder_.Full()) ImGui::TextWrapped("Discovery limit reached. Copy coordinates and clear the list to record more.");
        ImGui::TextWrapped("Discoveries stay while you travel. They retain the highest observed count and may be outdated. Changing server, dimension, filters or minimum clears the list. Save a waypoint to keep a location after restart.");
    } else {
        auto& c=smpConfig_.spawner;
        if(panel::BeginRows("spawner_settings")) {
            dirty|=panel::CheckRow("Enable Spawner ESP",&c.enabled); DrawBind(keys::Action::SpawnerEsp,true);
            dirty|=panel::SliderIntRow("Scan radius",&c.radius,4,48,"%d blocks");
            dirty|=panel::SliderIntRow("Scan interval",&c.interval,200,3000,"%d ms");
            dirty|=panel::SliderRow("Draw distance",&c.maxDraw,8,128,"%.0f m");
            dirty|=panel::CheckRow("Labels",&c.labels); dirty|=panel::CheckRow("Tracers",&c.tracers);
            dirty|=panel::SliderRow("Glow",&c.glow,0,2,"%.2f"); dirty|=panel::ColorRow("Spawner colour",&c.colour.x);
            ImGui::EndTable();
        }
        ImGui::TextWrapped("Highlights scanned mob spawner blocks. The mob type and activation state are not sampled.");
    }
    DrawScanStatus();
    if(dirty) { smpConfig_.Clamp(); rockstarConfig_.Clamp(); panicActive_=false; preferencesDirty_=true; preferenceSaveAt_=GetTickCount64()+500; PushScanRequest(); }
}
} // namespace mcov
