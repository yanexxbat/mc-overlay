#include "combat/spinbot.hpp"
#include "overlay/keybinds.hpp"
#include <iostream>
#include <stdexcept>
using namespace mcov::combat;
void Require(bool ok,const char* reason) { if(!ok) throw std::runtime_error(reason); }
double Run(double dt) {
    Spinbot spin; SpinConfig c; c.enabled=true; c.speed=100; c.direction=0;
    Angles current{}; const Angles camera{17,12};
    const int ticks=int(std::round(1/dt));
    for(int i=0;i<ticks;++i) current=*spin.Step(c,current,camera,dt);
    Require(current.pitch==12,"Spin changed the independent view pitch");
    return current.yaw;
}
int main() {
    try {
        Require(std::abs(Run(.05)-Run(.025))<1e-8&&std::abs(Run(.05)-100)<1e-8,"Spin speed depends on tick frequency");
        Spinbot spin; SpinConfig c; c.enabled=true; c.direction=1; c.speed=720;
        auto pose=spin.Step(c,{170,4},{30,4},.05);
        Require(pose&&std::abs(pose->yaw-134)<1e-8,"Counterclockwise spin is wrong");
        c.direction=0; spin.Reset(); pose=spin.Step(c,{170,4},{30,4},.05);
        Require(pose&&std::abs(pose->yaw+154)<1e-8,"Spin discontinuity at 180 degrees");
        c.speed=1800; pose=spin.Step(c,*pose,{30,4},99);
        Require(pose&&Finite(*pose),"Delayed tick produced invalid rotation");
        Require(!spin.Step(c,*pose,{30,4},.05,true),"Spin overwrote a Combat target");
        c.pitchMode=1; pose=spin.Step(c,{0,0},{0,12},.05); Require(pose&&pose->pitch==89,"Down pitch failed");
        c.pitchMode=2; pose=spin.Step(c,*pose,{0,12},.05); Require(pose&&pose->pitch==-89,"Up pitch failed");
        c.pitchMode=3; c.direction=2; c.reverseSeconds=.5f; c.speed=100; spin.Reset();
        Angles last{}; double first=0,late=0; bool softReverse=false;
        for(int i=0;i<40;++i) {
            pose=spin.Step(c,last,{0,70},.05); Require(pose&&Finite(*pose)&&std::abs(pose->pitch)<=89,"Pitch wave exceeded bounds");
            const double delta=Wrap(pose->yaw-last.yaw);
            if(i==0) first=delta;
            if(i==18) late=delta;
            softReverse|=delta>0&&delta<first-1e-6;
            last=*pose;
        }
        Require(first>0&&late<0&&softReverse,"Alternating direction did not reverse smoothly");
        c.enabled=false; Require(!spin.Step(c,last,{0,0},.05),"Disabled spin still rotates");
        c.enabled=true; c.speed=std::numeric_limits<float>::quiet_NaN();
        Require(!Valid(c)&&!spin.Step(c,last,{0,0},.05),"Invalid spin config was applied");
        Require(mcov::keys::Defaults()[mcov::keys::Index(mcov::keys::Action::Spinbot)].key==0,"Spin key collides with defaults");
        std::cout<<"PASS spinbot: time-based speed, angle wrap, pitch bounds, soft reversal, target priority, pause, invalid config\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
