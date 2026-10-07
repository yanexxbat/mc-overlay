package mcov.sync;

/** Separat auditiert: GameRenderer nach dem Weltpass, vor GUI/Depth-Clear. */
final class DepthProfiles {
    static boolean isTarget(String owner) {
        return owner.equals("hob") || owner.equals("net/minecraft/class_757") || owner.equals("net/minecraft/client/renderer/GameRenderer");
    }
    static boolean marker(String owner,String method,String desc) {
        return isTarget(owner) && desc.equals("()F") &&
            (owner.equals("hob") ? method.equals("l") : owner.equals("net/minecraft/class_757") ? method.equals("method_32796") : method.equals("getDepthFar"));
    }
    static boolean worldReturn(String owner,String method,String desc) {
        return owner.equals("hob") && method.equals("b") && desc.equals("(Lgez;)V") ||
            owner.equals("net/minecraft/class_757") && method.equals("method_3188") && desc.equals("(Lnet/minecraft/class_9779;)V") ||
            owner.equals("net/minecraft/client/renderer/GameRenderer") && method.equals("renderLevel") && desc.equals("(Lnet/minecraft/client/DeltaTracker;)V");
    }
}
