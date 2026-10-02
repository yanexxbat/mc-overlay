package mcov.sync;

import java.net.URLClassLoader;
import java.nio.file.Path;
import java.lang.reflect.Method;
import mcov.FrameSignal;

/** Actual instrumented Java fixture bytecode; no game code or render JNI. */
public final class FrameSyncCheck {
    private static void require(boolean value, String message) { if (!value) throw new AssertionError(message); }
    public static void main(String[] args) throws Exception {
        require(FrameSignal.enabled, "Bootstrap signals not installed");
        for (FrameProfiles.Profile profile : FrameProfiles.PROFILES) {
          for (String suffix : new String[]{"", "-field-camera"}) {
            Path jar = Path.of(args[0], profile.id() + suffix + ".jar");
            try (URLClassLoader loader = new URLClassLoader(new java.net.URL[]{jar.toUri().toURL()}, null)) {
                Class<?> client = Class.forName(profile.client().replace('/', '.'), true, loader);
                Class<?> camera = Class.forName(profile.camera().replace('/', '.'), true, loader);
                Object owner = client.getConstructor().newInstance(), view = camera.getConstructor().newInstance();
                Method frame = client.getDeclaredMethod(profile.frame(), boolean.class);
                Method update = null;
                for (Method method : camera.getDeclaredMethods()) if (method.getName().equals(profile.update()) && method.getReturnType() == void.class)
                    update = method;
                require(update != null, "Missing camera method");
                Class<?>[] types = update.getParameterTypes();
                for (int iteration = 0; iteration < 200; iteration++) {
                    frame.invoke(owner, true);
                    require((FrameSignal.epoch & 1) != 0, "New entity tick accepted with stale camera");
                    Object[] values = new Object[types.length];
                    for (int i = 0; i < types.length; i++) {
                        if (types[i] == boolean.class) values[i] = false;
                        else if (types[i] == float.class) values[i] = .5f;
                    }
                    if ((iteration & 1) != 0) values[0] = types[0].getConstructor().newInstance();
                    update.invoke(view, values);
                    long completed = FrameSignal.epoch;
                    require((completed & 1) == 0, "Camera return did not complete the frame");
                    frame.invoke(owner, true);
                    require(FrameSignal.epoch != completed && (FrameSignal.epoch & 1) != 0, "Mixed capture was accepted");
                }
                System.out.println("PASS frame markers " + profile.id() + suffix);
            }
          }
        }
    }
}
