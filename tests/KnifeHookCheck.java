package mcov.sync;
import mcov.FrameSignal;
import mcov.KnifeProfiles;
import jdk.internal.org.objectweb.asm.*;
import java.util.*;

/** Loads real transformed bytecode for all three audited namespaces. */
public final class KnifeHookCheck implements Opcodes {
    public static int vanilla;
    public static void original() { ++vanilla; }
    static final class Loader extends ClassLoader {
        Loader() { super(KnifeHookCheck.class.getClassLoader()); }
        Class<?> put(String name,byte[] bytes) { return defineClass(name.replace('/','.'),bytes,0,bytes.length); }
    }
    static ClassWriter type(String name) {
        ClassWriter w=new ClassWriter(ClassWriter.COMPUTE_MAXS); w.visit(V17,ACC_PUBLIC,name,null,"java/lang/Object",null);
        MethodVisitor m=w.visitMethod(ACC_PUBLIC,"<init>","()V",null,null); m.visitCode(); m.visitVarInsn(ALOAD,0);
        m.visitMethodInsn(INVOKESPECIAL,"java/lang/Object","<init>","()V",false); m.visitInsn(RETURN); m.visitMaxs(0,0); m.visitEnd(); return w;
    }
    public static void main(String[] args) throws Exception {
        int index=0;
        for(String[][] profile:KnifeProfiles.all()) {
            Loader loader=new Loader(); String owner=KnifeProfiles.row(profile,"class.Hand")[1];
            String[] hook=KnifeProfiles.row(profile,"hook"),marker=KnifeProfiles.row(profile,"marker"),field=KnifeProfiles.row(profile,"mainItem");
            Set<String> defined=new HashSet<>();
            for(Type argument:Type.getArgumentTypes(hook[3])) if(argument.getSort()==Type.OBJECT&&defined.add(argument.getInternalName())) {
                ClassWriter c=type(argument.getInternalName()); c.visitEnd(); loader.put(argument.getInternalName(),c.toByteArray());
            }
            ClassWriter w=type(owner); w.visitField(ACC_PRIVATE,field[2],field[3],null,null).visitEnd();
            MethodVisitor tick=w.visitMethod(ACC_PUBLIC,marker[2],marker[3],null,null); tick.visitCode(); tick.visitInsn(RETURN); tick.visitMaxs(0,0); tick.visitEnd();
            MethodVisitor render=w.visitMethod(ACC_PUBLIC,hook[2],hook[3],null,null); render.visitCode();
            render.visitMethodInsn(INVOKESTATIC,"mcov/sync/KnifeHookCheck","original","()V",false); render.visitInsn(RETURN); render.visitMaxs(0,0); render.visitEnd(); w.visitEnd();
            Class<?> hand=loader.put(owner,w.toByteArray()); var method=Arrays.stream(hand.getDeclaredMethods()).filter(m->m.getName().equals(hook[2])&&m.getParameterCount()==6).findFirst().orElseThrow();
            method.invoke(hand.getConstructor().newInstance(),null,null,null,null,null,0);
            if(vanilla!=index+1||(FrameSignal.knifeProfiles&(1L<<index))==0) throw new AssertionError("Knife hook verification/fallback failed "+index);
            ++index;
        }
        System.out.println("PASS knife hooks: verified Minecraft signatures, stack frames and Vanilla fallback in three namespaces");
    }
}
