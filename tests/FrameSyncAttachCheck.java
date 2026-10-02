package mcov.sync;

import com.sun.tools.attach.VirtualMachine;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.net.URLClassLoader;
import java.nio.file.Path;
import java.util.ArrayList;

/** Exercises agentmain on already loaded classes in this disposable test JVM. */
public final class FrameSyncAttachCheck {
    private record Target(String id, Object client, Object camera, Method frame, Method update) {}
    private static void require(boolean value, String message) { if (!value) throw new AssertionError(message); }
    private static void attach(String agent) throws Exception {
        VirtualMachine vm = VirtualMachine.attach(Long.toString(ProcessHandle.current().pid()));
        try { vm.loadAgent(agent); }
        finally { vm.detach(); }
    }
    public static void main(String[] args) throws Exception {
        ArrayList<URLClassLoader> loaders = new ArrayList<>();
        ArrayList<Target> targets = new ArrayList<>();
        try {
            // Load every game fixture before adding the transformer, like an
            // injector attaching to a Minecraft session that is already open.
            for (FrameProfiles.Profile profile : FrameProfiles.PROFILES) {
              for (String suffix : new String[]{"", "-field-camera"}) {
                URLClassLoader loader = new URLClassLoader(new java.net.URL[]{
                        Path.of(args[0], profile.id() + suffix + ".jar").toUri().toURL()}, null);
                loaders.add(loader);
                Class<?> client = Class.forName(profile.client().replace('/', '.'), true, loader);
                Class<?> camera = Class.forName(profile.camera().replace('/', '.'), true, loader);
                Method frame = client.getDeclaredMethod(profile.frame(), boolean.class);
                Method update = null;
                for (Method method : camera.getDeclaredMethods())
                    if (method.getName().equals(profile.update()) && method.getReturnType() == void.class) update = method;
                require(update != null, "Missing camera method " + profile.id());
                targets.add(new Target(profile.id() + suffix, client.getConstructor().newInstance(),
                        camera.getConstructor().newInstance(), frame, update));
              }
            }
            attach(args[1]);
            Class<?> signal = Class.forName("mcov.FrameSignal", true, null);
            require(signal.getField("enabled").getBoolean(null), "Attach did not enable frame signals");
            Field epoch = signal.getField("epoch");
            for (Target target : targets) {
                target.frame().invoke(target.client(), true);
                require((epoch.getLong(null) & 1) != 0, "Loaded client was not retransformed " + target.id());
                Class<?>[] types = target.update().getParameterTypes();
                Object[] values = new Object[types.length];
                for (int i = 0; i < types.length; i++) {
                    if (types[i] == boolean.class) values[i] = false;
                    else if (types[i] == float.class) values[i] = .5f;
                }
                target.update().invoke(target.camera(), values);
                require((epoch.getLong(null) & 1) == 0, "Loaded camera was not retransformed " + target.id());
                System.out.println("PASS dynamic attach " + target.id());
            }
            // A second injector attempt must keep the installed markers intact.
            attach(args[1]);
            targets.get(0).frame().invoke(targets.get(0).client(), true);
            require((epoch.getLong(null) & 1) != 0, "Repeated attach broke frame signals");
        } finally {
            for (URLClassLoader loader : loaders) loader.close();
        }
    }
}
