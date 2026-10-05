#include "overlay/keybinds.hpp"
#include "overlay/hud_layout.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>

using namespace mcov;
void Require(bool v,const char* text) { if(!v) throw std::runtime_error(text); }
int main() {
    try {
        auto bindings=keys::Defaults(); keys::Input input{}; keys::Engine engine;
        std::array<bool,keys::kCount> modules{}; int invocations=0;
        const auto action=keys::Action::KillAura; const auto index=keys::Index(action);
        bindings[index]={0x4B,keys::Ctrl,keys::Mode::Toggle};
        auto frame=[&](bool focus=true,bool allowed=true,bool capture=false) {
            engine.Update(bindings,input,focus,allowed,capture,[&](keys::Action a){return modules[keys::Index(a)];},
                [&](keys::Action a,bool v){modules[keys::Index(a)]=v;},[&](keys::Action){++invocations;});
        };
        input[0x4B]=true; frame(); Require(!modules[index],"missing modifier triggered keybind");
        input[0x11]=true; frame(); Require(modules[index],"modifier chord did not toggle real state");
        for(int i=0;i<30;++i) frame(); Require(modules[index],"held toggle repeated");
        input={}; frame(); input[0x4B]=input[0x11]=true; frame(); Require(!modules[index],"second press failed to toggle off");
        input={}; frame(); input[0x4B]=input[0x11]=true; frame(false); frame(); Require(!modules[index],"focus gain replayed held input");
        input={}; frame(); input[0x4B]=input[0x11]=true; frame(true,false); frame(); Require(!modules[index],"menu input replayed after closing");
        input={}; frame(); input[0x4B]=input[0x11]=true; frame(true,true,true); frame(); Require(!modules[index],"binding capture toggled module");
        input={}; frame(); bindings[index].mode=keys::Mode::Hold;
        input[0x4B]=input[0x11]=true; frame(); Require(modules[index],"hold did not activate");
        frame(false); Require(!modules[index],"focus loss did not restore hold state");
        input={}; frame(); modules[index]=true; input[0x4B]=input[0x11]=true; frame(); input={}; frame();
        Require(modules[index],"hold destroyed previously enabled state");
        const int crystal=keys::Index(keys::Action::CrystalMacro);
        bindings[crystal]={0x43,0,keys::Mode::Toggle}; // Modul-Keybind schaltet nur die Bereitschaft.
        input={}; frame(); input[0x43]=true; frame();
        Require(modules[crystal],"macro bind did not arm");
        input={}; frame(); Require(modules[crystal],"macro toggle did not stay armed");
        input[0x43]=true; frame(); Require(!modules[crystal],"second macro press did not disarm");
        input={}; frame();
        input[0x2D]=true; frame(); frame(); Require(invocations==1,"system hotkey repeated");
        input={}; frame(); input[0x2D]=true; frame(false); Require(invocations==1,"outside application triggered system key");
        auto duplicate=bindings; duplicate[keys::Index(keys::Action::Skeleton)]=bindings[index];
        Require(keys::Conflict(duplicate,index),"duplicate keybind was not detected");
        bindings[index]={0x05,0,keys::Mode::Toggle}; input={}; frame(); modules[index]=false;
        input[0x05]=true; frame(); Require(modules[index],"mouse side button did not toggle");
        input={}; input[0x10]=input[0xA0]=true;
        Require(keys::Matches({0xA0,0,keys::Mode::Hold},input),"standalone Shift failed to match");
        input[0x11]=true; Require(!keys::Matches({0xA0,0,keys::Mode::Hold},input),"extra modifier triggered standalone Shift");
        HudPlacement p{{.8f,.7f},1.4f,true};
        const auto normal=PlaceHud(p,{1920,1080},{232,180},{16,40});
        const auto small=PlaceHud(p,{480,360},{232,180},{16,40});
        Require(normal.max.x<=1920&&normal.max.y<=1080&&small.max.x<=480&&small.max.y<=360,"HUD escaped viewport on resize");
        Require(p.position.x==.8f&&p.scale==1.4f,"viewport resize changed saved placement/scale");
        MoveHud(p,small,{480,360},{-10000,10000});
        Require(p.position.x==0&&p.position.y==1,"HUD drag was not clamped");
        p.scale=std::numeric_limits<float>::quiet_NaN(); p.position.x=std::numeric_limits<float>::infinity();
        const auto safe=PlaceHud(p,{120,80},{232,180},{16,40});
        Require(std::isfinite(safe.max.x)&&safe.max.x<=120&&safe.max.y<=80,"invalid layout produced unsafe HUD geometry");
        std::cout<<"PASS client controls: toggle/hold, chords, focus, menu/capture suppression, mouse buttons, conflicts, HUD resize/drag/clamp\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
