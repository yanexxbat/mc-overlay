#include "smp_features.hpp"
#include <cstdio>
#include <cstring>

namespace mcov::smp {
namespace {
float Finite(float value,float low,float high,float fallback) {
    return std::isfinite(value)?std::clamp(value,low,high):fallback;
}
void ClampColour(ImVec4& c) {
    c.x=Finite(c.x,0,1,1); c.y=Finite(c.y,0,1,1); c.z=Finite(c.z,0,1,1); c.w=Finite(c.w,0,1,1);
}
double Distance(WorldPoint a,const WorldSnapshot& s) {
    return std::hypot(a.x-s.posX,a.y-s.posY,a.z-s.posZ);
}
void Label(ImDrawList& draw,ScreenRect rect,const char* text,ImU32 colour,ImVec2 viewport) {
    const ImVec2 size=ImGui::CalcTextSize(text);
    const ImVec2 at{std::clamp((rect.left+rect.right-size.x)*.5f,0.f,std::max(0.f,viewport.x-size.x)),
        std::clamp(rect.top-size.y-4,0.f,std::max(0.f,viewport.y-size.y))};
    draw.AddText({at.x+1,at.y+1},IM_COL32(0,0,0,220),text); draw.AddText(at,colour,text);
}
}
void Config::Clamp() {
    storage.mask&=kStorageMask;
    for(auto& c:storage.colours) ClampColour(c);
    stash.radius=std::clamp(stash.radius,4,48); stash.interval=std::clamp(stash.interval,200,3000);
    stash.minContainers=std::clamp(stash.minContainers,2,1024); stash.maxDraw=Finite(stash.maxDraw,8,512,128);
    stash.mask&=kStorageMask; ClampColour(stash.colour);
    spawner.radius=std::clamp(spawner.radius,4,48); spawner.interval=std::clamp(spawner.interval,200,3000);
    spawner.maxDraw=Finite(spawner.maxDraw,8,128,64); spawner.glow=Finite(spawner.glow,0,2,.75f); ClampColour(spawner.colour);
}
void StashFinder::Reset() {
    count_=0; scan_=0; mask_=0; minimum_=0; dimension_={}; server_={};
}
int StashFinder::Update(const WorldSnapshot& s,const StashSettings& c) {
    if(std::strncmp(dimension_.data(),s.dimension,kNameLen)||std::strncmp(server_.data(),s.serverAddress,kAddrLen)) {
        Reset(); std::memcpy(dimension_.data(),s.dimension,kNameLen); std::memcpy(server_.data(),s.serverAddress,kAddrLen);
    }
    if(!c.enabled||!s.dimension[0]) return 0;
    if(mask_!=c.mask||minimum_!=c.minContainers) { count_=0; scan_=0; mask_=c.mask; minimum_=c.minContainers; }
    if(!s.blockScanComplete||!s.blockScanId||s.blockScanId==scan_||!c.mask||(s.blockScanMask&c.mask)!=c.mask) return 0;
    if(s.blockScanId<scan_) { count_=0; } // Reattached agent starts a new session.
    scan_=s.blockScanId;
    std::array<Stash,kMaxBlocks> chunks{}; int chunkCount=0;
    for(const auto& block:std::span(s.blocks,std::min(s.blockCount,kMaxBlocks))) {
        if(!StorageKind(block.kind)||!(c.mask&(1u<<block.kind))||!std::isfinite(block.x)||!std::isfinite(block.y)||!std::isfinite(block.z)||
           std::abs(block.x)>3e7||std::abs(block.z)>3e7||std::abs(block.y)>4096) continue;
        // Only count this module's cube when another module asks for a larger scan.
        if(std::abs(block.x-s.blockScanX)>c.radius||std::abs(block.y-s.blockScanY)>c.radius||std::abs(block.z-s.blockScanZ)>c.radius) continue;
        const int x=static_cast<int>(std::floor(double(block.x)/16)),z=static_cast<int>(std::floor(double(block.z)/16));
        auto it=std::find_if(chunks.begin(),chunks.begin()+chunkCount,[&](const Stash& f){return f.chunkX==x&&f.chunkZ==z;});
        if(it==chunks.begin()+chunkCount) {
            it=chunks.begin()+chunkCount++; it->chunkX=x; it->chunkZ=z;
            it->bounds={{block.x,block.y,block.z},{block.x+1.,block.y+1.,block.z+1.}};
        }
        ++it->count;
        it->centre.x+=block.x+.5; it->centre.y+=block.y+.5; it->centre.z+=block.z+.5;
        it->bounds.min.x=std::min(it->bounds.min.x,double(block.x)); it->bounds.min.y=std::min(it->bounds.min.y,double(block.y));
        it->bounds.min.z=std::min(it->bounds.min.z,double(block.z)); it->bounds.max.x=std::max(it->bounds.max.x,block.x+1.);
        it->bounds.max.y=std::max(it->bounds.max.y,block.y+1.); it->bounds.max.z=std::max(it->bounds.max.z,block.z+1.);
    }
    int added=0;
    for(auto it=chunks.begin();it!=chunks.begin()+chunkCount;++it) {
        if(it->count<c.minContainers) continue;
        it->centre.x/=it->count; it->centre.y/=it->count; it->centre.z/=it->count;
        auto old=std::find_if(finds_.begin(),finds_.begin()+count_,[&](const Stash& f){return f.chunkX==it->chunkX&&f.chunkZ==it->chunkZ;});
        if(old!=finds_.begin()+count_) {
            // A sweep can cover only part of a chunk; retain the strongest observation.
            if(it->count>=old->count) *old=*it;
        } else if(count_<kMaxFinds) { finds_[count_++]=*it; ++added; }
    }
    return added;
}
void DrawBlocks(ImDrawList& draw,const WorldSnapshot& s,const CameraProjection& p,const Config& c,
                bool storageEnabled,float range,float opacity,float glow,ImVec2 viewport,ImTextureID glowTexture) {
    if(!storageEnabled&&!c.spawner.enabled) return;
    for(const auto& block:std::span(s.blocks,std::min(s.blockCount,kMaxBlocks))) {
        const bool storage=storageEnabled&&StorageKind(block.kind)&&(c.storage.mask&(1u<<block.kind));
        const bool spawner=c.spawner.enabled&&block.kind==kBlockSpawner;
        if(!storage&&!spawner) continue;
        const WorldPoint centre{block.x+.5,block.y+.5,block.z+.5}; const double distance=Distance(centre,s);
        if(!std::isfinite(distance)||distance>(storage?range:c.spawner.maxDraw)) continue;
        ScreenRect rect; if(!p.Bounds({{block.x,block.y,block.z},{block.x+1.,block.y+1.,block.z+1.}},rect)) continue;
        ImVec4 colour=storage?c.storage.Colour(block.kind):c.spawner.colour; if(storage) colour.w*=opacity;
        if(colour.w<=0) continue;
        const ImU32 ink=ImGui::ColorConvertFloat4ToU32(colour);
        soft::Outline(draw,{rect.left,rect.top},{rect.right,rect.bottom},4,1.5f,colour,colour,storage?glow:c.spawner.glow,glowTexture);
        if(storage?c.storage.tracers:c.spawner.tracers)
            draw.AddLine({viewport.x*.5f,viewport.y},{(rect.left+rect.right)*.5f,rect.bottom},ink,1);
        if(storage?c.storage.labels:c.spawner.labels) {
            char text[80]; std::snprintf(text,sizeof(text),"%s | %.0f m",BlockKindLabel(block.kind),distance); Label(draw,rect,text,ink,viewport);
        }
    }
}
void DrawStashes(ImDrawList& draw,const WorldSnapshot& s,const CameraProjection& p,const StashSettings& c,std::span<const Stash> finds,ImVec2 viewport) {
    if(!c.enabled||c.colour.w<=0) return;
    const ImU32 ink=ImGui::ColorConvertFloat4ToU32(c.colour);
    for(const auto& f:finds) {
        const double distance=Distance(f.centre,s); if(distance>c.maxDraw) continue;
        ScreenRect rect; if(!p.Bounds(f.bounds,rect)) continue;
        draw.AddRect({rect.left,rect.top},{rect.right,rect.bottom},ink,4,0,2);
        if(c.tracers) draw.AddLine({viewport.x*.5f,viewport.y},{(rect.left+rect.right)*.5f,rect.bottom},ink,1);
        if(c.labels) { char text[112]; std::snprintf(text,sizeof(text),"Stash | %d containers | %.0f m",f.count,distance); Label(draw,rect,text,ink,viewport); }
    }
}
} // namespace mcov::smp
