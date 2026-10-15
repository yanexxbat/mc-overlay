package mcov.sync;

import com.sun.tools.attach.VirtualMachine;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.net.URLClassLoader;
import java.nio.file.Path;
import java.util.ArrayList;

/** Exercises agentmain on already loaded classes in this disposable test JVM. */
public final class FrameSyncAttachCheck {
    private record Target(String id, Object client, Object camera, Method frame, Method update,
                          Object entity, Method turn, Method yaw, Method pitch) {}
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
                Class.forName(profile.renderer().replace('/','.'),true,loader);
                Method frame = client.getDeclaredMethod(profile.frame(), boolean.class);
                Method update=camera.getDeclaredMethod(profile.update(),java.lang.invoke.MethodType.fromMethodDescriptorString(profile.updateDesc(),loader).parameterArray());
                require(update != null, "Missing camera method " + profile.id());
                Class<?> entity = Class.forName(profile.entity().replace('/', '.'), true, loader);
                Class<?> local = Class.forName(profile.localPlayer().replace('/', '.'), true, loader);
                targets.add(new Target(profile.id() + suffix, client.getConstructor().newInstance(),
                        camera.getConstructor().newInstance(), frame, update, local.getConstructor().newInstance(),
                        entity.getDeclaredMethod(profile.turn(), double.class, double.class),
                        camera.getDeclaredMethod("testCameraYaw", entity, float.class),
                        camera.getDeclaredMethod("testCameraPitch", entity, float.class)));
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
                // Camera und Entity waren schon vor Attach geladen und verifiziert.
                signal.getField("viewYaw").setFloat(null, 12);
                signal.getField("viewPitch").setFloat(null, -7);
                signal.getField("silentPlayer").set(null, target.entity());
                target.turn().invoke(target.entity(), 40d, 20d);
                require(Math.abs((float)target.yaw().invoke(target.camera(), target.entity(), .5f)-18)<.001f,
                        "Loaded silent Camera/turn hook missing " + target.id());
                require(Math.abs((float)target.pitch().invoke(target.camera(), target.entity(), .5f)+4)<.001f,
                        "Loaded silent pitch hook missing " + target.id());
                require(target.entity().getClass().getField("testYaw").getFloat(target.entity())==0,
                        "Loaded turn hook changed player combat yaw " + target.id());
                FrameProfiles.Profile p=null;
                for(FrameProfiles.Profile candidate:FrameProfiles.PROFILES)
                    if(target.id().equals(candidate.id()) || target.id().equals(candidate.id()+"-field-camera")) p=candidate;
                require(p!=null,"Profile missing");
                signal.getField("movementEnabled").setBoolean(null,true);
                target.entity().getClass().getMethod(p.localStep()).invoke(target.entity());
                require(Math.abs(target.entity().getClass().getField("testMovementYaw").getFloat(target.entity())-18)<.001f,
                    "Loaded movement scope was not retransformed "+target.id());
                signal.getField("movementEnabled").setBoolean(null,false);
                signal.getMethod("updateFreecam",double.class,double.class,double.class).invoke(null,123d,81d,-46d);
                target.update().invoke(target.camera(),values);
                require(target.camera().getClass().getField(p.detached()).getBoolean(target.camera()),"Loaded freecam camera hook missing "+target.id());
                target.entity().getClass().getField("testMovementYaw").setFloat(target.entity(),-777);
                target.entity().getClass().getMethod(p.localStep()).invoke(target.entity());
                require(target.entity().getClass().getField("testMovementYaw").getFloat(target.entity())==-777,"Loaded freecam freeze hook missing "+target.id());
                signal.getMethod("stopFreecam").invoke(null);
                signal.getField("silentPlayer").set(null, null);
                target.turn().invoke(target.entity(), 20d, 20d);
                require(Math.abs((float)target.yaw().invoke(target.camera(), target.entity(), .5f)-3)<.001f,
                        "Normal turn did not resume " + target.id());
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
