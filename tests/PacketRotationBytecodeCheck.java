package mcov.sync;

import java.nio.file.Path;
import java.util.HashMap;
import java.util.Map;
import java.util.jar.JarFile;
import jdk.internal.org.objectweb.asm.ClassReader;
import jdk.internal.org.objectweb.asm.tree.ClassNode;
import jdk.internal.org.objectweb.asm.tree.MethodInsnNode;
import jdk.internal.org.objectweb.asm.tree.analysis.Analyzer;
import jdk.internal.org.objectweb.asm.tree.analysis.BasicVerifier;
import mcov.FrameSignal;

/** Optional offline audit: client.jar, LocalPlayer internal name, send method, riding tick.
 * Reads game bytecode without initializing the game or connecting to a server. */
public final class PacketRotationBytecodeCheck {
    public static void main(String[] args) throws Exception {
        byte[] original;
        try (JarFile jar=new JarFile(Path.of(args[0]).toFile())) {
            try (var stream=jar.getInputStream(jar.getJarEntry(args[1]+".class"))) {
                original=stream.readAllBytes();
            }
        }
        FrameTransformer transformer=new FrameTransformer(null,FrameSignal.class);
        byte[] transformed=transformer.transform(null,null,args[1],null,null,original);
        if(transformed==null || transformer.failure!=null) throw new AssertionError(transformer.failure);
        ClassNode type=new ClassNode();
        new ClassReader(transformed).accept(type,ClassReader.EXPAND_FRAMES);
        Map<String,Integer> hooks=new HashMap<>();
        for(var method:type.methods) {
            new Analyzer<>(new BasicVerifier()).analyze(type.name,method);
            for(var instruction:method.instructions)
                if(instruction instanceof MethodInsnNode call && call.owner.equals("mcov/FrameSignal") &&
                   (call.name.equals("packetYaw") || call.name.equals("packetPitch")))
                    hooks.merge(method.name+method.desc,1,Integer::sum);
        }
        if(hooks.size()!=2 || hooks.getOrDefault(args[2]+"()V",0)<6 ||
           hooks.getOrDefault(args[3]+"()V",0)!=2)
            throw new AssertionError("Unexpected packet getter sites: "+hooks);
        System.out.println("PASS real client packet hooks and bytecode verification: "+hooks);
    }
}
