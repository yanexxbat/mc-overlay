package mcov.diagnostics;
import java.lang.instrument.*;
import java.nio.file.*;

/** Development-only repair of our own loaded mesh consumer; no game classes edited. */
public final class BaseTextureHotfix {
    public static void agentmain(String options,Instrumentation inst) throws Exception {
        if(!inst.isRedefineClassesSupported()) throw new IllegalStateException("Class repair unsupported");
        Class<?> target=null;
        for(Class<?> type:inst.getAllLoadedClasses()) if(type.getName().equals("mcov.BaseTextureBridge$Builder")) target=type;
        if(target==null||!inst.isModifiableClass(target)) throw new IllegalStateException("No loaded mcov mesh consumer");
        Path source=Path.of(options).resolve("mcov/BaseTextureBridge$Builder.class");
        inst.redefineClasses(new ClassDefinition(target,Files.readAllBytes(source)));
        System.out.println("[mcov] packed-colour mesh consumer repair installed");
    }
}
