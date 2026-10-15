#pragma once
#include "../combat/types.hpp"

namespace mcov::freecam {
struct Config { bool enabled=false; float speed=8,boost=3; };
struct Input { float forward{},right{},up{}; bool boost{}; };
enum class Status { Disabled, Paused, NoWorld, Active, Unsupported, Error };
inline const char* Text(Status s) {
    switch(s) {
        case Status::Disabled:return "Off";
        case Status::Paused:return "Paused - close menus and focus Minecraft";
        case Status::NoWorld:return "Join a world";
        case Status::Active:return "Active";
        case Status::Unsupported:return "Freecam hooks missing - restart Minecraft";
        case Status::Error:return "Freecam stopped - reconnect with injector";
    }
    return "Unavailable";
}
inline bool Valid(Config c) { return std::isfinite(c.speed)&&c.speed>=1&&c.speed<=40&&std::isfinite(c.boost)&&c.boost>=1&&c.boost<=10; }
// Normalize combined axes so diagonal flight has the same maximum speed.
inline combat::Vec3 Move(combat::Vec3 position,combat::Angles view,Config c,Input input,double dt) {
    if(!Valid(c)||!std::isfinite(dt)||dt<=0||!std::isfinite(input.forward)||!std::isfinite(input.right)||!std::isfinite(input.up)) return position;
    const auto forward=combat::Direction(view);
    const double yaw=view.yaw/combat::kDegrees;
    combat::Vec3 delta{forward.x*input.forward+std::cos(yaw)*input.right,forward.y*input.forward+input.up,forward.z*input.forward+std::sin(yaw)*input.right};
    const double length=combat::Length(delta),scale=c.speed*(input.boost?c.boost:1)*std::min(dt,.2)/std::max(1.0,length);
    return {position.x+delta.x*scale,std::clamp(position.y+delta.y*scale,-4096.0,4096.0),position.z+delta.z*scale};
}
} // namespace mcov::freecam
