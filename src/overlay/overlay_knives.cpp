#include "overlay.hpp"

namespace mcov {
knife::Request Overlay::BuildKnifeRequest(std::uint64_t now) const {
    knife::Request r; r.sentAtMs=now; r.config=knifeConfig_; r.inspectSerial=knifeInspectSerial_;
    r.config.enabled=r.config.enabled&&!panicActive_&&agentConnected_&&haveSnapshot_&&lastSnapshot_.dimension[0]&&heartbeatClock_.Alive(now);
    r.inspectHeld=r.config.enabled&&knifeInputAllowed_&&!menuVisible_&&!hudEditing_&&captureAction_<0&&
        keys::Matches(bindings_[keys::Index(keys::Action::KnifeInspect)],keyInput_);
    return r;
}
void Overlay::DrawKnifeSettings() {
    namespace panel=control_panel;
    auto& c=knifeConfig_; bool dirty=false;
    if(panel::BeginRows("knife_controls")) {
        dirty|=panel::CheckRow("Enable CS Knife",&c.enabled); DrawBind(keys::Action::CsKnife,true);
        const char* models[]={"Butterfly","Karambit","Katana"};
        dirty|=panel::ComboRow("Knife model",&c.skin,models,3);
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Inspect key");
        ImGui::TableSetColumnIndex(1); DrawBind(keys::Action::KnifeInspect);
        dirty|=panel::SliderRow("Knife size",&c.size,.5f,2,"%.2fx");
        dirty|=panel::SliderRow("Animation speed",&c.speed,.5f,2,"%.2fx");
        dirty|=panel::CheckRow("Deploy animation",&c.deploy);
        dirty|=panel::CheckRow("Blade trails",&c.trails);
        ImGui::BeginDisabled(!c.trails);
        dirty|=panel::SliderRow("Trail duration",&c.trailSeconds,.05f,.35f,"%.2f s");
        ImGui::EndDisabled();
        dirty|=panel::CheckRow("Glow",&c.glow);
        ImGui::BeginDisabled(!c.glow);
        dirty|=panel::SliderRow("Glow strength",&c.glowStrength,0,3,"%.2fx");
        dirty|=panel::SliderRow("Glow width",&c.glowWidth,.25f,3,"%.2fx");
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted("Glow colour");
        ImGui::TableSetColumnIndex(1);
        float rgb[]={float(c.glowColour>>16&255)/255,float(c.glowColour>>8&255)/255,float(c.glowColour&255)/255};
        if(ImGui::ColorEdit3("##knife_glow_colour",rgb,ImGuiColorEditFlags_NoInputs)) {
            c.glowColour=(std::uint32_t(std::lround(rgb[0]*255))<<16)|(std::uint32_t(std::lround(rgb[1]*255))<<8)|std::uint32_t(std::lround(rgb[2]*255)); dirty=true;
        }
        ImGui::EndDisabled();
        ImGui::EndTable();
    }
    ImGui::TextWrapped("Adds a soft luminous edge to the blade and its trails.");
    panel::Section("How to use");
    ImGui::TextWrapped("Hold a sword in your main hand. Its first-person appearance changes to the selected knife; its material follows the sword. Other players still see your normal sword.");
    ImGui::TextWrapped("Assign an Inspect key that Minecraft does not use, such as R. Tap to inspect once, or hold to repeat. Attacking, using an item or changing slots cancels the inspect. The module key only toggles CS Knife.");
    panel::Section("Live status");
    if(!agentConnected_||!haveSnapshot_) ImGui::TextWrapped("Connect to Minecraft 1.21.11 and join a world.");
    else if(std::strcmp(lastSnapshot_.gameVersion,"1.21.11")) ImGui::TextWrapped("CS Knife requires Minecraft 1.21.11.");
    else if(!c.enabled) ImGui::TextWrapped("CS Knife is off.");
    else if(!knifeReport_.sampledAtMs||GetTickCount64()-knifeReport_.sampledAtMs>1500) ImGui::TextWrapped("Waiting for the hand renderer. Restart Minecraft after updating the client.");
    else switch(knifeReport_.status) {
        case knife::Status::Unsupported: ImGui::TextWrapped("Hand renderer unavailable. Restart Minecraft with the updated client."); break;
        case knife::Status::Failed: ImGui::TextWrapped("Could not draw the knife. Turn CS Knife off and on to retry. Your normal item remains available."); break;
        case knife::Status::Ready: ImGui::TextWrapped("Knife renderer active."); break;
        default: ImGui::TextWrapped("Hold a sword in first person and close Minecraft's screens."); break;
    }
    if(!bindings_[keys::Index(keys::Action::KnifeInspect)].key) ImGui::TextWrapped("Assign an Inspect key above to play the animation.");
    if(dirty) { panicActive_=false; preferencesDirty_=true; preferenceSaveAt_=GetTickCount64()+500; PushScanRequest(); }
}
} // namespace mcov
