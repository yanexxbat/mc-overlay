package mcov.sync;

import java.lang.instrument.Instrumentation;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Map;
import java.util.Set;
import java.util.jar.JarFile;

/** Installs frame markers through the standard Java attach-agent API. */
public final class FrameSynchronizer {
    private static boolean installed;
    private static JarFile signals;
    public static void premain(String options, Instrumentation inst) throws Exception { install(inst); }
    public static void agentmain(String options, Instrumentation inst) throws Exception { install(inst); }
    private static synchronized void install(Instrumentation inst) throws Exception {
        if (installed) return;
        if (!inst.isRetransformClassesSupported()) throw new IllegalStateException("JVM cannot install frame markers");
        Path agentJar = Path.of(FrameSynchronizer.class.getProtectionDomain().getCodeSource().getLocation().toURI());
        signals = new JarFile(agentJar.resolveSibling("frame-signal.jar").toFile());
        inst.appendToBootstrapClassLoaderSearch(signals);
        Class<?> signal = Class.forName("mcov.FrameSignal", true, null);
        Module helperModule = FrameSynchronizer.class.getModule();
        // Target-JDK ASM understands its own game bytecode version (61/65/69).
        inst.redefineModule(Object.class.getModule(), Set.of(),
                Map.of("jdk.internal.org.objectweb.asm", Set.of(helperModule),
                       "jdk.internal.org.objectweb.asm.commons", Set.of(helperModule)), Map.of(), Set.of(), Map.of());
        // Resolve the profile table before registering a transformer that uses
        // it, otherwise loading that table recursively re-enters transform().
        FrameProfiles.isTarget("");
        DepthProfiles.isTarget("");
        AntiVanishTransformer.isTarget("");
        Class.forName("mcov.AntiVanishPackets",true,null);
        AntiVanishTransformer packets=new AntiVanishTransformer(inst,signal.getModule());
        inst.addTransformer(packets,true);
        FrameTransformer transformer = new FrameTransformer(inst, signal);
        inst.addTransformer(transformer, true);
        try {
            ArrayList<Class<?>> loaded = new ArrayList<>();
            for (Class<?> type : inst.getAllLoadedClasses()) {
                if (FrameProfiles.isTarget(type.getName().replace('.', '/')) || DepthProfiles.isTarget(type.getName().replace('.', '/')) || AntiVanishTransformer.isTarget(type.getName().replace('.', '/'))) {
                    if (!inst.isModifiableClass(type)) throw new IllegalStateException("Cannot mark " + type.getName());
                    loaded.add(type);
                }
            }
            if (!loaded.isEmpty()) inst.retransformClasses(loaded.toArray(Class<?>[]::new));
            if (transformer.failure != null) throw new IllegalStateException(transformer.failure);
            signal.getField("enabled").setBoolean(null, true);
            installed = true;
            System.out.println("[mcov] camera frame markers installed");
        } catch (Throwable failure) {
            signal.getField("enabled").setBoolean(null, false);
            inst.removeTransformer(transformer); inst.removeTransformer(packets);
            throw failure;
        }
    }
}
