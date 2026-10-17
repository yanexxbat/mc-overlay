package mcov.sync;

import java.lang.instrument.*;
import java.security.ProtectionDomain;
import java.util.*;
import jdk.internal.org.objectweb.asm.*;
import mcov.AntiVanishProfiles;

/** Observes only the audited network class, preserving packet contents and delivery. */
final class AntiVanishTransformer implements ClassFileTransformer {
    private final Instrumentation inst;
    private final Module signals;
    AntiVanishTransformer(Instrumentation inst,Module signals){this.inst=inst;this.signals=signals;}
    static boolean isTarget(String name){
        for(String[][] p:AntiVanishProfiles.all())if(AntiVanishProfiles.row(p,"class.Network")[1].equals(name))return true;
        return false;
    }
    @Override public byte[] transform(Module module,ClassLoader loader,String name,Class<?> previous,ProtectionDomain domain,byte[] bytes){
        if(!isTarget(name))return null;
        try {
            Set<String> methods=new HashSet<>();ClassReader reader=new ClassReader(bytes);
            reader.accept(new ClassVisitor(Opcodes.ASM8){
                @Override public MethodVisitor visitMethod(int access,String n,String d,String s,String[] e){
                    if((access&(Opcodes.ACC_STATIC|Opcodes.ACC_ABSTRACT|Opcodes.ACC_NATIVE))==0)methods.add(n+d);return null;
                }
            },ClassReader.SKIP_CODE|ClassReader.SKIP_DEBUG|ClassReader.SKIP_FRAMES);
            String[][] matched=null;
            for(String[][] p:AntiVanishProfiles.all()){
                var in=AntiVanishProfiles.row(p,"packetIn");var out=AntiVanishProfiles.row(p,"packetOut");var marker=AntiVanishProfiles.row(p,"networkMarker");
                if(in[1].equals(name)&&methods.contains(in[2]+in[3])&&methods.contains(out[2]+out[3])&&methods.contains(marker[2]+marker[3])){matched=p;break;}
            }
            if(matched==null)return null;
            if(module!=null&&module.isNamed())inst.redefineModule(module,Set.of(signals),Map.of(),Map.of(),Set.of(),Map.of());
            final var in=AntiVanishProfiles.row(matched,"packetIn");final var out=AntiVanishProfiles.row(matched,"packetOut");
            ClassWriter writer=new ClassWriter(reader,ClassWriter.COMPUTE_MAXS);
            reader.accept(new ClassVisitor(Opcodes.ASM8,writer){
                @Override public MethodVisitor visitMethod(int access,String n,String d,String s,String[] e){
                    var next=super.visitMethod(access,n,d,s,e);boolean incoming=n.equals(in[2])&&d.equals(in[3]),outgoing=n.equals(out[2])&&d.equals(out[3]);
                    if(!incoming&&!outgoing)return next;
                    return new MethodVisitor(Opcodes.ASM8,next){
                        @Override public void visitCode(){super.visitCode();
                            super.visitVarInsn(Opcodes.ALOAD,0);super.visitVarInsn(Opcodes.ALOAD,incoming?2:1);
                            super.visitInsn(outgoing?Opcodes.ICONST_1:Opcodes.ICONST_0);
                            super.visitMethodInsn(Opcodes.INVOKESTATIC,"mcov/AntiVanishPackets","observe","(Ljava/lang/Object;Ljava/lang/Object;Z)V",false);
                        }
                    };
                }
            },0);
            mcov.AntiVanishPackets.installed=true;return writer.toByteArray();
        }catch(Throwable error){System.err.println("[mcov] Anti Vanish packet hook unavailable: "+error);return null;}
    }
}
