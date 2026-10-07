package mcov;

/** Bootstrap-visible frame signals and the explicitly armed client-tick bridge. */
public final class FrameSignal {
    public static volatile boolean depthEnabled;
    private static native void nativeDepthReady(Object renderer,long epoch);
    public static void depthReady(Object renderer) {
        if(!depthEnabled) return;
        try { nativeDepthReady(renderer,epoch); }
        catch(Throwable error) { depthEnabled=false; System.err.println("[mcov] depth capture disabled: "+error); }
    }
    public static volatile boolean macroEnabled;
    private static native boolean nativeMacroUse(Object client);
    public static boolean macroUse(Object client) {
        if (!macroEnabled) return false;
        try { return nativeMacroUse(client); }
        catch (Throwable error) { macroEnabled=false; return false; }
    }
    private static final double ENTITY_TURN_SCALE = 0.15; // Vanilla Entity.turn, nach Maussensitivität.
    // Kamera und Mouse-Look bleiben in Java: keine JNI-Abfrage pro Kameraframe.
    // Das Combat-Modell behält seine echte Rotation für normales Movement/Rendern.
    public static volatile Object silentPlayer;
    public static volatile float viewYaw, viewPitch;
    public static volatile long cameraProfiles, turnProfiles;
    public static synchronized void markCamera(long mask) { cameraProfiles |= mask; }
    public static synchronized void markTurn(long mask) { turnProfiles |= mask; }
    public static float cameraYaw(Object entity, float partial, float original) {
        return entity == silentPlayer ? viewYaw : original;
    }
    public static float cameraPitch(Object entity, float partial, float original) {
        return entity == silentPlayer ? viewPitch : original;
    }
    public static double turnYaw(Object entity, double delta) {
        if (entity != silentPlayer) return delta;
        if (Double.isFinite(delta)) viewYaw=(float)Math.IEEEremainder(viewYaw+delta*ENTITY_TURN_SCALE,360);
        return 0;
    }
    public static double turnPitch(Object entity, double delta) {
        if (entity != silentPlayer) return delta;
        if (Double.isFinite(delta)) viewPitch=(float)Math.max(-90,Math.min(90,viewPitch+delta*ENTITY_TURN_SCALE));
        return 0;
    }
    public static volatile boolean combatEnabled;
    // Nur nach RegisterNatives aktiviert. Der Hook läuft auf dem Minecraft-Client-Thread.
    private static native void nativeCombatTick(Object client);
    public static void combatTick(Object client) {
        if (!combatEnabled) return;
        try { nativeCombatTick(client); }
        catch (Throwable error) {
            combatEnabled = false;
            System.err.println("[mcov] combat tick disabled: " + error);
        }
    }
    public static volatile boolean enabled;
    public static volatile long epoch = 1;
    private FrameSignal() {}
    public static void begin() {
        if (enabled) epoch = (epoch + 2) | 1L;
    }
    public static void cameraReady() {
        if (enabled) epoch = (epoch + 2) & ~1L;
    }
}
