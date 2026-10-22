#include "overlay.hpp"
#include <windows.h>
#include <mmsystem.h>
#include <cstdio>
#include <cstring>
#include <numbers>

namespace mcov {
namespace {
namespace panel=control_panel;
using rockstar::Feature;
constexpr std::array<const char*,rockstar::kCount> descriptions={
    "Soft layered rings and light dust expand at your feet. Teleports are excluded; stairs and knockback can also trigger a ring.",
    "Soft filled feather wings follow sampled player shoulders. Your own wings are visible in F5; other players also get wings.",
    "A glowing halo follows player heads. Your own halo hides near the first-person camera.",
    "Soft glowing outlines use the sampled hitboxes. Friends use green outlines.",
    "Floating numbers show observed health loss. Other players and environmental damage also count.",
    "A red screen-edge fade appears after your sampled health drops.",
    "A ground ring or moving scan ring follows the current combat or crosshair target.",
    "An additional external crosshair with cross, circle, dot and movement-driven gap. Draws over the vanilla crosshair.",
    "Direction arrows show players outside the current screen, including targets behind the camera.",
    "Anonymizes external ESP, HUD and player statistics. Minecraft's own nametags remain under game control.",
    "A short tone plays when the current target loses sampled health. Damage is observed; the attacker is not identified.",
    "Short tones acknowledge module changes in the menu and through keybinds.",
    "Middle-click the player under the crosshair to add or remove their UUID from Friends. Friend hats and combat exclusion update immediately.",
    "Saves your last death location and shows its waypoint in the same server and dimension. Respawning keeps the marker.",
    "Lists nearby players in a movable HUD and notifies when a new player enters the sampled range. Adjust range here or from Radar.",
    "Shows short module, friend, proximity and death messages in a movable HUD element.",
    "Highlights chests, shulker boxes, barrels, hoppers and furnaces. Uses the existing loaded-block scanner.",
    "Draws a configurable range guide around scanned beacons. The chosen radius is a guide; pyramid level is not sampled.",
    "Shows chunk edges and vertical guides around your current chunk. Works with negative coordinates.",
    "Save named positions, copy coordinates, remove points and follow markers in the same server and dimension."
};
// Unveraenderliche PCM-Puffer bleiben auch fuer asynchrone Wiedergabe gueltig.
struct Tone {
    static constexpr int samples=720;
    std::array<unsigned char,44+samples*2> wav{};
    Tone(float frequency,float volume) {
        auto put=[&](int at,std::uint32_t value,int count) { for(int i=0;i<count;++i) wav[at+i]=static_cast<unsigned char>(value>>(8*i)); };
        std::memcpy(wav.data(),"RIFF",4); put(4,36+samples*2,4); std::memcpy(wav.data()+8,"WAVEfmt ",8);
        put(16,16,4); put(20,1,2); put(22,1,2); put(24,12000,4); put(28,24000,4); put(32,2,2); put(34,16,2);
        std::memcpy(wav.data()+36,"data",4); put(40,samples*2,4);
        for(int i=0;i<samples;++i) {
            const double envelope=std::sin(std::numbers::pi*i/samples);
            const auto pcm=static_cast<std::int16_t>(std::sin(2*std::numbers::pi*frequency*i/12000)*envelope*volume*20000);
            put(44+i*2,static_cast<std::uint16_t>(pcm),2);
        }
    }
};
}
void Overlay::PlayModuleSound(bool enabled,bool hit) {
    const auto& c=rockstarConfig_[hit?Feature::HitSounds:Feature::ModuleSounds];
    if(!c.enabled||c.opacity<=0) return;
    static const std::array<Tone,9> tones={Tone(440,.2f),Tone(440,.5f),Tone(440,.9f),Tone(660,.2f),Tone(660,.5f),Tone(660,.9f),Tone(880,.2f),Tone(880,.5f),Tone(880,.9f)};
    const int type=hit?std::clamp(c.mode,0,2):enabled?1:0,volume=std::clamp(int(c.opacity*3),0,2);
    PlaySoundW(reinterpret_cast<LPCWSTR>(tones[type*3+volume].wav.data()),nullptr,SND_MEMORY|SND_ASYNC|SND_NODEFAULT);
}
void Overlay::PlaySpectateSound() {
    static const Tone tone(660,.12f);
    PlaySoundW(reinterpret_cast<LPCWSTR>(tone.wav.data()),nullptr,SND_MEMORY|SND_ASYNC|SND_NODEFAULT);
}
void Overlay::Panic() {
    keyEngine_.DiscardHoldRestoration();
    for(int i=3;i<keys::kCount;++i) if(!keys::System(static_cast<keys::Action>(i))) SetModuleEnabled(static_cast<keys::Action>(i),false);
    visuals_.enabled=false;
    visuals_.boxEsp=visuals_.skeleton=visuals_.tracers=visuals_.nameTags=visuals_.healthBars=visuals_.distanceText=visuals_.targetInfo=false;
    auraConfig_.enabled=targetCubeConfig_.enabled=localHatConfig_.enabled=false;
    hud_.visible=false; rockstarPack_.ResetWorld(); worldVisuals_.Reset(); stashFinder_.Reset(); baseFinder_.Reset(); storagePulse_.Reset(); panicActive_=true;
    for(int i=0;i<kHudModules;++i) SetHudEnabled(i,false);
    auraRenderer_.Reset(); targetCube_.Reset(); nextClickTick_=0;
    preferencesDirty_=true; preferenceSaveAt_=GetTickCount64()+300; PushScanRequest(); PushCombatRequest();
    menu_.settingsOpen=false;
    captureAction_=-1;
    SetHudEditing(false);
    SetMenuVisible(false);
}
void Overlay::UpdateRockstar() {
    if(!rockstarConfig_.Enabled(rockstar::Feature::ChestEsp)) storagePulse_.Reset();
    UpdateAntiVanish();
    const double now=double(GetTickCount64())/1000;
    for(int i=3;i<keys::kCount;++i) {
        const auto action=static_cast<keys::Action>(i); if(keys::System(action)||action==keys::Action::BaseFinder) continue;
        const bool on=ModuleEnabled(action);
        if(moduleObservationReady_&&on!=observedModules_[i]&&!panicActive_) {
            if(rockstarConfig_.Enabled(Feature::Notifications)) { char text[112]; std::snprintf(text,sizeof(text),"%s %s",keys::kNames[i],on?"enabled":"disabled"); rockstarPack_.Notify(text,now); }
            PlayModuleSound(on);
        }
        observedModules_[i]=on;
    }
    moduleObservationReady_=true;
    if(panicActive_) for(int i=3;i<keys::kCount;++i) if(ModuleEnabled(static_cast<keys::Action>(i))) { panicActive_=false; break; }
    if(panicActive_&&(visuals_.healthBars||visuals_.distanceText||visuals_.tracers||visuals_.targetInfo)) panicActive_=false;
    rockstarPack_.Input(keyInput_,IsForegroundWindowOwnedBy(target_.pid)&&!menuVisible_&&!hudEditing_&&captureAction_<0,now);
    if(!haveSnapshot_||!agentConnected_||!heartbeatClock_.Alive(GetTickCount64())) { rockstarPack_.ResetWorld(); stashFinder_.Reset(); baseFinder_.Reset(); storagePulse_.Reset(); return; }
    // HUD/Statistik verarbeitet immer den neuesten Sampler, keine GPU-Verzoegerung.
    const auto& s=lastSnapshot_;
    const int newStashes=stashFinder_.Update(s,smpConfig_.Detection());
    UpdateChunkFinder();
    if(chunkFinder_.enabled)scannedChunks_.UpdateChunks(s,*chunkReport_);
    if(newStashes&&smpConfig_.stash.enabled&&smpConfig_.stash.notifications) {
        char text[112]; const auto* latest=stashFinder_.Latest();
        if(newStashes==1) std::snprintf(text,sizeof(text),"Stash found: %d containers at %.0f %.0f %.0f",latest->count,latest->centre.x,latest->centre.y,latest->centre.z);
        else std::snprintf(text,sizeof(text),"Stash Finder: %d new stashes. Open the discovery list for coordinates.",newStashes);
        rockstarPack_.Notify(text,now);
    }
    const auto target=combatMenu_.Store().Read().values.enabled&&combatReport_.status==CombatBridgeStatus::Active&&combatReport_.debug.target&&
        combatReport_.sampledAtMs&&GetTickCount64()-combatReport_.sampledAtMs<=kCombatLeaseMs?combatReport_.debug.target->id:s.hasTarget?s.targetEntityId:0;
    const auto oldDeath=std::find_if(rockstarPack_.Markers().begin(),rockstarPack_.Markers().end(),[](const auto& m){return m.used&&m.death;});
    const auto before=oldDeath==rockstarPack_.Markers().end()?rockstar::Marker{}:*oldDeath;
    const bool hadDeath=oldDeath!=rockstarPack_.Markers().end();
    rockstarPack_.Update(s,now,rockstarConfig_,target);
    const auto death=std::find_if(rockstarPack_.Markers().begin(),rockstarPack_.Markers().end(),[](const auto& m){return m.used&&m.death;});
    if(death!=rockstarPack_.Markers().end()&&(!hadDeath||death->p.x!=before.p.x||death->p.y!=before.p.y||death->p.z!=before.p.z||death->dimension!=before.dimension||death->server!=before.server)) {
        preferencesDirty_=true; preferenceSaveAt_=GetTickCount64()+500;
    }
    if(rockstarPack_.TakeHitSound()) PlayModuleSound(true,true);
}
void Overlay::DrawRockstarSettings(Feature feature) {
    if(feature==Feature::ChestEsp) { DrawSmpSettings(0); return; }
    auto& c=rockstarConfig_[feature]; const bool before=c.enabled; bool dirty=false;
    ImGui::PushID(rockstar::Index(feature)+22000);
    if(panel::BeginRows("rockstar_module")) {
        dirty|=panel::CheckRow("Enabled",&c.enabled);
        DrawBind(static_cast<keys::Action>(keys::Index(keys::Action::JumpCircles)+rockstar::Index(feature)),true);
        if(feature==Feature::Crosshair) {
            const char* modes[]={"Cross","Circle","Dot","Dynamic cross"}; dirty|=panel::ComboRow("Shape",&c.mode,modes,4);
            dirty|=panel::SliderRow("Gap",&c.range,0,20,"%.1f px"); dirty|=panel::SliderRow("Length / radius",&c.size,1,24,"%.1f px");
            dirty|=panel::SliderRow("Stroke",&c.speed,.7f,4,"%.1f px");
        } else if(feature==Feature::TargetCircle) {
            const char* modes[]={"Ground ring","Scanning ring"}; dirty|=panel::ComboRow("Mode",&c.mode,modes,2);
            dirty|=panel::SliderRow("Radius scale",&c.size,.5f,3,"%.2f x"); dirty|=panel::SliderRow("Scan speed",&c.speed,0,4,"%.2f");
        } else if(feature==Feature::GlowEsp) {
            const char* modes[]={"Players","All living entities"}; dirty|=panel::ComboRow("Subjects",&c.mode,modes,2);
            dirty|=panel::SliderRow("Distance",&c.range,4,256,"%.0f m"); dirty|=panel::SliderRow("Stroke",&c.size,.5f,4,"%.1f px");
        } else if(feature==Feature::Wings||feature==Feature::Halo||feature==Feature::JumpCircles) {
            dirty|=panel::SliderRow("Size",&c.size,.2f,3,"%.2f x");
            if(feature==Feature::Wings||feature==Feature::JumpCircles) dirty|=panel::SliderRow("Animation speed",&c.speed,0,4,"%.2f");
        } else if(feature==Feature::DamageMarkers) {
            dirty|=panel::SliderRow("Text size",&c.size,.5f,2,"%.2f x"); dirty|=panel::SliderRow("Rise speed",&c.speed,0,4,"%.2f");
        } else if(feature==Feature::DamageInfo) {
            dirty|=panel::SliderRow("Edge size",&c.size,.2f,4,"%.2f x");
        } else if(feature==Feature::Arrows) {
            dirty|=panel::SliderRow("Distance",&c.range,4,256,"%.0f m"); dirty|=panel::SliderRow("Arrow size",&c.size,4,20,"%.0f px");
        } else if(feature==Feature::DetectPlayer) dirty|=panel::SliderRow("Range",&c.range,8,128,"%.0f m");
        else if(feature==Feature::ChestEsp||feature==Feature::BeaconRadius) {
            dirty|=panel::SliderRow("Scan distance",&c.range,4,48,"%.0f m");
            if(feature==Feature::BeaconRadius) dirty|=panel::SliderRow("Radius guide",&c.size,10,80,"%.0f m");
        }
        else if(feature==Feature::ChunkBorders) dirty|=panel::SliderRow("Neighbour chunks",&c.range,0,3,"%.0f");
        else if(feature==Feature::HitSounds) { const char* sounds[]={"Low","Medium","High"}; dirty|=panel::ComboRow("Tone",&c.mode,sounds,3); }
        const bool sound=feature==Feature::HitSounds||feature==Feature::ModuleSounds;
        const bool visual=rockstar::Index(feature)<=rockstar::Index(Feature::Arrows)||feature==Feature::ChestEsp||feature==Feature::BeaconRadius||feature==Feature::ChunkBorders;
        if(visual||sound) dirty|=panel::SliderRow(sound?"Volume":"Opacity",&c.opacity,0,1,"%.2f");
        if(visual&&feature!=Feature::DamageInfo) {
            dirty|=panel::SliderRow("Glow",&c.glow,0,2,"%.2f"); dirty|=panel::CheckRow("Use Target Aura colours",&c.auraColours);
            ImGui::BeginDisabled(c.auraColours); dirty|=panel::ColorRow("Start colour",&c.start.x); dirty|=panel::ColorRow("End colour",&c.end.x); ImGui::EndDisabled();
        }
        if(feature==Feature::JumpCircles||feature==Feature::DamageMarkers||feature==Feature::DamageInfo||feature==Feature::Notifications)
            dirty|=panel::SliderRow("Lifetime",&c.lifetime,.3f,8,"%.1f s");
        ImGui::EndTable();
    }
    ImGui::TextWrapped("%s",descriptions[rockstar::Index(feature)]);
    if(feature==Feature::Notifications||feature==Feature::DetectPlayer) {
        if(!hud_.visible) ImGui::TextWrapped("Enable HUD to display this element.");
        if(ImGui::Button("Edit HUD position")) { hudSelected_=int(feature==Feature::Notifications?HudModule::Notifications:HudModule::Nearby); SetHudEditing(true); }
    }
    if(feature==Feature::Waypoints||feature==Feature::DeathCoordinates) {
        if(feature==Feature::Waypoints) {
            ImGui::SetNextItemWidth(-1); ImGui::InputText("##waypoint_name",waypointName_,sizeof(waypointName_));
            ImGui::BeginDisabled(!haveSnapshot_||!lastSnapshot_.dimension[0]);
            if(ImGui::Button("Save current position")) { dirty|=rockstarPack_.AddMarker(lastSnapshot_,waypointName_); }
            ImGui::EndDisabled();
        }
        for(int i=0;i<rockstar::Pack::kMarkers;++i) {
            auto& m=rockstarPack_.Markers()[i]; if(!m.used||(feature==Feature::DeathCoordinates&&!m.death)) continue;
            ImGui::PushID(i); ImGui::Separator(); ImGui::TextWrapped("%s | %s | %.0f %.0f %.0f",m.label.data(),m.dimension.data(),m.p.x,m.p.y,m.p.z);
            if(ImGui::SmallButton("Copy coordinates")) { char text[96]; std::snprintf(text,sizeof(text),"%.0f %.0f %.0f",m.p.x,m.p.y,m.p.z); ImGui::SetClipboardText(text); }
            ImGui::SameLine(); if(ImGui::SmallButton("Remove")) { m={}; dirty=true; } ImGui::PopID();
        }
    }
    if(before!=c.enabled&&c.enabled) panicActive_=false;
    if(dirty) { rockstarConfig_.Clamp(); preferencesDirty_=true; preferenceSaveAt_=GetTickCount64()+500; }
    ImGui::PopID();
}
void Overlay::DrawMiscTab() {
    constexpr const char* labels[]={"Middle click friend","Death coordinates","Detect players","Waypoints","Name protect","Module sounds","Notifications","Panic","Freecam","Anti Vanish"};
    constexpr Feature modules[]={Feature::MiddleFriend,Feature::DeathCoordinates,Feature::DetectPlayer,Feature::Waypoints,Feature::NameProtect,Feature::ModuleSounds,Feature::Notifications};
    miscSection_=panel::SubTabs("misc_sections",labels,IM_ARRAYSIZE(labels),miscSection_);
    if(miscSection_==9) { DrawAntiVanishSettings();return; }
    if(miscSection_<7) DrawRockstarSettings(modules[miscSection_]);
    else if(miscSection_==8) {
        auto& c=combat_.freecam;
        bool changed=false;
        if(panel::BeginRows("freecam_controls")) {
            changed|=panel::CheckRow("Enable Freecam",&c.enabled); DrawBind(keys::Action::Freecam,true);
            changed|=panel::SliderRow("Flight speed",&c.speed,1,40,"%.1f m/s");
            changed|=panel::SliderRow("Boost multiplier",&c.boost,1,10,"%.1fx");
            const auto now=GetTickCount64();
            const bool fresh=agentConnected_&&combatReport_.sampledAtMs&&now>=combatReport_.sampledAtMs&&now-combatReport_.sampledAtMs<1000;
            panel::ValueRow("Status",fresh?freecam::Text(combatReport_.freecamStatus):"Connect with the injector");
            ImGui::EndTable();
        }
        if(changed) { preferencesDirty_=true; preferenceSaveAt_=GetTickCount64()+500; }
        ImGui::TextWrapped("Close the menu to fly. WASD moves the camera, mouse looks around, Space rises, Shift descends and Ctrl boosts. Your player stays in place; attacks, item use and combat assists pause.");
        ImGui::TextWrapped("Disable Freecam to return to your player. Menus, focus loss and world changes release the camera. Only loaded chunks are visible. Set Toggle or Hold under Settings / Keybinds.");
    } else {
        panel::Section("Disable all modules"); ImGui::TextUnformatted("Panic key"); ImGui::SameLine(); DrawBind(keys::Action::Panic);
        ImGui::TextUnformatted("F12 always disables all modules and hides the UI.");
        if(ImGui::Button("Disable all modules now")) Panic();
        ImGui::TextWrapped("Stops combat and macros, clears transient effects and hides ESP/HUD. Friends and saved waypoints are retained. Enable a module to resume.");
    }
}
} // namespace mcov
