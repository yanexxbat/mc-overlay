#pragma once
#include "soft_glow.hpp"
namespace mcov::soft {
// Shared Storage ESP cell treatment, also used for Chunkfinder evidence.
inline void StorageCell(ImDrawList& draw,const WorldSnapshot& s,const CameraProjection& p,
                        WorldBounds bounds,ImVec4 colour,float glow,ImTextureID glowTexture) {
            std::array<WorldPoint,8> corners{};
            for(int i=0;i<8;++i) corners[i]={i&1?bounds.max.x:bounds.min.x,i&2?bounds.max.y:bounds.min.y,i&4?bounds.max.z:bounds.min.z};
            // Side faces run bottom/top/top/bottom for a world-height gradient.
            constexpr int faces[][4]={{0,2,6,4},{5,7,3,1},{0,4,5,1},{2,3,7,6},{1,3,2,0},{4,6,7,5}};
            // Fill only the camera-facing shell so the rear faces do not
            // double the opacity or wash out the underlying container model.
            const bool facing[]={s.camX<=bounds.min.x,s.camX>=bounds.max.x,s.camY<=bounds.min.y,s.camY>=bounds.max.y,
                s.camZ<=bounds.min.z,s.camZ>=bounds.max.z};
            ImVec4 bottom=colour,top=colour,cap=colour;
            bottom.w*=.025f; top.w*=.16f; cap.w*=.09f;
            const auto texture=ImGui::GetIO().Fonts->TexID;
            const auto white=ImGui::GetIO().Fonts->TexUvWhitePixel;
            for(int i=0;i<6;++i) if(facing[i]) {
                const auto& face=faces[i];
                const bool horizontal=i==2||i==3;
                soft::WorldQuad(draw,p,{corners[face[0]],corners[face[1]],corners[face[2]],corners[face[3]]},texture,
                    horizontal?cap:bottom,horizontal?cap:top,white,white);
            }
            ImVec4 edge=colour;
            edge.x+=(1-edge.x)*.12f; edge.y+=(1-edge.y)*.12f; edge.z+=(1-edge.z)*.12f;
            edge.w*=.9f;
            for(int i=0;i<8;++i) for(int bit:{1,2,4}) if(!(i&bit)) {
                ScreenPoint a,b;
                if(!p.Segment(corners[i],corners[i|bit],a,b)) continue;
                // Keep rear edges legible without giving every crossing the
                // same weight as the visible shell.
                const bool front=(bit!=1&&facing[i&1])||(bit!=2&&facing[2+((i>>1)&1)])||
                    (bit!=4&&facing[4+((i>>2)&1)]);
                ImVec4 tint=edge; if(!front) tint.w*=.30f;
                soft::Stroke(draw,{a.x,a.y},{b.x,b.y},.9f,tint,front?std::clamp(glow,0.f,1.5f):0.f,glowTexture);
            }
}
} // namespace mcov::soft
