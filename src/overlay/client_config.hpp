#pragma once
#include "overlay.hpp"
#include <limits>
#include <map>
#include <sstream>
#include <iomanip>
#include <type_traits>
#include <tuple>

namespace mcov::client_configs {
// User-facing settings only: no process handles, renderer history or game data.
struct Snapshot {
    VisualSettings visuals{};
    OverlaySettings overlay{};
    CombatSettings combat{};
    combat::Config killAura=combat::MakeKillAuraConfig({});
    combat::Relations relations{};
    AuraConfig aura{};
    TargetCubeConfig marker{};
    WorldVisualsConfig world{};
    FriendHatConfig friendHat{};
    LocalHatConfig hat{};
    BlockEspSettings blocks{};
    ChunkFinderSettings chunks{};
    items::MacroConfig crystal{},anchor{};
    HudSettings hud{};
    keys::Bindings binds=keys::Defaults();
    rockstar::Config extra{};
    std::array<rockstar::Marker,rockstar::Pack::kMarkers> waypoints{};
    bool reducedMotion=false;
};

// One typed schema drives both directions. Reads validate a detached snapshot,
// so a malformed or truncated profile cannot partially change the live client.
class Archive {
    std::ostream* out_{};
    std::map<std::string,std::string>* values_{};
public:
    bool valid=true;
    std::string invalidField;
    explicit Archive(std::ostream& out):out_(&out) {}
    explicit Archive(std::map<std::string,std::string>& values):values_(&values) {}
    template<class T,class... Bounds> void Field(const std::string& name,T& value,const std::tuple<Bounds...>& bounds) {
        std::apply([&](const auto... limits) { Value(name,value,limits...); },bounds);
    }
    template<class T> void Value(const std::string& name,T& value,long double low=0,
                                long double high=std::numeric_limits<T>::max()) {
        if constexpr(std::is_enum_v<T>) {
            int number=static_cast<int>(value); Value(name,number,low,high); value=static_cast<T>(number);
        } else {
            T next=value;
            if(values_) {
                const auto found=values_->find(name);
                if(found==values_->end()) { valid=false; return; }
                std::istringstream row(found->second); row.imbue(std::locale::classic());
                if constexpr(std::is_same_v<T,bool>) {
                    int number=-1; if(!(row>>number)||number<0||number>1) valid=false;
                    next=number==1;
                } else if(!(row>>next)) valid=false;
                row>>std::ws; if(!row.eof()) valid=false;
                values_->erase(found);
            }
            if constexpr(std::is_floating_point_v<T>) {
                if(!std::isfinite(next)||next<static_cast<T>(low)||next>static_cast<T>(high)) valid=false;
            } else if constexpr(!std::is_same_v<T,bool>) {
                if(static_cast<long double>(next)<low||static_cast<long double>(next)>high) valid=false;
            }
            if(!valid&&invalidField.empty()) invalidField=name;
            value=next;
            if(out_) *out_<<name<<' '<<value<<'\n';
        }
    }
    void Color(const std::string& name,ImVec4& c) {
        Value(name+".r",c.x,0,1); Value(name+".g",c.y,0,1);
        Value(name+".b",c.z,0,1); Value(name+".a",c.w,0,1);
    }
    void Color(const std::string& name,float (&c)[4]) {
        for(int i=0;i<4;++i) Value(name+"."+std::to_string(i),c[i],0,1);
    }
    template<std::size_t N> void Text(const std::string& name,std::array<char,N>& value) {
        if(values_) {
            const auto found=values_->find(name);
            if(found==values_->end()) { valid=false; return; }
            std::istringstream row(found->second); std::string text;
            if(!(row>>std::quoted(text))||text.size()>=N||text.find_first_of("\r\n\0",0,3)!=std::string::npos) valid=false;
            row>>std::ws; if(!row.eof()) valid=false;
            if(valid) { value={}; std::copy(text.begin(),text.end(),value.begin()); }
            values_->erase(found);
        }
        if(out_) *out_<<name<<' '<<std::quoted(value.data())<<'\n';
    }
};
inline void Palette(Archive& a,const std::string& name,EffectPalette& p) {
    a.Value(name+".aura",p.auraColours); a.Color(name+".start",p.start); a.Color(name+".end",p.end);
}
inline void Hat(Archive& a,const std::string& name,FriendHatConfig& c) {
    a.Value(name+".opacity",c.opacity,0,.65); a.Value(name+".glow",c.glow,0,2);
    a.Value(name+".radius",c.radiusScale,.5,2); a.Value(name+".height",c.heightScale,.2,1);
    a.Value(name+".gap",c.headGap,0,.2);
}
inline void Visit(Archive& a,Snapshot& s) {
    // Stable field names keep profiles readable and independent of struct layout.
#define FIELD(object,field,...) a.Field(#object "." #field,s.object.field,std::tuple{__VA_ARGS__})
    FIELD(overlay,occludeWorldEffects); FIELD(overlay,fovTrim,-20,20);
    FIELD(overlay,clickThroughWhenIdle); FIELD(overlay,hideWhenUnfocused);
    FIELD(overlay,showWatermark); FIELD(overlay,vsync); FIELD(overlay,excludeFromCapture);
    FIELD(overlay,focusMode,0,3); a.Value("menu.reducedMotion",s.reducedMotion);
    FIELD(visuals,boxEsp); FIELD(visuals,skeleton); FIELD(visuals,filledBox); FIELD(visuals,tracers);
    FIELD(visuals,nameTags); FIELD(visuals,nameTagHealth); FIELD(visuals,nameTagPing); FIELD(visuals,nameTagEquipment);
    FIELD(visuals,healthBars); FIELD(visuals,distanceText); FIELD(visuals,targetInfo); FIELD(visuals,nameProtect);
    FIELD(visuals,showPlayers); FIELD(visuals,showHostiles); FIELD(visuals,showOther);
    FIELD(visuals,maxDistance,1,4096); FIELD(visuals,boxThickness,.5,4); FIELD(visuals,boxRounding,0,16);
    FIELD(visuals,skeletonThickness,.5,3); FIELD(visuals,labelSize,9,18); FIELD(visuals,boxStyle,0,2);
    FIELD(visuals,cornerFraction,.08,.45); FIELD(visuals,fillAlpha,0,1); FIELD(visuals,fillBottomAlpha,0,1);
    FIELD(visuals,fillGradient); FIELD(visuals,barPlacement,0,1); FIELD(visuals,healthGradient); FIELD(visuals,armorBars);
    FIELD(visuals,barThickness,2,6); FIELD(visuals,tracerOrigin,0,3);
    FIELD(visuals,customTracerOrigin.x,0,1); FIELD(visuals,customTracerOrigin.y,0,1);
    FIELD(visuals,tracerThickness,.5,4); FIELD(visuals,tracerStartAlpha,0,1);
    FIELD(visuals,textOutline); FIELD(visuals,targetGlow); FIELD(visuals,glowRadius,0,10);
    FIELD(visuals,glowOpacity,0,1); FIELD(visuals,targetHighlight,0,2); FIELD(visuals,highlightAlpha,0,.7);
#define COLOR(object,field) a.Color(#object "." #field,s.object.field)
    COLOR(visuals,playerColor); COLOR(visuals,hostileColor); COLOR(visuals,otherColor);
    COLOR(visuals,healthColor); COLOR(visuals,healthLow); COLOR(visuals,healthMiddle); COLOR(visuals,armorColor);
    COLOR(visuals,tracerColor); COLOR(visuals,tracerEndColor); COLOR(visuals,targetColor);
    FIELD(combat,triggerbot); FIELD(combat,aimLock); FIELD(combat,aimKey,1,254);
    FIELD(combat,spin.enabled); FIELD(combat,spin.speed,30,1800); FIELD(combat,spin.direction,0,2);
    FIELD(combat,spin.reverseSeconds,.5,10); FIELD(combat,spin.pitchMode,0,3);
    FIELD(combat,spin.pitchAmplitude,0,80); FIELD(combat,spin.pitchFrequency,.1,3);
    FIELD(combat,aimFov,2,90); FIELD(combat,aimRange,1,8); FIELD(combat,aimGain,.02,1);
    FIELD(combat,aimMaxStep,2,80); FIELD(combat,aimPlayersOnly); FIELD(combat,autoTotem.enabled);
    FIELD(combat,autoTotem.retryMs,50,500); FIELD(combat,cps,1,20); FIELD(combat,jitter,0,.9);
    FIELD(combat,maxDistance,1,6); FIELD(combat,livingOnly); FIELD(combat,requireHoldKey); FIELD(combat,holdKey,1,254);
    FIELD(killAura,enabled); FIELD(killAura,seed); FIELD(killAura,target.playersOnly);
    FIELD(killAura,attack.range,.1,6); FIELD(killAura,hud.enabled); FIELD(killAura,hud.target);
    FIELD(killAura,hud.fov); FIELD(killAura,hud.trace); FIELD(killAura,hud.packetStats);
    FIELD(aura,enabled); FIELD(aura,animate); FIELD(aura,orbCount,1,8);
    FIELD(aura,orbitRadius,.3,2); FIELD(aura,orbSize,.05,.5); FIELD(aura,trailDegrees,10,220);
    FIELD(aura,rotationDegreesPerSecond,-90,90); FIELD(aura,pulseAmplitude,0,.3);
    FIELD(aura,pulseHz,0,3); FIELD(aura,opacity,0,1); FIELD(aura,fadeSeconds,0,.5);
    COLOR(aura,startColor); COLOR(aura,endColor);
    FIELD(marker,enabled); FIELD(marker,animate); FIELD(marker,showDepth); FIELD(marker,sizeFraction,.3,1.5);
    FIELD(marker,cornerFraction,.1,.4); FIELD(marker,roundFraction,.005,.09); FIELD(marker,strokeFraction,.005,.08);
    FIELD(marker,swingDegrees,20,180); FIELD(marker,directionCycleSeconds,2,20);
    FIELD(marker,opacity,0,1); FIELD(marker,glow,0,2); FIELD(marker,fadeSeconds,0,.5);
    COLOR(marker,lime); COLOR(marker,yellow);
    FIELD(world,maxDistance,4,128); FIELD(world,trails.enabled); FIELD(world,trails.subjects,0,3);
    FIELD(world,trails.shape,0,2); FIELD(world,trails.lifetime,.2,3); FIELD(world,trails.height,.1,2);
    FIELD(world,trails.width,.01,.15); FIELD(world,trails.opacity,0,1); FIELD(world,trails.glow,0,2);
    FIELD(world,hits.enabled); FIELD(world,hits.targetOnly); FIELD(world,hits.shape,0,1);
    FIELD(world,hits.amount,1,64); FIELD(world,hits.lifetime,.2,3); FIELD(world,hits.size,.02,.2);
    FIELD(world,hits.speed,.1,4); FIELD(world,hits.gravity,0,5); FIELD(world,hits.opacity,0,1); FIELD(world,hits.glow,0,2);
    FIELD(world,ambient.enabled); FIELD(world,ambient.shape,0,2); FIELD(world,ambient.amount,1,160);
    FIELD(world,ambient.radius,2,16); FIELD(world,ambient.size,.015,.15); FIELD(world,ambient.speed,0,2);
    FIELD(world,ambient.opacity,0,1); FIELD(world,ambient.glow,0,2);
    FIELD(world,bubbles.enabled); FIELD(world,bubbles.targetOnly); FIELD(world,bubbles.lifetime,.2,2);
    FIELD(world,bubbles.radius,.15,1.5); FIELD(world,bubbles.width,.005,.08);
    FIELD(world,bubbles.opacity,0,1); FIELD(world,bubbles.glow,0,2);
    FIELD(world,kills.enabled); FIELD(world,kills.targetOnly); FIELD(world,kills.shape,0,1);
    FIELD(world,kills.amount,1,96); FIELD(world,kills.lifetime,.3,3); FIELD(world,kills.radius,.3,3);
    FIELD(world,kills.size,.02,.2); FIELD(world,kills.speed,.1,4); FIELD(world,kills.opacity,0,1); FIELD(world,kills.glow,0,2);
    FIELD(world,crumbs.enabled); FIELD(world,crumbs.lifetime,.3,4); FIELD(world,crumbs.spacing,.1,1);
    FIELD(world,crumbs.size,.02,.15); FIELD(world,crumbs.opacity,0,1); FIELD(world,crumbs.glow,0,2);
    Palette(a,"world.trails.colours",s.world.trails.colours); Palette(a,"world.hits.colours",s.world.hits.colours);
    Palette(a,"world.ambient.colours",s.world.ambient.colours); Palette(a,"world.bubbles.colours",s.world.bubbles.colours);
    Palette(a,"world.kills.colours",s.world.kills.colours); Palette(a,"world.crumbs.colours",s.world.crumbs.colours);
    Hat(a,"friendHat",s.friendHat); Hat(a,"hat.shape",s.hat.shape);
    FIELD(hat,enabled); FIELD(hat,firstPerson); FIELD(hat,auraColours); COLOR(hat,start); COLOR(hat,end);
    FIELD(blocks,enabled); FIELD(blocks,mask,0,(1u<<kBlockKindCount)-1); FIELD(blocks,radius,4,48);
    FIELD(blocks,interval,200,3000); FIELD(blocks,tracers); FIELD(blocks,maxDraw,8,128);
    FIELD(chunks,enabled); FIELD(chunks,blocks); FIELD(chunks,labels); FIELD(chunks,radius,4,48);
    FIELD(chunks,interval,200,3000); FIELD(chunks,minHints,1,32); FIELD(chunks,maxDraw,8,128); COLOR(chunks,color);
    for(auto* macro:{&s.crystal,&s.anchor}) {
        const std::string n=macro==&s.crystal?"crystal.":"anchor.";
#define MACRO(field,...) a.Field(n+#field,macro->field,std::tuple{__VA_ARGS__})
        MACRO(enabled); MACRO(restoreSlot); MACRO(intervalMs,100,500); MACRO(confirmationMs,100,2000);
        MACRO(separateDelays); MACRO(requireTotem); MACRO(strictConfirmation); MACRO(chargeWithOffhandFuel);
        MACRO(requireCrosshairHit); MACRO(placeMs,50,1000); MACRO(breakMs,50,1000); MACRO(chargeMs,50,1000);
        MACRO(maxRange,1,6); MACRO(minHealth,0,20); MACRO(detonationSlot,0,9);
#undef MACRO
    }
    FIELD(hud,visible); FIELD(hud,equipment); FIELD(hud,effects); FIELD(hud,coordinates); FIELD(hud,compass);
    FIELD(hud,performance); FIELD(hud,reachMarker); FIELD(hud,modules); FIELD(hud,keybinds); FIELD(hud,durabilityWarn,.05,.5);
    for(int i=0;i<int(s.hud.extras.size());++i) a.Value("hud.extra."+std::to_string(i),s.hud.extras[i]);
    for(int i=0;i<kHudModules;++i) {
        auto& p=s.hud.layout[i]; const auto n="hud.layout."+std::to_string(i);
        a.Value(n+".x",p.position.x,0,1); a.Value(n+".y",p.position.y,0,1);
        a.Value(n+".scale",p.scale,.65,1.6); a.Value(n+".placed",p.placed);
    }
    for(int i=0;i<keys::kCount;++i) {
        auto& b=s.binds[i]; const auto n="bind."+std::to_string(i);
        a.Value(n+".key",b.key,i<2?1:0,254); a.Value(n+".modifiers",b.modifiers,0,7); a.Value(n+".mode",b.mode,0,1);
        if(keys::System(static_cast<keys::Action>(i))&&b.mode!=keys::Mode::Toggle) a.valid=false;
    }
    for(int i=0;i<keys::kCount;++i) if(keys::Conflict(s.binds,i)) a.valid=false;
    for(int i=0;i<rockstar::kCount;++i) {
        auto& c=s.extra.modules[i]; const auto n="module."+std::to_string(i);
        a.Value(n+".enabled",c.enabled); a.Value(n+".aura",c.auraColours); a.Value(n+".mode",c.mode,0,3);
        a.Value(n+".range",c.range,0,256); a.Value(n+".size",c.size,.1,128);
        a.Value(n+".lifetime",c.lifetime,.2,10); a.Value(n+".speed",c.speed,0,8);
        a.Value(n+".opacity",c.opacity,0,1); a.Value(n+".glow",c.glow,0,2);
        a.Color(n+".start",c.start); a.Color(n+".end",c.end);
    }
    for(int i=0;i<int(s.relations.size());++i) {
        auto& r=s.relations[i]; const auto n="relation."+std::to_string(i);
        a.Value(n+".used",r.used); a.Value(n+".high",r.uuid.high); a.Value(n+".low",r.uuid.low);
        a.Value(n+".kind",r.relation,0,2);
        if(r.used&&(r.relation==combat::Relation::Neutral||(!r.uuid.high&&!r.uuid.low))) a.valid=false;
        for(int j=0;j<i;++j) if(r.used&&s.relations[j].used&&r.uuid==s.relations[j].uuid) a.valid=false;
    }
    for(int i=0;i<int(s.waypoints.size());++i) {
        auto& w=s.waypoints[i]; const auto n="waypoint."+std::to_string(i);
        a.Value(n+".used",w.used); a.Value(n+".death",w.death);
        a.Value(n+".x",w.p.x,-3e7,3e7); a.Value(n+".y",w.p.y,-4096,4096); a.Value(n+".z",w.p.z,-3e7,3e7);
        a.Text(n+".label",w.label); a.Text(n+".dimension",w.dimension); a.Text(n+".server",w.server);
    }
#undef COLOR
#undef FIELD
}

inline bool Encode(Snapshot s,std::string& text,std::string* invalidField=nullptr) {
    std::ostringstream out; out.imbue(std::locale::classic());
    out<<"mcov-config 1\n"<<std::setprecision(std::numeric_limits<double>::max_digits10);
    Archive archive(out); Visit(archive,s);
    if(!archive.valid||!out) { if(invalidField) *invalidField=archive.invalidField; return false; }
    text=out.str(); return true;
}
inline bool Decode(const std::string& text,Snapshot& snapshot) {
    if(text.size()>256*1024) return false;
    std::istringstream in(text); std::string line;
    if(!std::getline(in,line)||line!="mcov-config 1") return false;
    std::map<std::string,std::string> values;
    for(int count=0;std::getline(in,line);++count) {
        if(count>=4096||line.size()>2048) return false;
        const auto split=line.find(' ');
        if(split==std::string::npos||!values.emplace(line.substr(0,split),line.substr(split+1)).second) return false;
    }
    // Vollstaendige Profile vor Spinbot behalten alle alten Binds/Module.
    // Nur wenn der gesamte neue Block fehlt, die ausgeschalteten Defaults
    // ergaenzen; teilweise beschaedigte neue Profile bleiben ein Fehler.
    const auto spinBind="bind."+std::to_string(keys::Index(keys::Action::Spinbot))+".";
    const bool hasSpin=std::any_of(values.begin(),values.end(),[&](const auto& entry) {
        return entry.first.starts_with("combat.spin.")||entry.first.starts_with(spinBind);
    });
    if(!hasSpin) {
        std::string defaults;
        if(!Encode(Snapshot{},defaults)) return false;
        std::istringstream defaultsIn(defaults);
        while(std::getline(defaultsIn,line)) {
            const auto split=line.find(' ');
            if(split==std::string::npos) continue;
            const auto key=line.substr(0,split);
            if(key.starts_with("combat.spin.")||key.starts_with(spinBind)) values.emplace(key,line.substr(split+1));
        }
    }
    Snapshot candidate; Archive archive(values); Visit(archive,candidate);
    candidate.killAura=combat::MakeKillAuraConfig(candidate.killAura);
    if(!archive.valid||!values.empty()||!combat::Validate(candidate.killAura)) return false;
    snapshot=std::move(candidate); return true;
}
inline bool ValidName(const std::string& name) {
    if(name.empty()||name.size()>48||name.front()==' '||name.back()==' ') return false;
    std::string upper;
    for(unsigned char c:name) {
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c==' '||c=='-'||c=='_')) return false;
        upper+=char(c>='a'&&c<='z'?c-32:c);
    }
    if(upper=="CON"||upper=="PRN"||upper=="AUX"||upper=="NUL") return false;
    return !(upper.size()==4&&(upper.starts_with("COM")||upper.starts_with("LPT"))&&upper[3]>='1'&&upper[3]<='9');
}
} // namespace mcov::client_configs
