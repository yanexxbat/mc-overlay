#include "smp_features.hpp"
#include "storage_cell.hpp"
#include "../common/block_scan_volume.hpp"
#include "../common/chunk_hints.hpp"
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
    storage.interval=std::clamp(storage.interval,200,3000);
    storage.pulseDuration=std::clamp(storage.pulseDuration,500,6000);
    for(auto& c:storage.colours) ClampColour(c);
    stash.radius=std::clamp(stash.radius,4,48); stash.interval=std::clamp(stash.interval,200,3000);
    stash.minContainers=std::clamp(stash.minContainers,2,128); stash.maxDraw=Finite(stash.maxDraw,8,512,128);
    stash.mask&=kStorageMask; ClampColour(stash.colour);
    spawner.radius=std::clamp(spawner.radius,4,48); spawner.interval=std::clamp(spawner.interval,200,3000);
    spawner.maxDraw=Finite(spawner.maxDraw,8,128,64); spawner.glow=Finite(spawner.glow,0,2,.75f); ClampColour(spawner.colour);
    base.maxDraw=Finite(base.maxDraw,8,512,192); base.padding=Finite(base.padding,0,16,4);
    base.textureOpacity=Finite(base.textureOpacity,0,1,1);
    base.fillOpacity=Finite(base.fillOpacity,0,.5f,.12f); base.lineWidth=Finite(base.lineWidth,.5f,4,1.8f); ClampColour(base.colour);
    finder.radius=std::clamp(finder.radius,kMinBlockScanRadius,kMaxBlockScanRadius); finder.interval=std::clamp(finder.interval,200,3000);
    finder.minKelpHeight=std::clamp(finder.minKelpHeight,6,64); finder.minKelpColumns=std::clamp(finder.minKelpColumns,1,64);
    finder.minAmethyst=std::clamp(finder.minAmethyst,1,64); finder.maxDraw=Finite(finder.maxDraw,8,kMaxBlockDrawDistance,192); ClampColour(finder.colour);
}
void StoragePulse::Reset() {
    markers_.clear(); positions_.clear(); dimension_={}; server_={}; scan_=mask_=0;
    previous_=epoch_=nextReveal_=0;
}
void StoragePulse::Update(const WorldSnapshot& s,const StorageSettings& c,bool enabled,float range,double now,bool reducedMotion,bool dedicated) {
    if(!enabled||!s.dimension[0]||!c.mask||!std::isfinite(now)||!std::isfinite(range)) { Reset(); return; }
    const auto scan=dedicated?s.storageScanId:s.blockScanId;
    if(std::strncmp(dimension_.data(),s.dimension,kNameLen)||std::strncmp(server_.data(),s.serverAddress,kAddrLen)||
       mask_!=c.mask||(scan&&scan<scan_)||now<previous_) {
        Reset(); std::memcpy(dimension_.data(),s.dimension,kNameLen); std::memcpy(server_.data(),s.serverAddress,kAddrLen);
        mask_=c.mask; epoch_=previous_=nextReveal_=now;
    }
    scan_=scan;
    const float dt=float(std::clamp(now-previous_,0.,.1)); previous_=now;
    for(auto& marker:markers_) marker.present=false;
    const auto count=std::min(dedicated?s.storageCount:s.blockCount,kMaxBlocks);
    const double duration=std::clamp(c.pulseDuration,500,6000)/1000.;
    const double spacing=std::min(.06,duration/std::max(1u,count));
    nextReveal_=std::clamp(nextReveal_,now,now+duration);
    for(const auto& b:std::span(dedicated?s.storageBlocks:s.blocks,count)) {
        if(!StorageKind(b.kind)||!(c.mask&(1u<<b.kind))||!std::isfinite(b.x)||!std::isfinite(b.y)||!std::isfinite(b.z)||
           std::abs(b.x)>3e7||std::abs(b.z)>3e7||std::abs(b.y)>4096||Distance({b.x+.5,b.y+.5,b.z+.5},s)>range) continue;
        const auto key=BlockPosition::Of(b); auto it=positions_.find(key);
        if(it==positions_.end()) {
            if(markers_.size()>=kMaxBlocks) continue;
            const auto index=markers_.size();
            markers_.push_back({b,reducedMotion?1.f:0.f,nextReveal_,true});
            positions_.emplace(key,index); nextReveal_=std::min(now+duration,nextReveal_+spacing);
        } else { auto& marker=markers_[it->second]; marker.block=b; marker.present=true; }
    }
    for(auto& marker:markers_) {
        const float target=marker.present&&(reducedMotion||now>=marker.revealAt)?1.f:0.f;
        marker.alpha=reducedMotion?target:std::lerp(marker.alpha,target,1-std::exp(-dt/.09f));
    }
    const auto removed=std::erase_if(markers_,[](const StorageMarker& marker){return !marker.present&&marker.alpha<.005f;});
    if(removed) {
        positions_.clear();
        for(std::size_t i=0;i<markers_.size();++i) positions_.emplace(BlockPosition::Of(markers_[i].block),i);
    }
}
StorageHighlight StoragePulse::Highlight(std::size_t index,const StorageSettings& c,double now,bool reducedMotion) const {
    if(!c.pulse||reducedMotion||index>=markers_.size()||!std::isfinite(now)) return {};
    const double period=std::clamp(c.pulseDuration,500,6000)/1000.;
    const double slot=period/markers_.size(),window=std::min(period,std::max(.3,slot*.85));
    const double age=std::fmod(std::max(0.,now-epoch_)+period-index*slot,period);
    if(age>=window) return {};
    const float height=float(age/window),wave=std::sin(height*std::numbers::pi_v<float>);
    return {wave*wave,height};
}
void BaseFinder::Update(const WorldSnapshot& s,const BaseFinderSettings& c) {
    if(!Matches(s)||mask_!=c.Mask()||height_!=c.minKelpHeight||columns_!=c.minKelpColumns||amethyst_!=c.minAmethyst||radius_!=c.radius||deepslate_!=c.minDeepslate) {
        Reset(); std::memcpy(dimension_.data(),s.dimension,kNameLen); std::memcpy(server_.data(),s.serverAddress,kAddrLen);
        mask_=c.Mask(); height_=c.minKelpHeight; columns_=c.minKelpColumns; amethyst_=c.minAmethyst; radius_=c.radius; deepslate_=c.minDeepslate;
    }
    if(!c.enabled||!s.dimension[0]||!mask_||!s.blockScanComplete||!s.blockScanId||s.blockScanId==scan_||
       (s.blockScanMask&mask_)!=mask_||s.blockScanRadius<static_cast<unsigned>(c.radius)) return;
    if(s.blockScanId<scan_) count_=0;
    scan_=s.blockScanId;
    std::vector<PossibleBase> chunks;
    for(const auto& b:std::span(s.blocks,std::min(s.blockCount,kMaxBlocks))) {
        const bool kelp=c.kelp&&b.kind==kBlockKelp&&b.kelpHeight>=c.minKelpHeight;
        const bool crystal=c.amethyst&&(b.kind==kBlockLargeAmethystBud||b.kind==kBlockAmethystCluster);
        const bool deepslate=c.deepslate&&b.kind==kBlockRotatedDeepslate;
        if((!kelp&&!crystal&&!deepslate)||!std::isfinite(b.x)||!std::isfinite(b.y)||!std::isfinite(b.z)||
           std::abs(b.x)>3e7||std::abs(b.z)>3e7||std::abs(b.y)>4096) continue;
        if(std::abs(b.x-s.blockScanX)>c.radius||std::abs(b.y-s.blockScanY)>c.radius||std::abs(b.z-s.blockScanZ)>c.radius) continue;
        const int x=static_cast<int>(std::floor(double(b.x)/16)),z=static_cast<int>(std::floor(double(b.z)/16));
        auto it=std::find_if(chunks.begin(),chunks.end(),[&](const PossibleBase& f){return f.chunkX==x&&f.chunkZ==z;});
        const double bottom=kelp?b.y-b.kelpHeight+1.:b.y;
        if(it==chunks.end()) {
            PossibleBase f; f.chunkX=x; f.chunkZ=z; f.bounds={{b.x,bottom,b.z},{b.x+1.,b.y+1.,b.z+1.}};
            chunks.push_back(f); it=chunks.end()-1;
        }
        if(kelp) { ++it->kelpColumns; it->tallestKelp=std::max(it->tallestKelp,int(b.kelpHeight)); }
        else if(crystal) ++it->amethyst;
        else ++it->deepslate;
        it->bounds.min.x=std::min(it->bounds.min.x,double(b.x)); it->bounds.min.y=std::min(it->bounds.min.y,bottom);
        it->bounds.min.z=std::min(it->bounds.min.z,double(b.z)); it->bounds.max.x=std::max(it->bounds.max.x,b.x+1.);
        it->bounds.max.y=std::max(it->bounds.max.y,b.y+1.); it->bounds.max.z=std::max(it->bounds.max.z,b.z+1.);
    }
    for(auto& f:chunks) {
        if(!(c.kelp&&f.kelpColumns>=c.minKelpColumns)&&!(c.amethyst&&f.amethyst>=c.minAmethyst)&&
           !(c.deepslate&&f.deepslate>=c.minDeepslate)) continue;
        f.centre={(f.bounds.min.x+f.bounds.max.x)*.5,(f.bounds.min.y+f.bounds.max.y)*.5,(f.bounds.min.z+f.bounds.max.z)*.5};
        auto old=std::find_if(finds_.begin(),finds_.begin()+count_,[&](const PossibleBase& previous){return previous.chunkX==f.chunkX&&previous.chunkZ==f.chunkZ;});
        if(old!=finds_.begin()+count_) { if(f.kelpColumns+f.amethyst+f.deepslate>=old->kelpColumns+old->amethyst+old->deepslate) *old=f; }
        else if(count_<kMaxFinds) finds_[count_++]=f;
    }
}
WorldBounds PossibleChunkBounds(const PossibleBase& f,const WorldSnapshot& s) {
    // The snapshot carries no custom dimension height. Use the vanilla column,
    // expanding it to include any observed evidence outside that height range.
    const auto dimension=std::string_view(s.dimension);
    const bool shortWorld=dimension.find("minecraft:the_nether")!=std::string_view::npos||dimension.find("minecraft:the_end")!=std::string_view::npos||
        dimension=="nether"||dimension=="the_nether"||dimension=="Nether"||dimension=="end"||dimension=="the_end"||dimension=="End";
    return {{f.chunkX*16.,std::min(shortWorld?0.:-64.,f.bounds.min.y),f.chunkZ*16.},
            {f.chunkX*16.+16,std::max(shortWorld?256.:320.,f.bounds.max.y),f.chunkZ*16.+16}};
}
void DrawPossibleBases(ImDrawList& draw,const WorldSnapshot& s,const CameraProjection& p,const BaseFinderSettings& c,
                       std::span<const PossibleBase> finds,ImVec2 viewport,bool chunks,bool blocks) {
    if(!c.enabled||c.colour.w<=0) return;
    auto* savedTags=depth::tags;
    struct Restore { std::vector<float>* previous; ~Restore() { depth::tags=previous; } } restore{savedTags};
    depth::tags=nullptr; const int first=draw.VtxBuffer.Size;
    const auto ink=ImGui::ColorConvertFloat4ToU32(c.colour);
    const auto origin=ImGui::GetMainViewport()->Pos;
    const auto segment=[&](WorldPoint from,WorldPoint to,ImU32 colour,float width) {
        ScreenPoint a,b;
        if(p.Segment(from,to,a,b)) {
            const ImVec2 start{a.x+origin.x,a.y+origin.y},end{b.x+origin.x,b.y+origin.y};
            draw.AddLine(start,end,IM_COL32(0,0,0,110),width+1.5f);
            draw.AddLine(start,end,colour,width);
        }
    };
    const auto wire=[&](const WorldBounds& bounds,ImU32 colour,float width) {
        const auto corner=[&](int i)->WorldPoint {
            return {i&1?bounds.max.x:bounds.min.x,i&2?bounds.max.y:bounds.min.y,i&4?bounds.max.z:bounds.min.z};
        };
        for(int i=0;i<8;++i) for(int bit:{1,2,4}) if(!(i&bit)) segment(corner(i),corner(i|bit),colour,width);
    };
    for(const auto& f:finds) {
        const double distance=Distance(f.centre,s); if(!std::isfinite(distance)||distance>c.maxDraw) continue;
        if(chunks) {
            const auto bounds=PossibleChunkBounds(f,s);
            wire(bounds,ink,1.6f);
            // Section rings make the enclosing column visible from inside it.
            ImVec4 subtle=c.colour; subtle.w*=.4f;
            const auto grid=ImGui::ColorConvertFloat4ToU32(subtle);
            for(double y=std::ceil(bounds.min.y/16)*16;y<bounds.max.y;y+=16) {
                const WorldPoint a{bounds.min.x,y,bounds.min.z},b{bounds.max.x,y,bounds.min.z},
                    d{bounds.min.x,y,bounds.max.z},e{bounds.max.x,y,bounds.max.z};
                segment(a,b,grid,1); segment(b,e,grid,1); segment(e,d,grid,1); segment(d,a,grid,1);
            }
        }
        if(blocks) for(const auto& b:std::span(s.blocks,std::min(s.blockCount,kMaxBlocks))) {
            const bool kelp=c.kelp&&b.kind==kBlockKelp&&b.kelpHeight>=c.minKelpHeight;
            const bool crystal=c.amethyst&&(b.kind==kBlockLargeAmethystBud||b.kind==kBlockAmethystCluster);
            const bool slate=c.deepslate&&b.kind==kBlockRotatedDeepslate;
            if((!kelp&&!crystal&&!slate)||!std::isfinite(b.x)||!std::isfinite(b.y)||!std::isfinite(b.z)||
               std::abs(b.x)>3e7||std::abs(b.z)>3e7||std::abs(b.y)>4096||
               ChunkCoordinate(b.x)!=f.chunkX||ChunkCoordinate(b.z)!=f.chunkZ) continue;
            const double blockDistance=Distance({b.x+.5,b.y+.5,b.z+.5},s);
            if(!std::isfinite(blockDistance)||blockDistance>c.maxDraw) continue;
            // Keep every kelp stem block visible, although the scanner publishes
            // one top/height record per plant instead of repeating its stems.
            const int height=kelp?std::clamp(int(b.kelpHeight),1,64):1;
            for(int offset=0;offset<height;++offset)
                wire({{b.x,b.y-offset,b.z},{b.x+1.,b.y-offset+1.,b.z+1.}},ink,1.4f);
        }
        if(!c.tracers&&!c.labels) continue;
        ScreenRect rect; if(!p.Bounds(f.bounds,rect)) continue;
        if(c.tracers) draw.AddLine({viewport.x*.5f,viewport.y},{(rect.left+rect.right)*.5f,rect.bottom},ink,1);
        if(c.labels) {
            char text[192]; std::snprintf(text,sizeof(text),"Possible Base | Chunk %d, %d | %.0f m\n%d kelp | %d amethyst | %d deepslate",
                f.chunkX,f.chunkZ,distance,f.kelpColumns,f.amethyst,f.deepslate);
            Label(draw,rect,text,ink,viewport);
        }
    }
    if(savedTags) { savedTags->resize(draw.VtxBuffer.Size,0.f); std::fill(savedTags->begin()+first,savedTags->end(),0.f); }
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
    if(!s.blockScanComplete||!s.blockScanId||s.blockScanId==scan_||!c.mask||(s.blockScanMask&c.mask)!=c.mask||s.blockScanRadius<static_cast<unsigned>(c.radius)) return 0;
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
                bool storageEnabled,float range,float opacity,float glow,ImVec2 viewport,ImTextureID glowTexture,
                const StoragePulse* pulse,double now,bool reducedMotion) {
    if(!storageEnabled&&!c.spawner.enabled) return;
    auto* savedTags=depth::tags;
    struct Restore { std::vector<float>* previous; ~Restore() { depth::tags=previous; } } restore{savedTags};
    depth::tags=nullptr; const int first=draw.VtxBuffer.Size;
    const auto render=[&](const BlockSnapshot& block,float alpha,StorageHighlight highlight) {
        const bool storage=storageEnabled&&StorageKind(block.kind)&&(c.storage.mask&(1u<<block.kind));
        const bool spawner=c.spawner.enabled&&block.kind==kBlockSpawner;
        if(!storage&&!spawner) return;
        const WorldPoint centre{block.x+.5,block.y+.5,block.z+.5}; const double distance=Distance(centre,s);
        if(!std::isfinite(distance)||distance>(storage?range:c.spawner.maxDraw)) return;
        const WorldBounds bounds{{block.x,block.y,block.z},{block.x+1.,block.y+1.,block.z+1.}};
        ImVec4 colour=storage?c.storage.Colour(block.kind):c.spawner.colour;
        if(storage) colour.w*=opacity*alpha;
        if(colour.w<=.001f) return;
        const ImU32 ink=ImGui::ColorConvertFloat4ToU32(colour);
        ScreenRect rect; const bool haveRect=p.Bounds(bounds,rect);
        // Keep the real container texture visible beneath a lightly tinted
        // world-space cell, with all twelve edges visible through walls.
        if(storage) {
            soft::StorageCell(draw,s,p,bounds,colour,glow,glowTexture);
        }
        if(spawner&&haveRect) soft::Outline(draw,{rect.left,rect.top},{rect.right,rect.bottom},4,1.5f,colour,colour,c.spawner.glow,glowTexture);
        if(!haveRect) return;
        if(storage?c.storage.tracers:c.spawner.tracers)
            draw.AddLine({viewport.x*.5f,viewport.y},{(rect.left+rect.right)*.5f,rect.bottom},ink,1);
        if(storage?c.storage.labels:c.spawner.labels) {
            char text[80]; std::snprintf(text,sizeof(text),"%s | %.0f m",BlockKindLabel(block.kind),distance); Label(draw,rect,text,ink,viewport);
        }
    };
    if(storageEnabled&&pulse) {
        const auto markers=pulse->Markers();
        for(std::size_t i=0;i<markers.size();++i) render(markers[i].block,markers[i].alpha,pulse->Highlight(i,c.storage,now,reducedMotion));
    }
    for(const auto& b:std::span(s.blocks,std::min(s.blockCount,kMaxBlocks)))
        if(!pulse||!StorageKind(b.kind)) render(b,1,{});
    if(savedTags) { savedTags->resize(draw.VtxBuffer.Size,0.f); std::fill(savedTags->begin()+first,savedTags->end(),0.f); }
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
void DrawBases(ImDrawList& draw,const WorldSnapshot& s,const CameraProjection& p,const BaseSettings& c,std::span<const Stash> finds,ImVec2 viewport) {
    if(!c.enabled||c.colour.w<=0) return;
    // Base ESP deliberately bypasses world-depth tagging, including the filled
    // WorldQuad path. Other world effects keep their normal occlusion policy.
    auto* savedTags=depth::tags;
    struct Restore { std::vector<float>* previous; ~Restore() { depth::tags=previous; } } restore{savedTags};
    depth::tags=nullptr;
    const int firstVertex=draw.VtxBuffer.Size;
    const ImU32 ink=ImGui::ColorConvertFloat4ToU32(c.colour);
    constexpr int faces[][4]={{0,2,6,4},{1,5,7,3},{0,4,5,1},{2,3,7,6},{0,1,3,2},{4,6,7,5}};
    for(const auto& f:finds) {
        const double distance=Distance(f.centre,s); if(!std::isfinite(distance)||distance>c.maxDraw) continue;
        const double margin=std::clamp(double(c.padding),0.,16.);
        const WorldBounds bounds{{f.bounds.min.x-margin,f.bounds.min.y-margin,f.bounds.min.z-margin},
            {f.bounds.max.x+margin,f.bounds.max.y+margin,f.bounds.max.z+margin}};
        ScreenRect rect; if(!p.Bounds(bounds,rect)) continue;
        std::array<WorldPoint,8> corners{};
        for(int i=0;i<8;++i) corners[i]={i&1?bounds.max.x:bounds.min.x,i&2?bounds.max.y:bounds.min.y,i&4?bounds.max.z:bounds.min.z};
        if(!c.textured&&c.fillOpacity>0) {
            ImVec4 tint=c.colour; tint.w*=c.fillOpacity;
            const bool facing[]={s.camX<=bounds.min.x,s.camX>=bounds.max.x,s.camY<=bounds.min.y,s.camY>=bounds.max.y,
                s.camZ<=bounds.min.z,s.camZ>=bounds.max.z};
            const auto texture=ImGui::GetIO().Fonts->TexID; const auto white=ImGui::GetIO().Fonts->TexUvWhitePixel;
            for(int i=0;i<6;++i) if(facing[i]) {
                const auto& face=faces[i];
                soft::WorldQuad(draw,p,{corners[face[0]],corners[face[1]],corners[face[2]],corners[face[3]]},texture,tint,tint,white,white);
            }
        }
        if(!c.textured) for(int i=0;i<8;++i) for(int bit:{1,2,4}) if(!(i&bit)) {
            ScreenPoint a,b;
            if(p.Segment(corners[i],corners[i|bit],a,b)) draw.AddLine({a.x,a.y},{b.x,b.y},ink,c.lineWidth);
        }
        if(c.tracers) draw.AddLine({viewport.x*.5f,viewport.y},{(rect.left+rect.right)*.5f,rect.bottom},ink,1);
        if(c.labels) { char text[112]; std::snprintf(text,sizeof(text),"Base | %d containers | %.0f m",f.count,distance); Label(draw,rect,text,ink,viewport); }
    }
    if(savedTags) {
        savedTags->resize(draw.VtxBuffer.Size,0.f);
        std::fill(savedTags->begin()+firstVertex,savedTags->end(),0.f);
    }
}
void BuildStorageRequest(base::StorageRequest& r,const WorldSnapshot& s,const StorageSettings& c,bool enabled,
                                                float range,float opacity,bool fresh,std::uint64_t now) {
    r={}; r.sentAtMs=now; r.opacity=opacity;
    if(!c.textures) return;
    if(!enabled||!fresh||!s.dimension[0]||!std::isfinite(opacity)||opacity<=0||opacity>1||!std::isfinite(range)||range<=0) return;
    std::snprintf(r.dimension,sizeof(r.dimension),"%s",s.dimension);
    std::snprintf(r.server,sizeof(r.server),"%s",s.serverAddress);
    for(const auto& b:std::span(s.storageBlocks,std::min(s.storageCount,kMaxBlocks))) {
        if(!StorageKind(b.kind)||!(c.mask&(1u<<b.kind))||!std::isfinite(b.x)||!std::isfinite(b.y)||!std::isfinite(b.z)) continue;
        const double distance=Distance({b.x+.5,b.y+.5,b.z+.5},s);
        if(!std::isfinite(distance)||distance>range) continue;
        const double x=std::floor(b.x),y=std::floor(b.y),z=std::floor(b.z);
        if(x < -3e7||x+1 > 3e7||z < -3e7||z+1 > 3e7||y < -4096||y+1 > 4096) continue;
        r.regions[r.count++]={x,y,z,x+1,y+1,z+1};
    }
    // Scanner iteration order and camera movement must not restart extraction.
    const auto less=[](const base::Region& a,const base::Region& b) {
        if(a.minX!=b.minX) return a.minX<b.minX;
        if(a.minY!=b.minY) return a.minY<b.minY;
        return a.minZ<b.minZ;
    };
    std::sort(r.regions.begin(),r.regions.begin()+r.count,less);
    const auto end=std::unique(r.regions.begin(),r.regions.begin()+r.count,[&](const auto& a,const auto& b){return !less(a,b)&&!less(b,a);});
    r.count=std::uint32_t(end-r.regions.begin()); r.enabled=r.count>0;
    std::uint64_t hash=14695981039346656037ULL;
    auto feed=[&](const void* data,std::size_t size){auto* bytes=static_cast<const unsigned char*>(data); for(std::size_t i=0;i<size;++i) { hash^=bytes[i]; hash*=1099511628211ULL; }};
    feed(r.dimension,sizeof(r.dimension)); feed(r.server,sizeof(r.server)); feed(&r.count,sizeof(r.count));
    feed(r.regions.data(),r.count*sizeof(base::Region)); r.revision=hash?hash:1; return;
}
base::Request BuildBaseRequest(const WorldSnapshot& s,const BaseSettings& c,std::span<const Stash> finds,bool fresh,std::uint64_t now) {
    base::Request r; r.sentAtMs=now; r.opacity=c.textureOpacity;
    if(!c.enabled||!c.textured||!fresh||!s.dimension[0]||c.textureOpacity<=0) return r;
    std::snprintf(r.dimension,sizeof(r.dimension),"%s",s.dimension); std::snprintf(r.server,sizeof(r.server),"%s",s.serverAddress);
    std::vector<const Stash*> nearest;
    for(const auto& f:finds) { const double d=Distance(f.centre,s); if(std::isfinite(d)&&d<=c.maxDraw) nearest.push_back(&f); }
    std::stable_sort(nearest.begin(),nearest.end(),[&](auto a,auto b){return Distance(a->centre,s)<Distance(b->centre,s);});
    nearest.resize(std::min(nearest.size(),std::size_t(base::kMaxRegions)));
    // Stable geometry order prevents camera movement from restarting extraction.
    std::sort(nearest.begin(),nearest.end(),[](auto a,auto b){return a->chunkX==b->chunkX?a->chunkZ<b->chunkZ:a->chunkX<b->chunkX;});
    const double p=std::clamp(double(c.padding),0.,16.);
    for(auto f:nearest) {
        auto& b=r.regions[r.count++]; const auto& a=f->bounds;
        b={std::floor(a.min.x-p),std::max(-4096.,std::floor(a.min.y-p)),std::floor(a.min.z-p),
           std::ceil(a.max.x+p),std::min(4096.,std::ceil(a.max.y+p)),std::ceil(a.max.z+p)};
    }
    r.enabled=r.count>0;
    std::uint64_t hash=14695981039346656037ULL;
    auto feed=[&](const void* data,std::size_t size){auto* bytes=static_cast<const unsigned char*>(data); for(std::size_t i=0;i<size;++i) { hash^=bytes[i]; hash*=1099511628211ULL; }};
    feed(r.dimension,sizeof(r.dimension)); feed(r.server,sizeof(r.server)); feed(&r.count,sizeof(r.count));
    feed(r.regions.data(),r.count*sizeof(base::Region)); r.revision=hash?hash:1; return r;
}
} // namespace mcov::smp
