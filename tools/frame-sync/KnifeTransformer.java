package mcov.sync;

import java.lang.instrument.Instrumentation;
import java.util.*;
import mcov.KnifeProfiles;
import jdk.internal.org.objectweb.asm.*;

/** Optional, signature-audited replacement of first-person sword submission. */
final class KnifeTransformer {
    static byte[] rewrite(Instrumentation inst,Module module,Class<?> signal,String name,byte[] original) throws Exception {
        for(int index=0;index<KnifeProfiles.all().length;++index) {
            String[][] profile=KnifeProfiles.all()[index];
            if(!KnifeProfiles.row(profile,"class.Hand")[1].equals(name)) continue;
            String[] hook=KnifeProfiles.row(profile,"hook"),marker=KnifeProfiles.row(profile,"marker"),field=KnifeProfiles.row(profile,"mainItem");
            boolean[] proof=new boolean[3]; ClassReader reader=new ClassReader(original);
            reader.accept(new ClassVisitor(Opcodes.ASM8) {
                @Override public FieldVisitor visitField(int access,String n,String d,String s,Object v) {
                    proof[2]|=(access&Opcodes.ACC_STATIC)==0&&n.equals(field[2])&&d.equals(field[3]); return null;
                }
                @Override public MethodVisitor visitMethod(int access,String n,String d,String s,String[] e) {
                    if((access&(Opcodes.ACC_STATIC|Opcodes.ACC_NATIVE|Opcodes.ACC_ABSTRACT))==0) {
                        proof[0]|=n.equals(hook[2])&&d.equals(hook[3]); proof[1]|=n.equals(marker[2])&&d.equals(marker[3]);
                    }
                    return null;
                }
            },ClassReader.SKIP_CODE);
            if(!proof[0]||!proof[1]||!proof[2]) return null;
            if(module!=null&&module.isNamed()) {
                Set<Module> helper=Set.of(signal.getModule()); Map<String,Set<Module>> opened=new HashMap<>();
                for(String alias:new String[]{"class.Pipelines","class.Type"}) {
                    String type=KnifeProfiles.row(profile,alias)[1]; int slash=type.lastIndexOf('/');
                    if(slash>0) opened.put(type.substring(0,slash).replace('/','.'),helper);
                }
                inst.redefineModule(module,helper,Map.of(),opened,Set.of(),Map.of());
            }
            ClassWriter writer=new ClassWriter(reader,ClassWriter.COMPUTE_MAXS);
            reader.accept(new ClassVisitor(Opcodes.ASM8,writer) {
                @Override public MethodVisitor visitMethod(int access,String n,String d,String s,String[] e) {
                    MethodVisitor next=super.visitMethod(access,n,d,s,e);
                    if(!n.equals(hook[2])||!d.equals(hook[3])) return next;
                    return new MethodVisitor(Opcodes.ASM8,next) {
                        @Override public void visitCode() {
                            super.visitCode();
                            for(int i=0;i<=5;++i) super.visitVarInsn(Opcodes.ALOAD,i);
                            super.visitVarInsn(Opcodes.ILOAD,6);
                            super.visitMethodInsn(Opcodes.INVOKESTATIC,"mcov/KnifeBridge","render",
                                "(Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Object;I)Z",false);
                            Label vanilla=new Label(); super.visitJumpInsn(Opcodes.IFEQ,vanilla); super.visitInsn(Opcodes.RETURN);
                            super.visitLabel(vanilla);
                            Object[] locals=new Object[7]; locals[0]=name;
                            Type[] args=Type.getArgumentTypes(d);
                            for(int i=0;i<args.length;++i) locals[i+1]=args[i].getSort()==Type.INT?Opcodes.INTEGER:args[i].getInternalName();
                            super.visitFrame(Opcodes.F_NEW,locals.length,locals,0,null);
                        }
                    };
                }
            },ClassReader.EXPAND_FRAMES);
            byte[] result=writer.toByteArray();
            signal.getMethod("markKnife",long.class).invoke(null,1L<<index);
            return result;
        }
        return null;
    }
}
