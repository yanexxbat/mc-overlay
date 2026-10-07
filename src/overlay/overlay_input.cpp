#include "overlay.hpp"
#include "../common/flat_style.hpp"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace mcov {
std::string BindName(keys::Bind b) {
    if(!b.key) return "None";
    std::string out;
    if(b.modifiers&keys::Ctrl) out+="Ctrl+";
    if(b.modifiers&keys::Shift) out+="Shift+";
    if(b.modifiers&keys::Alt) out+="Alt+";
    const char* mouse=b.key==VK_LBUTTON?"Mouse 1":b.key==VK_RBUTTON?"Mouse 2":b.key==VK_MBUTTON?"Mouse 3":
                      b.key==VK_XBUTTON1?"Mouse 4":b.key==VK_XBUTTON2?"Mouse 5":nullptr;
    if(mouse) return out+mouse;
    char text[80]{}; UINT scan=MapVirtualKeyW(b.key,MAPVK_VK_TO_VSC);
    if(b.key==VK_INSERT||b.key==VK_DELETE||b.key==VK_HOME||b.key==VK_END||b.key==VK_PRIOR||b.key==VK_NEXT||
       (b.key>=VK_LEFT&&b.key<=VK_DOWN)||b.key==VK_DIVIDE) scan|=0x100;
    if(GetKeyNameTextA(static_cast<LONG>(scan<<16),text,sizeof(text))) return out+text;
    return out+"Key "+std::to_string(b.key);
}
bool Overlay::ModuleEnabled(keys::Action a) const {
    if(a>=keys::Action::JumpCircles&&a<=keys::Action::Waypoints)
        return rockstarConfig_.modules[keys::Index(a)-keys::Index(keys::Action::JumpCircles)].enabled;
    if(a>=keys::Action::Radar&&a<=keys::Action::Cps)
        return hud_.extras[keys::Index(a)-keys::Index(keys::Action::Radar)];
    switch(a) {
        case keys::Action::KillAura:return combatMenu_.Store().Read().values.enabled;
        case keys::Action::AimAssist:return combat_.aimLock;
        case keys::Action::Triggerbot:return combat_.triggerbot;
        case keys::Action::AutoTotem:return combat_.autoTotem.enabled;
        case keys::Action::BoxEsp:return visuals_.boxEsp;
        case keys::Action::Skeleton:return visuals_.skeleton;
        case keys::Action::NameTags:return visuals_.nameTags;
        case keys::Action::BlockEsp:return blocks_.enabled;
        case keys::Action::TargetAura:return auraConfig_.enabled;
        case keys::Action::TargetMarker:return targetCubeConfig_.enabled;
        case keys::Action::Hud:return hud_.visible;
        case keys::Action::Trails:return worldVisualsConfig_.trails.enabled;
        case keys::Action::HitParticles:return worldVisualsConfig_.hits.enabled;
        case keys::Action::AmbientParticles:return worldVisualsConfig_.ambient.enabled;
        case keys::Action::HitBubbles:return worldVisualsConfig_.bubbles.enabled;
        case keys::Action::KillEffect:return worldVisualsConfig_.kills.enabled;
        case keys::Action::BreadCrumbs:return worldVisualsConfig_.crumbs.enabled;
        case keys::Action::ChinaHat:return localHatConfig_.enabled;
        case keys::Action::CrystalMacro:return crystalMacro_.enabled;
        case keys::Action::AnchorMacro:return anchorMacro_.enabled;
        case keys::Action::ChunkFinder:return chunkFinder_.enabled;
        default:return false;
    }
}
void Overlay::SetModuleEnabled(keys::Action a,bool value) {
    if(a>=keys::Action::JumpCircles&&a<=keys::Action::Waypoints) {
        rockstarConfig_.modules[keys::Index(a)-keys::Index(keys::Action::JumpCircles)].enabled=value; preferencesDirty_=true;
    } else if(a>=keys::Action::Radar&&a<=keys::Action::Cps) {
        hud_.extras[keys::Index(a)-keys::Index(keys::Action::Radar)]=value; preferencesDirty_=true;
    }
    if(value) panicActive_=false;
    switch(a) {
        case keys::Action::KillAura: { auto c=combatMenu_.Store().Read().values; c.enabled=value; combatMenu_.Store().Publish(c); break; }
        case keys::Action::AimAssist:combat_.aimLock=value;break;
        case keys::Action::Triggerbot:combat_.triggerbot=value;break;
        case keys::Action::AutoTotem:combat_.autoTotem.enabled=value;break;
        case keys::Action::BoxEsp:visuals_.boxEsp=value;break;
        case keys::Action::Skeleton:visuals_.skeleton=value;break;
        case keys::Action::NameTags:visuals_.nameTags=value;break;
        case keys::Action::BlockEsp:blocks_.enabled=value;break;
        case keys::Action::TargetAura:auraConfig_.enabled=value;break;
        case keys::Action::TargetMarker:targetCubeConfig_.enabled=value;break;
        case keys::Action::Hud:hud_.visible=value;preferencesDirty_=true;break;
        case keys::Action::Trails:worldVisualsConfig_.trails.enabled=value;break;
        case keys::Action::HitParticles:worldVisualsConfig_.hits.enabled=value;break;
        case keys::Action::AmbientParticles:worldVisualsConfig_.ambient.enabled=value;break;
        case keys::Action::HitBubbles:worldVisualsConfig_.bubbles.enabled=value;break;
        case keys::Action::KillEffect:worldVisualsConfig_.kills.enabled=value;break;
        case keys::Action::BreadCrumbs:worldVisualsConfig_.crumbs.enabled=value;break;
        case keys::Action::ChinaHat:localHatConfig_.enabled=value;break;
        case keys::Action::CrystalMacro:crystalMacro_.enabled=value;break;
        case keys::Action::AnchorMacro:anchorMacro_.enabled=value;break;
        case keys::Action::ChunkFinder:chunkFinder_.enabled=value;break;
        default:break;
    }
}
void Overlay::ProcessKeyInput(const keys::Input& input,bool focused) {
    const bool middlePressed=input[VK_MBUTTON]&&!keyInput_[VK_MBUTTON];
    keyInput_=input; const bool capturing=captureAction_>=0;
    if(capturing) {
        if(!focused||!menuVisible_) captureAction_=-1;
        else {
            const auto assign=[&](keys::Bind next) {
                auto candidate=bindings_; candidate[captureAction_]=next;
                if(keys::Conflict(candidate,captureAction_)) { preferenceStatus_="Key already assigned. Choose another key."; return false; }
                bindings_=candidate; captureAction_=-1; captureModifier_=0; preferencesDirty_=true; preferenceStatus_.clear(); SavePreferences(); return true;
            };
            // Der Klick auf das Bind-Feld darf nicht als Mouse-1-Belegung enden.
            if(!input[VK_LBUTTON]&&!input[VK_RBUTTON]&&!input[VK_MBUTTON]) captureArmed_=true;
            if(captureArmed_) for(int k=1;k<255;++k) {
                if(!input[k]||capturePrevious_[k]) continue;
                if(keys::ModifierKey(k)) { captureModifier_=k; captureModifierMask_=keys::Modifiers(input)&~keys::ModifierForKey(k); continue; }
                captureModifier_=0; // Eine abgewiesene Kombination darf nicht nachträglich nur den Modifier binden.
                if(k==VK_ESCAPE) { captureAction_=-1; break; }
                const bool clear=k==VK_BACK||k==VK_DELETE;
                if(clear&&captureAction_<=keys::Index(keys::Action::Quit)) { preferenceStatus_="Menu and Quit must keep a key."; break; }
                const keys::Bind next{clear?0:k,clear?0:keys::Modifiers(input),keys::System(static_cast<keys::Action>(captureAction_))?keys::Mode::Toggle:bindings_[captureAction_].mode};
                assign(next); break;
            }
            // Ein einzelner Modifier wird beim Loslassen übernommen; bis dahin
            // kann er noch Teil einer Kombination wie Ctrl+K werden.
            if(captureAction_>=0&&captureModifier_&&!input[captureModifier_])
                assign({captureModifier_,captureModifierMask_,keys::System(static_cast<keys::Action>(captureAction_))?keys::Mode::Toggle:bindings_[captureAction_].mode});
            capturePrevious_=input;
        }
    }
    if(hudEditing_&&input[VK_ESCAPE]&&!capturePrevious_[VK_ESCAPE]) SetHudEditing(false);
    keyEngine_.Update(bindings_,input,focused,!menuVisible_&&!hudEditing_,capturing,
        [&](keys::Action a){return ModuleEnabled(a);},[&](keys::Action a,bool on){SetModuleEnabled(a,on);},
        [&](keys::Action a){
            if(a==keys::Action::Menu) SetMenuVisible(!menuVisible_);
            else if(a==keys::Action::Quit) running_=false;
            else if(a==keys::Action::HudEditor) SetHudEditing(!hudEditing_);
            else if(a==keys::Action::Panic) Panic();
        });
    if(middlePressed&&focused&&!capturing&&!menuVisible_&&!hudEditing_&&rockstarConfig_.Enabled(rockstar::Feature::MiddleFriend)&&haveSnapshot_&&agentConnected_&&heartbeatClock_.Alive(GetTickCount64())) {
        for(const auto& entity:std::span(lastSnapshot_.entities,std::min(lastSnapshot_.entityCount,kMaxEntities))) {
            if(entity.entityId!=lastSnapshot_.targetEntityId||!lastSnapshot_.hasTarget||!(entity.flags&kEntityIsPlayer)||(entity.flags&kEntityIsSelf)||(!entity.uuid.high&&!entity.uuid.low)) continue;
            auto& book=combatMenu_.Relations(); const bool friendNow=combat::Lookup(book.Read(),entity.uuid)==combat::Relation::Friend;
            const bool changed=book.Set(entity.uuid,friendNow?combat::Relation::Neutral:combat::Relation::Friend);
            rockstarPack_.Notify(changed?(friendNow?"Friend removed":"Friend added"):"Friend list is full",double(GetTickCount64())/1000); break;
        }
    }
    capturePrevious_=input;
}
void Overlay::DrawBind(keys::Action a,bool inlineRow) {
    const int index=keys::Index(a); ImGui::PushID(index+10000);
    const auto label=captureAction_==index?"Press key...":BindName(bindings_[index]);
    if(inlineRow) {
        const float right=ImGui::GetCursorScreenPos().x+ImGui::GetContentRegionAvail().x;
        ImGui::SameLine(); ImGui::SetCursorScreenPos({right-88,ImGui::GetCursorScreenPos().y});
    }
    if(ImGui::Button(label.c_str(),{inlineRow?88.f:150.f,0})) {
        captureAction_=captureAction_==index?-1:index; captureModifier_=0; captureArmed_=false; capturePrevious_=keyInput_; preferenceStatus_.clear();
    }
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Click, then press a key. Esc cancels. Backspace / Delete clears.");
    ImGui::PopID();
}
void Overlay::DrawKeybindSettings() {
    ImGui::TextWrapped("Click a binding and press a key or mouse button. Ctrl / Shift / Alt combinations work too.");
    if(captureAction_>=0) ImGui::Text("Binding: %s  (Esc cancels)",keys::kNames[captureAction_]);
    if(!preferenceStatus_.empty()) ImGui::TextWrapped("%s",preferenceStatus_.c_str());
    if(ImGui::BeginTable("keybind_editor",3,ImGuiTableFlags_Borders|ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Module",ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Key",ImGuiTableColumnFlags_WidthFixed,150);
        ImGui::TableSetupColumn("Mode",ImGuiTableColumnFlags_WidthFixed,90); ImGui::TableHeadersRow();
        for(int i=0;i<keys::kCount;++i) {
            const auto a=static_cast<keys::Action>(i); ImGui::PushID(i);
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(keys::kNames[i]);
            ImGui::TableSetColumnIndex(1); DrawBind(a);
            ImGui::TableSetColumnIndex(2);
            if(keys::System(a)) ImGui::TextDisabled("Toggle");
            else { int mode=int(bindings_[i].mode); const char* modes[]={"Toggle","Hold"}; ImGui::SetNextItemWidth(-1);
                if(ImGui::Combo("##mode",&mode,modes,2)) { bindings_[i].mode=static_cast<keys::Mode>(mode); preferencesDirty_=true; preferenceSaveAt_=GetTickCount64()+500; }
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::TextWrapped("Toggle switches once per press. Hold enables the module until release. Module keys pause while the menu or HUD editor is open.");
    if(ImGui::Button("Reset keybinds")) { bindings_=keys::Defaults(); captureAction_=-1; preferencesDirty_=true; SavePreferences(); }
}
void Overlay::SavePreferences() {
    if(preferencesPath_.empty()) return;
    const auto temporary=std::filesystem::path(preferencesPath_.wstring()+L".tmp");
    { std::ofstream out(temporary,std::ios::trunc);
      if(!out) { preferenceStatus_="Could not save client-settings.ini."; preferencesDirty_=false; return; }
      out<<"mcov-client 1\n"<<std::setprecision(9);
      out<<"world-depth "<<settings_.occludeWorldEffects<<'\n';
      // Einstellungen bleiben erhalten; Module starten weiterhin deaktiviert.
      for(int i=0;i<2;++i) {
          auto c=i?anchorMacro_:crystalMacro_; c.Clamp();
          out<<"macro "<<i<<' '<<c.restoreSlot<<' '<<c.intervalMs<<' '<<c.confirmationMs<<' '
             <<c.separateDelays<<' '<<c.requireTotem<<' '<<c.strictConfirmation<<' '<<c.chargeWithOffhandFuel<<' '
             <<c.placeMs<<' '<<c.breakMs<<' '<<c.chargeMs<<' '<<c.maxRange<<' '<<c.minHealth<<' '<<c.detonationSlot<<' '<<c.requireCrosshairHit<<'\n';
      }
      for(int i=0;i<keys::kCount;++i) out<<"bind "<<i<<' '<<bindings_[i].key<<' '<<bindings_[i].modifiers<<' '<<int(bindings_[i].mode)<<'\n';
      unsigned flags=hud_.visible?1:0; for(int i=0;i<kHudModules;++i) if(HudEnabled(i)) flags|=1u<<(i+1);
      out<<"hud "<<flags<<' '<<hud_.durabilityWarn<<'\n';
      for(int i=0;i<kHudModules;++i) { const auto& p=hud_.layout[i]; out<<"place "<<i<<' '<<p.position.x<<' '<<p.position.y<<' '<<p.scale<<' '<<p.placed<<'\n'; }
      for(int i=0;i<rockstar::kCount;++i) { const auto& c=rockstarConfig_.modules[i];
          out<<"rockstar "<<i<<' '<<c.enabled<<' '<<c.auraColours<<' '<<c.mode<<' '<<c.range<<' '<<c.size<<' '<<c.lifetime<<' '<<c.speed<<' '<<c.opacity<<' '<<c.glow;
          for(const auto& color:{c.start,c.end}) out<<' '<<color.x<<' '<<color.y<<' '<<color.z<<' '<<color.w; out<<'\n'; }
      for(int i=0;i<rockstar::Pack::kMarkers;++i) { const auto& m=rockstarPack_.Markers()[i]; if(m.used)
          out<<"waypoint "<<i<<' '<<m.death<<' '<<m.p.x<<' '<<m.p.y<<' '<<m.p.z<<' '<<std::quoted(m.label.data())<<' '<<std::quoted(m.dimension.data())<<' '<<std::quoted(m.server.data())<<'\n'; }
      out.flush(); if(!out) { preferenceStatus_="Could not write client preferences."; preferencesDirty_=false; return; }
    }
    if(!MoveFileExW(temporary.c_str(),preferencesPath_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        preferenceStatus_="Could not replace client-settings.ini.";
    else preferenceStatus_.clear();
    preferencesDirty_=false;
}
void Overlay::LoadPreferences() {
    std::ifstream in(preferencesPath_); if(!in) return;
    std::string header; std::getline(in,header); if(header!="mcov-client 1") { preferenceStatus_="Unsupported preferences format; defaults loaded."; return; }
    std::string line; for(int lines=0;lines<128&&std::getline(in,line);++lines) {
        if(line.size()>512) continue; std::istringstream row(line); std::string tag; row>>tag;
        if(tag=="bind") { int i,k,mode; unsigned mods;
            if(row>>i>>k>>mods>>mode&&i>=0&&i<keys::kCount&&k>=0&&k<255&&mods<=7&&mode>=0&&mode<=1&&
                (i>1||k>0)) bindings_[i]={k,mods,keys::System(static_cast<keys::Action>(i))?keys::Mode::Toggle:static_cast<keys::Mode>(mode)};
        } else if(tag=="macro") {
            int i; items::MacroConfig c;
            if(row>>i>>c.restoreSlot>>c.intervalMs>>c.confirmationMs>>c.separateDelays>>c.requireTotem>>c.strictConfirmation>>c.chargeWithOffhandFuel
                  >>c.placeMs>>c.breakMs>>c.chargeMs>>c.maxRange>>c.minHealth>>c.detonationSlot&&i>=0&&i<2) {
                // Ältere Settings ohne den optionalen Treffer-Check bleiben lesbar.
                bool direct; if(row>>direct) c.requireCrosshairHit=direct;
                c.Clamp(); auto& current=i?anchorMacro_:crystalMacro_; c.enabled=current.enabled; current=c;
            }
        } else if(tag=="world-depth") { int value; if(row>>value&&(value==0||value==1)) settings_.occludeWorldEffects=value==1;
        } else if(tag=="hud") { unsigned flags; float warn;
            if(row>>flags>>warn&&std::isfinite(warn)) { hud_.visible=(flags&1)!=0; for(int i=0;i<kHudModules;++i) SetHudEnabled(i,(flags&(1u<<(i+1)))!=0); hud_.durabilityWarn=std::clamp(warn,.05f,.5f); }
        } else if(tag=="place") { int i,placed; float x,y,scale;
            if(row>>i>>x>>y>>scale>>placed&&i>=0&&i<kHudModules&&std::isfinite(x)&&std::isfinite(y)&&std::isfinite(scale))
                hud_.layout[i]={{std::clamp(x,0.f,1.f),std::clamp(y,0.f,1.f)},std::clamp(scale,.65f,1.6f),placed==1};
        } else if(tag=="rockstar") {
            int i=0; rockstar::Options c;
            if(row>>i>>c.enabled>>c.auraColours>>c.mode>>c.range>>c.size>>c.lifetime>>c.speed>>c.opacity>>c.glow>>c.start.x>>c.start.y>>c.start.z>>c.start.w>>c.end.x>>c.end.y>>c.end.z>>c.end.w&&i>=0&&i<rockstar::kCount) rockstarConfig_.modules[i]=c;
        } else if(tag=="waypoint") {
            int i=0; rockstar::Marker m; std::string label,dimension,server;
            if(row>>i>>m.death>>m.p.x>>m.p.y>>m.p.z>>std::quoted(label)>>std::quoted(dimension)>>std::quoted(server)&&i>=0&&i<rockstar::Pack::kMarkers&&
               std::isfinite(m.p.x)&&std::isfinite(m.p.y)&&std::isfinite(m.p.z)&&std::abs(m.p.x)<=3e7&&std::abs(m.p.z)<=3e7&&std::abs(m.p.y)<=4096) {
                m.used=true; std::snprintf(m.label.data(),m.label.size(),"%s",label.c_str()); std::snprintf(m.dimension.data(),m.dimension.size(),"%s",dimension.c_str());
                std::snprintf(m.server.data(),m.server.size(),"%s",server.c_str()); rockstarPack_.Markers()[i]=m;
            }
        }
    }
    // Beschädigte/doppelte Belegungen aktivieren keine zwei Module zugleich.
    for(int i=0;i<keys::kCount;++i) for(int j=0;j<i;++j)
        if(bindings_[i].key&&bindings_[i].key==bindings_[j].key&&bindings_[i].modifiers==bindings_[j].modifiers)
            bindings_[i]={};
    if(!bindings_[0].key||!bindings_[1].key||keys::Conflict(bindings_,0)||keys::Conflict(bindings_,1)) bindings_=keys::Defaults();
    preferencesDirty_=false;
    rockstarConfig_.Clamp();
}
} // namespace mcov
