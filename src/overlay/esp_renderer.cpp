#include "esp_renderer.hpp"
#include "visual_draw.hpp"
#include "visual_geometry.hpp"
#include "../common/flat_style.hpp"
#include "../common/equipment_icons.hpp"
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

using visual::Safe;
using visual::Color;
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
    bool active_;
public:
    ClipScope(ImDrawList& draw, ImVec2 origin, ImVec2 size, bool active=true) : draw_(draw),active_(active) {
        if (active_) draw_.PushClipRect(origin,{origin.x+size.x,origin.y+size.y},true);
    }
    ~ClipScope() { if (active_) draw_.PopClipRect(); }
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
void Label(ImDrawList& draw, ImFont& font, float size, ImVec2 at, ImU32 color,
           const char* text, const VisualSettings& config) {
    if (config.textOutline) visual::RenderTextWithOutline(draw,font,size,at,color,text);
    else Text(draw,font,size,at,color,text);
}
// Fester Stack-Puffer, begrenzte Snapshot-Strings und UTF-8-sichere Kürzung.
using NameBuffer = std::array<char,kNameLen+4>;
NameBuffer Name(ImFont& font, float size, const EntitySnapshot& e, float maxWidth,bool protect=false) {
    NameBuffer result{};
    const char* end=std::find(e.name,e.name+kNameLen,'\0');
    const char* begin=e.name;
    if(protect&&(e.flags&kEntityIsPlayer)) { begin="Player"; end=begin+6; }
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
                  ImU32 color, const char* text, ImVec2 origin, ImVec2 viewport,
                  const VisualSettings& config) {
    const float width=TextWidth(font,size,text);
    const float x=std::clamp(center-width*.5f,origin.x+2,std::max(origin.x+2,origin.x+viewport.x-width-2));
    const float top=std::clamp(y,origin.y+2,std::max(origin.y+2,origin.y+viewport.y-size-2));
    Label(draw,font,size,{x,top},color,text,config);
}
void FillCuboid(ImDrawList& draw, const visual::Cuboid& cube, const WorldSnapshot& snapshot,
                const CameraProjection& projection, ScreenRect rect, ImU32 top, ImU32 bottom,
                ImVec2 origin, ImVec2 viewport) {
    const WorldPoint camera{snapshot.camX,snapshot.camY,snapshot.camZ};
    for (int face=0;face<6;++face) {
        if (!visual::FacingCamera(cube,face,camera)) continue;
        visual::ProjectedFace polygon;
        if (!visual::ProjectFace(projection,cube,face,viewport.x,viewport.y,polygon)) continue;
        ImVec2 points[12];
        for (int i=0;i<polygon.count;++i)
            points[i]={origin.x+polygon.points[i].x,origin.y+polygon.points[i].y};
        visual::DrawGradientPolygon(draw,points,polygon.count,{rect.left,rect.top},
                                    {rect.left,rect.bottom},top,bottom);
    }
}
void WireBox(ImDrawList& draw, const visual::Cuboid& cube, const CameraProjection& projection,
             ImU32 color, const VisualSettings& config, bool selected, ImVec2 origin) {
    const float thickness=Safe(config.boxThickness,1.6f,.5f,4);
    for (const auto& edge:visual::Cuboid::edges) {
        ScreenPoint a,b;
        if (!projection.Segment(cube.points[edge[0]],cube.points[edge[1]],a,b)) continue;
        const ImVec2 start{origin.x+a.x,origin.y+a.y},end{origin.x+b.x,origin.y+b.y};
        if (selected && config.targetGlow) {
            const float radius=Safe(config.glowRadius,5,0,10),opacity=Safe(config.glowOpacity,.45f,0,1);
            for (int layer=3;layer>=1 && radius>0 && opacity>0;--layer) {
                const float t=float(layer)/3;
                draw.AddLine(start,end,Color(config.targetColor,opacity*(.16f+(1-t)*.2f)),thickness+radius*t*2);
            }
        }
        draw.AddLine(start,end,visual::Alpha(kLineShadow,float((color>>IM_COL32_A_SHIFT)&255)/255.f),thickness+1.5f);
        const ImU32 bright=(IM_COL32_WHITE&~IM_COL32_A_MASK)|(color&IM_COL32_A_MASK);
        visual::DrawGradientLine(draw,start,end,visual::Mix(color,bright,.12f),color,thickness);
    }
}
void Box(ImDrawList& draw, ScreenRect rect, const visual::Cuboid& cube,
         const WorldSnapshot& snapshot, const CameraProjection& projection,
         ImU32 color, const VisualSettings& config, bool selected, ImVec2 origin, ImVec2 viewport) {
    const float rounding=std::min({Safe(config.boxRounding,7,0,16),
                                  (rect.right-rect.left)*.25f,(rect.bottom-rect.top)*.1f});
    const float thickness=Safe(config.boxThickness,1.6f,.5f,4);
    const bool wire=config.boxStyle==EspBoxStyle::Wire3D;
    const bool corners=config.boxStyle!=EspBoxStyle::Full2D;
    const float fraction=Safe(config.cornerFraction,.28f,.08f,.45f);
    const ImVec2 lo{rect.left,rect.top},hi{rect.right,rect.bottom};
    if (config.filledBox) {
        const float opacity=Safe(config.fillAlpha,.12f,0,1);
        const ImU32 top=visual::Alpha(color,opacity);
        const ImU32 bottom=visual::Alpha(color,config.fillGradient ?
            Safe(config.fillBottomAlpha,.025f,0,1) : opacity);
        if (wire) FillCuboid(draw,cube,snapshot,projection,rect,top,bottom,origin,viewport);
        else visual::DrawGradientRect(draw,lo,hi,top,bottom,rounding);
    }
    if (selected && config.targetGlow && !wire)
        visual::DrawBoxGlow(draw,lo,hi,Color(config.targetColor),thickness,rounding,
                            corners,fraction,config.glowRadius,config.glowOpacity);
    if (wire) WireBox(draw,cube,projection,color,config,selected,origin);
    else visual::DrawBoxOutline(draw,lo,hi,color,thickness,rounding,corners,fraction);
}
void Skeleton(ImDrawList& draw, const HumanoidRig& rig, const CameraProjection& projection,
              ImU32 color, float thickness, ImVec2 origin) {
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
ImU32 HealthColor(float fraction,const VisualSettings& config) {
    if (!config.healthGradient) return Color(config.healthColor);
    const float t=Safe(fraction,0,0,1);
    return t<.5f ? visual::Mix(Color(config.healthLow),Color(config.healthMiddle),t*2) :
        visual::Mix(Color(config.healthMiddle),Color(config.healthColor),(t-.5f)*2);
}
void Meter(ImDrawList& draw, ImVec2 lo, ImVec2 hi, float fraction, bool horizontal,
           ImU32 firstColor, ImU32 lastColor) {
    draw.AddRectFilled({lo.x-1,lo.y-1},{hi.x+1,hi.y+1},IM_COL32(9,11,18,200),2.f);
    draw.AddRectFilled(lo,hi,IM_COL32(31,35,44,190),1.f);
    if (fraction<=0) return;
    const ImVec2 fillLo=horizontal ? lo : ImVec2{lo.x,hi.y-(hi.y-lo.y)*fraction};
    const ImVec2 fillHi=horizontal ? ImVec2{lo.x+(hi.x-lo.x)*fraction,hi.y} : hi;
    const int first=draw.VtxBuffer.Size;
    draw.AddRectFilled(fillLo,fillHi,IM_COL32_WHITE,1.f);
    visual::ShadeGradient(draw,first,horizontal ? lo : ImVec2{lo.x,hi.y},
                          horizontal ? hi : lo,firstColor,lastColor);
}
float HealthBar(ImDrawList& draw, ImFont& font, float fontSize, const EntitySnapshot& e,
               ScreenRect rect, const VisualSettings& config, ImVec2 origin, ImVec2 viewport) {
    const bool horizontal=config.barPlacement==EspBarPlacement::Horizontal;
    const float thickness=Safe(config.barThickness,3,2,6);
    const float height=rect.bottom-rect.top;
    const float width=rect.right-rect.left;
    const bool health=HasHealth(e);
    const bool armor=config.armorBars && (e.flags&kEntityIsLiving) && e.details.armorValue>0;
    int rows=0;
    if (health) {
        const float fraction=std::clamp(e.health/e.maxHealth,0.f,1.f);
        const ImVec2 lo=horizontal ? ImVec2{rect.left,rect.bottom+5} : ImVec2{rect.left-6-thickness,rect.top};
        const ImVec2 hi=horizontal ? ImVec2{rect.right,lo.y+thickness} : ImVec2{rect.left-6,rect.bottom};
        const ImU32 current=HealthColor(fraction,config);
        // Use a short range around current HP, preserving HP-dependent color
        // even on a partially filled bar instead of retaining green at low HP.
        Meter(draw,lo,hi,fraction,horizontal,HealthColor(std::max(0.f,fraction-.22f),config),current);
        ++rows;
        if (!config.nameTags && !horizontal && height>=48 && width>=30) {
            char value[24]; std::snprintf(value,sizeof(value),"%.0f",std::max(0.f,e.health));
            CenteredText(draw,font,fontSize,(lo.x+hi.x)*.5f,rect.top-fontSize-4,current,value,origin,viewport,config);
        }
    }
    if (armor) {
        const float fraction=float(std::clamp(e.details.armorValue,0,20))/20.f;
        const ImVec2 lo=horizontal ? ImVec2{rect.left,rect.bottom+5+rows*(thickness+3)} : ImVec2{rect.right+6,rect.top};
        const ImVec2 hi=horizontal ? ImVec2{rect.right,lo.y+thickness} : ImVec2{lo.x+thickness,rect.bottom};
        const ImU32 accent=Color(config.armorColor);
        Meter(draw,lo,hi,fraction,horizontal,visual::Mix(accent,IM_COL32(54,87,164,255),.45f),accent);
        ++rows;
    }
    return horizontal && rows ? 5+rows*(thickness+3) : 0.f;
}
void Highlight(ImDrawList& draw, const EntitySnapshot& entity, const HumanoidRig* rig,
               const WorldSnapshot& snapshot, const CameraProjection& projection, ScreenRect rect,
               const VisualSettings& config, ImVec2 origin, ImVec2 viewport) {
    if (config.targetHighlight==EspHighlight::Off) return;
    const ImU32 top=Color(config.targetColor,Safe(config.highlightAlpha,.18f,0,.7f));
    const ImU32 bottom=visual::Alpha(top,.35f);
    if (config.targetHighlight==EspHighlight::Humanoid && rig) {
        const auto model=visual::BuildHighlight(entity,*rig);
        for (int i=0;i<model.count;++i)
            FillCuboid(draw,model.parts[i],snapshot,projection,rect,top,bottom,origin,viewport);
    } else FillCuboid(draw,visual::BoundsCuboid(EntityBounds(entity)),snapshot,projection,rect,top,bottom,origin,viewport);
}
void Tracer(ImDrawList& draw, const EntitySnapshot& entity, const CameraProjection& projection,
            const VisualSettings& config, ImVec2 origin, ImVec2 viewport) {
    // Anchor to the real feet, never the viewport-clamped bounding rectangle.
    ScreenPoint foot;
    if (!projection.Point({entity.x,entity.y,entity.z},foot)) return;
    if (foot.x<0 || foot.y<0 || foot.x>viewport.x || foot.y>viewport.y) return;
    ImVec2 start{.5f,1.f};
    switch (config.tracerOrigin) {
        case EspTracerOrigin::Center: start={.5f,.5f}; break;
        case EspTracerOrigin::Top: start={.5f,0}; break;
        case EspTracerOrigin::Custom: start={Safe(config.customTracerOrigin.x,.5f,0,1),Safe(config.customTracerOrigin.y,1,0,1)}; break;
        default: break;
    }
    visual::DrawGradientLine(draw,{origin.x+start.x*viewport.x,origin.y+start.y*viewport.y},
        {origin.x+foot.x,origin.y+foot.y},Color(config.tracerColor,Safe(config.tracerStartAlpha,.08f,0,1)),
        Color(config.tracerEndColor,Safe(config.tracerColor[3],1,0,1)),Safe(config.tracerThickness,1.25f,.5f,4));
}
void Equipment(ImDrawList& draw,ImFont& font,const EntitySnapshot& e,ImVec2 at,
               float center,ImTextureID texture,ImVec2 origin,ImVec2 viewport,const VisualSettings& config) {
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
            Label(draw,font,11,{lo.x+4,lo.y+1},kMuted,"?",config);
        }
        if (item.maxDamage>0) {
            const float fraction=std::clamp(1.f-float(item.damage)/float(item.maxDamage),0.f,1.f);
            draw.AddRectFilled({lo.x,hi.y+2},{hi.x,hi.y+4},IM_COL32(20,20,26,210),1);
            if (fraction>0) draw.AddRectFilled({lo.x,hi.y+2},{lo.x+iconSize*fraction,hi.y+4},
                fraction<.2f ? IM_COL32(244,107,118,255) : IM_COL32(174,150,246,255),1);
        }
        if (item.count>1) {
            char value[12]; std::snprintf(value,sizeof(value),"%d",std::min(item.count,999));
            Label(draw,font,9,{hi.x-TextWidth(font,9,value),hi.y-8},kText,value,config);
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
    const auto name=Name(font,size,e,std::max(12.f,maxWidth-fixed),config.nameProtect);
    const float width=std::min(maxWidth,fixed+TextWidth(font,size,name.data()));
    const float height=size+7,center=(rect.left+rect.right)*.5f;
    bool gear=false;
    if (config.nameTagEquipment) for (const auto& item:e.details.equipment) gear|=item.present!=0;
    const float top=std::clamp(rect.top-height-4,origin.y+(gear ? 28.f : 2.f),
                               std::max(origin.y+(gear ? 28.f : 2.f),origin.y+viewport.y-height-2));
    const ImVec2 lo{std::clamp(center-width*.5f,origin.x+3,std::max(origin.x+3,origin.x+viewport.x-width-3)),top};
    if (gear) Equipment(draw,font,e,{0,top-25},lo.x+width*.5f,texture,origin,viewport,config);
    draw.AddRectFilled(lo,{lo.x+width,lo.y+height},IM_COL32(14,14,18,215),5);
    // Normal tags already fit the viewport and need no individual scissor
    // command. Only unusually narrow viewports/overflowing fixed fields clip.
    ClipScope clip(draw,{lo.x+padding,lo.y},{std::max(1.f,width-padding*2),height},
                   fixed+TextWidth(font,size,name.data())>maxWidth);
    float x=lo.x+padding;
    const auto field=[&](const char* text,ImU32 color) {
        Label(draw,font,size,{x,lo.y+3},color,text,config); x+=TextWidth(font,size,text)+gap;
    };
    if (config.distanceText) field(distance,kMuted);
    if (ping) field(latency,e.details.ping<0 ? kMuted : e.details.ping>250 ? IM_COL32(248,101,116,255) :
                           e.details.ping>100 ? IM_COL32(244,202,113,255) : IM_COL32(112,231,151,255));
    field(name.data(),kText);
    if (hp) field(health,HealthColor(std::clamp(e.health/e.maxHealth,0.f,1.f),config));
}
void TargetInfo(ImDrawList& draw, ImFont& font, const WorldSnapshot& snapshot,
                const EntitySnapshot& e, float labelSize, ImVec2 origin, ImVec2 viewport,bool protect) {
    if (viewport.x<160 || viewport.y<120) return;
    const float size=std::clamp(labelSize+1,11.f,19.f),gap=16.f,padding=10.f;
    char distance[32],ping[24],health[24];
    std::snprintf(distance,sizeof(distance),"%.1fm",e.distance);
    std::snprintf(ping,sizeof(ping),"%dms",e.details.ping);
    const bool showPing=e.details.ping>=0,showHealth=HasHealth(e);
    std::snprintf(health,sizeof(health),"%.1f",showHealth ? std::max(0.f,e.health) : 0.f);
    const float fixed=TextWidth(font,size,distance)+(showPing ? TextWidth(font,size,ping)+gap : 0)+
                      (showHealth ? TextWidth(font,size,health)+gap : 0)+gap+padding*2;
    const auto name=Name(font,size,e,std::max(20.f,std::min(240.f,viewport.x-12-fixed)),protect);
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
        const auto bounds=EntityBounds(e);
        if (!visual::ValidBounds(bounds)) continue;
        // Reuse the sampled rig for both skeleton and the target silhouette.
        const bool selected=snapshot.hasTarget && e.entityId==snapshot.targetEntityId;
        const bool haveRig=e.rigKind!=kRigNone && std::isfinite(e.yaw) && std::isfinite(e.headPitch) &&
            std::isfinite(e.walkPhase) && std::isfinite(e.walkSpeed) && std::isfinite(e.modelHeight);
        HumanoidRig rig{};
        if (haveRig && (config.skeleton || (selected && config.boxEsp && config.targetHighlight==EspHighlight::Humanoid)))
            rig=BuildHumanoidRig(e);
        ScreenRect rect;
        if (!projection.Bounds(bounds,rect)) {
            if (config.skeleton && haveRig) Skeleton(draw,rig,projection,color,skeletonThickness,origin);
            continue;
        }
        rect={rect.left+origin.x,rect.top+origin.y,rect.right+origin.x,rect.bottom+origin.y};
        if (selected) { target=&e; targetRect=rect; }
        if (selected && config.boxEsp) Highlight(draw,e,haveRig ? &rig : nullptr,snapshot,projection,rect,config,origin,viewport);
        if (config.boxEsp) Box(draw,rect,visual::BoundsCuboid(bounds),snapshot,projection,
                              color,config,selected,origin,viewport);
        if (config.skeleton && haveRig) Skeleton(draw,rig,projection,color,skeletonThickness,origin);
        if (config.tracers) Tracer(draw,e,projection,config,origin,viewport);
        const float barOffset=config.healthBars ? HealthBar(draw,font,size,e,rect,config,origin,viewport) : 0.f;
        const float width=rect.right-rect.left,center=(rect.left+rect.right)*.5f;
        if (width>=24 && config.nameTags) {
            NameTag(draw,font,size,e,rect,config,origin,viewport,itemTexture);
        }
        // Beim Ziel steht die Entfernung schon oben in der kompakten Infozeile.
        if (width>=24 && !config.nameTags && config.distanceText && !(selected && config.targetInfo)) {
            char distance[32]; std::snprintf(distance,sizeof(distance),"%.1fm",e.distance);
            CenteredText(draw,font,size,center,rect.bottom+4+barOffset,kMuted,distance,origin,viewport,config);
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
        if (!overlap) TargetInfo(draw,font,snapshot,*target,size,origin,viewport,config.nameProtect);
    }
}

} // namespace mcov
