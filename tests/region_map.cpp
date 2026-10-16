#include "overlay/region_map.hpp"
#include "common/dimension_name.hpp"
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
using namespace mcov::regionmap;
void Require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
int main() {
    try {
        Require(CellAt(-225000,-225000)==0&&CellAt(225000,225000)==80,"World border endpoints lost");
        Require(CellAt(0,0)==40&&CellAt(-.1,-.1)==40,"Origin must be inside central cell");
        Require(CellAt(-175000,-225000)==1&&CellAt(-175000.1,-225000)==0,"Negative boundary rounding");
        Require(CellAt(24999.9,0)==40&&CellAt(25000,0)==41,"Positive boundary rounding");
        for(auto point:{std::pair{-225001.,0.},std::pair{0.,225001.},std::pair{std::numeric_limits<double>::quiet_NaN(),0.},std::pair{0.,std::numeric_limits<double>::infinity()}})
            Require(CellAt(point.first,point.second)==-1,"Invalid position mapped to a region");
        std::array<int,6> seen{};
        for(int row=0;row<9;++row) for(int col=0;col<9;++col) {
            const int index=row*9+col;
            Require(CellAt(-200000+col*50000,-200000+row*50000)==index,"Map orientation or cell spacing changed");
            Require(kRegions[index]>=0&&kRegions[index]<6,"Unknown region type"); ++seen[kRegions[index]];
        }
        for(int count:seen) Require(count>0,"Missing server region");
        Require(kRegions[62]==2&&kRegions[79]==0&&kRegions[40]==2,"Current map reference changed");
        Require(DonutServer("DonutSMP.NET:25565")&&DonutServer("play.donutsmp.net"),"Valid Donut host rejected");
        Require(!DonutServer("donutsmp.net.example.com")&&!DonutServer("fakedonutsmp.net")&&!DonutServer("Local world"),"Other server accepted");
        Require(Overworld("minecraft:overworld")&&Overworld("Overworld")&&Overworld("ResourceKey[minecraft:dimension / minecraft:overworld]")&&!Overworld("ResourceKey[minecraft:dimension / minecraft:the_nether]")&&!Overworld("The End"),"Dimension guard failed");
        ImGui::CreateContext(); auto& io=ImGui::GetIO(); io.DisplaySize={640,480}; io.DeltaTime=1.f/60;
        unsigned char* pixels; int w,h; io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
        auto scene=std::make_unique<mcov::WorldSnapshot>();
        for(const char* name:{"overworld","the_nether","the_end"}) {
            const auto identifier=std::string("minecraft:")+name;
            mcov::CopyDimensionName(scene->dimension,"ResourceKey[minecraft:dimension / "+identifier+"]");
            Require(scene->dimension==identifier,"Agent dimension name truncated before identifier");
            Require(Overworld(scene->dimension)==(identifier=="minecraft:overworld"),"Published dimension misidentified");
        }
        mcov::CopyField(scene->serverAddress,"donutsmp.net"); mcov::CopyField(scene->dimension,"Overworld");
        for(int variant=0;variant<5;++variant) {
            Config c; if(variant==1) c.labels=c.grid=c.coordinates=false;
            if(variant==2) mcov::CopyField(scene->dimension,"The Nether");
            if(variant==3) mcov::CopyField(scene->serverAddress,"Local world");
            ImGui::NewFrame(); auto& draw=*ImGui::GetBackgroundDrawList(); Draw(draw,*scene,c,variant!=4);
            Require(draw.VtxBuffer.Size>100,"Map did not draw");
            // ImGui's antialias fringe extends half a pixel past panel edges.
            for(const auto& vertex:draw.VtxBuffer) Require(std::isfinite(vertex.pos.x)&&std::isfinite(vertex.pos.y)&&vertex.pos.x>=-1&&vertex.pos.x<=kSize.x+1&&vertex.pos.y>=-1&&vertex.pos.y<=kSize.y+1,"Map geometry leaves its HUD bounds");
            ImGui::Render();
        }
        ImGui::DestroyContext(); std::cout<<"PASS region map: coordinates, bounds, six server regions, host/dimension guards and drawing\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
