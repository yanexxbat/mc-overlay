package mcov.sync;

import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import com.sun.tools.attach.VirtualMachine;

/** Verifies transformed installed classes without initializing or starting Minecraft. */
public final class RealFrameVerify {
    public static void main(String[] args) throws Exception {
        ArrayList<URL> urls=new ArrayList<>();
        for(String line:Files.readAllLines(Path.of(args[0]))) if(!line.isBlank()) urls.add(Path.of(line).toUri().toURL());
        boolean attach=args.length>2;
        FrameTransformer transformer=attach?null:new FrameTransformer(null,Class.forName("mcov.FrameSignal"));
        try(URLClassLoader loader=new URLClassLoader(urls.toArray(URL[]::new),RealFrameVerify.class.getClassLoader()) {
            @Override protected Class<?> findClass(String name) throws ClassNotFoundException {
                String internal=name.replace('.','/');
                if(attach) return super.findClass(name);
                if(!FrameProfiles.isTarget(internal)&&!DepthProfiles.isTarget(internal)) return super.findClass(name);
                try {
                    URL source=findResource(internal+".class");
                    if(source==null) return super.findClass(name);
                    byte[] original;
                    try(var in=source.openStream()) { original=in.readAllBytes(); }
                    byte[] changed=transformer.transform(null,this,internal,null,null,original);
                    if(transformer.failure!=null) throw new IllegalStateException(transformer.failure);
                    byte[] bytes=changed==null?original:changed;
                    return defineClass(name,bytes,0,bytes.length);
                } catch(Exception failure) { throw new ClassNotFoundException(name,failure); }
            }
        }) {
            for(String name:args[1].split(",")) {
                System.out.println("Verifying "+name);
                Class<?> type=Class.forName(name,false,loader);
                type.getDeclaredMethods(); type.getDeclaredConstructors();
                System.out.println("PASS "+name);
            }
            if(attach) {
                VirtualMachine vm=VirtualMachine.attach(Long.toString(ProcessHandle.current().pid()));
                try { vm.loadAgent(Path.of(args[2]).toAbsolutePath().toString()); }
                finally { vm.detach(); }
                Class<?> signal=Class.forName("mcov.FrameSignal",true,null);
                if(!signal.getField("enabled").getBoolean(null)) throw new AssertionError("Attach did not enable hooks");
                if(signal.getField("freecamProfiles").getLong(null)==0) throw new AssertionError("Real freecam hooks missing");
                System.out.println("PASS dynamic attach to installed Minecraft classes (no game initialization)");
            }
        }
    }
}
