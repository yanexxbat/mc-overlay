#include "overlay.hpp"
#include "storage_cell.hpp"
#include "aura_projection.hpp"
#include "../common/chunk_hints.hpp"
#include "../common/hud_style.hpp"
#include <cstdio>
#include <cmath>

namespace mcov {
namespace panel=control_panel;
namespace {
std::string ChunkClues(const chunks::Find& f) {
    std::string text;
    const auto add=[&](const char* name){if(!text.empty())text+=" / ";text+=name;};
    if(f.category&1) {
        if(f.evidence&(1u<<21))add("Deepslate Y > 8");
        if(f.evidence&(1u<<22))add("Rotated deepslate");
        if(f.evidence&(1u<<23))add("Cobbled deepslate Y > 0");
        if(f.evidence&(1u<<24))add("End stone outside End");
        if(text.empty())add("Placed");
    }
    constexpr const char* names[]={"","","","Cobble","Masonry","Obsidian","Pistons","Stairs","Shaft","Room","Ore","Storage","Items","Mobs","","Redstone","Bees","Floor","Plants","Workshop","Geode"};
    for(unsigned bit=3;bit<chunks::SignalCount;++bit) {
        const unsigned group=bit<=13?2:bit==20?8:4;
        if((f.category&group)&&(f.evidence&(1u<<bit))&&names[bit][0])add(names[bit]);
    }
    return text;
}
}


items::Request Overlay::BuildItemRequest(std::uint64_t now,bool inputAllowed,bool combatActive) const {
    items::Request r;
    r.sentAtMs=now; r.inputAllowed=inputAllowed; r.combatActive=combatActive;
    r.totem=combat_.autoTotem; r.crystal=crystalMacro_; r.anchor=anchorMacro_; r.trident=autoTrident_;
    // Minecraft liest seine eigene, eventuell umbelegte Place/Use-Taste im Client-Tick.
    return r;
}

void Overlay::DrawTridentSettings() {
    bool changed=false;
    if(panel::BeginRows("trident_controls")) {
        changed|=panel::CheckRow("Enable Auto Trident",&autoTrident_.enabled);
        DrawBind(keys::Action::AutoTrident,true);
        changed|=panel::SliderIntRow("Charge time",&autoTrident_.chargeTicks,10,40,"%d ticks");
        changed|=panel::CheckRow("Randomize charge",&autoTrident_.randomizeCharge);
        if(autoTrident_.randomizeCharge) {
            changed|=panel::SliderIntRow("Extra charge up to",&autoTrident_.extraChargeTicks,0,10,"%d ticks");
            char range[32]; std::snprintf(range,sizeof(range),"%d - %d ticks",std::clamp(autoTrident_.chargeTicks,10,40),
                std::min(40,std::clamp(autoTrident_.chargeTicks,10,40)+std::clamp(autoTrident_.extraChargeTicks,0,10)));
            panel::ValueRow("Charge range",range);
        }
        char count[32]; std::snprintf(count,sizeof(count),"%llu",static_cast<unsigned long long>(itemReport_.tridentActions));
        panel::ValueRow("Releases sent",count);
        ImGui::EndTable();
    }
    if(changed) { preferencesDirty_=true; preferenceSaveAt_=GetTickCount64()+500; }
    panel::Section("Live status");
    ImGui::TextWrapped("%s",items::Text(agentConnected_?itemReport_.trident:items::Status::Unsupported));
    panel::Section("How to use");
    ImGui::TextWrapped("Hold a Riptide trident in your main hand, then hold Minecraft's Place / Use key (right mouse by default). It repeatedly charges and releases while you stay in water or rain.");
    ImGui::TextWrapped("Release Use to stop. Menus, focus loss and Kill Aura pause automatic releases. Charge time uses game ticks: 10 ticks is the normal minimum (0.5 seconds at 20 TPS). Minecraft controls reuse and durability.");
    ImGui::TextWrapped("Randomize charge chooses one duration per charge within the displayed range (default 10-13 ticks). Repeated values are possible; the duration stays fixed during that charge.");
    ImGui::TextWrapped("Automatic releases wait until the previous Riptide boost has ended, even when the next charge is already ready.");
}

void Overlay::DrawItemAssistSettings(int section) {
    const bool totem=section==0,anchor=section==2;
    const auto action=totem?keys::Action::AutoTotem:anchor?keys::Action::AnchorMacro:keys::Action::CrystalMacro;
    auto& macro=anchor?anchorMacro_:crystalMacro_;
    auto status=totem?itemReport_.totem:anchor?itemReport_.anchor:itemReport_.crystal;
    if(!agentConnected_) status=items::Status::Unsupported;
    bool changed=false;
    if(panel::BeginRows("item_assist_controls")) {
        panel::CheckRow(totem?"Enable Auto Totem":"Enable macro",totem?&combat_.autoTotem.enabled:&macro.enabled);
        DrawBind(action,true);
        if(totem) {
            changed|=panel::SliderRow("Totem ab X HP",&combat_.autoTotem.healthThreshold,1,20,"%.1f HP");
            int retry=static_cast<int>(combat_.autoTotem.retryMs);
            if(panel::SliderIntRow("Retry interval",&retry,50,500,"%d ms")) { combat_.autoTotem.retryMs=retry; changed=true; }
        } else {
            panel::ValueRow("Activation","Hold Minecraft Place / Use");
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
            if(ImGui::Button("Server conservative")) { macro.Conservative(); changed=true; }
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("Short range, slower actions and strict world confirmation. Server acceptance is not guaranteed.");
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
            if(ImGui::Button("GrimAC conservative")) { macro.GrimConservative(); changed=true; }
            if(ImGui::IsItemHovered()) ImGui::SetTooltip("Also requires a real crosshair hit and uses Minecraft's clicked block face. Not a verified bypass.");
            changed|=panel::CheckRow("Separate action delays",&macro.separateDelays);
            const auto delay=[&](const char* label,std::uint32_t& value,int low,int high) {
                int ms=static_cast<int>(value);
                if(panel::SliderIntRow(label,&ms,low,high,"%d ms")) { value=ms; changed=true; }
            };
            if(macro.separateDelays) {
                delay("Place delay",macro.placeMs,50,1000);
                if(anchor) delay("Charge delay",macro.chargeMs,50,1000);
                delay(anchor?"Detonate delay":"Break delay",macro.breakMs,50,1000);
            } else delay("Action interval",macro.intervalMs,100,500);
            int confirmation=static_cast<int>(macro.confirmationMs);
            if(panel::SliderIntRow("World confirmation timeout",&confirmation,100,2000,"%d ms")) { macro.confirmationMs=confirmation; changed=true; }
            changed|=panel::CheckRow("Strict world confirmation",&macro.strictConfirmation);
            changed|=panel::CheckRow("Require real crosshair hit",&macro.requireCrosshairHit);
            changed|=panel::SliderRow("Maximum range",&macro.maxRange,1,6,"%.1f m");
            changed|=panel::SliderRow("Minimum health (0 = off)",&macro.minHealth,0,20,"%.0f HP");
            changed|=panel::CheckRow("Require offhand totem",&macro.requireTotem);
            changed|=panel::CheckRow("Restore previous slot",&macro.restoreSlot);
            if(anchor) {
                changed|=panel::CheckRow("Charge to 4 with offhand glowstone",&macro.chargeWithOffhandFuel);
                changed|=panel::SliderIntRow("Detonation slot (0 = auto)",&macro.detonationSlot,0,9,"%d");
            }
        }
        const auto count=totem?itemReport_.swaps:anchor?itemReport_.anchorActions:itemReport_.crystalActions;
        char text[32]; std::snprintf(text,sizeof(text),"%llu",static_cast<unsigned long long>(count));
        panel::ValueRow(totem?"Swaps sent":"Actions sent",text);
        ImGui::EndTable();
    }
    if(changed) { macro.Clamp(); preferencesDirty_=true; preferenceSaveAt_=GetTickCount64()+500; }
    panel::Section("Live status");
    ImGui::TextWrapped("%s",items::Text(status));
    panel::Section("How to use");
    if(totem) ImGui::TextWrapped("Put a totem in the hotbar or main inventory. It moves to the offhand when needed. Chest screens and items held by the cursor pause swapping.");
    else if(anchor) ImGui::TextWrapped("Enable the macro, select an anchor and hold Minecraft's Place / Use key on a placement block or anchor. It repeatedly places, charges and detonates at the saved position. Keep glowstone and a non-glowstone item in the hotbar. Respawn dimensions pause detonation.");
    else ImGui::TextWrapped("Enable the macro, select end crystals and hold Minecraft's Place / Use key on obsidian or bedrock. It repeatedly places and breaks the crystal at that position.");
    if(!totem) ImGui::TextWrapped("Release Place / Use to stop. A manual slot change, looking away, menus or focus loss also stops the cycle. The optional module keybind arms the macro; it does not replace Minecraft's Use key. Kill Aura pauses macros.");
    if(!totem) ImGui::TextWrapped("Minimum health and the totem check pause the cycle until satisfied. Range never exceeds Minecraft's own reach. Strict confirmation suppresses retries after a timeout; release Use to reset. Timing and protection settings are saved automatically.");
    if(!totem&&macro.requireCrosshairHit) ImGui::TextWrapped("Real crosshair hit is on: keep the next interaction target under the crosshair. Blocks use Minecraft's actual hit face. The cycle pauses until you aim correctly.");
}

void Overlay::NormalizeChunkFinder() {
    auto& old=smpConfig_.finder;
    if(old.enabled&&!chunkFinder_.enabled) {
        chunkFinder_.radius=old.radius; chunkFinder_.interval=old.interval;
        chunkFinder_.maxDraw=std::clamp(old.maxDraw,8.f,float(kMaxBlockDrawDistance));
        chunkFinder_.color=old.colour; chunkFinder_.labels=old.labels;
    }
    chunkFinder_.enabled=old.enabled=chunkFinder_.enabled||old.enabled;
}
smp::BaseFinderSettings Overlay::ChunkDetection() const {
    auto c=smpConfig_.finder;
    c.enabled=chunkFinder_.enabled||c.enabled;
    c.radius=chunkFinder_.radius; c.interval=chunkFinder_.interval;
    c.deepslate=chunkFinder_.deepslate; c.minDeepslate=chunkFinder_.minHints;
    c.maxDraw=chunkFinder_.maxDraw; c.labels=chunkFinder_.labels; c.colour=chunkFinder_.color;
    return c;
}
void Overlay::DrawChunkFinderSettings() {
    auto& c=chunkFinder_;auto& d=c.detection;bool dirty=false;
    const auto hint=[](const char* text){if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s",text);};
    const auto toggle=[&](const char* label,chunks::Signal signal,const char* text){bool on=(d.signals&(1u<<signal))!=0;if(panel::CheckRow(label,&on)){if(on)d.signals|=1u<<signal;else d.signals&=~(1u<<signal);dirty=true;}hint(text);};
    panel::Section("General");
    if(panel::BeginRows("chunkfinder_controls")) {
        bool enabled=ModuleEnabled(keys::Action::ChunkFinder);
        if(panel::CheckRow("Enable Chunkfinder",&enabled)){SetModuleEnabled(keys::Action::ChunkFinder,enabled);dirty=true;}
        DrawBind(keys::Action::ChunkFinder,true);
        toggle("Placed blocks",chunks::Placed,"Blocks outside their natural height, orientation or dimension.");
        ImGui::BeginDisabled(!(d.signals&(1u<<chunks::Placed)));
        dirty|=panel::SliderIntRow("Deepslate min",&d.deepslateMin,1,16,"%d");hint("Deepslate above Y 8.");
        dirty|=panel::SliderIntRow("Rotated min",&d.rotatedMin,0,16,"%d");hint("Raw deepslate with horizontal X/Z axis at any height. Set 0 to disable this clue on servers that generate or report rotated deepslate naturally.");
        dirty|=panel::SliderIntRow("Cobbled min",&d.cobbledMin,1,32,"%d");hint("Cobbled deepslate above Y 0.");
        dirty|=panel::SliderIntRow("End stone min",&d.endStoneMin,1,16,"%d");hint("End stone outside the End.");
        toggle("Ignore exposed",chunks::IgnoreExposed,"Exclude placed clues touching air, fluid or an unloaded boundary.");
        ImGui::EndDisabled();ImGui::EndTable();
    }
    panel::Section("Options");
    if(panel::BeginRows("chunkfinder_options")) {
        toggle("Base signals",chunks::Base,"Combine underground evidence into a score. Natural structures are filtered using nearby block clues.");
        ImGui::BeginDisabled(!(d.signals&(1u<<chunks::Base)));
        dirty|=panel::SliderIntRow("Base min score",&d.baseScore,2,40,"%d pts");
        toggle("Cobble lines",chunks::Cobble,"Rows of at least four cobblestone below Y 0; nearby spawners excluded.");
        toggle("Deepslate masonry",chunks::Masonry,"Polished, brick, tile and chiseled deepslate below Y 0; nearby sculk excluded.");
        toggle("Obsidian",chunks::Obsidian,"Buried obsidian below Y 20; exposed fluid formations and portal clues excluded.");
        toggle("Pistons",chunks::Pistons,"Pistons below Y 40.");
        toggle("Stairs",chunks::Stairs,"Stairs below Y 0; ancient-city and mineshaft clues excluded.");
        toggle("Mined shafts",chunks::Shafts,"Walled 1 x 1 vertical air shafts at least eight blocks deep below Y 0.");
        toggle("Flat rooms",chunks::Rooms,"Several flat-floor spaces with two to four air blocks of clearance below Y 0.");
        toggle("Stacked ore",chunks::Ore,"Four ore blocks in one column. A weak clue that also occurs naturally.");
        toggle("Storage",chunks::Storage,"Chests, barrels, shulker boxes and hoppers; higher weight below Y 0.");
        toggle("Dropped items",chunks::Items,"Dropped item entities below Y 0.");
        toggle("Mob clusters",chunks::Mobs,"At least 20 mobs per chunk, or polar bears below Y 0.");
        ImGui::EndDisabled();
        toggle("Farm signals",chunks::Farm,"Combine redstone, bee, floor, plant and workshop clues. Chance is a heuristic confidence value.");
        ImGui::BeginDisabled(!(d.signals&(1u<<chunks::Farm)));
        dirty|=panel::SliderIntRow("Farm min chance",&d.farmChance,25,99,"%d %%");
        toggle("Repeaters",chunks::Repeaters,"Powered repeaters contribute more confidence.");
        toggle("Bee farms",chunks::Bees,"Occupied crafted hives, or hives with nearby bees. Natural nests contribute less.");
        toggle("Placed floors",chunks::Floors,"Cobbled deepslate across multiple columns between Y 0 and 20.");
        toggle("Underground plants",chunks::Plants,"Roofed flowers below Y 40 and vines below Y 50; lush caves, jungles and swamps excluded.");
        toggle("Workshops",chunks::Workshops,"Furnaces, anvils, enchanting and crafting tables; village clues excluded.");
        ImGui::EndDisabled();
        toggle("Mined geodes",chunks::Geodes,"Connected amethyst shells with no budding amethyst. Incomplete or unloaded shells are skipped.");
        ImGui::BeginDisabled(!(d.signals&(1u<<chunks::Geodes)));
        dirty|=panel::SliderIntRow("Geode sensitivity",&d.geodeSensitivity,1,10,"%d");hint("Higher values accept smaller shells and increase false positives.");
        ImGui::EndDisabled();
        const char* displays[]={"Color only","Detailed"};dirty|=panel::ComboRow("Display",&c.display,displays,2);
        dirty|=panel::SliderRow("Tint opacity",&c.opacity,5,90,"%.0f %%");
        const char* heights[]={"Feet","Fixed","Column"};dirty|=panel::ComboRow("Height",&c.height,heights,3);
        ImGui::BeginDisabled(c.height!=1);dirty|=panel::SliderIntRow("Render Y",&c.renderY,-64,320,"%d");ImGui::EndDisabled();
        ImGui::BeginDisabled(c.height==2);dirty|=panel::SliderRow("Thickness",&c.thickness,.02f,2,"%.2f b");ImGui::EndDisabled();
        dirty|=panel::SliderIntRow("Max chunks",&c.maxChunks,8,400,"%d");hint("Draw nearest first. Other discoveries remain remembered.");
        dirty|=panel::CheckRow("Beacons",&c.beacons);
        dirty|=panel::CheckRow("Orbiting orbs",&c.orbs);hint("Target Aura glow around the nearest 24 marked chunks. Reduce motion freezes the orbit.");
        ImGui::BeginDisabled(c.display==0);
        dirty|=panel::CheckRow("Labels",&c.labels);hint("Up to 12 nearest non-overlapping labels. Full discoveries remain listed below.");dirty|=panel::CheckRow("Highlight blocks",&c.blocks);
        ImGui::BeginDisabled(!c.blocks);dirty|=panel::SliderIntRow("Max blocks",&c.maxBlocks,20,1000,"%d");ImGui::EndDisabled();ImGui::EndDisabled();
        dirty|=panel::ColorRow("Placed colour",&c.color.x);dirty|=panel::ColorRow("Base colour",&c.baseColor.x);
        dirty|=panel::ColorRow("Farm colour",&c.farmColor.x);dirty|=panel::ColorRow("Geode colour",&c.geodeColor.x);
        dirty|=panel::SliderIntRow("Threads",&c.threads,1,4,"%d");hint("Applied when the module is next enabled.");
        dirty|=panel::SliderIntRow("Chunks per tick",&c.chunksPerTick,1,12,"%d");hint("Maximum queued per client tick, subject to the snapshot time budget.");
        dirty|=panel::CheckRow("Chat alerts",&c.chat);dirty|=panel::CheckRow("Toast alerts",&c.toasts);dirty|=panel::CheckRow("Sound",&c.sound);
        dirty|=panel::SliderIntRow("Alerts per minute",&c.alertsPerMinute,1,30,"%d");
        ImGui::EndTable();
    }
    if(dirty){preferencesDirty_=true;preferenceSaveAt_=GetTickCount64()+500;PushScanRequest();}
    const auto& r=*chunkReport_;const auto now=GetTickCount64();
    bool fresh=agentConnected_&&r.sampledAtMs&&now>=r.sampledAtMs&&now-r.sampledAtMs<1500;
    if(!c.enabled)ImGui::TextWrapped("Enable Chunkfinder to scan loaded chunks.");
    else if(!(d.signals&((1u<<chunks::Placed)|(1u<<chunks::Base)|(1u<<chunks::Farm)|(1u<<chunks::Geodes))))ImGui::TextWrapped("Enable a detection group to start scanning.");
    else if(!fresh)ImGui::TextWrapped("Waiting for the Minecraft connection.");
    else if(r.status==chunks::Status::Unavailable)ImGui::TextWrapped("Chunk scanning is unavailable. Restart Minecraft with the updated client.");
    else if(r.status==chunks::Status::Failed)ImGui::TextWrapped("Chunk scanning stopped after an error. Toggle the module off and on to retry.");
    else if(r.status==chunks::Status::Waiting)ImGui::TextWrapped("Join a world to start scanning.");
    else ImGui::TextWrapped("%d chunks scanned | %d queued | %d discoveries",r.scanned,r.pending,r.count);
    ImGui::TextWrapped("Only loaded chunks are scanned. Structure filters and confidence scores are clues, not proof of a base. Discoveries outside loaded chunks may be outdated.");
    if(r.limited)ImGui::TextWrapped("Discovery or entity limit reached. Save useful locations and clear discoveries to free space.");
    if(ImGui::Button("Clear discoveries")){++chunkClearId_;chunkReport_->count=chunkReport_->blockCount=0;PushScanRequest();}
    if(fresh&&c.enabled)for(int i=0;i<std::clamp(r.count,0,chunks::kMaxFinds);++i) {
        const auto& f=r.finds[i];ImGui::PushID(i);ImGui::Separator();
        ImGui::TextWrapped("Chunk %d, %d | Base %d pts | Farm %d%%",f.x,f.z,f.score,f.chance);
        ImGui::TextWrapped("Reason: %s",ChunkClues(f).c_str());
        int shown=0;
        for(int j=0;j<std::clamp(r.blockCount,0,chunks::kMaxEvidence)&&shown<3;++j){const auto& q=r.blocks[j];
            if(ChunkCoordinate(q.x)!=f.x||ChunkCoordinate(q.z)!=f.z)continue;
            ImGui::TextDisabled("Evidence block: %d %d %d",q.x,q.y,q.z);++shown;
        }
        char coords[80];std::snprintf(coords,sizeof(coords),"%d %d %d",f.x*16+8,c.renderY,f.z*16+8);
        if(ImGui::SmallButton("Copy coordinates"))ImGui::SetClipboardText(coords);
        ImGui::SameLine();if(ImGui::SmallButton("Save waypoint")) {
            auto point=lastSnapshot_;point.posX=float(f.x*16+8);point.posY=c.height==1?float(c.renderY):lastSnapshot_.posY;point.posZ=float(f.z*16+8);
            char name[64];std::snprintf(name,sizeof(name),"Chunkfinder %d, %d",f.x,f.z);
            if(rockstarPack_.AddMarker(point,name)){rockstarConfig_[rockstar::Feature::Waypoints].enabled=true;preferencesDirty_=true;}
            else rockstarPack_.Notify("Waypoint list is full. Remove a waypoint first.",double(now)/1000);
        }ImGui::PopID();
    }
}

void Overlay::UpdateChunkFinder() {
    const auto& r=*chunkReport_;const auto now=GetTickCount64();
    if(!chunkFinder_.enabled||panicActive_||!agentConnected_||r.status!=chunks::Status::Scanning||!r.sampledAtMs||now<r.sampledAtMs||now-r.sampledAtMs>1500)return;
    if(chunkWorld_!=r.world){chunkWorld_=r.world;chunkAlert_=0;chunkNotices_.clear();}
    auto latest=chunkAlert_;
    for(int i=0;i<std::clamp(r.count,0,chunks::kMaxFinds);++i) {
        const auto& f=r.finds[i];latest=std::max(latest,f.alert);if(!f.alert||f.alert<=chunkAlert_)continue;
        char text[128];std::snprintf(text,sizeof(text),"Chunkfinder: %d, %d | base %d pts | farm %d%%",f.x*16+8,f.z*16+8,f.score,f.chance);
        if(chunkFinder_.toasts){if(chunkNotices_.size()>=3)chunkNotices_.erase(chunkNotices_.begin());chunkNotices_.emplace_back(now,text);}
        if(chunkFinder_.sound)MessageBeep(MB_OK);
    }chunkAlert_=latest;
}

void Overlay::DrawChunkFinder(const WorldSnapshot& s,const CameraProjection& p) {
    const auto& c=chunkFinder_;const auto& r=*chunkReport_;const auto now=GetTickCount64();
    if(!c.enabled||panicActive_||r.status!=chunks::Status::Scanning||!r.sampledAtMs||now<r.sampledAtMs||now-r.sampledAtMs>1500)return;
    auto& draw=*ImGui::GetBackgroundDrawList();const auto origin=ImGui::GetMainViewport()->Pos;
    auto* tags=depth::tags;depth::tags=nullptr;
    struct Restore{std::vector<float>* value;ImDrawList& draw;int first;~Restore(){if(value){value->resize(draw.VtxBuffer.Size,0.f);std::fill(value->begin()+first,value->end(),0.f);}depth::tags=value;}}restore{tags,draw,draw.VtxBuffer.Size};
    const auto colour=[&](int category){return category&2?c.baseColor:category&4?c.farmColor:category&8?c.geodeColor:c.color;};
    const auto wire=[&](WorldBounds b,ImU32 ink,float width){
        const auto corner=[&](int i)->WorldPoint{return {i&1?b.max.x:b.min.x,i&2?b.max.y:b.min.y,i&4?b.max.z:b.min.z};};
        for(int i=0;i<8;++i)for(int bit:{1,2,4})if(!(i&bit)){ScreenPoint a,d;if(p.Segment(corner(i),corner(i|bit),a,d))draw.AddLine({a.x+origin.x,a.y+origin.y},{d.x+origin.x,d.y+origin.y},ink,width);}
    };
    const auto fill=[&](std::vector<WorldPoint> poly,ImU32 ink){
        std::vector<WorldPoint> clipped;
        for(std::size_t i=0;i<poly.size();++i){auto a=poly[i],b=poly[(i+1)%poly.size()];double da=p.Depth(a)-p.NearPlane()*1.01,db=p.Depth(b)-p.NearPlane()*1.01;
            if(da>=0)clipped.push_back(a);if((da>=0)!=(db>=0)){double t=da/(da-db);clipped.push_back({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t});}}
        std::vector<ImVec2> points;for(auto q:clipped){ScreenPoint a;if(!p.Point(q,a))return;points.push_back({a.x,a.y});}
        const auto size=ImGui::GetIO().DisplaySize;
        for(int side=0;side<4&&!points.empty();++side){std::vector<ImVec2> out;
            const auto dist=[&](ImVec2 a){return side==0?a.x:side==1?size.x-a.x:side==2?a.y:size.y-a.y;};
            for(std::size_t i=0;i<points.size();++i){auto a=points[i],b=points[(i+1)%points.size()];float da=dist(a),db=dist(b);if(da>=0)out.push_back(a);if((da>=0)!=(db>=0)){float t=da/(da-db);out.push_back({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t});}}points=std::move(out);}
        if(points.size()<3)return;
        double area=0;for(std::size_t i=0;i<points.size();++i){auto a=points[i],b=points[(i+1)%points.size()];area+=double(a.x)*b.y-double(b.x)*a.y;}
        if(area<0)std::reverse(points.begin(),points.end());
        for(auto& q:points){q.x+=origin.x;q.y+=origin.y;}draw.AddConvexPolyFilled(points.data(),int(points.size()),ink);
    };
    std::vector<const chunks::Find*> sorted;for(int i=0;i<std::clamp(r.count,0,chunks::kMaxFinds);++i)sorted.push_back(&r.finds[i]);
    const auto distance=[&](const chunks::Find* f){return std::hypot(f->x*16.+8-s.posX,f->z*16.+8-s.posZ);};
    std::sort(sorted.begin(),sorted.end(),[&](auto a,auto b){return distance(a)<distance(b);});if(sorted.size()>std::size_t(c.maxChunks))sorted.resize(c.maxChunks);
    int drawnBlocks=0,orbChunks=0;
    std::vector<ImVec4> labelRects;
    AuraRenderer orbRenderer;
    const auto materialColour=[](const chunks::Evidence& q)->ImVec4 {
        switch(q.signal>>8) {
            case 4:case 5:case 6:case 9:return {.53f,.56f,.61f,1}; // deepslate
            case 7:return {.90f,.88f,.62f,1}; // end stone
            case 8:return {.66f,.68f,.70f,1};
            case 10:return {.36f,.23f,.53f,1};
            case 11:return {.64f,.72f,.43f,1};
            case 13:return {.30f,.80f,.88f,1};
            case 14:return {.88f,.62f,.27f,1};
            case 15:case 16:return {1,.22f,.20f,1};
            case 17:case 18:return {.98f,.73f,.24f,1};
            case 19:case 20:return {.35f,.82f,.43f,1};
            case 22:case 23:return {.69f,.43f,.94f,1}; // amethyst
            default:return (q.signal&255)==chunks::Geodes?ImVec4{.69f,.43f,.94f,1}:ImVec4{.62f,.66f,.72f,1};
        }
    };
    for(auto* f:sorted){const auto rgba=colour(f->category);const auto ink=ImGui::ColorConvertFloat4ToU32(rgba);auto tint=rgba;tint.w*=c.opacity/100;
        double y=c.height==1?c.renderY:s.posY;WorldBounds b{{f->x*16.,c.height==2?double(f->minY):y,f->z*16.},{f->x*16.+16,c.height==2?double(f->maxY):y+c.thickness,f->z*16.+16}};
        const auto corner=[&](int i)->WorldPoint{return {i&1?b.max.x:b.min.x,i&2?b.max.y:b.min.y,i&4?b.max.z:b.min.z};};
        if(c.height==2){for(auto face:std::array<std::array<int,4>,4>{{{{0,1,3,2}},{{4,6,7,5}},{{0,2,6,4}},{{1,5,7,3}}}})fill({corner(face[0]),corner(face[1]),corner(face[2]),corner(face[3])},ImGui::ColorConvertFloat4ToU32(tint));}
        else fill({corner(2),corner(3),corner(7),corner(6)},ImGui::ColorConvertFloat4ToU32(tint));
        if(c.display==1||c.height==2)wire(b,ink,1.4f);
        if(c.beacons)wire({{f->x*16.+7.7,double(f->minY),f->z*16.+7.7},{f->x*16.+8.3,double(f->maxY),f->z*16.+8.3}},ink,2);
        if(c.display==1&&c.blocks)for(int j=0;j<std::clamp(r.blockCount,0,chunks::kMaxEvidence)&&drawnBlocks<c.maxBlocks;++j){const auto& q=r.blocks[j];if(ChunkCoordinate(q.x)!=f->x||ChunkCoordinate(q.z)!=f->z)continue;// Geometry clues describe air, not a detected solid block.
            if((q.signal>>8)==1||(q.signal>>8)==2)continue;
            soft::StorageCell(draw,s,p,{{double(q.x),double(q.y),double(q.z)},{q.x+1.,q.y+1.,q.z+1.}},materialColour(q),.8f,auraBlend_.GlowTexture());++drawnBlocks;}
        if(c.orbs&&orbChunks++<24) {
            AuraConfig style;style.startColor=rgba;style.endColor=soft::Mix(rgba,{.35f,.55f,1,rgba.w},.35f);
            style.opacity=.8f;
            AuraWorldFog world;world.orbCount=4;
            const double time=menu_.reduceMotion?0:ImGui::GetTime();
            const double phase=time*.45+double((std::uint32_t(f->x)*31u+std::uint32_t(f->z)*17u)%100)*.0628;
            for(int i=0;i<4;++i)for(int j=0;j<kAuraTrailSamples;++j) {
                const double angle=phase+i*1.57079632679-j*.012;
                const double cs=std::cos(angle),sn=std::sin(angle),scale=8.6/std::max(std::abs(cs),std::abs(sn));
                world.trails[i][j]={f->x*16.+8+cs*scale,y+1.1+.3*std::sin(angle*2),f->z*16.+8+sn*scale,.13f};
            }
            AuraScreenFog screen;
            const auto size=ImGui::GetIO().DisplaySize;
            if(ProjectAuraFog(p,world,size.x,size.y,screen,origin))orbRenderer.Draw(draw,screen,1,style,auraBlend_.Callbacks(),auraBlend_.GlowTexture());
        }
        if(c.display==1&&c.labels&&labelRects.size()<12){ScreenPoint screen;if(p.Point({f->x*16.+8,(c.height==2?s.posY:y)+1,f->z*16.+8},screen)){
            const auto clues=ChunkClues(*f);
            char text[512];std::snprintf(text,sizeof(text),"Chunk %d, %d | %.0f m\n%s",f->x,f->z,distance(f),clues.c_str());
            ImVec2 at{screen.x+origin.x,screen.y+origin.y};
            const auto extent=ImGui::CalcTextSize(text),size=ImGui::GetIO().DisplaySize;
            if(screen.x<0||screen.y<0||screen.x+extent.x>size.x||screen.y+extent.y>size.y)continue;
            const ImVec4 rect{at.x-4,at.y-4,at.x+extent.x+4,at.y+extent.y+4};
            if(std::any_of(labelRects.begin(),labelRects.end(),[&](ImVec4 other){return rect.x<other.z&&rect.z>other.x&&rect.y<other.w&&rect.w>other.y;}))continue;
            labelRects.push_back(rect);
            draw.AddText({at.x+1,at.y+1},IM_COL32(0,0,0,230),text);draw.AddText(at,ink,text);
        }}
    }
    if(c.toasts){
        std::erase_if(chunkNotices_,[&](const auto& note){return now<note.first||now-note.first>4000;});
        const auto size=ImGui::GetIO().DisplaySize;const float width=std::min(340.f,std::max(100.f,size.x-32));float y=16;
        for(const auto& note:chunkNotices_){const auto textSize=ImGui::CalcTextSize(note.second.c_str(),nullptr,false,width-20);
            const ImVec2 at{origin.x+size.x-width-16,origin.y+y};const float height=textSize.y+20;
            style::hud::Panel(draw,at,{at.x+width,at.y+height});style::hud::Label(draw,ImGui::GetFont(),ImGui::GetFontSize(),{at.x+10,at.y+10},style::hud::Text,note.second.c_str(),width-20);y+=height+8;
        }
    }
}
} // namespace mcov
