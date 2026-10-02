// Generated from the same audited profiles as generated_profiles.hpp.
package mcov.sync;
final class FrameProfiles {
    record Profile(String id, String client, String frame, String frameDesc, String camera, String update, String updateDesc, String instance, String instanceDesc, String position, String positionDesc, String positionField, String positionFieldDesc) {}
    static final Profile[] PROFILES = {
        new Profile("mojmap-26.2", "net/minecraft/client/Minecraft", "runTick", "(Z)V", "net/minecraft/client/Camera", "update", "(Lnet/minecraft/client/DeltaTracker;)V", "getInstance", "()Lnet/minecraft/client/Minecraft;", "position", "()Lnet/minecraft/world/phys/Vec3;", "position", "Lnet/minecraft/world/phys/Vec3;"),
        new Profile("mojmap-1.21.11", "net/minecraft/client/Minecraft", "runTick", "(Z)V", "net/minecraft/client/Camera", "setup", "(Lnet/minecraft/world/level/Level;Lnet/minecraft/world/entity/Entity;ZZF)V", "getInstance", "()Lnet/minecraft/client/Minecraft;", "position", "()Lnet/minecraft/world/phys/Vec3;", "position", "Lnet/minecraft/world/phys/Vec3;"),
        new Profile("vanilla-1.21.11", "gfj", "d", "(Z)V", "ger", "a", "(Ldwo;Lcgk;ZZF)V", "V", "()Lgfj;", "b", "()Lftm;", "h", "Lftm;"),
        new Profile("intermediary-1.21.11", "net/minecraft/class_310", "method_1523", "(Z)V", "net/minecraft/class_4184", "method_19321", "(Lnet/minecraft/class_1937;Lnet/minecraft/class_1297;ZZF)V", "method_1551", "()Lnet/minecraft/class_310;", "method_71156", "()Lnet/minecraft/class_243;", "field_18712", "Lnet/minecraft/class_243;"),
        new Profile("mojmap-1.21.1", "net/minecraft/client/Minecraft", "runTick", "(Z)V", "net/minecraft/client/Camera", "setup", "(Lnet/minecraft/world/level/BlockGetter;Lnet/minecraft/world/entity/Entity;ZZF)V", "getInstance", "()Lnet/minecraft/client/Minecraft;", "getPosition", "()Lnet/minecraft/world/phys/Vec3;", "position", "Lnet/minecraft/world/phys/Vec3;"),
        new Profile("vanilla-1.21.1", "fgo", "c", "(Z)V", "ffy", "a", "(Ldcc;Lbsr;ZZF)V", "Q", "()Lfgo;", "b", "()Lexc;", "i", "Lexc;"),
        new Profile("intermediary-1.21.1", "net/minecraft/class_310", "method_1523", "(Z)V", "net/minecraft/class_4184", "method_19321", "(Lnet/minecraft/class_1922;Lnet/minecraft/class_1297;ZZF)V", "method_1551", "()Lnet/minecraft/class_310;", "method_19326", "()Lnet/minecraft/class_243;", "field_18712", "Lnet/minecraft/class_243;"),
        new Profile("mojmap-1.20.1", "net/minecraft/client/Minecraft", "runTick", "(Z)V", "net/minecraft/client/Camera", "setup", "(Lnet/minecraft/world/level/BlockGetter;Lnet/minecraft/world/entity/Entity;ZZF)V", "getInstance", "()Lnet/minecraft/client/Minecraft;", "getPosition", "()Lnet/minecraft/world/phys/Vec3;", "position", "Lnet/minecraft/world/phys/Vec3;"),
        new Profile("vanilla-1.20.1", "enn", "f", "(Z)V", "emz", "a", "(Lcls;Lbfj;ZZF)V", "N", "()Lenn;", "b", "()Leei;", "e", "Leei;"),
        new Profile("intermediary-1.20.1", "net/minecraft/class_310", "method_1523", "(Z)V", "net/minecraft/class_4184", "method_19321", "(Lnet/minecraft/class_1922;Lnet/minecraft/class_1297;ZZF)V", "method_1551", "()Lnet/minecraft/class_310;", "method_19326", "()Lnet/minecraft/class_243;", "field_18712", "Lnet/minecraft/class_243;"),
        new Profile("forge-srg-1.20.1", "net/minecraft/client/Minecraft", "m_91383_", "(Z)V", "net/minecraft/client/Camera", "m_90575_", "(Lnet/minecraft/world/level/BlockGetter;Lnet/minecraft/world/entity/Entity;ZZF)V", "m_91087_", "()Lnet/minecraft/client/Minecraft;", "m_90583_", "()Lnet/minecraft/world/phys/Vec3;", "f_90552_", "Lnet/minecraft/world/phys/Vec3;"),
    };
    static boolean isTarget(String owner) {
        for (Profile p : PROFILES) if (p.client().equals(owner) || p.camera().equals(owner)) return true;
        return false;
    }
    static boolean marker(String owner, String name, String descriptor, boolean isStatic) {
        for (Profile p : PROFILES) {
            if (isStatic && p.client().equals(owner) && p.instance().equals(name) && p.instanceDesc().equals(descriptor)) return true;
            if (!isStatic && p.camera().equals(owner) && p.position().equals(name) && p.positionDesc().equals(descriptor)) return true;
        }
        return false;
    }
    static boolean fieldMarker(String owner, String name, String descriptor) {
        for (Profile p : PROFILES)
            if (p.camera().equals(owner) && p.positionField().equals(name) && p.positionFieldDesc().equals(descriptor)) return true;
        return false;
    }
    static int role(String owner, String name, String descriptor) {
        for (Profile p : PROFILES) {
            if (p.client().equals(owner) && p.frame().equals(name) && p.frameDesc().equals(descriptor)) return 1;
            if (p.camera().equals(owner) && p.update().equals(name) && p.updateDesc().equals(descriptor)) return 2;
        }
        return 0;
    }
}
