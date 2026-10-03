#include "esp_renderer.hpp"
#include "../common/flat_style.hpp"
#include "../common/equipment_icons.hpp"
#include "imgui_internal.h"
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace mcov {
namespace {
constexpr ImU32 kText = IM_COL32(240, 240, 245, 255);
constexpr ImU32 kMuted = IM_COL32(168, 172, 181, 255);
constexpr ImU32 kLineShadow = IM_COL32(8, 8, 12, 100);
constexpr ImU32 kTextShadow = IM_COL32(0, 0, 0, 190);

float Safe(float value, float fallback, float low, float high) {
    return std::clamp(std::isfinite(value) ? value : fallback, low, high);
}
ImU32 Color(const float rgba[4], float opacity = 1.f) {
    return ImGui::ColorConvertFloat4ToU32({Safe(rgba[0],1,0,1), Safe(rgba[1],1,0,1),
        Safe(rgba[2],1,0,1), Safe(rgba[3],1,0,1)*opacity});
}
bool Visible(const EntitySnapshot& e, const VisualSettings& config) {
    if ((e.flags & kEntityIsSelf) || !std::isfinite(e.distance) || e.distance < 0 ||
        e.distance > Safe(config.maxDistance,128,1,4096)) return false;
    if (e.flags & kEntityIsPlayer) return config.showPlayers;
    if (e.flags & kEntityIsHostile) return config.showHostiles;
    return config.showOther;
}
bool HasHealth(const EntitySnapshot& e) {
    return (e.flags & kEntityIsLiving) && std::isfinite(e.health) &&
           std::isfinite(e.maxHealth) && e.maxHealth > 0;
}
class ClipScope final {
    ImDrawList& draw_;
public:
    ClipScope(ImDrawList& draw, ImVec2 origin, ImVec2 size) : draw_(draw) {
        draw_.PushClipRect(origin,{origin.x+size.x,origin.y+size.y},true);
    }
    ~ClipScope() { draw_.PopClipRect(); }
    ClipScope(const ClipScope&) = delete;
    ClipScope& operator=(const ClipScope&) = delete;
};
float TextWidth(ImFont& font, float size, const char* text) {
    return font.CalcTextSizeA(size,FLT_MAX,0,text).x;
}
void Text(ImDrawList& draw, ImFont& font, float size, ImVec2 at, ImU32 color, const char* text) {
    draw.AddText(&font,size,{at.x+1,at.y+1},kTextShadow,text);
    draw.AddText(&font,size,at,color,text);
}
// Fester Stack-Puffer, begrenzte Snapshot-Strings und UTF-8-sichere Kürzung.
using NameBuffer = std::array<char,kNameLen+4>;
NameBuffer Name(ImFont& font, float size, const EntitySnapshot& e, float maxWidth) {
    NameBuffer result{};
    const char* end=std::find(e.name,e.name+kNameLen,'\0');
    const char* begin=e.name;
    if (begin==end) { begin="Entity"; end=begin+6; }
    if (font.CalcTextSizeA(size,FLT_MAX,0,begin,end).x <= maxWidth) {
        std::memcpy(result.data(),begin,std::size_t(end-begin));
    } else {
        const char* stop=begin;
        font.CalcTextSizeA(size,std::max(0.f,maxWidth-TextWidth(font,size,"...")),0,begin,end,&stop);
        const auto count=std::size_t(stop-begin);
        std::memcpy(result.data(),begin,count);
        std::memcpy(result.data()+count,"...",3);
    }
    return result;
}
void CenteredText(ImDrawList& draw, ImFont& font, float size, float center, float y,
                  ImU32 color, const char* text, ImVec2 origin, ImVec2 viewport) {
    const float width=TextWidth(font,size,text);
    const float x=std::clamp(center-width*.5f,origin.x+2,std::max(origin.x+2,origin.x+viewport.x-width-2));
    const float top=std::clamp(y,origin.y+2,std::max(origin.y+2,origin.y+viewport.y-size-2));
    Text(draw,font,size,{x,top},color,text);
}
void Box(ImDrawList& draw, ScreenRect rect, ImU32 color, const VisualSettings& config) {
    const float rounding=std::min({Safe(config.boxRounding,7,0,16),
                                  (rect.right-rect.left)*.25f,(rect.bottom-rect.top)*.1f});
    const float thickness=Safe(config.boxThickness,1.6f,.5f,4);
    const ImVec2 lo{rect.left,rect.top},hi{rect.right,rect.bottom};
    if (config.filledBox) {
        const auto alpha=unsigned(float((color>>IM_COL32_A_SHIFT)&255)*.055f);
        draw.AddRectFilled(lo,hi,(color&~IM_COL32_A_MASK)|(alpha<<IM_COL32_A_SHIFT),rounding);
    }
    // Nur eine dezente Lesekante; keine breite schwarze Doppelbox.
    draw.AddRect(lo,hi,kLineShadow,rounding,0,thickness+1.f);
    draw.AddRect(lo,hi,color,rounding,0,thickness);
}
void Skeleton(ImDrawList& draw, const EntitySnapshot& e, const CameraProjection& projection,
              ImU32 color, float thickness, ImVec2 origin) {
    if (e.rigKind==kRigNone) return;
    const auto rig=BuildHumanoidRig(e);
    for (const auto& edge:HumanoidRig::edges) {
        // Die geraden Schulter-/Hüftquerlinien reichen: keine zusätzlichen
        // Halsdiagonalen oder doppelt überzeichneten Hüftverbindungen.
        if ((edge[0]==1 && (edge[1]==3 || edge[1]==4)) || edge[0]==2) continue;
        ScreenPoint a,b;
        if (!projection.Segment(rig.joints[edge[0]],rig.joints[edge[1]],a,b)) continue;
        const ImVec2 start{origin.x+a.x,origin.y+a.y},end{origin.x+b.x,origin.y+b.y};
        draw.AddLine(start,end,kLineShadow,thickness+1.f);
        draw.AddLine(start,end,color,thickness);
    }
}
void HealthBar(ImDrawList& draw, ImFont& font, float fontSize, const EntitySnapshot& e,
               ScreenRect rect, const VisualSettings& config, ImVec2 origin, ImVec2 viewport) {
    const float fraction=std::clamp(e.health/e.maxHealth,0.f,1.f);
    const float height=rect.bottom-rect.top;
    const ImVec2 lo{rect.left-9.f,rect.top},hi{rect.left-6.f,rect.bottom};
    draw.AddRectFilled({lo.x-1,lo.y-1},{hi.x+1,hi.y+1},IM_COL32(12,12,17,150),2.f);
    if (fraction>0) {
        const ImVec2 fill{lo.x,hi.y-height*fraction};
        const int first=draw.VtxBuffer.Size;
        const ImU32 accent=Color(config.healthColor);
        const auto alpha=accent&IM_COL32_A_MASK;
        draw.AddRectFilled(fill,hi,(kText&~IM_COL32_A_MASK)|alpha,1.5f);
        // Verlauf auf der abgerundeten Geometrie; AA-Alpha an den Rändern bleibt erhalten.
        ImGui::ShadeVertsLinearColorGradientKeepAlpha(&draw,first,draw.VtxBuffer.Size,
            {lo.x,rect.top},{lo.x,rect.bottom},kText,accent);
    }
    if (!config.nameTags && height>=48 && rect.right-rect.left>=30) {
        char value[24]; std::snprintf(value,sizeof(value),"%.0f",std::max(0.f,e.health));
        CenteredText(draw,font,fontSize,(lo.x+hi.x)*.5f,rect.top-fontSize-4,kText,value,origin,viewport);
    }
}
void Equipment(ImDrawList& draw,ImFont& font,const EntitySnapshot& e,ImVec2 at,
               float center,ImTextureID texture,ImVec2 origin,ImVec2 viewport) {
    constexpr float step=22,iconSize=16;
    int count=0;
    for (const auto& item:e.details.equipment) if (item.present) ++count;
    if (!count) return;
    const float width=step*count-6;
    const float left=std::clamp(center-width*.5f,origin.x+3,std::max(origin.x+3,origin.x+viewport.x-width-3));
    // Zuerst nur Atlas-Quads, danach Text/Balken mit der Font-Textur zeichnen.
    if (texture) {
        draw.PushTextureID(texture);
        int n=0;
        for (const auto& item:e.details.equipment) if (item.present) {
            const auto index=item.iconIndex;
            if (index && index<=equipment_icons::kNames.size()) {
                const float x=float((index%equipment_icons::kColumns)*equipment_icons::kCell+2);
                const float y=float((index/equipment_icons::kColumns)*equipment_icons::kCell+2);
                draw.AddImage(texture,{left+step*n,at.y},{left+step*n+iconSize,at.y+iconSize},
                    {x/equipment_icons::kWidth,y/equipment_icons::kHeight},
                    {(x+equipment_icons::kSize)/equipment_icons::kWidth,(y+equipment_icons::kSize)/equipment_icons::kHeight});
            }
            ++n;
        }
        draw.PopTextureID();
    }
    int n=0;
    for (const auto& item:e.details.equipment) if (item.present) {
        const ImVec2 lo{left+step*n,at.y},hi{left+step*n+iconSize,at.y+iconSize};
        if (!texture || !item.iconIndex || item.iconIndex>equipment_icons::kNames.size()) {
            draw.AddRectFilled(lo,hi,IM_COL32(28,28,36,220),3);
            draw.AddRect(lo,hi,IM_COL32(146,146,164,190),3);
            Text(draw,font,11,{lo.x+4,lo.y+1},kMuted,"?");
        }
        if (item.maxDamage>0) {
            const float fraction=std::clamp(1.f-float(item.damage)/float(item.maxDamage),0.f,1.f);
            draw.AddRectFilled({lo.x,hi.y+2},{hi.x,hi.y+4},IM_COL32(20,20,26,210),1);
            if (fraction>0) draw.AddRectFilled({lo.x,hi.y+2},{lo.x+iconSize*fraction,hi.y+4},
                fraction<.2f ? IM_COL32(244,107,118,255) : IM_COL32(174,150,246,255),1);
        }
        if (item.count>1) {
            char value[12]; std::snprintf(value,sizeof(value),"%d",std::min(item.count,999));
            Text(draw,font,9,{hi.x-TextWidth(font,9,value),hi.y-8},kText,value);
        }
        ++n;
    }
}
void NameTag(ImDrawList& draw,ImFont& font,float size,const EntitySnapshot& e,ScreenRect rect,
             const VisualSettings& config,ImVec2 origin,ImVec2 viewport,ImTextureID texture) {
    const bool hp=config.nameTagHealth && HasHealth(e);
    const bool ping=config.nameTagPing && (e.flags&kEntityIsPlayer);
    const float gap=10,padding=8;
    char health[24],latency[24],distance[24];
    std::snprintf(health,sizeof(health),"%.1f",hp ? std::max(0.f,e.health) : 0.f);
    if (e.details.ping>=0) std::snprintf(latency,sizeof(latency),"%dms",e.details.ping);
    else std::snprintf(latency,sizeof(latency),"n/a");
    std::snprintf(distance,sizeof(distance),"%.1fm",e.distance);
    const float fixed=padding*2+(hp ? TextWidth(font,size,health)+gap : 0)+
        (ping ? TextWidth(font,size,latency)+gap : 0)+(config.distanceText ? TextWidth(font,size,distance)+gap : 0);
    const float maxWidth=std::min(viewport.x-12,300.f);
    const auto name=Name(font,size,e,std::max(12.f,maxWidth-fixed));
    const float width=std::min(maxWidth,fixed+TextWidth(font,size,name.data()));
    const float height=size+7,center=(rect.left+rect.right)*.5f;
    bool gear=false;
    if (config.nameTagEquipment) for (const auto& item:e.details.equipment) gear|=item.present!=0;
    const float top=std::clamp(rect.top-height-4,origin.y+(gear ? 28.f : 2.f),
                               std::max(origin.y+(gear ? 28.f : 2.f),origin.y+viewport.y-height-2));
    const ImVec2 lo{std::clamp(center-width*.5f,origin.x+3,std::max(origin.x+3,origin.x+viewport.x-width-3)),top};
    if (gear) Equipment(draw,font,e,{0,top-25},lo.x+width*.5f,texture,origin,viewport);
    draw.AddRectFilled(lo,{lo.x+width,lo.y+height},IM_COL32(14,14,18,215),5);
    ClipScope clip(draw,{lo.x+padding,lo.y},{std::max(1.f,width-padding*2),height});
    float x=lo.x+padding;
    const auto field=[&](const char* text,ImU32 color) {
        Text(draw,font,size,{x,lo.y+3},color,text); x+=TextWidth(font,size,text)+gap;
    };
    if (config.distanceText) field(distance,kMuted);
    if (ping) field(latency,e.details.ping<0 ? kMuted : e.details.ping>250 ? IM_COL32(248,101,116,255) :
                           e.details.ping>100 ? IM_COL32(244,202,113,255) : IM_COL32(112,231,151,255));
    field(name.data(),kText);
    if (hp) field(health,IM_COL32(248,101,116,255));
}
void TargetInfo(ImDrawList& draw, ImFont& font, const WorldSnapshot& snapshot,
                const EntitySnapshot& e, float labelSize, ImVec2 origin, ImVec2 viewport) {
    if (viewport.x<160 || viewport.y<120) return;
    const float size=std::clamp(labelSize+1,11.f,19.f),gap=16.f,padding=10.f;
    char distance[32],ping[24],health[24];
    std::snprintf(distance,sizeof(distance),"%.1fm",e.distance);
    std::snprintf(ping,sizeof(ping),"%dms",e.details.ping);
    const bool showPing=e.details.ping>=0,showHealth=HasHealth(e);
    std::snprintf(health,sizeof(health),"%.1f",showHealth ? std::max(0.f,e.health) : 0.f);
    const float fixed=TextWidth(font,size,distance)+(showPing ? TextWidth(font,size,ping)+gap : 0)+
                      (showHealth ? TextWidth(font,size,health)+gap : 0)+gap+padding*2;
    const auto name=Name(font,size,e,std::max(20.f,std::min(240.f,viewport.x-12-fixed)));
    const float width=std::min(viewport.x-12,fixed+TextWidth(font,size,name.data()));
    const float height=size+8;
    const ImVec2 lo{origin.x+(viewport.x-width)*.5f,origin.y+std::clamp(viewport.y*.13f,48.f,96.f)};
    draw.AddRectFilled(lo,{lo.x+width,lo.y+height},IM_COL32(14,14,18,215),height*.4f);
    // Das eigene Pill-ClipRect begrenzt auch sehr lange Namen auf kleinen Viewports.
    ClipScope clip(draw,{lo.x+padding,lo.y},{width-padding*2,height});
    float x=lo.x+padding;
    const auto field=[&](const char* value,ImU32 color) {
        Text(draw,font,size,{x,lo.y+4},color,value); x+=TextWidth(font,size,value)+gap;
    };
    field(distance,kMuted);
    if (showPing) field(ping,IM_COL32(112,231,151,255));
    field(name.data(),kText);
    if (showHealth) field(health,IM_COL32(248,101,116,255));
}
} // namespace

void EspRenderer::Draw(ImDrawList& draw, const WorldSnapshot& snapshot,
                       const CameraProjection& projection, const VisualSettings& config,
                       ImVec2 viewport, ImVec2 origin,ImTextureID itemTexture) {
    if (!std::isfinite(viewport.x) || !std::isfinite(viewport.y) ||
        !std::isfinite(origin.x) || !std::isfinite(origin.y) || viewport.x<1 || viewport.y<1) return;
    ClipScope clip(draw,origin,viewport);
    auto& font=*style::MenuFont();
    const float size=Safe(config.labelSize,12,9,18);
    const float skeletonThickness=Safe(config.skeletonThickness,1.1f,.5f,3);
    const EntitySnapshot* target=nullptr;
    ScreenRect targetRect{};
    for (std::uint32_t i=0;i<std::min(snapshot.entityCount,kMaxEntities);++i) {
        const auto& e=snapshot.entities[i];
        if (!Visible(e,config)) continue;
        const float* rgba=(e.flags&kEntityIsPlayer) ? config.playerColor :
                          (e.flags&kEntityIsHostile) ? config.hostileColor : config.otherColor;
        const ImU32 color=Color(rgba);
        ScreenRect rect;
        if (!projection.Bounds(EntityBounds(e),rect)) {
            if (config.skeleton) Skeleton(draw,e,projection,color,skeletonThickness,origin);
            continue;
        }
        rect={rect.left+origin.x,rect.top+origin.y,rect.right+origin.x,rect.bottom+origin.y};
        const bool selected=snapshot.hasTarget && e.entityId==snapshot.targetEntityId;
        if (selected) { target=&e; targetRect=rect; }
        if (config.boxEsp) Box(draw,rect,color,config);
        if (config.skeleton) Skeleton(draw,e,projection,color,skeletonThickness,origin);
        if (config.tracers)
            draw.AddLine({origin.x+viewport.x*.5f,origin.y+viewport.y},
                         {(rect.left+rect.right)*.5f,rect.bottom},Color(config.tracerColor),1.f);
        if (config.healthBars && HasHealth(e)) HealthBar(draw,font,size,e,rect,config,origin,viewport);
        const float width=rect.right-rect.left,center=(rect.left+rect.right)*.5f;
        if (width>=24 && config.nameTags) {
            NameTag(draw,font,size,e,rect,config,origin,viewport,itemTexture);
        }
        // Beim Ziel steht die Entfernung schon oben in der kompakten Infozeile.
        if (width>=24 && !config.nameTags && config.distanceText && !(selected && config.targetInfo)) {
            char distance[32]; std::snprintf(distance,sizeof(distance),"%.1fm",e.distance);
            CenteredText(draw,font,size,center,rect.bottom+4,kMuted,distance,origin,viewport);
        }
    }
    if (config.targetInfo && target) {
        bool gear=false;
        if (config.nameTagEquipment) for (const auto& item:target->details.equipment) gear|=item.present!=0;
        const float infoTop=origin.y+std::clamp(viewport.y*.13f,48.f,96.f);
        const float nameTop=std::max(targetRect.top-size-11,origin.y+(gear ? 28.f : 2.f));
        // Der Entity-Nametag hat Vorrang, wenn er am oberen Rand die globale
        // Zielzeile schneiden würde. So überlagert diese keine Ausrüstung.
        const bool overlap=config.nameTags && targetRect.right-targetRect.left>=24 &&
            nameTop+size+7>=infoTop && nameTop-(gear ? 25.f : 0.f)<=infoTop+size+9;
        if (!overlap) TargetInfo(draw,font,snapshot,*target,size,origin,viewport);
    }
}

} // namespace mcov
