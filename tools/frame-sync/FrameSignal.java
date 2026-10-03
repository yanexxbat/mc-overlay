package mcov;

/** Bootstrap-visible frame signals and the explicitly armed client-tick bridge. */
public final class FrameSignal {
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
