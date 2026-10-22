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
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
                if(ImGui::Selectable(BlockKindLabel(kind),false,ImGuiSelectableFlags_None,{0,ImGui::GetFrameHeight()})) {
                    selected=!selected; mask=selected?mask|bit:mask&~bit; dirty=true;
                }
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
void Overlay::DrawHoleSettings() {
    bool dirty=false; auto& c=holeSettings_;
    if(panel::BeginRows("hole_settings")) {
        dirty|=panel::CheckRow("Enable Hole ESP",&c.enabled); DrawBind(keys::Action::HoleEsp,true);
        dirty|=panel::SliderIntRow("Scan radius",&c.radius,holes::kMinRadius,holes::kMaxRadius,"%d blocks");
        dirty|=panel::SliderRow("Draw distance",&c.distance,4,64,"%.0f m");
        dirty|=panel::SliderRow("Marker height",&c.height,.05f,1,"%.2f m");
        dirty|=panel::SliderRow("Floor opacity",&c.fill,0,.5f,"%.2f");
        dirty|=panel::CheckRow("Bedrock holes",&c.bedrock);
        dirty|=panel::ColorRow("Bedrock colour",&c.bedrockColour.x);
        dirty|=panel::CheckRow("Obsidian holes",&c.obsidian);
        dirty|=panel::ColorRow("Obsidian colour",&c.obsidianColour.x);
        dirty|=panel::CheckRow("Mixed holes",&c.mixed);
        dirty|=panel::ColorRow("Mixed colour",&c.mixedColour.x);
        dirty|=panel::CheckRow("Hole labels",&c.labels);
        ImGui::EndTable();
    }
    ImGui::TextWrapped("Marks 1x1 holes with three air blocks above a bedrock/obsidian floor and four matching walls. Uses a separate short-range scan; large Block ESP scans do not delay it. Loaded blocks only.");
    if(c.enabled) ImGui::TextDisabled("Found: %u holes",lastSnapshot_.holeCount);
    if(dirty) { panicActive_=false; preferencesDirty_=true; preferenceSaveAt_=GetTickCount64()+500; PushScanRequest(); }
}
void Overlay::DrawSmpSettings(int section) {
    bool dirty=false;
    if(section==0) {
        auto& appearance=rockstarConfig_[rockstar::Feature::ChestEsp]; auto& c=smpConfig_.storage;
        if(panel::BeginRows("storage_settings")) {
            dirty|=panel::CheckRow("Enable Storage ESP",&appearance.enabled); DrawBind(keys::Action::ChestEsp,true);
            dirty|=panel::SliderRow("Scan radius",&appearance.range,kMinBlockScanRadius,kMaxBlockScanRadius,"%.0f blocks");
            dirty|=panel::SliderIntRow("Scan interval",&c.interval,200,3000,"%d ms");
            dirty|=panel::CheckRow("Block names and distance",&c.labels); dirty|=panel::CheckRow("Tracers",&c.tracers);
            dirty|=panel::SliderRow("Opacity",&appearance.opacity,0,1,"%.2f");
            dirty|=panel::SliderRow("Soft glow",&appearance.glow,0,1.5f,"%.2f");
            dirty|=panel::CheckRow("Container textures",&c.textures);
            panel::ValueRow("Visibility",c.textures?"Filled boxes + textures":"Filled boxes");
            ImGui::EndTable();
        }
        panel::Section("Block types and colours"); dirty|=Types(c.mask,&c.colours);
        if(ImGui::Button("Reset colours")) { c.colours=smp::StorageSettings{}.colours; dirty=true; }
        if(!c.mask) ImGui::TextWrapped("Select at least one block type to show Storage ESP.");
        ImGui::TextWrapped("Soft outlines and transparent gradients use each container type's colour. Enable Container textures to also show Minecraft's resource-pack models through walls (1.21.11). Colours also apply to labels and tracers.");
        ImGui::TextWrapped("Searches loaded containers independently of Block ESP and Chunkfinder. Scan interval is the pause after a sweep.");
        if(c.textures&&appearance.enabled&&agentConnected_&&haveSnapshot_) {
            const auto now=GetTickCount64();
            const bool fresh=storageTextureReport_.sampledAtMs&&now>=storageTextureReport_.sampledAtMs&&now-storageTextureReport_.sampledAtMs<1500;
            if(std::strcmp(lastSnapshot_.gameVersion,"1.21.11")) ImGui::TextWrapped("Container textures require Minecraft 1.21.11.");
            else if(!fresh) ImGui::TextDisabled("Waiting for the texture renderer...");
            else switch(storageTextureReport_.status) {
                case base::Status::Unsupported: ImGui::TextWrapped("Texture renderer unavailable. Restart Minecraft with the updated client."); break;
                case base::Status::Failed: ImGui::TextWrapped("Could not render container textures. Turn Storage ESP off and on to retry."); break;
                case base::Status::Building: ImGui::TextDisabled("Loading textures: %u containers checked",storageTextureReport_.scanned); break;
                case base::Status::Ready: ImGui::TextDisabled("Rendering %u loaded containers",storageTextureReport_.blocks); break;
                case base::Status::Empty: ImGui::TextDisabled("No drawable loaded containers"); break;
                default: ImGui::TextDisabled("Waiting for loaded containers"); break;
            }
            if(fresh&&storageTextureReport_.limited) ImGui::TextWrapped("Texture limit reached. Reduce the radius or select fewer types.");
        }
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
    } else if(section==2) {
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
    } else if(section==4) {
        auto& c=smpConfig_.finder;
        if(panel::BeginRows("base_finder_settings")) {
            dirty|=panel::CheckRow("Tall kelp",&c.kelp);
            ImGui::BeginDisabled(!c.kelp);
            dirty|=panel::SliderIntRow("Minimum kelp height",&c.minKelpHeight,6,64,"%d blocks");
            dirty|=panel::SliderIntRow("Tall plants / chunk",&c.minKelpColumns,1,64,"%d");
            ImGui::EndDisabled();
            dirty|=panel::CheckRow("Large amethyst / clusters",&c.amethyst);
            ImGui::BeginDisabled(!c.amethyst);
            dirty|=panel::SliderIntRow("Amethyst blocks / chunk",&c.minAmethyst,1,64,"%d");
            ImGui::EndDisabled();
            ImGui::EndTable();
        }
        if(!ChunkDetection().Mask()) ImGui::TextWrapped("Select deepslate, tall kelp or amethyst to start searching.");
        ImGui::TextWrapped("Marks Possible Base when any selected clue reaches its own minimum in a scanned chunk. Raw deepslate with a horizontal X/Z axis, tall kelp, large amethyst buds and fully grown clusters count. Small and medium buds do not. Kelp is counted once per plant, with height measured up to 64 blocks below its top.");
        ImGui::TextWrapped("Whole chunks and individual clue blocks are highlighted through walls. Chunk frames cover the full 16 x 16 footprint and vanilla build height; custom dimension heights are not sampled.");
        ImGui::TextWrapped("Growth clues can also occur naturally. Only loaded blocks around you are scanned; a marker is a possible base, not confirmation.");
        panel::Section("Possible bases this session");
        const auto finds=baseFinder_.Finds();
        if(finds.empty()) ImGui::TextWrapped(ModuleEnabled(keys::Action::ChunkFinder)?"No possible base found yet. Wait for a completed scan or adjust the minimums.":"Enable Chunkfinder to search for suspicious blocks.");
        else {
            ImGui::TextDisabled("%zu / %d discoveries",finds.size(),smp::BaseFinder::kMaxFinds);
            const bool clear=ImGui::Button("Clear possible bases");
            for(std::size_t i=0;i<finds.size();++i) {
                const auto& f=finds[i]; ImGui::PushID(int(i)); ImGui::Separator();
                ImGui::TextWrapped("Possible Base | Chunk %d, %d",f.chunkX,f.chunkZ);
                ImGui::TextWrapped("%d tall kelp (max %d blocks), %d amethyst, %d deepslate",f.kelpColumns,f.tallestKelp,f.amethyst,f.deepslate);
                char coordinates[64]; std::snprintf(coordinates,sizeof(coordinates),"%.0f %.0f %.0f",std::floor(f.centre.x),std::floor(f.centre.y),std::floor(f.centre.z));
                ImGui::TextUnformatted(coordinates);
                if(ImGui::SmallButton("Copy coordinates")) ImGui::SetClipboardText(coordinates);
                ImGui::SameLine();
                if(ImGui::SmallButton("Save waypoint")) {
                    auto position=lastSnapshot_; position.posX=float(f.centre.x); position.posY=float(f.centre.y); position.posZ=float(f.centre.z);
                    char name[kNameLen]; std::snprintf(name,sizeof(name),"Possible Base %d, %d",f.chunkX,f.chunkZ);
                    if(rockstarPack_.AddMarker(position,name)) { rockstarConfig_[rockstar::Feature::Waypoints].enabled=true; dirty=true; }
                    else rockstarPack_.Notify("Waypoint list is full. Remove a waypoint first.",double(GetTickCount64())/1000);
                }
                ImGui::PopID();
            }
            if(clear) baseFinder_.Clear();
            if(finds.size()==smp::BaseFinder::kMaxFinds) ImGui::TextWrapped("Discovery limit reached. Save locations and clear the list to record more.");
        }
        ImGui::TextWrapped("Discoveries retain their strongest observation and may be outdated. Changing server, dimension, detection filters, radius or minimums clears the list. Save a waypoint to keep a location after restart.");
    } else {
        auto& c=smpConfig_.base; auto& detection=smpConfig_.stash;
        if(panel::BeginRows("base_settings")) {
            dirty|=panel::CheckRow("Enable Base ESP",&c.enabled); DrawBind(keys::Action::BaseEsp,true);
            const char* modes[]={"Minecraft textures","Area marker"}; int mode=c.textured?0:1;
            if(panel::ComboRow("Display",&mode,modes,2)) { c.textured=mode==0; dirty=true; }
            dirty|=panel::SliderIntRow("Scan radius",&detection.radius,4,48,"%d blocks");
            dirty|=panel::SliderIntRow("Minimum containers / chunk",&detection.minContainers,2,128,"%d");
            dirty|=panel::SliderRow("Draw distance",&c.maxDraw,8,512,"%.0f m");
            dirty|=panel::SliderRow("Expand detected area",&c.padding,0,16,"%.1f blocks");
            if(c.textured) dirty|=panel::SliderRow("Texture opacity",&c.textureOpacity,0,1,"%.2f");
            else {
                dirty|=panel::SliderRow("Fill opacity",&c.fillOpacity,0,.5f,"%.2f");
                dirty|=panel::SliderRow("Frame width",&c.lineWidth,.5f,4,"%.1f px");
            }
            dirty|=panel::ColorRow(c.textured?"Label colour":"Base colour",&c.colour.x);
            dirty|=panel::CheckRow("Labels",&c.labels); dirty|=panel::CheckRow("Tracers",&c.tracers);
            panel::ValueRow("Visibility","Through walls");
            ImGui::EndTable();
        }
        ImGui::TextWrapped(c.textured?"Renders loaded blocks in the detected area through walls, using Minecraft's models and current resource-pack textures (1.21.11).":"A transparent 3D area and frame mark detected container clusters through walls.");
        if(c.textured&&c.enabled&&agentConnected_&&haveSnapshot_) {
            const bool fresh=baseReport_.sampledAtMs&&GetTickCount64()-baseReport_.sampledAtMs<1500;
            if(std::strcmp(lastSnapshot_.gameVersion,"1.21.11")) ImGui::TextWrapped("Minecraft textures require 1.21.11. Use Area marker on this version.");
            else if(!fresh) ImGui::TextDisabled("Waiting for the texture renderer...");
            else switch(baseReport_.status) {
                case base::Status::Unsupported: ImGui::TextWrapped("Texture renderer unavailable. Restart Minecraft with the updated client or choose Area marker."); break;
                case base::Status::Failed: ImGui::TextWrapped("Could not render game textures. Turn Base ESP off and on to retry, or choose Area marker."); break;
                case base::Status::Building: ImGui::TextDisabled("Loading area: %u blocks checked",baseReport_.scanned); break;
                case base::Status::Ready: ImGui::TextDisabled("Rendering %u loaded blocks",baseReport_.blocks); break;
                case base::Status::Empty: ImGui::TextDisabled("No drawable loaded blocks in the detected area"); break;
                default: ImGui::TextDisabled("Waiting for a detected, loaded area"); break;
            }
            if(fresh&&baseReport_.limited) ImGui::TextWrapped("Area limit reached. Reduce the expansion or draw distance to show smaller areas in full.");
            if(fresh&&baseReport_.skipped) ImGui::TextDisabled("Some special render effects could not be drawn (%u)",baseReport_.skipped);
        }
        ImGui::TextDisabled("Detected areas: %zu",stashFinder_.Finds().size());
        if(!detection.mask) ImGui::TextWrapped("Choose container types under Stash Finder to detect bases.");
        ImGui::TextWrapped("Base ESP works on its own. Detection uses container clusters plus your chosen expansion; it does not trace an entire building. Radius, minimum and container filters are shared with Stash Finder.");
    }
    if(section==0) {
        if(!agentConnected_||!haveSnapshot_) ImGui::TextWrapped("Connect to Minecraft and join a world to scan containers.");
        else if(lastSnapshot_.storageState==storage::State::Scanning) ImGui::TextDisabled("%s: %u%% | %u matching containers",
            lastSnapshot_.storageFast?"Scanning loaded containers":"Compatibility block scan",lastSnapshot_.storageProgress,lastSnapshot_.storageCount);
        else if(lastSnapshot_.storageState==storage::State::Unavailable) ImGui::TextWrapped("Container scan could not finish. Retrying; check Settings > Agent if this persists.");
        else if(lastSnapshot_.storageState==storage::State::Ready) ImGui::TextDisabled("%s | %u matching containers",lastSnapshot_.storageFast?"Loaded-container scan finished":"Compatibility scan finished",lastSnapshot_.storageCount);
        else ImGui::TextDisabled("Waiting for container scan");
        if(lastSnapshot_.storageLimited) ImGui::TextWrapped("Container result limit reached. Reduce the radius or select fewer types.");
    } else DrawScanStatus();
    if(dirty) { smpConfig_.Clamp(); rockstarConfig_.Clamp(); panicActive_=false; preferencesDirty_=true; preferenceSaveAt_=GetTickCount64()+500; PushScanRequest(); }
}
} // namespace mcov
