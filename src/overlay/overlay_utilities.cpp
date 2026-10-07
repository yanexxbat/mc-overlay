#include "overlay.hpp"
#include "../common/chunk_hints.hpp"
#include <cstdio>

namespace mcov {
namespace panel=control_panel;

items::Request Overlay::BuildItemRequest(std::uint64_t now,bool inputAllowed,bool combatActive) const {
    items::Request r;
    r.sentAtMs=now; r.inputAllowed=inputAllowed; r.combatActive=combatActive;
    r.totem=combat_.autoTotem; r.crystal=crystalMacro_; r.anchor=anchorMacro_;
    // Minecraft liest seine eigene, eventuell umbelegte Place/Use-Taste im Client-Tick.
    return r;
}

void Overlay::DrawItemAssistSettings(int section) {
    const bool totem=section==0,anchor=section==2;
    const auto action=totem?keys::Action::AutoTotem:anchor?keys::Action::AnchorMacro:keys::Action::CrystalMacro;
    auto& macro=anchor?anchorMacro_:crystalMacro_;
    auto status=totem?itemReport_.totem:anchor?itemReport_.anchor:itemReport_.crystal;
    if(!agentConnected_) status=items::Status::Unsupported;
    bool changed=false;
    if(panel::BeginRows("item_assist_controls")) {
        panel::CheckRow(totem?"Enable Auto Totem":"Enable macro",totem?&combat_.autoTotem.enabled:&macro.enabled);
        DrawBind(action,true);
        if(totem) {
            int retry=static_cast<int>(combat_.autoTotem.retryMs);
            if(panel::SliderIntRow("Retry interval",&retry,50,500,"%d ms")) combat_.autoTotem.retryMs=retry;
        } else {
            panel::ValueRow("Activation","Hold Minecraft Place / Use");
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
            if(ImGui::Button("Server conservative")) { macro.Conservative(); changed=true; }
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("Short range, slower actions and strict world confirmation. Server acceptance is not guaranteed.");
            changed|=panel::CheckRow("Separate action delays",&macro.separateDelays);
            const auto delay=[&](const char* label,std::uint32_t& value,int low,int high) {
                int ms=static_cast<int>(value);
                if(panel::SliderIntRow(label,&ms,low,high,"%d ms")) { value=ms; changed=true; }
            };
            if(macro.separateDelays) {
                delay("Place delay",macro.placeMs,50,1000);
                if(anchor) delay("Charge delay",macro.chargeMs,50,1000);
                delay(anchor?"Detonate delay":"Break delay",macro.breakMs,50,1000);
            } else delay("Action interval",macro.intervalMs,100,500);
            int confirmation=static_cast<int>(macro.confirmationMs);
            if(panel::SliderIntRow("World confirmation timeout",&confirmation,100,2000,"%d ms")) { macro.confirmationMs=confirmation; changed=true; }
            changed|=panel::CheckRow("Strict world confirmation",&macro.strictConfirmation);
            changed|=panel::SliderRow("Maximum range",&macro.maxRange,1,6,"%.1f m");
            changed|=panel::SliderRow("Minimum health (0 = off)",&macro.minHealth,0,20,"%.0f HP");
            changed|=panel::CheckRow("Require offhand totem",&macro.requireTotem);
            changed|=panel::CheckRow("Restore previous slot",&macro.restoreSlot);
            if(anchor) {
                changed|=panel::CheckRow("Charge to 4 with offhand glowstone",&macro.chargeWithOffhandFuel);
                changed|=panel::SliderIntRow("Detonation slot (0 = auto)",&macro.detonationSlot,0,9,"%d");
            }
        }
        const auto count=totem?itemReport_.swaps:anchor?itemReport_.anchorActions:itemReport_.crystalActions;
        char text[32]; std::snprintf(text,sizeof(text),"%llu",static_cast<unsigned long long>(count));
        panel::ValueRow(totem?"Swaps sent":"Actions sent",text);
        ImGui::EndTable();
    }
    if(changed) { macro.Clamp(); preferencesDirty_=true; preferenceSaveAt_=GetTickCount64()+500; }
    panel::Section("Live status");
    ImGui::TextWrapped("%s",items::Text(status));
    panel::Section("How to use");
    if(totem) ImGui::TextWrapped("Put a totem in the hotbar or main inventory. It moves to the offhand when needed. Chest screens and items held by the cursor pause swapping.");
    else if(anchor) ImGui::TextWrapped("Enable the macro, select an anchor and hold Minecraft's Place / Use key on a placement block or anchor. It repeatedly places, charges and detonates at the saved position. Keep glowstone and a non-glowstone item in the hotbar. Respawn dimensions pause detonation.");
    else ImGui::TextWrapped("Enable the macro, select end crystals and hold Minecraft's Place / Use key on obsidian or bedrock. It repeatedly places and breaks the crystal at that position; no re-aiming between steps is needed.");
    if(!totem) ImGui::TextWrapped("Release Place / Use to stop. A manual slot change, looking away, menus or focus loss also stops the cycle. The optional module keybind arms the macro; it does not replace Minecraft's Use key. Kill Aura pauses macros.");
    if(!totem) ImGui::TextWrapped("Minimum health and the totem check pause the cycle until satisfied. Range never exceeds Minecraft's own reach. Strict confirmation suppresses retries after a timeout; release Use to reset. Timing and protection settings are saved automatically.");
}

void Overlay::DrawChunkFinderSettings() {
    if(panel::BeginRows("chunkfinder_controls")) {
        panel::CheckRow("Enable Chunkfinder",&chunkFinder_.enabled); DrawBind(keys::Action::ChunkFinder,true);
        panel::SliderIntRow("Scan radius",&chunkFinder_.radius,4,48,"%d blocks");
        panel::SliderIntRow("Scan interval",&chunkFinder_.interval,200,3000,"%d ms");
        panel::SliderIntRow("Minimum hints / chunk",&chunkFinder_.minHints,1,32,"%d");
        panel::SliderRow("Draw distance",&chunkFinder_.maxDraw,8,128,"%.0f m");
        panel::CheckRow("Highlight blocks",&chunkFinder_.blocks);
        panel::CheckRow("Chunk labels",&chunkFinder_.labels);
        panel::ColorRow("Hint colour",&chunkFinder_.color.x);
        unsigned count=0;
        for(unsigned i=0;i<std::min(lastSnapshot_.blockCount,kMaxBlocks);++i)
            count+=lastSnapshot_.blocks[i].kind==kBlockRotatedDeepslate;
        char text[48]; std::snprintf(text,sizeof(text),"%u",count); panel::ValueRow("Block hints",text);
        if(const auto* block=channel_.block()) {
            const auto state=static_cast<ScanState>(block->scanState.load(std::memory_order_acquire));
            const char* label=state==ScanState::Scanning?"Scanning":state==ScanState::Ready?"Ready":state==ScanState::MissingBindings?"Binding unavailable":"Off";
            std::snprintf(text,sizeof(text),"%s (%u%%)",label,block->scanProgress.load(std::memory_order_acquire));
            panel::ValueRow("Scanner",text);
        } else panel::ValueRow("Scanner","Waiting for agent");
        ImGui::EndTable();
    }
    ImGui::TextWrapped("Finds raw deepslate with a horizontal X/Z axis in loaded blocks near you. Labels show chunk coordinates and hint count. These are clues, not proof of a base; world generation and other builds can produce them too.");
    if(lastSnapshot_.blockCount>=kMaxBlocks) ImGui::TextWrapped("Result limit reached. Reduce radius or disable other Block ESP filters.");
}

void Overlay::DrawChunkFinder(const WorldSnapshot& s,const CameraProjection& projection) {
    if(!chunkFinder_.enabled||!s.blockCount) return;
    // Bei Radius <= 48 passen alle betroffenen Chunks in dieses feste Array.
    struct Group { int x{},z{},count{}; double y{}; };
    std::array<Group,64> groups{}; int size=0;
    const auto valid=[&](const BlockSnapshot& b) {
        const double dx=b.x+.5-s.camX,dy=b.y+.5-s.camY,dz=b.z+.5-s.camZ;
        return b.kind==kBlockRotatedDeepslate&&std::isfinite(dx+dy+dz)&&
            dx*dx+dy*dy+dz*dz<=chunkFinder_.maxDraw*chunkFinder_.maxDraw;
    };
    auto find=[&](const BlockSnapshot& b) {
        const int x=ChunkCoordinate(b.x),z=ChunkCoordinate(b.z);
        for(int i=0;i<size;++i) if(groups[i].x==x&&groups[i].z==z) return i;
        return -1;
    };
    for(unsigned i=0;i<std::min(s.blockCount,kMaxBlocks);++i) {
        const auto& b=s.blocks[i]; if(!valid(b)) continue;
        int group=find(b);
        if(group<0) { if(size==int(groups.size())) continue; group=size++; groups[group].x=ChunkCoordinate(b.x); groups[group].z=ChunkCoordinate(b.z); }
        ++groups[group].count; groups[group].y+=b.y+1;
    }
    auto* draw=ImGui::GetBackgroundDrawList(); const auto origin=ImGui::GetMainViewport()->Pos;
    const ImU32 color=ImGui::ColorConvertFloat4ToU32(chunkFinder_.color);
    if(chunkFinder_.blocks) for(unsigned i=0;i<std::min(s.blockCount,kMaxBlocks);++i) {
        const auto& b=s.blocks[i]; if(!valid(b)) continue;
        const int group=find(b); if(group<0||groups[group].count<chunkFinder_.minHints) continue;
        // Echte projizierte Würfelkanten; Near-Plane und Viewport werden geclippt.
        for(int corner=0;corner<8;++corner) for(int bit:{1,2,4}) if(!(corner&bit)) {
            const auto point=[&](int c)->WorldPoint { return {b.x+(c&1?1.:0.),b.y+(c&2?1.:0.),b.z+(c&4?1.:0.)}; };
            ScreenPoint a,z;
            if(projection.Segment(point(corner),point(corner|bit),a,z)) {
                const ImVec2 first{a.x+origin.x,a.y+origin.y},last{z.x+origin.x,z.y+origin.y};
                draw->AddLine(first,last,IM_COL32(0,0,0,120),3);
                draw->AddLine(first,last,color,1.4f);
            }
        }
    }
    if(chunkFinder_.labels) for(int i=0;i<size;++i) {
        const auto& g=groups[i]; if(g.count<chunkFinder_.minHints) continue;
        ScreenPoint p;
        if(!projection.Point({g.x*16.+8,g.y/g.count+.6,g.z*16.+8},p)) continue;
        char text[80]; std::snprintf(text,sizeof(text),"Chunk %d, %d | %d hints",g.x,g.z,g.count);
        const auto measure=ImGui::CalcTextSize(text); const ImVec2 pos{p.x-measure.x*.5f+origin.x,p.y+origin.y};
        draw->AddRectFilled({pos.x-5,pos.y-3},{pos.x+measure.x+5,pos.y+measure.y+3},IM_COL32(18,18,20,220),3);
        draw->AddText(pos,color,text);
    }
}
} // namespace mcov
