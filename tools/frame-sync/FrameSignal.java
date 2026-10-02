package mcov;

/** Bootstrap-visible signals; these methods perform no JNI or game data reads. */
public final class FrameSignal {
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
