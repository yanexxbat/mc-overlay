#include "overlay/menu_motion.hpp"
#include <cmath>
#include <iostream>
int main() {
    namespace m=mcov::control_panel::motion;
    for(float hz:{30.f,60.f,144.f}) {
        m::Tween t;
        for(int i=0;i<int(hz*.1f);++i) t.Step(1,1/hz,true);
        if(std::abs(t.value-m::Cubic((int(hz*.1f)/hz)/m::Transition))>.0001f) return 1;
        const auto before=t.value;
        t.Step(0,0,true); if(t.value!=before) return 2;
        t.Step(0,m::Transition,true); if(t.value!=0) return 3;
        t.Step(1,0,false); if(t.value!=1) return 4;
    }
    m::State a,b; a.dt=b.dt=.1f;
    a.To(42,1); if(!b.widgets.empty()) return 5;
    std::cout<<"PASS overlay motion: 30/60/144 Hz, interruption, exit, reduced motion, isolated owners\n";
}
