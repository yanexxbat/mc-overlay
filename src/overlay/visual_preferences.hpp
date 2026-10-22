#pragma once

#include "esp_renderer.hpp"
#include "visual_draw.hpp"
#include <istream>
#include <ostream>
#include <string_view>

namespace mcov::visual_preferences {

// Player ESP keeps its master and component choices alongside styling/filters.
// Other modules and HUD preferences retain their own persistence paths.
inline void Write(std::ostream& out,const VisualSettings& c) {
    out<<"visual-player "<<c.enabled<<' '<<c.boxEsp<<' '<<c.skeleton<<' '<<c.nameTags<<' '<<c.healthBars<<' '<<c.distanceText<<' '<<c.tracers<<' '<<c.targetInfo<<'\n';
    out<<"visual-box "<<int(c.boxStyle)<<' '<<c.cornerFraction<<' '<<c.boxThickness<<' '<<c.boxRounding<<' '
       <<c.filledBox<<' '<<c.fillGradient<<' '<<c.fillAlpha<<' '<<c.fillBottomAlpha<<' '<<c.skeletonThickness<<'\n';
    out<<"visual-bars "<<int(c.barPlacement)<<' '<<c.healthGradient<<' '<<c.armorBars<<' '<<c.barThickness<<'\n';
    out<<"visual-tracers "<<int(c.tracerOrigin)<<' '<<c.customTracerOrigin.x<<' '<<c.customTracerOrigin.y<<' '
       <<c.tracerThickness<<' '<<c.tracerStartAlpha;
    for(const auto* color:{c.tracerColor,c.tracerEndColor}) for(int i=0;i<4;++i) out<<' '<<color[i];
    out<<'\n';
    out<<"visual-target "<<int(c.targetHighlight)<<' '<<c.targetGlow<<' '<<c.glowRadius<<' '
       <<c.glowOpacity<<' '<<c.highlightAlpha;
    for(float value:c.targetColor) out<<' '<<value;
    out<<'\n';
    out<<"visual-labels "<<c.textOutline<<' '<<c.labelSize<<' '<<c.nameTagHealth<<' '<<c.nameTagPing<<' '
       <<c.nameTagEquipment<<'\n';
    out<<"visual-colours";
    for(const auto* color:{c.playerColor,c.hostileColor,c.otherColor,c.healthLow,c.healthMiddle,c.healthColor,c.armorColor})
        for(int i=0;i<4;++i) out<<' '<<color[i];
    out<<'\n';
    out<<"visual-filters "<<c.maxDistance<<' '<<c.showPlayers<<' '<<c.showHostiles<<' '<<c.showOther<<'\n';
}

inline bool Read(std::string_view tag,std::istream& row,VisualSettings& current) {
    auto c=current;
    if(tag=="visual-player") {
        if(!(row>>c.enabled>>c.boxEsp>>c.skeleton>>c.nameTags>>c.healthBars>>c.distanceText>>c.tracers>>c.targetInfo)) return false;
    } else if(tag=="visual-box") {
        int style;
        if(!(row>>style>>c.cornerFraction>>c.boxThickness>>c.boxRounding>>c.filledBox>>c.fillGradient>>
             c.fillAlpha>>c.fillBottomAlpha>>c.skeletonThickness)||style<0||style>int(EspBoxStyle::Wire3D)) return false;
        c.boxStyle=static_cast<EspBoxStyle>(style);
        c.cornerFraction=visual::Safe(c.cornerFraction,.28f,.08f,.45f);
        c.boxThickness=visual::Safe(c.boxThickness,1.6f,.5f,4);
        c.boxRounding=visual::Safe(c.boxRounding,7,0,16);
        c.fillAlpha=visual::Safe(c.fillAlpha,.12f,0,1);
        c.fillBottomAlpha=visual::Safe(c.fillBottomAlpha,.025f,0,1);
        c.skeletonThickness=visual::Safe(c.skeletonThickness,1.1f,.5f,3);
    } else if(tag=="visual-bars") {
        int placement;
        if(!(row>>placement>>c.healthGradient>>c.armorBars>>c.barThickness)||placement<0||placement>1) return false;
        c.barPlacement=static_cast<EspBarPlacement>(placement);
        c.barThickness=visual::Safe(c.barThickness,3,2,6);
    } else if(tag=="visual-tracers") {
        int origin;
        if(!(row>>origin>>c.customTracerOrigin.x>>c.customTracerOrigin.y>>c.tracerThickness>>c.tracerStartAlpha)||
           origin<0||origin>int(EspTracerOrigin::Custom)) return false;
        c.tracerOrigin=static_cast<EspTracerOrigin>(origin);
        for(auto* color:{c.tracerColor,c.tracerEndColor}) for(int i=0;i<4;++i) {
            if(!(row>>color[i])) return false;
            color[i]=visual::Safe(color[i],1,0,1);
        }
        c.customTracerOrigin.x=visual::Safe(c.customTracerOrigin.x,.5f,0,1);
        c.customTracerOrigin.y=visual::Safe(c.customTracerOrigin.y,1,0,1);
        c.tracerThickness=visual::Safe(c.tracerThickness,1.25f,.5f,4);
        c.tracerStartAlpha=visual::Safe(c.tracerStartAlpha,.08f,0,1);
    } else if(tag=="visual-target") {
        int highlight;
        if(!(row>>highlight>>c.targetGlow>>c.glowRadius>>c.glowOpacity>>c.highlightAlpha)||
           highlight<0||highlight>int(EspHighlight::Humanoid)) return false;
        c.targetHighlight=static_cast<EspHighlight>(highlight);
        for(float& value:c.targetColor) {
            if(!(row>>value)) return false;
            value=visual::Safe(value,1,0,1);
        }
        c.glowRadius=visual::Safe(c.glowRadius,5,0,10);
        c.glowOpacity=visual::Safe(c.glowOpacity,.45f,0,1);
        c.highlightAlpha=visual::Safe(c.highlightAlpha,.18f,0,.7f);
    } else if(tag=="visual-labels") {
        if(!(row>>c.textOutline>>c.labelSize>>c.nameTagHealth>>c.nameTagPing>>c.nameTagEquipment)) return false;
        c.labelSize=visual::Safe(c.labelSize,12,9,18);
    } else if(tag=="visual-colours") {
        for(auto* color:{c.playerColor,c.hostileColor,c.otherColor,c.healthLow,c.healthMiddle,c.healthColor,c.armorColor})
            for(int i=0;i<4;++i) {
                if(!(row>>color[i])) return false;
                color[i]=visual::Safe(color[i],1,0,1);
            }
    } else if(tag=="visual-filters") {
        if(!(row>>c.maxDistance>>c.showPlayers>>c.showHostiles>>c.showOther)) return false;
        c.maxDistance=visual::Safe(c.maxDistance,128,1,4096);
    } else return false;
    current=c;
    return true;
}

} // namespace mcov::visual_preferences
