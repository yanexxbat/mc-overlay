#include "combat/simulated_game.hpp"
#include <cstdio>

int main() {
    using namespace mcov::combat;
    ConfigStore store;
    FriendEnemyBook relations;
    SimulatedGame game;
    Config config;
    config.enabled=config.attack.enabled=true;
    config.target.coneDegrees=40;
    if(!store.Publish(config) || !relations.Set(game.Entities()[2].uuid,Relation::Friend)) return 1;
    CombatAssist assist(store,relations,game);
    constexpr double step=0.020;
    DebugSnapshot state;
    for(int i=0;i<250;++i) state=assist.Step(game.Capture(i*step,step));
    std::printf("Combat Assist simulation (seed %llu)\n",static_cast<unsigned long long>(config.seed));
    std::printf("Commands: %llu sent, %zu buffered, %llu discarded, %llu coalesced\n",static_cast<unsigned long long>(state.packets.sent),state.packets.buffered,static_cast<unsigned long long>(state.packets.discarded),static_cast<unsigned long long>(state.packets.coalesced));
    std::printf("Attacks: %llu | friend health: %.0f | target health: %.0f\n",static_cast<unsigned long long>(game.Attacks()),game.Entities()[2].health,game.Entities()[0].health);
    return game.Attacks()>0 && game.Entities()[2].health==20 ? 0 : 2;
}
