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
    std::istringstream oldTextureInput(text); std::ostringstream oldTextures;
    while(std::getline(oldTextureInput,line)) if(!line.starts_with("smp.base.textured ")&&!line.starts_with("smp.base.textureOpacity ")) oldTextures<<line<<'\n';
    Require(Decode(oldTextures.str(),result)&&result.smp.base.textured&&result.smp.base.textureOpacity==1,"Old profiles did not migrate to game textures");
    auto partialTextures=text; const auto textureField=partialTextures.find("smp.base.textured "); partialTextures.erase(textureField,partialTextures.find('\n',textureField)-textureField+1);
    Require(!Decode(partialTextures,result),"Partially damaged texture settings were silently repaired");
    auto bad=text; const auto start=bad.find("smp.storage.colour.10.a "); bad.erase(start,bad.find('\n',start)-start+1);
    result.smp.stash.minContainers=71; Require(!Decode(bad,result)&&result.smp.stash.minContainers==71,"Damaged SMP profile partially applied");
    s.smp.stash.mask=1u<<kBlockDiamond; Require(!Encode(s,text),"Ore accepted in stash filter");
    smp::Config config; config.storage.colours[0].x=std::numeric_limits<float>::quiet_NaN(); config.stash.radius=1000;
    config.spawner.glow=std::numeric_limits<float>::infinity(); config.Clamp();
    Require(std::isfinite(config.storage.colours[0].x)&&config.stash.radius==48&&config.spawner.glow==.75f,"Invalid SMP settings survived clamping");
    config.stash.enabled=false; config.base.enabled=true;
    Require(config.Detection().enabled&&!config.stash.enabled,"Base ESP depends on enabled Stash Finder");
}
void GrowthDetection() {
    smp::BaseFinderSettings c; c.enabled=true; smp::BaseFinder finder; auto s=Scene(); s.blockScanMask=c.Mask();
    for(int i=0;i<3;++i) { Add(s,-1-i,20,1,kBlockKelp); s.blocks[s.blockCount-1].kelpHeight=18; }
    for(int i=0;i<6;++i) Add(s,1+i,3,4,i%2?kBlockAmethystCluster:kBlockLargeAmethystBud);
    s.blockScanComplete=0; finder.Update(s,c); Require(finder.Finds().empty(),"Partial growth scan became a possible base");
    s.blockScanComplete=1; s.blockScanMask=1u<<kBlockKelp; finder.Update(s,c);
    Require(finder.Finds().empty(),"Incomplete growth filter coverage was consumed");
    s.blockScanMask=c.Mask(); finder.Update(s,c);
    Require(finder.Finds().size()==2,"Independent kelp/amethyst thresholds or negative chunks failed");
    Require(finder.Finds()[0].chunkX==-1&&finder.Finds()[0].kelpColumns==3&&finder.Finds()[0].bounds.min.y==3,
        "Kelp height/count/bounds incorrect");
    finder.Update(s,c); Require(finder.Finds().size()==2,"Repeated scan duplicated growth discoveries");
    ++s.blockScanId; s.blockCount=0; finder.Update(s,c); Require(finder.Finds().size()==2,"Travelling removed session discoveries");
    c.minKelpColumns=4; finder.Update(s,c); Require(finder.Finds().empty(),"Changed growth thresholds retained old finds");
    ++s.blockScanId;
    for(int i=0;i<3;++i) { Add(s,1+i,20,4,kBlockKelp); s.blocks[s.blockCount-1].kelpHeight=18; }
    for(int i=0;i<5;++i) Add(s,1+i,3,4,kBlockAmethystCluster);
    finder.Update(s,c); Require(finder.Finds().empty(),"Different subthreshold clues were added together");
    c.minKelpColumns=3; s.blocks[0].kelpHeight=17; finder.Update(s,c);
    Require(finder.Finds().empty(),"Short kelp satisfied the height threshold");
    s.blocks[0].kelpHeight=18; ++s.blockScanId; s.blockScanTruncated=1; finder.Update(s,c);
    Require(finder.Finds().size()==1,"Positive lower-bound growth evidence was discarded");
    c.radius=4; finder.Update(s,c); Require(finder.Finds().empty(),"Larger shared scan escaped finder radius");
    c.radius=32; finder.Update(s,c); Require(finder.Finds().size()==1,"Growth evidence could not be re-evaluated after radius change");
    CopyField(s.dimension,"nether"); s.blockCount=0; finder.Update(s,c); Require(finder.Finds().empty(),"Growth discoveries crossed dimensions");
    CopyField(s.serverAddress,"different.example"); finder.Update(s,c); Require(finder.Matches(s),"Growth finder did not follow server identity");
    c.enabled=false; ++s.blockScanId; Add(s,1,0,4,kBlockAmethystCluster); c.minAmethyst=1;
    finder.Update(s,c); Require(finder.Finds().empty(),"Disabled growth finder collected evidence");
    Require(ClassifyBlock("block.minecraft.small_amethyst_bud")==-1&&ClassifyBlock("block.minecraft.medium_amethyst_bud")==-1&&
        ClassifyBlock("block.minecraft.dried_kelp_block")==-1,"Unrelated growth blocks became hints");
}
void GrowthProfiles() {
    using namespace client_configs;
    Snapshot s; s.smp.finder.enabled=true; s.smp.finder.minKelpHeight=25; s.smp.finder.minAmethyst=12;
    s.binds[keys::Index(keys::Action::BaseFinder)]={0x47,keys::Ctrl,keys::Mode::Hold};
    std::string text; Require(Encode(s,text),"Growth profile encoding failed"); Snapshot restored;
    Require(Decode(text,restored)&&restored.smp.finder.enabled&&restored.smp.finder.minKelpHeight==25&&restored.smp.finder.minAmethyst==12&&
        restored.binds[keys::Index(keys::Action::BaseFinder)].mode==keys::Mode::Hold,"Growth settings/binding roundtrip failed");
    const auto bind="bind."+std::to_string(keys::Index(keys::Action::BaseFinder))+".";
    std::istringstream in(text); std::ostringstream old; std::string line;
    while(std::getline(in,line)) if(!line.starts_with("smp.finder.")&&!line.starts_with(bind)) old<<line<<'\n';
    Require(Decode(old.str(),restored)&&!restored.smp.finder.enabled&&restored.smp.finder.minKelpHeight==18&&
        !restored.binds[keys::Index(keys::Action::BaseFinder)].key,"Older profile did not receive disabled finder defaults");
    auto broken=text; const auto start=broken.find("smp.finder.height "); broken.erase(start,broken.find('\n',start)-start+1);
    Require(!Decode(broken,restored),"Partially damaged growth profile was silently repaired");
    s.chunks.chunks=false; s.chunks.deepslate=false; s.chunks.blocks=false;
    Require(Encode(s,text)&&Decode(text,restored)&&!restored.chunks.chunks&&!restored.chunks.deepslate&&!restored.chunks.blocks,
        "Chunk/block highlight settings did not round-trip");
    std::istringstream oldChunks(text); std::ostringstream beforeChunkHighlight;
    while(std::getline(oldChunks,line)) if(!line.starts_with("chunks.chunks ")&&!line.starts_with("chunks.deepslate ")) beforeChunkHighlight<<line<<'\n';
    Require(Decode(beforeChunkHighlight.str(),restored)&&restored.chunks.chunks&&restored.chunks.deepslate,
        "Older chunk profiles did not receive whole-chunk highlight defaults");
    auto partial=text; const auto field=partial.find("chunks.chunks "); partial.erase(field,partial.find('\n',field)-field+1);
    Require(!Decode(partial,restored),"Partly damaged chunk-highlight settings were accepted");
}
void UnifiedChunks() {
    smp::BaseFinderSettings c; c.enabled=true; c.deepslate=true; c.minDeepslate=2;
    auto s=Scene(); s.blockScanMask=c.Mask(); smp::BaseFinder finder;
    Add(s,-16,0,6,kBlockRotatedDeepslate); Add(s,-15,0,6,kBlockRotatedDeepslate);
    Add(s,-17,0,6,kBlockRotatedDeepslate); Add(s,-18,0,6,kBlockDiamond);
    Add(s,0,0,6,kBlockAmethystCluster);
    finder.Update(s,c);
    Require(finder.Finds().size()==1&&finder.Finds()[0].chunkX==-1&&finder.Finds()[0].deepslate==2,
        "Unified finder crossed negative chunk boundaries or counted ore");
    const auto column=smp::PossibleChunkBounds(finder.Finds()[0],s);
    Require(column.min.x==-16&&column.max.x==0&&column.min.z==0&&column.max.z==16&&column.min.y==-64&&column.max.y==320,
        "Whole-chunk bounds did not cover the exact footprint/build height");
    c.minAmethyst=1; finder.Update(s,c);
    Require(finder.Finds().size()==2,"Unified finder dropped an independently qualifying growth chunk");
    c.deepslate=false; finder.Update(s,c);
    Require(finder.Finds().size()==1&&finder.Finds()[0].deepslate==0,"Old deepslate results survived filter changes");
    s.blockScanMask=c.Mask(); ++s.blockScanId; finder.Update(s,c);
    Require(finder.Finds().size()==1&&finder.Finds()[0].amethyst==1&&finder.Finds()[0].deepslate==0,
        "Disabling deepslate removed growth detection");
    CopyField(s.dimension,"minecraft:the_nether");
    const auto nether=smp::PossibleChunkBounds(finder.Finds()[0],s);
    Require(nether.min.y==0&&nether.max.y==256,"Nether chunk column used Overworld height");
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
    c.base.textured=true;
    const int textures=draw.VtxBuffer.Size; smp::DrawBases(draw,s,projection,c.base,finder.Finds(),{1280,720});
    Require(draw.VtxBuffer.Size==textures,"Texture mode still drew a coloured area");
    auto request=smp::BuildBaseRequest(s,c.base,finder.Finds(),true,1000);
    Require(request.enabled&&request.count==1&&request.opacity==1&&request.sentAtMs==1000,"Texture request omitted a detected base");
    c.base.textureOpacity=.5f;
    Require(smp::BuildBaseRequest(s,c.base,finder.Finds(),true,1100).revision==request.revision,"Opacity change unnecessarily rebuilt geometry");
    Require(!smp::BuildBaseRequest(s,c.base,finder.Finds(),false,1100).enabled,"Stale snapshot kept game rendering active");
    const int end=draw.VtxBuffer.Size; c.base.enabled=false;
    smp::DrawBases(draw,s,projection,c.base,finder.Finds(),{1280,720}); Require(draw.VtxBuffer.Size==end,"Disabled Base ESP drew geometry");
    smp::BaseFinderSettings growth; growth.enabled=true; growth.minAmethyst=1;
    smp::BaseFinder growthFinder; s.blockScanMask|=growth.Mask(); ++s.blockScanId; Add(s,0,0,6,kBlockAmethystCluster);
    growthFinder.Update(s,growth); Require(growthFinder.Finds().size()==1,"Growth render fixture not detected");
    const int growthStart=draw.VtxBuffer.Size; std::vector<float> growthTags(growthStart,.4f);
    { depth::Scope scope(growthTags); smp::DrawPossibleBases(draw,s,projection,growth,growthFinder.Finds(),{1280,720});
      Require(depth::tags==&growthTags,"Growth renderer did not restore depth scope"); }
    Require(draw.VtxBuffer.Size>growthStart,"Possible Base marker generated no geometry");
    for(int i=growthStart;i<draw.VtxBuffer.Size;++i) Require(growthTags[i]==0,"Possible Base marker became occluded");
    growth.labels=growth.tracers=false;
    const auto vertices=[&](bool chunks,bool blocks) {
        const int start=draw.VtxBuffer.Size;
        smp::DrawPossibleBases(draw,s,projection,growth,growthFinder.Finds(),{1280,720},chunks,blocks);
        return draw.VtxBuffer.Size-start;
    };
    Require(vertices(true,false)>0,"Whole chunk disappeared with individual block boxes off");
    Require(vertices(false,true)>0,"Individual clue blocks disappeared with whole chunks off");
    Require(vertices(false,false)==0,"Disabled chunk/block highlights still drew geometry");
    growth.enabled=false; const int growthEnd=draw.VtxBuffer.Size;
    smp::DrawPossibleBases(draw,s,projection,growth,growthFinder.Finds(),{1280,720});
    Require(draw.VtxBuffer.Size==growthEnd,"Disabled Base Finder drew markers");
    for(const auto& v:draw.VtxBuffer) Require(std::isfinite(v.pos.x)&&std::isfinite(v.pos.y),"SMP renderer emitted an invalid vertex");
}
}
int main() {
    try {
        Detection(); Profiles(); GrowthDetection(); GrowthProfiles(); UnifiedChunks(); ImGui::CreateContext(); auto& io=ImGui::GetIO(); io.IniFilename=nullptr;
        io.DisplaySize={1280,720}; io.DeltaTime=1.f/60; io.Fonts->AddFontDefault(); io.Fonts->Build();
        ImGui::NewFrame(); Rendering(); ImGui::Render(); ImGui::DestroyContext();
        std::cout<<"PASS SMP: block classification, completed scans, filters, world isolation, limits, profiles and per-type rendering\n"; return 0;
    } catch(const std::exception& error) { std::cerr<<"FAIL SMP: "<<error.what()<<'\n'; return 1; }
}
