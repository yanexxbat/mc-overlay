#include "overlay/client_config.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mcov;
void Require(bool yes,const char* why) { if(!yes) throw std::runtime_error(why); }
int main() {
    try {
        knife::Request r; r.sentAtMs=100; r.config.enabled=true;
        Require(knife::Fresh(r,600)&&!knife::Fresh(r,601)&&!knife::Fresh(r,99),"Knife lease boundaries are invalid");
        r.config.speed=std::numeric_limits<float>::quiet_NaN(); Require(!knife::Fresh(r,100),"NaN knife configuration accepted");
        client_configs::Snapshot c; c.knife={true,false,true,2,1.5f,1.25f,.21f};
        c.binds[keys::Index(keys::Action::KnifeInspect)]={0x52,keys::Alt,keys::Mode::Toggle};
        c.binds[keys::Index(keys::Action::CsKnife)]={0x4B,keys::Ctrl,keys::Mode::Hold};
        std::string text; Require(client_configs::Encode(c,text),"Knife profile encoding failed"); client_configs::Snapshot restored;
        Require(client_configs::Decode(text,restored)&&restored.knife.enabled&&restored.knife.skin==2&&!restored.knife.deploy&&restored.knife.trails&&
            restored.knife.size==1.25f&&restored.knife.speed==1.5f&&restored.knife.trailSeconds==.21f&&
            restored.binds[keys::Index(keys::Action::KnifeInspect)].key==0x52,"Knife profile round-trip lost configuration");
        const auto module="bind."+std::to_string(keys::Index(keys::Action::CsKnife))+".",inspect="bind."+std::to_string(keys::Index(keys::Action::KnifeInspect))+".";
        std::istringstream in(text); std::ostringstream old; std::string line;
        while(std::getline(in,line)) if(!line.starts_with("knife.")&&!line.starts_with(module)&&!line.starts_with(inspect)) old<<line<<'\n';
        Require(client_configs::Decode(old.str(),restored)&&!restored.knife.enabled&&restored.knife.skin==0&&!restored.binds[keys::Index(keys::Action::KnifeInspect)].key,
            "Older profile did not receive safe knife defaults");
        old<<"knife.size 1\n"; Require(!client_configs::Decode(old.str(),restored),"Partly damaged knife profile accepted");
        c.knife.skin=3; Require(!client_configs::Encode(c,text),"Unknown knife model accepted");
        keys::Engine engine; auto binds=keys::Defaults(); keys::Input input{};
        binds[keys::Index(keys::Action::KnifeInspect)]={0x52,0,keys::Mode::Toggle}; int calls=0;
        auto frame=[&](bool focused=true,bool allowed=true,bool capture=false) {
            engine.Update(binds,input,focused,allowed,capture,[](auto){return false;},[](auto,bool){},[&](auto a){if(a==keys::Action::KnifeInspect) ++calls;});
        };
        frame(); input[0x52]=true; frame(); frame(); Require(calls==1,"Held inspect key repeated its press event");
        input={}; frame(); input[0x52]=true; frame(false); frame(); Require(calls==1,"Focus change replayed inspect input");
        std::cout<<"PASS knives: lease, validation, profiles, migration and inspect key edges\n";
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
