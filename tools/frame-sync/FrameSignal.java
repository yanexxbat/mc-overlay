package mcov;

/** Bootstrap-visible frame signals and the explicitly armed client-tick bridge. */
public final class FrameSignal {
    public static volatile long knifeProfiles;
    public static synchronized void markKnife(long mask) { knifeProfiles|=mask; }
    public static volatile boolean texturedBaseEnabled;
    private static native void nativeTexturedBase(Object renderer,long epoch);
    public static volatile boolean depthEnabled;
    private static native void nativeDepthReady(Object renderer,long epoch);
    public static void depthReady(Object renderer) {
        if(depthEnabled) {
            try { nativeDepthReady(renderer,epoch); }
            catch(Throwable error) { depthEnabled=false; System.err.println("[mcov] depth capture disabled: "+error); }
        }
        if(texturedBaseEnabled) {
            try { nativeTexturedBase(renderer,epoch); }
            catch(Throwable error) { texturedBaseEnabled=false; System.err.println("[mcov] textured base disabled: "+error); }
        }
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
    // Das Modell behält seine echte Rotation; Spin-Movement nutzt separat die Kamera.
    public static volatile Object silentPlayer;
    public static volatile float viewYaw, viewPitch;
    public static volatile long cameraProfiles, turnProfiles;
    public static synchronized void markCamera(long mask) { cameraProfiles |= mask; }
    public static synchronized void markTurn(long mask) { turnProfiles |= mask; }
    public static volatile boolean movementEnabled;
    public static volatile long movementProfiles;
    private static final long[] movementParts = new long[4];
    public static synchronized void markMovement(int part, long mask) {
        movementParts[part - 1] |= mask;
        movementProfiles = movementParts[0] & movementParts[1] & movementParts[2] & movementParts[3];
    }
    // Einmal pro Physik-Thread angelegt, keine Allokationen pro Tick. Verschachtelte
    // travel/moveRelative/jump-Aufrufe teilen denselben Ausnahme-sicheren Scope.
    private static final ThreadLocal<int[]> movementDepth = ThreadLocal.withInitial(() -> new int[1]);
    public static void beginMovement() { ++movementDepth.get()[0]; }
    public static void endMovement() { --movementDepth.get()[0]; }
    public static float movementYaw(Object entity, float original) {
        return movementEnabled && entity == silentPlayer && movementDepth.get()[0] > 0 ? viewYaw : original;
    }
    public static float movementPitch(Object entity, float original) {
        return movementEnabled && entity == silentPlayer && movementDepth.get()[0] > 0 ? viewPitch : original;
    }
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
    public static volatile long freecamProfiles;
    private static final long[] freecamParts = new long[4];
    public static synchronized void markFreecam(int part,long mask) {
        freecamParts[part-1] |= mask;
        freecamProfiles = freecamParts[0] & freecamParts[1] & freecamParts[2] & freecamParts[3];
    }
    private static volatile long freecamAt;
    private static double freecamX,freecamY,freecamZ,previousX,previousY,previousZ;
    public static double renderX,renderY,renderZ;
    public static boolean freecamActive() {
        long at=freecamAt;
        return at!=0 && silentPlayer!=null && System.nanoTime()-at<250_000_000L;
    }
    // Called only by the native client-tick bridge, never by the render sampler.
    public static void updateFreecam(double x,double y,double z) {
        boolean active=freecamActive();
        previousX=active?freecamX:x; previousY=active?freecamY:y; previousZ=active?freecamZ:z;
        freecamX=x; freecamY=y; freecamZ=z;
        freecamAt=System.nanoTime();
    }
    public static void stopFreecam() { freecamAt=0; }
    public static boolean freezePlayer(Object player) { return player==silentPlayer && freecamActive(); }
    public static boolean freecamFrame() {
        if(!freecamActive()) return false;
        double partial=Math.max(0,Math.min(1,(System.nanoTime()-freecamAt)/50_000_000.0));
        renderX=previousX+(freecamX-previousX)*partial;
        renderY=previousY+(freecamY-previousY)*partial;
        renderZ=previousZ+(freecamZ-previousZ)*partial;
        return true;
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
