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
import jdk.internal.org.objectweb.asm.Label;

final class FrameTransformer implements ClassFileTransformer {
    private final Instrumentation inst;
    private final Module signalModule;
    private final Class<?> signals;
    volatile String failure;
    FrameTransformer(Instrumentation inst, Class<?> signals) { this.inst = inst; this.signals=signals; this.signalModule = signals.getModule(); }
    @Override public byte[] transform(Module module, ClassLoader loader, String name, Class<?> previous,
                                      ProtectionDomain domain, byte[] original) {
        if (!FrameProfiles.isTarget(name) && !DepthProfiles.isTarget(name)) return null;
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
                    if((access & Opcodes.ACC_STATIC)==0) recognized[0] |= DepthProfiles.marker(name,method,descriptor);
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
            boolean[] cameraAxes={false,false};
            long[] turnBits={0};
            reader.accept(new ClassVisitor(Opcodes.ASM8, writer) {
                @Override public MethodVisitor visitMethod(int access, String method, String descriptor, String signature, String[] exceptions) {
                    MethodVisitor next = super.visitMethod(access, method, descriptor, signature, exceptions);
                    int role = FrameProfiles.role(name, method, descriptor);
                    boolean depthPass=DepthProfiles.worldPass(name,method,descriptor);
                    if (role == 0 && FrameProfiles.cameraMask(name)==0 && !depthPass) return next;
                    if (role != 0) changed[0] = true;
                    return new MethodVisitor(Opcodes.ASM8, next) {
                        @Override public void visitCode() {
                            super.visitCode();
                            if (role == 5) {
                                // Nur übernommene Macro-Platzierungen unterdrücken Vanilla-Use.
                                Label vanilla=new Label();
                                super.visitVarInsn(Opcodes.ALOAD,0);
                                super.visitMethodInsn(Opcodes.INVOKESTATIC,"mcov/FrameSignal","macroUse","(Ljava/lang/Object;)Z",false);
                                super.visitJumpInsn(Opcodes.IFEQ,vanilla);
                                super.visitInsn(Opcodes.RETURN);
                                super.visitLabel(vanilla);
                                super.visitFrame(Opcodes.F_SAME,0,null,0,null);
                            } else if (role == 1 || role == 2) super.visitMethodInsn(Opcodes.INVOKESTATIC, "mcov/FrameSignal", "begin", "()V", false);
                            else if (role == 4) {
                                turnBits[0] |= FrameProfiles.turnMask(name,method,descriptor);
                                super.visitVarInsn(Opcodes.ALOAD,0); super.visitVarInsn(Opcodes.DLOAD,1);
                                super.visitMethodInsn(Opcodes.INVOKESTATIC,"mcov/FrameSignal","turnYaw","(Ljava/lang/Object;D)D",false);
                                super.visitVarInsn(Opcodes.DSTORE,1);
                                super.visitVarInsn(Opcodes.ALOAD,0); super.visitVarInsn(Opcodes.DLOAD,3);
                                super.visitMethodInsn(Opcodes.INVOKESTATIC,"mcov/FrameSignal","turnPitch","(Ljava/lang/Object;D)D",false);
                                super.visitVarInsn(Opcodes.DSTORE,3);
                            }
                        }
                        @Override public void visitMethodInsn(int opcode,String owner,String callee,String desc,boolean iface) {
                            // Der Welt-Depth wird vor dem Hand-/GUI-Pass geloescht.
                            // Den bestehenden Operand-Stack unveraendert lassen.
                            if(depthPass && owner.equals("com/mojang/blaze3d/systems/CommandEncoder") && callee.equals("clearDepthTexture") && desc.equals("(Lcom/mojang/blaze3d/textures/GpuTexture;D)V")) {
                                changed[0]=true;
                                super.visitVarInsn(Opcodes.ALOAD,0);
                                super.visitMethodInsn(Opcodes.INVOKESTATIC,"mcov/FrameSignal","depthReady","(Ljava/lang/Object;)V",false);
                            }
                            final int axis=FrameProfiles.cameraAxis(name,owner,callee,desc);
                            if(axis!=0 && opcode!=Opcodes.INVOKESTATIC) {
                                // receiver/partial werden vor dem Getter dupliziert; keine neuen Locals/Branches.
                                super.visitInsn(Opcodes.DUP2);
                                super.visitMethodInsn(opcode,owner,callee,desc,iface);
                                super.visitMethodInsn(Opcodes.INVOKESTATIC,"mcov/FrameSignal",axis==1 ? "cameraYaw" : "cameraPitch","(Ljava/lang/Object;FF)F",false);
                                cameraAxes[axis-1]=true; changed[0]=true;
                            } else super.visitMethodInsn(opcode,owner,callee,desc,iface);
                        }
                        @Override public void visitInsn(int opcode) {
                            if (role == 3 && opcode == Opcodes.RETURN) {
                                // Erst nach Vanilla-Movement und Modell-Updates:
                                // neue Zielpositionen, kein anschließendes Überschreiben.
                                super.visitVarInsn(Opcodes.ALOAD, 0);
                                super.visitMethodInsn(Opcodes.INVOKESTATIC, "mcov/FrameSignal", "combatTick", "(Ljava/lang/Object;)V", false);
                            }
                            if (role == 2 && opcode == Opcodes.RETURN)
                                super.visitMethodInsn(Opcodes.INVOKESTATIC, "mcov/FrameSignal", "cameraReady", "()V", false);
                            super.visitInsn(opcode);
                        }
                    };
                }
            }, 0);
            // Preserve stack-map frames; markers have no arguments/new locals.
            if(cameraAxes[0] && cameraAxes[1]) signals.getMethod("markCamera",long.class).invoke(null,FrameProfiles.cameraMask(name));
            if(turnBits[0]!=0) signals.getMethod("markTurn",long.class).invoke(null,turnBits[0]);
            return changed[0] ? writer.toByteArray() : null;
        } catch (Throwable error) {
            failure = "Frame marker for " + name + ": " + error;
            System.err.println("[mcov] " + failure);
            return null;
        }
    }
}
