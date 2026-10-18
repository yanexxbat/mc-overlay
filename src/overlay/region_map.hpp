#pragma once
#include "../common/shared_state.hpp"
#include "../common/hud_style.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>

namespace mcov::regionmap {
struct Config {
    bool enabled=false;
    bool labels=true,grid=true,coordinates=true;
};
inline constexpr double kBorder=225000, kCell=50000;
inline constexpr ImVec2 kSize{308,414};
inline constexpr std::array<const char*,6> kNames={"EU Central","EU West","NA East","NA West","Asia","Oceania"};
inline constexpr std::array<const char*,6> kShort={"EUc","EUw","NAe","NAw","Asia","Oce"};
inline constexpr std::array<ImU32,6> kColors={IM_COL32(97,132,112,255),IM_COL32(60,108,100,255),
    IM_COL32(86,121,148,255),IM_COL32(60,81,104,255),IM_COL32(152,132,86,255),IM_COL32(153,107,81,255)};
// Geographic facts transcribed from https://donut.auction/regions, 2026-10-15.
// North (-Z) at the top; +X to the right. Server numbers are map cells, not
// live scoreboard shard IDs. The map can change independently of the client.
inline constexpr std::array<int,81> kRegions={
    5,3,3,3,2,2,2,2,2,
    5,3,3,3,2,2,2,2,2,
    5,3,3,3,2,2,2,2,2,
    5,5,5,3,2,2,2,2,2,
    4,4,4,4,2,2,2,2,2,
    4,1,1,0,0,0,0,0,2,
    4,1,1,0,0,0,0,0,2,
    0,1,0,0,0,0,0,0,0,
    0,1,1,1,1,1,1,0,0
};
inline int CellAt(double x,double z) {
    if(!std::isfinite(x)||!std::isfinite(z)||x < -kBorder||x > kBorder||z < -kBorder||z > kBorder) return -1;
    const int column=std::min(8,int(std::floor((x+kBorder)/kCell)));
    const int row=std::min(8,int(std::floor((z+kBorder)/kCell)));
    return row*9+column;
}
inline std::string Lower(std::string_view text) {
    std::string out(text);
    for(auto& c:out) if(c>='A'&&c<='Z') c=char(c+'a'-'A');
    return out;
}
inline bool DonutServer(std::string_view address) {
    auto host=Lower(address); const auto port=host.find(':');
    if(port!=std::string::npos) host.resize(port);
    if(host.ends_with('.')) host.pop_back();
    return host=="donutsmp.net"||host.ends_with(".donutsmp.net");
}
inline bool Overworld(std::string_view dimension) {
    const auto name=Lower(dimension);
    return name=="overworld"||name=="minecraft:overworld"||
           name=="resourcekey[minecraft:dimension / minecraft:overworld]";
}
inline void Draw(ImDrawList& d,const WorldSnapshot& s,const Config& c,bool live) {
    using namespace style::hud;
    Panel(d,{0,0},kSize); Heading(d,{0,0},kSize.x,"DonutSMP region map");
    constexpr float left=19,top=52,side=270,cell=side/9;
    style::hud::Label(d,{left,30},Muted,"-225k");
    const auto right=ImGui::CalcTextSize("+225k X");
    style::hud::Label(d,{left+side-right.x,30},Muted,"+225k X");
    const bool server=DonutServer(s.serverAddress),world=Overworld(s.dimension);
    const int current=live&&server&&world?CellAt(s.posX,s.posZ):-1;
    for(int i=0;i<81;++i) {
        const ImVec2 a{left+(i%9)*cell,top+(i/9)*cell},b{a.x+cell,a.y+cell};
        d.AddRectFilled(a,b,kColors[kRegions[i]]);
        if(c.labels) {
            const char* label=kShort[kRegions[i]];
            const auto extent=ImGui::CalcTextSize(label);
            style::hud::Label(d,{a.x+(cell-extent.x)*.5f,a.y+(cell-extent.y)*.5f},Text,label);
        }
    }
    if(c.grid) for(int n=0;n<=9;++n) {
        const float v=n*cell;
        d.AddLine({left+v,top},{left+v,top+side},IM_COL32(12,19,25,125));
        d.AddLine({left,top+v},{left+side,top+v},IM_COL32(12,19,25,125));
    }
    // The world origin is in the middle of cell 41, not at a grid corner.
    d.AddLine({left+side*.5f-4,top+side*.5f},{left+side*.5f+4,top+side*.5f},IM_COL32(12,19,25,180));
    d.AddLine({left+side*.5f,top+side*.5f-4},{left+side*.5f,top+side*.5f+4},IM_COL32(12,19,25,180));
    if(current>=0) {
        const ImVec2 a{left+(current%9)*cell,top+(current/9)*cell};
        d.AddRect({a.x+1,a.y+1},{a.x+cell-1,a.y+cell-1},IM_COL32(255,255,255,255),0,0,2);
        const ImVec2 p{left+float((s.posX+kBorder)/(kBorder*2))*side,
                       top+float((s.posZ+kBorder)/(kBorder*2))*side};
        d.AddCircleFilled(p,5,IM_COL32(12,19,25,255),16);
        d.AddCircleFilled(p,3,IM_COL32(255,255,255,255),16);
    }
    style::hud::Label(d,{left,top+side+4},Muted,"Z: -225k top / +225k bottom");
    for(int i=0;i<6;++i) {
        const float x=left+(i%3)*94.f,y=346+(i/3)*19.f;
        d.AddRectFilled({x,y+3},{x+7,y+10},kColors[i],2);
        style::hud::Label(d,{x+11,y},Text,kNames[i]);
    }
    char status[100]{};
    if(!live) std::snprintf(status,sizeof(status),"Waiting for live player data");
    else if(!server) std::snprintf(status,sizeof(status),"DonutSMP Overworld map");
    else if(!world) std::snprintf(status,sizeof(status),"Position: Overworld only");
    else if(current<0) std::snprintf(status,sizeof(status),"Position outside mapped area");
    else if(c.coordinates) std::snprintf(status,sizeof(status),"%s | X %.0f  Z %.0f",kNames[kRegions[current]],s.posX,s.posZ);
    else std::snprintf(status,sizeof(status),"%s",kNames[kRegions[current]]);
    style::hud::Label(d,{10,390},current>=0?Text:Muted,status);
}
} // namespace mcov::regionmap
