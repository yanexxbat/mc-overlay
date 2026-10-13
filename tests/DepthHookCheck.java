package mcov.sync;

import mcov.FrameSignal;
import jdk.internal.org.objectweb.asm.*;

/** Echte transformierte Bytecodes pruefen den Hook vor clearDepthTexture. */
public final class DepthHookCheck implements Opcodes {
    public static int clears;
    public static void cleared(double value) {
        if(FrameSignal.depthEnabled) throw new AssertionError("Depth callback ran after clear");
        if(FrameSignal.texturedBaseEnabled) throw new AssertionError("Textured base callback ran after clear");
        if(value!=1.0) throw new AssertionError("Operand stack was changed");
        ++clears;
    }
    private static final class Loader extends ClassLoader {
        Loader() { super(DepthHookCheck.class.getClassLoader()); }
        Class<?> put(String name,byte[] bytes) { return defineClass(name.replace('/','.'),bytes,0,bytes.length); }
    }
    private static ClassWriter type(String name,String parent,int access) {
        ClassWriter writer=new ClassWriter(ClassWriter.COMPUTE_MAXS);
        writer.visit(V17,access,name,null,parent,null);
        return writer;
    }
    private static void constructor(ClassWriter w,String parent) {
        MethodVisitor m=w.visitMethod(ACC_PUBLIC,"<init>","()V",null,null);
        m.visitCode(); m.visitVarInsn(ALOAD,0); m.visitMethodInsn(INVOKESPECIAL,parent,"<init>","()V",false);
        m.visitInsn(RETURN); m.visitMaxs(0,0); m.visitEnd();
    }
    public static void main(String[] args) throws Exception {
        for(String[] names:new String[][]{
            {"hob","l","b","gez"},
            {"net/minecraft/class_757","method_32796","method_3188","net/minecraft/class_9779"},
            {"net/minecraft/client/renderer/GameRenderer","getDepthFar","renderLevel","net/minecraft/client/DeltaTracker"}}) {
            Loader loader=new Loader();
            String texture="com/mojang/blaze3d/textures/GpuTexture",encoder="com/mojang/blaze3d/systems/CommandEncoder";
            ClassWriter t=type(texture,"java/lang/Object",ACC_PUBLIC); constructor(t,"java/lang/Object"); t.visitEnd(); loader.put(texture,t.toByteArray());
            ClassWriter e=type(encoder,"java/lang/Object",ACC_PUBLIC|ACC_INTERFACE|ACC_ABSTRACT);
            e.visitMethod(ACC_PUBLIC|ACC_ABSTRACT,"clearDepthTexture","(L"+texture+";D)V",null,null).visitEnd(); e.visitEnd(); loader.put(encoder,e.toByteArray());
            ClassWriter impl=new ClassWriter(ClassWriter.COMPUTE_MAXS);
            impl.visit(V17,ACC_PUBLIC,"fixture/Encoder",null,"java/lang/Object",new String[]{encoder}); constructor(impl,"java/lang/Object");
            MethodVisitor c=impl.visitMethod(ACC_PUBLIC,"clearDepthTexture","(L"+texture+";D)V",null,null);
            c.visitCode(); c.visitVarInsn(DLOAD,2); c.visitMethodInsn(INVOKESTATIC,"mcov/sync/DepthHookCheck","cleared","(D)V",false);
            c.visitInsn(RETURN); c.visitMaxs(0,0); c.visitEnd(); impl.visitEnd(); loader.put("fixture/Encoder",impl.toByteArray());
            ClassWriter delta=type(names[3],"java/lang/Object",ACC_PUBLIC); constructor(delta,"java/lang/Object"); delta.visitEnd();
            Class<?> deltaType=loader.put(names[3],delta.toByteArray());
            ClassWriter game=type(names[0],"java/lang/Object",ACC_PUBLIC); constructor(game,"java/lang/Object");
            MethodVisitor marker=game.visitMethod(ACC_PUBLIC,names[1],"()F",null,null);
            marker.visitCode(); marker.visitLdcInsn(1000.f); marker.visitInsn(FRETURN); marker.visitMaxs(0,0); marker.visitEnd();
            MethodVisitor world=game.visitMethod(ACC_PUBLIC,names[2],"(L"+names[3]+";)V",null,null);
            world.visitCode(); world.visitTypeInsn(NEW,"fixture/Encoder"); world.visitInsn(DUP);
            world.visitMethodInsn(INVOKESPECIAL,"fixture/Encoder","<init>","()V",false);
            world.visitInsn(ACONST_NULL); world.visitInsn(DCONST_1);
            world.visitMethodInsn(INVOKEINTERFACE,encoder,"clearDepthTexture","(L"+texture+";D)V",true);
            world.visitInsn(RETURN); world.visitMaxs(0,0); world.visitEnd(); game.visitEnd();
            Class<?> renderer=loader.put(names[0],game.toByteArray());
            // Ohne registrierte native Methode deaktiviert depthReady die Bruecke.
            FrameSignal.depthEnabled=true;
            FrameSignal.texturedBaseEnabled=true;
            renderer.getMethod(names[2],deltaType).invoke(renderer.getConstructor().newInstance(),deltaType.getConstructor().newInstance());
            if(FrameSignal.depthEnabled) throw new AssertionError("Depth hook missing: "+names[0]);
            System.out.println("PASS depth-before-clear "+names[0]);
        }
        if(clears!=3) throw new AssertionError("Expected three original clear calls");
    }
}
