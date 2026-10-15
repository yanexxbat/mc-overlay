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
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
            if(ImGui::Button("GrimAC conservative")) { macro.GrimConservative(); changed=true; }
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("Also requires a real crosshair hit and uses Minecraft's clicked block face. Not a verified bypass.");
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
            changed|=panel::CheckRow("Require real crosshair hit",&macro.requireCrosshairHit);
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
    else ImGui::TextWrapped("Enable the macro, select end crystals and hold Minecraft's Place / Use key on obsidian or bedrock. It repeatedly places and breaks the crystal at that position.");
    if(!totem) ImGui::TextWrapped("Release Place / Use to stop. A manual slot change, looking away, menus or focus loss also stops the cycle. The optional module keybind arms the macro; it does not replace Minecraft's Use key. Kill Aura pauses macros.");
    if(!totem) ImGui::TextWrapped("Minimum health and the totem check pause the cycle until satisfied. Range never exceeds Minecraft's own reach. Strict confirmation suppresses retries after a timeout; release Use to reset. Timing and protection settings are saved automatically.");
    if(!totem&&macro.requireCrosshairHit) ImGui::TextWrapped("Real crosshair hit is on: keep the next interaction target under the crosshair. Blocks use Minecraft's actual hit face. The cycle pauses until you aim correctly.");
}

void Overlay::NormalizeChunkFinder() {
    auto& old=smpConfig_.finder;
    if(old.enabled&&!chunkFinder_.enabled) {
        chunkFinder_.radius=old.radius; chunkFinder_.interval=old.interval;
        chunkFinder_.maxDraw=std::clamp(old.maxDraw,8.f,128.f);
        chunkFinder_.color=old.colour; chunkFinder_.labels=old.labels;
    }
    chunkFinder_.enabled=old.enabled=chunkFinder_.enabled||old.enabled;
}
smp::BaseFinderSettings Overlay::ChunkDetection() const {
    auto c=smpConfig_.finder;
    c.enabled=chunkFinder_.enabled||c.enabled;
    c.radius=chunkFinder_.radius; c.interval=chunkFinder_.interval;
    c.deepslate=chunkFinder_.deepslate; c.minDeepslate=chunkFinder_.minHints;
    c.maxDraw=chunkFinder_.maxDraw; c.labels=chunkFinder_.labels; c.colour=chunkFinder_.color;
    return c;
}
smp::BaseFinderSettings Overlay::ChunkDetection() const {
    auto c=smpConfig_.finder;
    c.enabled=chunkFinder_.enabled||c.enabled;
    c.radius=chunkFinder_.radius; c.interval=chunkFinder_.interval;
    c.deepslate=chunkFinder_.deepslate; c.minDeepslate=chunkFinder_.minHints;
    c.maxDraw=chunkFinder_.maxDraw; c.labels=chunkFinder_.labels; c.colour=chunkFinder_.color;
    return c;
}
void Overlay::DrawChunkFinderSettings() {
    bool dirty=false;
    if(panel::BeginRows("chunkfinder_controls")) {
        bool enabled=ModuleEnabled(keys::Action::ChunkFinder);
        if(panel::CheckRow("Enable Chunkfinder",&enabled)) { SetModuleEnabled(keys::Action::ChunkFinder,enabled); dirty=true; }
        DrawBind(keys::Action::ChunkFinder,true);
        dirty|=panel::SliderIntRow("Scan radius",&chunkFinder_.radius,4,48,"%d blocks");
        dirty|=panel::SliderIntRow("Scan interval",&chunkFinder_.interval,200,3000,"%d ms");
        dirty|=panel::SliderRow("Draw distance",&chunkFinder_.maxDraw,8,128,"%.0f m");
        dirty|=panel::CheckRow("Highlight whole chunks",&chunkFinder_.chunks);
        dirty|=panel::CheckRow("Highlight individual blocks",&chunkFinder_.blocks);
        dirty|=panel::CheckRow("Chunk labels",&chunkFinder_.labels);
        dirty|=panel::CheckRow("Tracers",&smpConfig_.finder.tracers);
        dirty|=panel::ColorRow("Hint colour",&chunkFinder_.color.x);
        ImGui::EndTable();
    }
    panel::Section("Suspicious blocks");
    if(panel::BeginRows("chunkfinder_deepslate")) {
        dirty|=panel::CheckRow("Horizontal deepslate",&chunkFinder_.deepslate);
        ImGui::BeginDisabled(!chunkFinder_.deepslate);
        dirty|=panel::SliderIntRow("Deepslate blocks / chunk",&chunkFinder_.minHints,1,32,"%d");
        ImGui::EndDisabled(); ImGui::EndTable();
    }
    if(dirty) {
        preferencesDirty_=true; preferenceSaveAt_=GetTickCount64()+500;
        PushScanRequest();
    }
    DrawSmpSettings(4);
}

void Overlay::DrawChunkFinder(const WorldSnapshot& s,const CameraProjection& projection) {
    if(!baseFinder_.Matches(s)) return;
    smp::DrawPossibleBases(*ImGui::GetBackgroundDrawList(),s,projection,ChunkDetection(),baseFinder_.Finds(),
        ImGui::GetIO().DisplaySize,chunkFinder_.chunks,chunkFinder_.blocks);
}
} // namespace mcov
