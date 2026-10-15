#include "common/freecam.hpp"
#include "overlay/client_config.hpp"
#include <iostream>
#include <stdexcept>
using namespace mcov;
void Require(bool value,const char* why) { if(!value) throw std::runtime_error(why); }
int main() {
    try {
        freecam::Config c; c.enabled=true;
        auto forward=freecam::Move({}, {0,0},c,{1,0,0,false},.05);
        Require(std::abs(forward.z-.4)<1e-7&&std::abs(forward.x)<1e-8,"Forward does not follow Minecraft view");
        auto right=freecam::Move({}, {90,0},c,{0,1,0,false},.05);
        Require(std::abs(right.z-.4)<1e-7&&std::abs(right.x)<1e-8,"Strafe uses wrong camera axis");
        auto up=freecam::Move({}, {0,-90},c,{1,0,0,false},.05);
        Require(std::abs(up.y-.4)<1e-7,"Forward flight ignores pitch");
        auto diagonal=freecam::Move({}, {0,0},c,{1,1,1,false},.05);
        Require(std::abs(combat::Length(diagonal)-.4)<1e-7,"Diagonal flight is faster");
        auto boost=freecam::Move({}, {},c,{1,0,0,true},.05);
        Require(std::abs(boost.z-1.2)<1e-7,"Boost multiplier is wrong");
        combat::Vec3 a{},b{};
        for(int i=0;i<20;++i) a=freecam::Move(a,{17,23},c,{1,0,0,false},.05);
        for(int i=0;i<40;++i) b=freecam::Move(b,{17,23},c,{1,0,0,false},.025);
        Require(combat::Length(combat::Difference(a,b))<1e-7,"Flight speed depends on tick frequency");
        Require(combat::Length(freecam::Move({}, {},c,{1,0,0,false},99))<=1.60001,"Stalled tick teleported camera");
        c.speed=std::numeric_limits<float>::quiet_NaN();
        Require(!freecam::Valid(c)&&combat::Length(freecam::Move({}, {},c,{1,0,0,false},.05))==0,"Invalid speed moved camera");
        client_configs::Snapshot profile; profile.combat.freecam={true,13,4};
        profile.binds[keys::Index(keys::Action::Freecam)]={0x46,keys::Alt,keys::Mode::Hold};
        std::string encoded; Require(client_configs::Encode(profile,encoded),"Freecam profile encode failed");
        client_configs::Snapshot result;
        Require(client_configs::Decode(encoded,result)&&result.combat.freecam.enabled&&result.combat.freecam.speed==13&&result.combat.freecam.boost==4&&result.binds[keys::Index(keys::Action::Freecam)].mode==keys::Mode::Hold,"Freecam profile lost settings/binding");
        const auto binding="bind."+std::to_string(keys::Index(keys::Action::Freecam))+".";
        std::istringstream source(encoded); std::ostringstream old; std::string line;
        while(std::getline(source,line)) if(!line.starts_with("combat.freecam.")&&!line.starts_with(binding)) old<<line<<'\n';
        Require(client_configs::Decode(old.str(),result)&&!result.combat.freecam.enabled&&result.combat.freecam.speed==8&&!result.binds[keys::Index(keys::Action::Freecam)].key,"Older profile did not get safe Freecam defaults");
        old<<"combat.freecam.speed 12\n";
        Require(!client_configs::Decode(old.str(),result),"Incomplete Freecam profile was accepted");
        std::cout<<"PASS Freecam movement, boost, normalization, timing and profile migration\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
