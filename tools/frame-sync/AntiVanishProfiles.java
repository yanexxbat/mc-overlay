// Generated from audited 1.21.11 signatures.
package mcov;
public final class AntiVanishProfiles {
    private static final String[][] P0={
        {"class.Client","net/minecraft/client/Minecraft","",""},
        {"class.Level","net/minecraft/client/multiplayer/ClientLevel","",""},
        {"class.Local","net/minecraft/client/player/LocalPlayer","",""},
        {"class.Player","net/minecraft/world/entity/player/Player","",""},
        {"class.Entity","net/minecraft/world/entity/Entity","",""},
        {"class.Connection","net/minecraft/client/multiplayer/ClientPacketListener","",""},
        {"class.Info","net/minecraft/client/multiplayer/PlayerInfo","",""},
        {"instance","net/minecraft/client/Minecraft","getInstance","()Lnet/minecraft/client/Minecraft;"},
        {"player","net/minecraft/client/Minecraft","player","Lnet/minecraft/client/player/LocalPlayer;"},
        {"level","net/minecraft/client/Minecraft","level","Lnet/minecraft/client/multiplayer/ClientLevel;"},
        {"connection","net/minecraft/client/Minecraft","getConnection","()Lnet/minecraft/client/multiplayer/ClientPacketListener;"},
        {"online","net/minecraft/client/multiplayer/ClientPacketListener","getOnlinePlayers","()Ljava/util/Collection;"},
        {"listed","net/minecraft/client/multiplayer/ClientPacketListener","getListedOnlinePlayers","()Ljava/util/Collection;"},
        {"profile","net/minecraft/client/multiplayer/PlayerInfo","getProfile","()Lcom/mojang/authlib/GameProfile;"},
        {"players","net/minecraft/client/multiplayer/ClientLevel","players","()Ljava/util/List;"},
        {"playerProfile","net/minecraft/world/entity/player/Player","getGameProfile","()Lcom/mojang/authlib/GameProfile;"},
        {"uuid","net/minecraft/world/entity/Entity","getUUID","()Ljava/util/UUID;"},
        {"invisible","net/minecraft/world/entity/Entity","isInvisible","()Z"},
        {"distance","net/minecraft/world/entity/Entity","distanceToSqr","(Lnet/minecraft/world/entity/Entity;)D"},
    };
    private static final String[][] P1={
        {"class.Client","gfj","",""},
        {"class.Level","hif","",""},
        {"class.Local","hnh","",""},
        {"class.Player","ddm","",""},
        {"class.Entity","cgk","",""},
        {"class.Connection","hig","",""},
        {"class.Info","hiq","",""},
        {"instance","gfj","V","()Lgfj;"},
        {"player","gfj","s","Lhnh;"},
        {"level","gfj","r","Lhif;"},
        {"connection","gfj","R","()Lhig;"},
        {"online","hig","o","()Ljava/util/Collection;"},
        {"listed","hig","n","()Ljava/util/Collection;"},
        {"profile","hiq","a","()Lcom/mojang/authlib/GameProfile;"},
        {"players","hif","E","()Ljava/util/List;"},
        {"playerProfile","ddm","gI","()Lcom/mojang/authlib/GameProfile;"},
        {"uuid","cgk","cY","()Ljava/util/UUID;"},
        {"invisible","cgk","cG","()Z"},
        {"distance","cgk","g","(Lcgk;)D"},
    };
    private static final String[][] P2={
        {"class.Client","net/minecraft/class_310","",""},
        {"class.Level","net/minecraft/class_638","",""},
        {"class.Local","net/minecraft/class_746","",""},
        {"class.Player","net/minecraft/class_1657","",""},
        {"class.Entity","net/minecraft/class_1297","",""},
        {"class.Connection","net/minecraft/class_634","",""},
        {"class.Info","net/minecraft/class_640","",""},
        {"instance","net/minecraft/class_310","method_1551","()Lnet/minecraft/class_310;"},
        {"player","net/minecraft/class_310","field_1724","Lnet/minecraft/class_746;"},
        {"level","net/minecraft/class_310","field_1687","Lnet/minecraft/class_638;"},
        {"connection","net/minecraft/class_310","method_1562","()Lnet/minecraft/class_634;"},
        {"online","net/minecraft/class_634","method_2880","()Ljava/util/Collection;"},
        {"listed","net/minecraft/class_634","method_45732","()Ljava/util/Collection;"},
        {"profile","net/minecraft/class_640","method_2966","()Lcom/mojang/authlib/GameProfile;"},
        {"players","net/minecraft/class_638","method_18456","()Ljava/util/List;"},
        {"playerProfile","net/minecraft/class_1657","method_7334","()Lcom/mojang/authlib/GameProfile;"},
        {"uuid","net/minecraft/class_1297","method_5667","()Ljava/util/UUID;"},
        {"invisible","net/minecraft/class_1297","method_5767","()Z"},
        {"distance","net/minecraft/class_1297","method_5858","(Lnet/minecraft/class_1297;)D"},
    };
    public static String[][] get(String id) {
        if(id.equals("mojmap-1.21.11")) return P0;
        if(id.equals("vanilla-1.21.11")) return P1;
        if(id.equals("intermediary-1.21.11")) return P2;
        return null;
    }
    public static String[][][] all() { return new String[][][]{P0,P1,P2}; }
    public static String[] row(String[][] p,String key) { for(String[] r:p) if(r[0].equals(key)) return r; throw new IllegalArgumentException(key); }
}
