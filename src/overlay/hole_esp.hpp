#pragma once
#include "esp_geometry.hpp"
#include "../common/hole_detection.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>

namespace mcov::holes {
struct Settings {
    bool enabled=false,bedrock=true,obsidian=true,mixed=true,labels=false;
    int radius=8;
    float distance=32,height=.2f,fill=.12f;
    ImVec4 bedrockColour{.56f,.86f,.1f,1},obsidianColour{.65f,.53f,1,1},mixedColour{.95f,.75f,.35f,1};
};
inline void Draw(ImDrawList& draw,const WorldSnapshot& s,const CameraProjection& p,const Settings& c,ImVec2 viewport) {
    if(!c.enabled) return;
    const auto origin=ImGui::GetMainViewport()->Pos;
    for(unsigned i=0;i<std::min(s.holeCount,kMaxResults);++i) {
        const auto& h=s.holes[i]; const auto type=static_cast<Type>(h.kind);
        if((type==Type::Bedrock&&!c.bedrock)||(type==Type::Obsidian&&!c.obsidian)||(type==Type::Mixed&&!c.mixed)||
           type<Type::Bedrock||type>Type::Mixed) continue;
        const double distance=std::hypot(h.x+.5-s.posX,h.y-s.posY,h.z+.5-s.posZ);
        if(!std::isfinite(distance)||distance>c.distance||std::abs(h.y)>4096) continue;
        const auto colour=type==Type::Bedrock?c.bedrockColour:type==Type::Obsidian?c.obsidianColour:c.mixedColour;
        const auto ink=ImGui::ColorConvertFloat4ToU32(colour);
        const auto corner=[&](int n)->WorldPoint { return {h.x+((n&1)?1.:0.),h.y+.02+((n&2)?c.height:0.),h.z+((n&4)?1.:0.)}; };
        for(int n=0;n<8;++n) for(int bit:{1,2,4}) if(!(n&bit)) {
            ScreenPoint a,b; if(!p.Segment(corner(n),corner(n|bit),a,b)) continue;
            draw.AddLine({origin.x+a.x,origin.y+a.y},{origin.x+b.x,origin.y+b.y},ink,1.6f);
        }
        ScreenPoint floor[4]; bool visible=true; constexpr int order[]={0,1,5,4};
        for(int n=0;n<4;++n) visible&=p.Point(corner(order[n]),floor[n]);
        if(visible&&c.fill>0) {
            ImVec2 points[4]; for(int n=0;n<4;++n) points[n]={origin.x+floor[n].x,origin.y+floor[n].y};
            auto tint=colour; tint.w*=c.fill; draw.AddConvexPolyFilled(points,4,ImGui::ColorConvertFloat4ToU32(tint));
        }
        ScreenRect rect;
        if(c.labels&&p.Bounds({{h.x,h.y,h.z},{h.x+1.,h.y+c.height,h.z+1.}},rect)) {
            const auto extent=ImGui::CalcTextSize(Label(type));
            draw.AddText({origin.x+std::clamp((rect.left+rect.right-extent.x)*.5f,0.f,std::max(0.f,viewport.x-extent.x)),
                origin.y+std::clamp(rect.top-extent.y-3,0.f,std::max(0.f,viewport.y-extent.y))},ink,Label(type));
        }
    }
}
}
