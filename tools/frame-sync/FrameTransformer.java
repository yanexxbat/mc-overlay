package mcov.sync;

import java.lang.instrument.ClassFileTransformer;
import java.lang.instrument.Instrumentation;
import java.security.ProtectionDomain;
import java.util.Map;
import java.util.Set;
import jdk.internal.org.objectweb.asm.ClassReader;
import jdk.internal.org.objectweb.asm.ClassVisitor;
import jdk.internal.org.objectweb.asm.ClassWriter;
import jdk.internal.org.objectweb.asm.FieldVisitor;
import jdk.internal.org.objectweb.asm.MethodVisitor;
import jdk.internal.org.objectweb.asm.Opcodes;

final class FrameTransformer implements ClassFileTransformer {
    private final Instrumentation inst;
    private final Module signalModule;
    volatile String failure;
    FrameTransformer(Instrumentation inst, Module signalModule) { this.inst = inst; this.signalModule = signalModule; }
    @Override public byte[] transform(Module module, ClassLoader loader, String name, Class<?> previous,
                                      ProtectionDomain domain, byte[] original) {
        if (!FrameProfiles.isTarget(name)) return null;
        try {
            ClassReader reader = new ClassReader(original);
            boolean[] recognized = {false};
            reader.accept(new ClassVisitor(Opcodes.ASM8) {
                @Override public FieldVisitor visitField(int access, String field, String descriptor, String signature, Object value) {
                    if ((access & Opcodes.ACC_STATIC) == 0) recognized[0] |= FrameProfiles.fieldMarker(name, field, descriptor);
                    return null;
                }
                @Override public MethodVisitor visitMethod(int access, String method, String descriptor, String signature, String[] exceptions) {
                    recognized[0] |= FrameProfiles.marker(name, method, descriptor, (access & Opcodes.ACC_STATIC) != 0);
                    return null;
                }
            }, ClassReader.SKIP_CODE | ClassReader.SKIP_DEBUG | ClassReader.SKIP_FRAMES);
            // Obfuscated class names can be reused by unrelated Minecraft classes.
            // Require a second audited signature before adding frame markers.
            // Camera's position field also works when the optional accessor is
            // remapped/inherited through an interface or replaced by a mod.
            if (!recognized[0]) return null;
            // Forge's named game modules must read the bootstrap helper module.
            if (module != null && module.isNamed()) inst.redefineModule(module, Set.of(signalModule),
                    Map.of(), Map.of(), Set.of(), Map.of());
            ClassWriter writer = new ClassWriter(reader, ClassWriter.COMPUTE_MAXS);
            boolean[] changed = {false};
            reader.accept(new ClassVisitor(Opcodes.ASM8, writer) {
                @Override public MethodVisitor visitMethod(int access, String method, String descriptor, String signature, String[] exceptions) {
                    MethodVisitor next = super.visitMethod(access, method, descriptor, signature, exceptions);
                    int role = FrameProfiles.role(name, method, descriptor);
                    if (role == 0) return next;
                    changed[0] = true;
                    return new MethodVisitor(Opcodes.ASM8, next) {
                        @Override public void visitCode() {
                            super.visitCode();
                            if (role == 3) {
                                super.visitVarInsn(Opcodes.ALOAD, 0);
                                super.visitMethodInsn(Opcodes.INVOKESTATIC, "mcov/FrameSignal", "combatTick", "(Ljava/lang/Object;)V", false);
                            } else super.visitMethodInsn(Opcodes.INVOKESTATIC, "mcov/FrameSignal", "begin", "()V", false);
                        }
                        @Override public void visitInsn(int opcode) {
                            if (role == 2 && opcode == Opcodes.RETURN)
                                super.visitMethodInsn(Opcodes.INVOKESTATIC, "mcov/FrameSignal", "cameraReady", "()V", false);
                            super.visitInsn(opcode);
                        }
                    };
                }
            }, 0);
            // Preserve stack-map frames; markers have no arguments/new locals.
            return changed[0] ? writer.toByteArray() : null;
        } catch (Throwable error) {
            failure = "Frame marker for " + name + ": " + error;
            System.err.println("[mcov] " + failure);
            return null;
        }
    }
}
