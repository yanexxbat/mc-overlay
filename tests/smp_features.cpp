#include "overlay/client_config.hpp"
#include "imgui_internal.h"
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace mcov;
namespace {
void Require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
WorldSnapshot Scene() {
    WorldSnapshot s{}; CopyField(s.dimension,"overworld"); CopyField(s.serverAddress,"private.example");
    s.blockScanId=1; s.blockScanMask=smp::kStorageMask; s.blockScanRadius=32; s.blockScanComplete=1;
    s.fovDeg=70; s.camY=3; s.nearPlane=.05f; s.farPlane=1024; return s;
}
void Add(WorldSnapshot& s,float x,float y,float z,std::uint8_t kind=kBlockChest) {
    auto& b=s.blocks[s.blockCount++]; b={}; b.x=x; b.y=y; b.z=z; b.kind=kind;
}
void Detection() {
    for(std::uint8_t kind=0;kind<kBlockKindCount;++kind)
        Require(ClassifyBlock(std::string("block.minecraft.")+BlockKindKey(kind))==kind,"Exact block classification failed");
    for(auto pair:{std::pair{"deepslate_diamond_ore",kBlockDiamond},{"nether_gold_ore",kBlockGold},{"nether_quartz_ore",kBlockQuartz},
                   {"purple_shulker_box",kBlockShulker},{"trapped_chest",kBlockTrappedChest},{"ender_chest",kBlockEnderChest},{"blast_furnace",kBlockBlastFurnace}})
        Require(ClassifyBlock(std::string("block.minecraft.")+pair.first)==pair.second,"Variant collapsed into wrong kind");
    Require(ClassifyBlock("entity.minecraft.chest_minecart")==-1&&ClassifyBlock("block.minecraft.chestnut_planks")==-1&&
            ClassifyBlock("block.minecraft.polished_deepslate")==-1,"Unrelated block misclassified");
    smp::StashSettings c; c.enabled=true; c.minContainers=3; smp::StashFinder finder; auto s=Scene();
    Add(s,-1,0,1); Add(s,-2,0,1,kBlockBarrel); Add(s,-3,0,1,kBlockShulker);
    Add(s,1,0,1,kBlockEnderChest); Add(s,2,0,1,kBlockEnderChest); Add(s,3,0,1,kBlockEnderChest);
    s.blockScanComplete=0; Require(finder.Update(s,c)==0&&finder.Finds().empty(),"Partial scan became a stash");
    s.blockScanComplete=1; Require(finder.Update(s,c)==1&&finder.Finds()[0].chunkX==-1&&finder.Finds()[0].count==3,"Negative chunk or container filter failed");
    Require(finder.Update(s,c)==0&&finder.Finds().size()==1,"Repeated snapshot duplicated a discovery");
    ++s.blockScanId; Add(s,-4,0,1); Require(finder.Update(s,c)==0&&finder.Finds()[0].count==4,"Existing chunk notified again or count did not update");
    ++s.blockScanId; s.blockCount=0; Add(s,-1,0,1); Add(s,-2,0,1); Add(s,-3,0,1);
    Require(finder.Update(s,c)==0&&finder.Finds()[0].count==4,"Partial chunk overwrote the strongest observation");
    ++s.blockScanId; s.blockCount=0; Require(finder.Update(s,c)==0&&finder.Finds().size()==1,"Leaving scan range erased history");
    CopyField(s.serverAddress,"another.example"); Require(finder.Update(s,c)==0&&finder.Finds().empty(),"Server switch retained stash history");
    s=Scene(); Add(s,1,0,4); Add(s,2,0,4); Add(s,3,0,4);
    s.blockScanMask=1u<<kBlockChest; Require(finder.Update(s,c)==0,"Missing selected block kinds accepted as a complete scan");
    s.blockScanMask=smp::kStorageMask; s.blockScanRadius=12; Require(finder.Update(s,c)==0,"Undersized scan evaluated too early");
    s.blockScanRadius=48; Add(s,45,0,4); Add(s,46,0,4); Add(s,47,0,4);
    Require(finder.Update(s,c)==1&&finder.Finds().size()==1,"Other module's larger radius leaked into stash detection");
    c.minContainers=4; Require(finder.Update(s,c)==0&&finder.Finds().empty(),"Threshold change retained invalid discoveries");
    c.minContainers=3; Require(finder.Update(s,c)==1,"Changing minimum failed to re-evaluate finished scan");
    finder.Clear(); Require(finder.Update(s,c)==0&&finder.Finds().empty(),"Clear immediately repopulated the same scan");
    ++s.blockScanId; Require(finder.Update(s,c)==1,"Fresh scan after Clear did not find stash");
    CopyField(s.dimension,"nether"); s.blockCount=0; Require(finder.Update(s,c)==0&&finder.Finds().empty(),"Dimension switch retained discoveries");
    finder.Reset(); c.minContainers=2;
    for(int i=0;i<140;++i) {
        s=Scene(); s.blockScanId=i+1; s.blockScanX=i*16; Add(s,float(i*16),0,4); Add(s,float(i*16+1),0,4);
        finder.Update(s,c);
    }
    Require(finder.Full()&&finder.Finds().size()==smp::StashFinder::kMaxFinds,"Discovery pool exceeded capacity");
    finder.Reset(); s=Scene(); c.enabled=false; Add(s,1,0,4); Add(s,2,0,4);
    Require(finder.Update(s,c)==0&&finder.Finds().empty(),"Disabled finder collected results");
}
void Profiles() {
    using namespace client_configs;
    Snapshot s; s.smp.storage.colours[0]={.1f,.2f,.3f,.4f}; s.smp.storage.mask=1u<<kBlockEnderChest;
    s.smp.stash.enabled=true; s.smp.stash.minContainers=16; s.smp.spawner.enabled=true;
    s.smp.base.enabled=true; s.smp.base.padding=6; s.smp.base.colour={.3f,.4f,.5f,.6f};
    s.binds[keys::Index(keys::Action::BaseEsp)]={0x42,0,keys::Mode::Toggle};
    s.binds[keys::Index(keys::Action::StashFinder)]={0x4A,keys::Ctrl,keys::Mode::Hold};
    std::string text; Require(Encode(s,text),"Could not encode SMP profile"); Snapshot result;
    Require(Decode(text,result)&&result.smp.storage.colours[0].w==.4f&&result.smp.storage.mask==(1u<<kBlockEnderChest)&&
        result.smp.stash.enabled&&result.smp.stash.minContainers==16&&result.smp.spawner.enabled&&
        result.binds[keys::Index(keys::Action::StashFinder)].mode==keys::Mode::Hold&&result.smp.base.enabled&&result.smp.base.padding==6&&
        result.smp.base.colour.w==.6f&&result.binds[keys::Index(keys::Action::BaseEsp)].key==0x42,"SMP profile round-trip lost values");
    std::istringstream source(text); std::ostringstream legacy; std::string line;
    const auto stashBind="bind."+std::to_string(keys::Index(keys::Action::StashFinder))+".";
    const auto spawnBind="bind."+std::to_string(keys::Index(keys::Action::SpawnerEsp))+".";
    const auto baseBind="bind."+std::to_string(keys::Index(keys::Action::BaseEsp))+".";
    while(std::getline(source,line)) if(!line.starts_with("smp.")&&!line.starts_with(stashBind)&&!line.starts_with(spawnBind)&&!line.starts_with(baseBind)) legacy<<line<<'\n';
    Require(Decode(legacy.str(),result)&&!result.smp.stash.enabled&&!result.smp.spawner.enabled&&
        result.smp.storage.mask==smp::kStorageMask&&!result.binds[keys::Index(keys::Action::StashFinder)].key,"Legacy profile did not get safe SMP defaults");
    source.clear(); source.str(text); std::ostringstream beforeBase;
    while(std::getline(source,line)) if(!line.starts_with("smp.base.")&&!line.starts_with(baseBind)) beforeBase<<line<<'\n';
    Require(Decode(beforeBase.str(),result)&&!result.smp.base.enabled&&!result.binds[keys::Index(keys::Action::BaseEsp)].key&&
        result.smp.storage.colours[0].w==.4f&&result.smp.stash.minContainers==16,"Pre-Base profile lost existing SMP values");
    auto brokenBase=text; const auto baseField=brokenBase.find("smp.base.padding "); brokenBase.erase(baseField,brokenBase.find('\n',baseField)-baseField+1);
    Require(!Decode(brokenBase,result),"Partial Base ESP profile was accepted");
    auto bad=text; const auto start=bad.find("smp.storage.colour.10.a "); bad.erase(start,bad.find('\n',start)-start+1);
    result.smp.stash.minContainers=71; Require(!Decode(bad,result)&&result.smp.stash.minContainers==71,"Damaged SMP profile partially applied");
    s.smp.stash.mask=1u<<kBlockDiamond; Require(!Encode(s,text),"Ore accepted in stash filter");
    smp::Config config; config.storage.colours[0].x=std::numeric_limits<float>::quiet_NaN(); config.stash.radius=1000;
    config.spawner.glow=std::numeric_limits<float>::infinity(); config.Clamp();
    Require(std::isfinite(config.storage.colours[0].x)&&config.stash.radius==48&&config.spawner.glow==.75f,"Invalid SMP settings survived clamping");
    config.stash.enabled=false; config.base.enabled=true;
    Require(config.Detection().enabled&&!config.stash.enabled,"Base ESP depends on enabled Stash Finder");
}
void Rendering() {
    auto s=Scene(); smp::Config c; c.storage.colours[0]={1,0,0,1}; c.storage.colours[2]={0,1,0,1};
    c.storage.mask=(1u<<kBlockChest)|(1u<<kBlockEnderChest); Add(s,-2,0,6); Add(s,2,0,6,kBlockEnderChest); Add(s,0,0,8,kBlockSpawner);
    auto& draw=*ImGui::GetBackgroundDrawList(); CameraProjection projection(s,1280,720); const int before=draw.VtxBuffer.Size;
    smp::DrawBlocks(draw,s,projection,c,false,32,1,0,{1280,720}); Require(draw.VtxBuffer.Size==before,"Disabled block modules drew geometry");
    smp::DrawBlocks(draw,s,projection,c,true,32,1,0,{1280,720});
    bool red=false,green=false;
    for(const auto& v:draw.VtxBuffer) {
        const auto rgb=v.col&~IM_COL32_A_MASK;
        red|=rgb==(IM_COL32(255,0,0,255)&~IM_COL32_A_MASK)&&(v.col&IM_COL32_A_MASK);
        green|=rgb==(IM_COL32(0,255,0,255)&~IM_COL32_A_MASK)&&(v.col&IM_COL32_A_MASK);
    }
    Require(red&&green,"Storage renderer ignored per-type colours");
    const int count=draw.VtxBuffer.Size; c.spawner.enabled=true; c.spawner.tracers=true;
    smp::DrawBlocks(draw,s,projection,c,false,32,1,0,{1280,720}); Require(draw.VtxBuffer.Size>count,"Spawner ESP generated no geometry");
    c.stash.enabled=true; c.stash.minContainers=2; c.stash.mask=1u<<kBlockChest; Add(s,-1,0,6);
    smp::StashFinder finder; Require(finder.Update(s,c.stash)==1,"Render fixture stash not detected");
    const int stashes=draw.VtxBuffer.Size; smp::DrawStashes(draw,s,projection,c.stash,finder.Finds(),{1280,720});
    Require(draw.VtxBuffer.Size>stashes,"Stash markers generated no geometry");
    c.base.enabled=true; c.base.textured=false; c.base.padding=0; c.base.labels=false; c.base.colour={0,1,1,1};
    ImGui::GetIO().Fonts->SetTexID(ImTextureID(123));
    const int bases=draw.VtxBuffer.Size; std::vector<float> tags(bases,.25f);
    { depth::Scope scope(tags); smp::DrawBases(draw,s,projection,c.base,finder.Finds(),{1280,720});
      Require(depth::tags==&tags,"Base rendering did not restore the depth scope"); }
    Require(draw.VtxBuffer.Size>bases,"Base ESP generated no 3D geometry");
    bool filled=false;
    for(int i=bases;i<draw.VtxBuffer.Size;++i) {
        Require(tags[i]==0,"Base ESP became occluded by world depth");
        const auto& v=draw.VtxBuffer[i]; filled|=(v.col&IM_COL32_A_MASK)>0&&(v.col&IM_COL32_A_MASK)<(IM_COL32(0,0,0,255)&IM_COL32_A_MASK);
    }
    Require(filled&&tags[0]==.25f,"Base fill missing or previous effects' depth tags changed");
    const int end=draw.VtxBuffer.Size; c.base.enabled=false;
    smp::DrawBases(draw,s,projection,c.base,finder.Finds(),{1280,720}); Require(draw.VtxBuffer.Size==end,"Disabled Base ESP drew geometry");
    for(const auto& v:draw.VtxBuffer) Require(std::isfinite(v.pos.x)&&std::isfinite(v.pos.y),"SMP renderer emitted an invalid vertex");
}
}
int main() {
    try {
        Detection(); Profiles(); ImGui::CreateContext(); auto& io=ImGui::GetIO(); io.IniFilename=nullptr;
        io.DisplaySize={1280,720}; io.DeltaTime=1.f/60; io.Fonts->AddFontDefault(); io.Fonts->Build();
        ImGui::NewFrame(); Rendering(); ImGui::Render(); ImGui::DestroyContext();
        std::cout<<"PASS SMP: block classification, completed scans, filters, world isolation, limits, profiles and per-type rendering\n"; return 0;
    } catch(const std::exception& error) { std::cerr<<"FAIL SMP: "<<error.what()<<'\n'; return 1; }
}
