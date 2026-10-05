#include "overlay.hpp"
#include <cstdio>

namespace mcov {
namespace {
namespace panel=control_panel;
void PaletteRows(EffectPalette& colours) {
    panel::CheckRow("Use Target Aura colours",&colours.auraColours);
    ImGui::BeginDisabled(colours.auraColours);
    panel::ColorRow("Start colour",&colours.start.x);
    panel::ColorRow("End colour",&colours.end.x);
    ImGui::EndDisabled();
}
template<class Enum> void EnumRow(const char* label,Enum& value,const char* const* names,int count) {
    int index=static_cast<int>(value);
    if(panel::ComboRow(label,&index,names,count)) value=static_cast<Enum>(index);
}
}
void Overlay::DrawWorldVisualSettings(int section) {
    const auto stats=worldVisuals_.Stats();
    if(section==0) {
        auto& c=worldVisualsConfig_.trails;
        if(panel::BeginRows("trail_settings")) {
            panel::CheckRow("Enable trails",&c.enabled); DrawBind(keys::Action::Trails,true);
            ImGui::BeginDisabled(!c.enabled);
            constexpr const char* subjects[]={"Self","Players (including self)","Current target","All living entities"};
            constexpr const char* shapes[]={"Ribbon","Line","Orbs"};
            EnumRow("Follow",c.subjects,subjects,IM_ARRAYSIZE(subjects));
            EnumRow("Style",c.shape,shapes,IM_ARRAYSIZE(shapes));
            panel::SliderRow("Lifetime",&c.lifetime,.2f,3,"%.1f s");
            panel::SliderRow("Width / orb size",&c.width,.01f,.15f,"%.3f m");
            ImGui::BeginDisabled(c.shape!=TrailShape::Ribbon);
            panel::SliderRow("Ribbon height",&c.height,.1f,2,"%.2f m");
            ImGui::EndDisabled();
            panel::SliderRow("Opacity",&c.opacity,0,1,"%.2f");
            panel::SliderRow("Glow intensity",&c.glow,0,2,"%.2f");
            panel::SliderRow("Effect distance (shared)",&worldVisualsConfig_.maxDistance,4,128,"%.0f m");
            PaletteRows(c.colours);
            ImGui::EndDisabled(); ImGui::EndTable();
        }
        ImGui::TextWrapped("World-space trails fade behind moving entities. Self trails are easiest to see in F5. Current target follows Kill Aura, or the crosshair when Kill Aura is off.");
        ImGui::TextDisabled("Tracked trails: %d / %d",stats.trails,WorldVisualsRenderer::kTracks);
    } else if(section==1) {
        auto& c=worldVisualsConfig_.hits;
        if(panel::BeginRows("hit_particle_settings")) {
            panel::CheckRow("Enable hit particles",&c.enabled); DrawBind(keys::Action::HitParticles,true);
            ImGui::BeginDisabled(!c.enabled);
            constexpr const char* shapes[]={"Orbs","Stars"};
            EnumRow("Style",c.shape,shapes,IM_ARRAYSIZE(shapes));
            panel::CheckRow("Current target only",&c.targetOnly);
            panel::SliderIntRow("Particles per hit",&c.amount,1,24,"%d");
            panel::SliderRow("Lifetime",&c.lifetime,.2f,3,"%.1f s");
            panel::SliderRow("Size",&c.size,.02f,.2f,"%.3f m");
            panel::SliderRow("Speed",&c.speed,.1f,4,"%.1f m/s");
            panel::SliderRow("Gravity",&c.gravity,0,5,"%.1f m/s2");
            panel::SliderRow("Opacity",&c.opacity,0,1,"%.2f");
            panel::SliderRow("Glow intensity",&c.glow,0,2,"%.2f");
            panel::SliderRow("Effect distance (shared)",&worldVisualsConfig_.maxDistance,4,128,"%.0f m");
            PaletteRows(c.colours);
            ImGui::EndDisabled(); ImGui::EndTable();
        }
        ImGui::TextWrapped("A burst appears when a living entity's sampled HP drops. Damage from other players or the environment also counts. Particles fade without block collision.");
        ImGui::TextDisabled("Active particles: %d / %d",stats.hitParticles,WorldVisualsRenderer::kHitParticles);
    } else if(section==2) {
        auto& c=worldVisualsConfig_.ambient;
        if(panel::BeginRows("ambient_particle_settings")) {
            panel::CheckRow("Enable ambient particles",&c.enabled); DrawBind(keys::Action::AmbientParticles,true);
            ImGui::BeginDisabled(!c.enabled);
            constexpr const char* shapes[]={"Fireflies","Stars","Snow"};
            EnumRow("Style",c.shape,shapes,IM_ARRAYSIZE(shapes));
            panel::SliderIntRow("Amount",&c.amount,8,WorldVisualsRenderer::kAmbientParticles,"%d");
            panel::SliderRow("Radius",&c.radius,2,16,"%.1f m");
            panel::SliderRow("Size",&c.size,.015f,.15f,"%.3f m");
            panel::SliderRow("Drift speed",&c.speed,0,2,"%.2f m/s");
            panel::SliderRow("Opacity",&c.opacity,0,1,"%.2f");
            panel::SliderRow("Glow intensity",&c.glow,0,2,"%.2f");
            PaletteRows(c.colours);
            ImGui::EndDisabled(); ImGui::EndTable();
        }
        ImGui::TextWrapped("Soft fireflies, stars or snow float around your player. Reduced motion keeps the particles still while their opacity fades.");
        ImGui::TextDisabled("Active particles: %d / %d",stats.ambientParticles,WorldVisualsRenderer::kAmbientParticles);
    } else if(section==3) {
        auto& c=worldVisualsConfig_.bubbles;
        if(panel::BeginRows("hit_bubble_settings")) {
            panel::CheckRow("Enable hit bubbles",&c.enabled); DrawBind(keys::Action::HitBubbles,true);
            ImGui::BeginDisabled(!c.enabled);
            panel::CheckRow("Current target only",&c.targetOnly);
            panel::SliderRow("Lifetime",&c.lifetime,.2f,2,"%.2f s");
            panel::SliderRow("Ring radius",&c.radius,.15f,1.5f,"%.2f m");
            panel::SliderRow("Ring width",&c.width,.005f,.08f,"%.3f m");
            panel::SliderRow("Effect distance (shared)",&worldVisualsConfig_.maxDistance,4,128,"%.0f m");
            panel::SliderRow("Opacity",&c.opacity,0,1,"%.2f");
            panel::SliderRow("Glow intensity",&c.glow,0,2,"%.2f");
            PaletteRows(c.colours);
            ImGui::EndDisabled(); ImGui::EndTable();
        }
        ImGui::TextWrapped("An expanding glow ring appears at the damage position whenever sampled HP drops. It faces the camera and fades in world space.");
        ImGui::TextDisabled("Active bubbles: %d",stats.bubbles);
    } else if(section==4) {
        auto& c=worldVisualsConfig_.kills;
        if(panel::BeginRows("kill_effect_settings")) {
            panel::CheckRow("Enable kill effect",&c.enabled); DrawBind(keys::Action::KillEffect,true);
            ImGui::BeginDisabled(!c.enabled);
            constexpr const char* shapes[]={"Orbs","Stars"};
            EnumRow("Particle style",c.shape,shapes,IM_ARRAYSIZE(shapes));
            panel::CheckRow("Current target only",&c.targetOnly);
            panel::SliderIntRow("Particles per death",&c.amount,8,48,"%d");
            panel::SliderRow("Lifetime",&c.lifetime,.3f,3,"%.1f s");
            panel::SliderRow("Ground ring radius",&c.radius,.3f,3,"%.2f m");
            panel::SliderRow("Particle size",&c.size,.02f,.2f,"%.3f m");
            panel::SliderRow("Burst speed",&c.speed,.1f,4,"%.1f m/s");
            panel::SliderRow("Effect distance (shared)",&worldVisualsConfig_.maxDistance,4,128,"%.0f m");
            panel::SliderRow("Opacity",&c.opacity,0,1,"%.2f");
            panel::SliderRow("Glow intensity",&c.glow,0,2,"%.2f");
            PaletteRows(c.colours);
            ImGui::EndDisabled(); ImGui::EndTable();
        }
        ImGui::TextWrapped("A ground ring and glowing burst appear when an entity changes from positive HP to zero. Deaths between snapshots may be missed; a disappearing entity alone does not trigger the effect.");
        ImGui::TextDisabled("Death rings: %d   Particles: %d / %d",stats.kills,stats.killParticles,WorldVisualsRenderer::kKillParticles);
    } else if(section==5) {
        auto& c=worldVisualsConfig_.crumbs;
        if(panel::BeginRows("breadcrumb_settings")) {
            panel::CheckRow("Enable BreadCrumbs",&c.enabled); DrawBind(keys::Action::BreadCrumbs,true);
            ImGui::BeginDisabled(!c.enabled);
            panel::SliderRow("Lifetime",&c.lifetime,.3f,4,"%.1f s");
            panel::SliderRow("Point spacing",&c.spacing,.1f,1,"%.2f m");
            panel::SliderRow("Point size",&c.size,.02f,.15f,"%.3f m");
            panel::SliderRow("Effect distance (shared)",&worldVisualsConfig_.maxDistance,4,128,"%.0f m");
            panel::SliderRow("Opacity",&c.opacity,0,1,"%.2f");
            panel::SliderRow("Glow intensity",&c.glow,0,2,"%.2f");
            PaletteRows(c.colours);
            ImGui::EndDisabled(); ImGui::EndTable();
        }
        ImGui::TextWrapped("Leaves a fading glow path at your feet, visible in first person and F5. During jumps the points follow your feet; there is no ground raycast.");
        ImGui::TextDisabled("Buffered points: %d / %d",stats.crumbs,WorldVisualsRenderer::kCrumbs);
    } else if(section==6) {
        auto& c=localHatConfig_;
        if(panel::BeginRows("local_hat_settings")) {
            panel::CheckRow("Enable own China hat",&c.enabled); DrawBind(keys::Action::ChinaHat,true);
            ImGui::BeginDisabled(!c.enabled);
            panel::CheckRow("Also draw in first person",&c.firstPerson);
            panel::SliderRow("Transparency / opacity",&c.shape.opacity,0,.65f,"%.2f");
            panel::SliderRow("Glow intensity",&c.shape.glow,0,1,"%.2f");
            panel::SliderRow("Brim radius",&c.shape.radiusScale,.5f,2,"%.2f x");
            panel::SliderRow("Cone height",&c.shape.heightScale,.2f,1,"%.2f x");
            panel::SliderRow("Head gap",&c.shape.headGap,0,.2f,"%.3f m");
            panel::CheckRow("Use Target Aura colours",&c.auraColours);
            ImGui::BeginDisabled(c.auraColours);
            panel::ColorRow("Start colour",&c.start.x); panel::ColorRow("End colour",&c.end.x);
            ImGui::EndDisabled(); ImGui::EndDisabled(); ImGui::EndTable();
        }
        ImGui::TextWrapped("Your own hat is enabled in F5 by default. It follows the sampled head pose. First-person hiding uses camera distance; very close F5 wall collisions can also hide it. Friend hats stay automatic.");
    }
}
} // namespace mcov
