// Generated from the same audited profiles as generated_profiles.hpp.
package mcov.sync;
final class FrameProfiles {
    record Profile(String id, String client, String frame, String frameDesc, String camera, String update, String updateDesc, String instance, String instanceDesc, String position, String positionDesc, String positionField, String positionFieldDesc, String tick, String tickDesc, String entity, String uuid, String uuidDesc, String turn, String turnDesc, String viewYaw, String viewPitch, String use, String useDesc, String living, String player, String localPlayer, String yaw, String pitch, String move, String moveDesc, String travel, String travelDesc, String jump, String playerTravel, String playerJump, String localStep, String health, String cooldown, String setPosition, String setRotation, String detached, String attack, String continueAttack, String freeSetup, String freeSetupDesc, String renderer, String hand, String handDesc) {}
    static final Profile[] PROFILES = {
        new Profile("mojmap-26.2", "net/minecraft/client/Minecraft", "runTick", "(Z)V", "net/minecraft/client/Camera", "update", "(Lnet/minecraft/client/DeltaTracker;)V", "getInstance", "()Lnet/minecraft/client/Minecraft;", "position", "()Lnet/minecraft/world/phys/Vec3;", "position", "Lnet/minecraft/world/phys/Vec3;", "tick", "()V", "net/minecraft/world/entity/Entity", "getUUID", "()Ljava/util/UUID;", "turn", "(DD)V", "getViewYRot", "getViewXRot", "startUseItem", "()V", "net/minecraft/world/entity/LivingEntity", "net/minecraft/world/entity/player/Player", "net/minecraft/client/player/LocalPlayer", "getYRot", "getXRot", "moveRelative", "(FLnet/minecraft/world/phys/Vec3;)V", "travel", "(Lnet/minecraft/world/phys/Vec3;)V", "jumpFromGround", "travel", "", "aiStep", "getHealth", "getAttackStrengthScale", "setPosition", "setRotation", "detached", "startAttack", "continueAttack", "alignWithEntity", "(F)V", "net/minecraft/client/renderer/GameRenderer", "renderItemInHand", "(Lnet/minecraft/client/renderer/state/level/CameraRenderState;FLorg/joml/Matrix4fc;)V"),
        new Profile("mojmap-1.21.11", "net/minecraft/client/Minecraft", "runTick", "(Z)V", "net/minecraft/client/Camera", "setup", "(Lnet/minecraft/world/level/Level;Lnet/minecraft/world/entity/Entity;ZZF)V", "getInstance", "()Lnet/minecraft/client/Minecraft;", "position", "()Lnet/minecraft/world/phys/Vec3;", "position", "Lnet/minecraft/world/phys/Vec3;", "tick", "()V", "net/minecraft/world/entity/Entity", "getUUID", "()Ljava/util/UUID;", "turn", "(DD)V", "getViewYRot", "getViewXRot", "startUseItem", "()V", "net/minecraft/world/entity/LivingEntity", "net/minecraft/world/entity/player/Player", "net/minecraft/client/player/LocalPlayer", "getYRot", "getXRot", "moveRelative", "(FLnet/minecraft/world/phys/Vec3;)V", "travel", "(Lnet/minecraft/world/phys/Vec3;)V", "jumpFromGround", "travel", "", "aiStep", "getHealth", "getAttackStrengthScale", "setPosition", "setRotation", "detached", "startAttack", "continueAttack", "setup", "(Lnet/minecraft/world/level/Level;Lnet/minecraft/world/entity/Entity;ZZF)V", "net/minecraft/client/renderer/GameRenderer", "renderItemInHand", "(FZLorg/joml/Matrix4f;)V"),
        new Profile("vanilla-1.21.11", "gfj", "d", "(Z)V", "ger", "a", "(Ldwo;Lcgk;ZZF)V", "V", "()Lgfj;", "b", "()Lftm;", "h", "Lftm;", "x", "()V", "cgk", "cY", "()Ljava/util/UUID;", "b", "(DD)V", "j", "i", "bv", "()V", "chl", "ddm", "hnh", "ec", "ee", "a", "(FLftm;)V", "m", "(Lftm;)V", "u", "m", "", "d_", "eZ", "I", "a", "a", "p", "bu", "e", "a", "(Ldwo;Lcgk;ZZF)V", "hob", "a", "(FZLorg/joml/Matrix4f;)V"),
        new Profile("intermediary-1.21.11", "net/minecraft/class_310", "method_1523", "(Z)V", "net/minecraft/class_4184", "method_19321", "(Lnet/minecraft/class_1937;Lnet/minecraft/class_1297;ZZF)V", "method_1551", "()Lnet/minecraft/class_310;", "method_71156", "()Lnet/minecraft/class_243;", "field_18712", "Lnet/minecraft/class_243;", "method_1574", "()V", "net/minecraft/class_1297", "method_5667", "()Ljava/util/UUID;", "method_5872", "(DD)V", "method_5705", "method_5695", "method_1583", "()V", "net/minecraft/class_1309", "net/minecraft/class_1657", "net/minecraft/class_746", "method_36454", "method_36455", "method_5724", "(FLnet/minecraft/class_243;)V", "method_6091", "(Lnet/minecraft/class_243;)V", "method_6043", "method_6091", "", "method_6007", "method_6032", "method_7261", "method_19327", "method_19325", "field_18719", "method_1536", "method_1590", "method_19321", "(Lnet/minecraft/class_1937;Lnet/minecraft/class_1297;ZZF)V", "net/minecraft/class_757", "method_3172", "(FZLorg/joml/Matrix4f;)V"),
        new Profile("mojmap-1.21.1", "net/minecraft/client/Minecraft", "runTick", "(Z)V", "net/minecraft/client/Camera", "setup", "(Lnet/minecraft/world/level/BlockGetter;Lnet/minecraft/world/entity/Entity;ZZF)V", "getInstance", "()Lnet/minecraft/client/Minecraft;", "getPosition", "()Lnet/minecraft/world/phys/Vec3;", "position", "Lnet/minecraft/world/phys/Vec3;", "tick", "()V", "net/minecraft/world/entity/Entity", "getUUID", "()Ljava/util/UUID;", "turn", "(DD)V", "getViewYRot", "getViewXRot", "startUseItem", "()V", "net/minecraft/world/entity/LivingEntity", "net/minecraft/world/entity/player/Player", "net/minecraft/client/player/LocalPlayer", "getYRot", "getXRot", "moveRelative", "(FLnet/minecraft/world/phys/Vec3;)V", "travel", "(Lnet/minecraft/world/phys/Vec3;)V", "jumpFromGround", "travel", "jumpFromGround", "aiStep", "getHealth", "getAttackStrengthScale", "setPosition", "setRotation", "detached", "startAttack", "continueAttack", "setup", "(Lnet/minecraft/world/level/BlockGetter;Lnet/minecraft/world/entity/Entity;ZZF)V", "net/minecraft/client/renderer/GameRenderer", "renderItemInHand", "(Lnet/minecraft/client/Camera;FLorg/joml/Matrix4f;)V"),
        new Profile("vanilla-1.21.1", "fgo", "c", "(Z)V", "ffy", "a", "(Ldcc;Lbsr;ZZF)V", "Q", "()Lfgo;", "b", "()Lexc;", "i", "Lexc;", "t", "()V", "bsr", "cz", "()Ljava/util/UUID;", "b", "(DD)V", "i", "h", "bm", "()V", "btn", "cmx", "geb", "dE", "dG", "a", "(FLexc;)V", "a", "(Lexc;)V", "ff", "a", "ff", "m_", "ew", "F", "a", "a", "q", "bl", "d", "a", "(Ldcc;Lbsr;ZZF)V", "ges", "a", "(Lffy;FLorg/joml/Matrix4f;)V"),
        new Profile("intermediary-1.21.1", "net/minecraft/class_310", "method_1523", "(Z)V", "net/minecraft/class_4184", "method_19321", "(Lnet/minecraft/class_1922;Lnet/minecraft/class_1297;ZZF)V", "method_1551", "()Lnet/minecraft/class_310;", "method_19326", "()Lnet/minecraft/class_243;", "field_18712", "Lnet/minecraft/class_243;", "method_1574", "()V", "net/minecraft/class_1297", "method_5667", "()Ljava/util/UUID;", "method_5872", "(DD)V", "method_5705", "method_5695", "method_1583", "()V", "net/minecraft/class_1309", "net/minecraft/class_1657", "net/minecraft/class_746", "method_36454", "method_36455", "method_5724", "(FLnet/minecraft/class_243;)V", "method_6091", "(Lnet/minecraft/class_243;)V", "method_6043", "method_6091", "method_6043", "method_6007", "method_6032", "method_7261", "method_19327", "method_19325", "field_18719", "method_1536", "method_1590", "method_19321", "(Lnet/minecraft/class_1922;Lnet/minecraft/class_1297;ZZF)V", "net/minecraft/class_757", "method_3172", "(Lnet/minecraft/class_4184;FLorg/joml/Matrix4f;)V"),
        new Profile("mojmap-1.20.1", "net/minecraft/client/Minecraft", "runTick", "(Z)V", "net/minecraft/client/Camera", "setup", "(Lnet/minecraft/world/level/BlockGetter;Lnet/minecraft/world/entity/Entity;ZZF)V", "getInstance", "()Lnet/minecraft/client/Minecraft;", "getPosition", "()Lnet/minecraft/world/phys/Vec3;", "position", "Lnet/minecraft/world/phys/Vec3;", "tick", "()V", "net/minecraft/world/entity/Entity", "getUUID", "()Ljava/util/UUID;", "turn", "(DD)V", "getViewYRot", "getViewXRot", "startUseItem", "()V", "net/minecraft/world/entity/LivingEntity", "net/minecraft/world/entity/player/Player", "net/minecraft/client/player/LocalPlayer", "getYRot", "getXRot", "moveRelative", "(FLnet/minecraft/world/phys/Vec3;)V", "travel", "(Lnet/minecraft/world/phys/Vec3;)V", "jumpFromGround", "travel", "jumpFromGround", "aiStep", "getHealth", "getAttackStrengthScale", "setPosition", "setRotation", "detached", "startAttack", "continueAttack", "setup", "(Lnet/minecraft/world/level/BlockGetter;Lnet/minecraft/world/entity/Entity;ZZF)V", "net/minecraft/client/renderer/GameRenderer", "renderItemInHand", "(Lcom/mojang/blaze3d/vertex/PoseStack;Lnet/minecraft/client/Camera;F)V"),
        new Profile("vanilla-1.20.1", "enn", "f", "(Z)V", "emz", "a", "(Lcls;Lbfj;ZZF)V", "N", "()Lenn;", "b", "()Leei;", "e", "Leei;", "s", "()V", "bfj", "ct", "()Ljava/util/UUID;", "b", "(DD)V", "h", "g", "bs", "()V", "bfz", "byo", "fiy", "dy", "dA", "a", "(FLeei;)V", "h", "(Leei;)V", "eW", "h", "eW", "b_", "er", "A", "b", "a", "m", "bj", "g", "a", "(Lcls;Lbfj;ZZF)V", "fjq", "a", "(Leij;Lemz;F)V"),
        new Profile("intermediary-1.20.1", "net/minecraft/class_310", "method_1523", "(Z)V", "net/minecraft/class_4184", "method_19321", "(Lnet/minecraft/class_1922;Lnet/minecraft/class_1297;ZZF)V", "method_1551", "()Lnet/minecraft/class_310;", "method_19326", "()Lnet/minecraft/class_243;", "field_18712", "Lnet/minecraft/class_243;", "method_1574", "()V", "net/minecraft/class_1297", "method_5667", "()Ljava/util/UUID;", "method_5872", "(DD)V", "method_5705", "method_5695", "method_1583", "()V", "net/minecraft/class_1309", "net/minecraft/class_1657", "net/minecraft/class_746", "method_36454", "method_36455", "method_5724", "(FLnet/minecraft/class_243;)V", "method_6091", "(Lnet/minecraft/class_243;)V", "method_6043", "method_6091", "method_6043", "method_6007", "method_6032", "method_7261", "method_19327", "method_19325", "field_18719", "method_1536", "method_1590", "method_19321", "(Lnet/minecraft/class_1922;Lnet/minecraft/class_1297;ZZF)V", "net/minecraft/class_757", "method_3172", "(Lnet/minecraft/class_4587;Lnet/minecraft/class_4184;F)V"),
        new Profile("forge-srg-1.20.1", "net/minecraft/client/Minecraft", "m_91383_", "(Z)V", "net/minecraft/client/Camera", "m_90575_", "(Lnet/minecraft/world/level/BlockGetter;Lnet/minecraft/world/entity/Entity;ZZF)V", "m_91087_", "()Lnet/minecraft/client/Minecraft;", "m_90583_", "()Lnet/minecraft/world/phys/Vec3;", "f_90552_", "Lnet/minecraft/world/phys/Vec3;", "m_91398_", "()V", "net/minecraft/world/entity/Entity", "m_20148_", "()Ljava/util/UUID;", "m_19884_", "(DD)V", "m_5675_", "m_5686_", "m_91277_", "()V", "net/minecraft/world/entity/LivingEntity", "net/minecraft/world/entity/player/Player", "net/minecraft/client/player/LocalPlayer", "m_146908_", "m_146909_", "m_19920_", "(FLnet/minecraft/world/phys/Vec3;)V", "m_7023_", "(Lnet/minecraft/world/phys/Vec3;)V", "m_6135_", "m_7023_", "m_6135_", "m_8107_", "m_21223_", "m_36403_", "m_90584_", "m_90572_", "f_90560_", "m_202354_", "m_91386_", "m_90575_", "(Lnet/minecraft/world/level/BlockGetter;Lnet/minecraft/world/entity/Entity;ZZF)V", "net/minecraft/client/renderer/GameRenderer", "m_109120_", "(Lcom/mojang/blaze3d/vertex/PoseStack;Lnet/minecraft/client/Camera;F)V"),
    };
    static boolean isTarget(String owner) {
        for (Profile p : PROFILES) if (p.client().equals(owner) || p.camera().equals(owner) || p.entity().equals(owner) || p.living().equals(owner) || p.player().equals(owner) || p.localPlayer().equals(owner) || p.renderer().equals(owner)) return true;
        return false;
    }
    static boolean marker(String owner, String name, String descriptor, boolean isStatic) {
        for (Profile p : PROFILES) {
            if (isStatic && p.client().equals(owner) && p.instance().equals(name) && p.instanceDesc().equals(descriptor)) return true;
            if (!isStatic && p.camera().equals(owner) && p.position().equals(name) && p.positionDesc().equals(descriptor)) return true;
            if (!isStatic && p.entity().equals(owner) && p.uuid().equals(name) && p.uuidDesc().equals(descriptor)) return true;
            if (!isStatic && p.living().equals(owner) && p.health().equals(name) && descriptor.equals("()F")) return true;
            if (!isStatic && p.player().equals(owner) && p.cooldown().equals(name) && descriptor.equals("(F)F")) return true;
            if (!isStatic && p.localPlayer().equals(owner) && p.localStep().equals(name) && descriptor.equals("()V")) return true;
            if (!isStatic && p.renderer().equals(owner) && p.hand().equals(name) && p.handDesc().equals(descriptor)) return true;
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
            if (p.client().equals(owner) && p.tick().equals(name) && p.tickDesc().equals(descriptor)) return 3;
            if (p.entity().equals(owner) && p.turn().equals(name) && p.turnDesc().equals(descriptor)) return 4;
            if (p.client().equals(owner) && p.use().equals(name) && p.useDesc().equals(descriptor)) return 5;
            if (p.client().equals(owner) && p.attack().equals(name) && descriptor.equals("()Z")) return 6;
            if (p.client().equals(owner) && p.continueAttack().equals(name) && descriptor.equals("(Z)V")) return 7;
            if (p.renderer().equals(owner) && p.hand().equals(name) && p.handDesc().equals(descriptor)) return 8;
        }
        return 0;
    }
    static int cameraAxis(String camera, String entity, String method, String descriptor) {
        if (!descriptor.equals("(F)F")) return 0;
        for (Profile p : PROFILES) if (p.camera().equals(camera) && p.entity().equals(entity)) {
            if (p.viewYaw().equals(method)) return 1;
            if (p.viewPitch().equals(method)) return 2;
        }
        return 0;
    }
    static long cameraMask(String owner) {
        long bits=0; for (int i=0;i<PROFILES.length;i++) if (PROFILES[i].camera().equals(owner)) bits |= 1L<<i;
        return bits;
    }
    static long turnMask(String owner, String method, String descriptor) {
        long bits=0; for (int i=0;i<PROFILES.length;i++) { Profile p=PROFILES[i];
            if (p.entity().equals(owner) && p.turn().equals(method) && p.turnDesc().equals(descriptor)) bits |= 1L<<i;
        } return bits;
    }
    static int movementAxis(String owner, String method, String descriptor) {
        if (!descriptor.equals("()F")) return 0;
        for (Profile p : PROFILES) if (p.entity().equals(owner)) {
            if (p.yaw().equals(method)) return 1;
            if (p.pitch().equals(method)) return 2;
        } return 0;
    }
    static int movementPart(String owner, String method, String descriptor) {
        for (Profile p : PROFILES) {
            if (p.entity().equals(owner) && p.move().equals(method) && p.moveDesc().equals(descriptor)) return 1;
            if (p.living().equals(owner) && ((p.travel().equals(method) && p.travelDesc().equals(descriptor)) || (p.jump().equals(method) && descriptor.equals("()V")))) return 2;
            if (p.player().equals(owner) && ((p.playerTravel().equals(method) && p.travelDesc().equals(descriptor)) || (!p.playerJump().isEmpty() && p.playerJump().equals(method) && descriptor.equals("()V")))) return 3;
            if (p.localPlayer().equals(owner) && p.localStep().equals(method) && descriptor.equals("()V")) return 4;
        } return 0;
    }
    static long movementMask(String owner, java.util.Set<String> found, int part) {
        long bits=0;
        for (int i=0;i<PROFILES.length;i++) { Profile p=PROFILES[i];
            boolean ready=switch(part) {
                case 1 -> p.entity().equals(owner) && found.contains(p.move()+p.moveDesc()) && found.contains(p.yaw()+"()F") && found.contains(p.pitch()+"()F");
                case 2 -> p.living().equals(owner) && found.contains(p.travel()+p.travelDesc()) && found.contains(p.jump()+"()V");
                case 3 -> p.player().equals(owner) && found.contains(p.playerTravel()+p.travelDesc()) && (p.playerJump().isEmpty() || found.contains(p.playerJump()+"()V"));
                case 4 -> p.localPlayer().equals(owner) && found.contains(p.localStep()+"()V");
                default -> false;
            };
            if (ready) bits |= 1L<<i;
        } return bits;
    }
}
