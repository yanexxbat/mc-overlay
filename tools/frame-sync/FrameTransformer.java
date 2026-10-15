package mcov.sync;

import java.lang.instrument.ClassFileTransformer;
import java.lang.instrument.Instrumentation;
import java.security.ProtectionDomain;
import java.util.Map;
import java.util.Set;
import java.util.HashSet;
import jdk.internal.org.objectweb.asm.ClassReader;
import jdk.internal.org.objectweb.asm.ClassVisitor;
import jdk.internal.org.objectweb.asm.ClassWriter;
import jdk.internal.org.objectweb.asm.FieldVisitor;
import jdk.internal.org.objectweb.asm.MethodVisitor;
import jdk.internal.org.objectweb.asm.Opcodes;
import jdk.internal.org.objectweb.asm.Label;
import jdk.internal.org.objectweb.asm.commons.AnalyzerAdapter;

final class FrameTransformer implements ClassFileTransformer {
    private final Instrumentation inst;
    private final Module signalModule;
    private final Class<?> signals;
    volatile String failure;
    FrameTransformer(Instrumentation inst, Class<?> signals) { this.inst = inst; this.signals=signals; this.signalModule = signals.getModule(); }
    @Override public byte[] transform(Module module, ClassLoader loader, String name, Class<?> previous,
                                      ProtectionDomain domain, byte[] original) {
        if(mcov.KnifeProfiles.target(name)) {
            try { byte[] knife=KnifeTransformer.rewrite(inst,module,signals,name,original); if(knife!=null) return knife; }
            catch(Throwable error) { System.err.println("[mcov] optional knife hook unavailable: "+error); }
        }
        if (!FrameProfiles.isTarget(name) && !DepthProfiles.isTarget(name)) return null;
        try {
            ClassReader reader = new ClassReader(original);
            boolean[] recognized = {false};
            Set<String> declaredMethods=new HashSet<>(),declaredFields=new HashSet<>();
            Set<String> overwrittenReceivers=new HashSet<>();
            reader.accept(new ClassVisitor(Opcodes.ASM8) {
                @Override public FieldVisitor visitField(int access, String field, String descriptor, String signature, Object value) {
                    if((access & Opcodes.ACC_STATIC)==0) declaredFields.add(field+descriptor);
                    if ((access & Opcodes.ACC_STATIC) == 0) recognized[0] |= FrameProfiles.fieldMarker(name, field, descriptor);
                    return null;
                }
                @Override public MethodVisitor visitMethod(int access, String method, String descriptor, String signature, String[] exceptions) {
                    if((access & (Opcodes.ACC_STATIC|Opcodes.ACC_ABSTRACT|Opcodes.ACC_NATIVE))==0) declaredMethods.add(method+descriptor);
                    recognized[0] |= FrameProfiles.marker(name, method, descriptor, (access & Opcodes.ACC_STATIC) != 0);
                    if((access & Opcodes.ACC_STATIC)==0) recognized[0] |= DepthProfiles.marker(name,method,descriptor);
                    boolean cameraSetup=false;
                    for(FrameProfiles.Profile p:FrameProfiles.PROFILES) if(p.camera().equals(name)&&p.freeSetup().equals(method)&&p.freeSetupDesc().equals(descriptor)) cameraSetup=true;
                    if(!cameraSetup) return null;
                    return new MethodVisitor(Opcodes.ASM8) {
                        @Override public void visitVarInsn(int opcode,int variable) {
                            if(variable==0 && opcode>=Opcodes.ISTORE && opcode<=Opcodes.ASTORE) overwrittenReceivers.add(method+descriptor);
                        }
                        @Override public void visitIincInsn(int variable,int increment) {
                            if(variable==0) overwrittenReceivers.add(method+descriptor);
                        }
                    };
                }
            }, ClassReader.SKIP_DEBUG | ClassReader.SKIP_FRAMES);
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
            Set<String> movementMethods=new HashSet<>();
            reader.accept(new ClassVisitor(Opcodes.ASM8, writer) {
                @Override public MethodVisitor visitMethod(int access, String method, String descriptor, String signature, String[] exceptions) {
                    MethodVisitor next = super.visitMethod(access, method, descriptor, signature, exceptions);
                    int role = FrameProfiles.role(name, method, descriptor);
                    boolean concrete=(access & (Opcodes.ACC_STATIC | Opcodes.ACC_ABSTRACT | Opcodes.ACC_NATIVE))==0;
                    int movementAxis=concrete ? FrameProfiles.movementAxis(name,method,descriptor) : 0;
                    int movementPart=concrete ? FrameProfiles.movementPart(name,method,descriptor) : 0;
                    boolean depthPass=DepthProfiles.worldPass(name,method,descriptor);
                    FrameProfiles.Profile cameraProfile=null;
                    for(FrameProfiles.Profile p:FrameProfiles.PROFILES)
                        if(p.camera().equals(name)&&p.freeSetup().equals(method)&&p.freeSetupDesc().equals(descriptor)&&
                            !overwrittenReceivers.contains(method+descriptor)&&
                            declaredMethods.contains(p.setPosition()+"(DDD)V")&&declaredMethods.contains(p.setRotation()+"(FF)V")&&declaredFields.contains(p.detached()+"Z")) { cameraProfile=p; break; }
                    final FrameProfiles.Profile freeCamera=cameraProfile;
                    java.util.ArrayList<Object> parameters=new java.util.ArrayList<>(); parameters.add(name);
                    for(jdk.internal.org.objectweb.asm.Type type:jdk.internal.org.objectweb.asm.Type.getArgumentTypes(descriptor)) parameters.add(switch(type.getSort()) {
                        case jdk.internal.org.objectweb.asm.Type.BOOLEAN,jdk.internal.org.objectweb.asm.Type.INT -> Opcodes.INTEGER;
                        case jdk.internal.org.objectweb.asm.Type.FLOAT -> Opcodes.FLOAT;
                        case jdk.internal.org.objectweb.asm.Type.DOUBLE -> Opcodes.DOUBLE;
                        case jdk.internal.org.objectweb.asm.Type.LONG -> Opcodes.LONG;
                        default -> type.getInternalName();
                    });
                    final Object[] initialLocals=parameters.toArray();
                    if (role == 0 && FrameProfiles.cameraMask(name)==0 && !depthPass && movementAxis==0 && movementPart==0) return next;
                    if(movementAxis!=0 || movementPart!=0) { changed[0]=true; movementMethods.add(method+descriptor); }
                    if (role != 0 || freeCamera!=null) changed[0] = true;
                    final AnalyzerAdapter analyzer=freeCamera!=null?new AnalyzerAdapter(name,access,method,descriptor,next):null;
                    return new MethodVisitor(Opcodes.ASM8, analyzer!=null?analyzer:next) {
                        final Label movementStart=new Label(),movementEnd=new Label(),movementHandler=new Label();
                        @Override public void visitFrame(int type,int count,Object[] locals,int stackCount,Object[] stack) {
                            if(freeCamera!=null && (count==0 || Opcodes.TOP.equals(locals[0]))) {
                                // Minecraft deliberately chops even `this` before a terminal
                                // return. Our hook still needs it. Slot 0 is proven unchanged
                                // by the first pass, so retain it in the original frames too.
                                Object[] retained=new Object[Math.max(1,count)];
                                if(count>0) System.arraycopy(locals,0,retained,0,count);
                                retained[0]=name;
                                super.visitFrame(Opcodes.F_NEW,retained.length,retained,stackCount,stack);
                            } else super.visitFrame(type,count,locals,stackCount,stack);
                        }
                        @Override public void visitCode() {
                            super.visitCode();
                            if(movementPart==4 || role==5 || role==6 || role==7 || role==8) {
                                Label vanilla=new Label();
                                if(movementPart==4) super.visitVarInsn(Opcodes.ALOAD,0);
                                super.visitMethodInsn(Opcodes.INVOKESTATIC,"mcov/FrameSignal",movementPart==4?"freezePlayer":"freecamActive",movementPart==4?"(Ljava/lang/Object;)Z":"()Z",false);
                                super.visitJumpInsn(Opcodes.IFEQ,vanilla);
                                if(role==6) super.visitInsn(Opcodes.ICONST_0);
                                super.visitInsn(role==6?Opcodes.IRETURN:Opcodes.RETURN);
                                super.visitLabel(vanilla);
                                super.visitFrame(Opcodes.F_NEW,initialLocals.length,initialLocals,0,null);
                            }
                            if(movementPart!=0) {
                                super.visitMethodInsn(Opcodes.INVOKESTATIC,"mcov/FrameSignal","beginMovement","()V",false);
                                super.visitLabel(movementStart);
                            }
                            if (role == 5) {
                                // Nur übernommene Macro-Platzierungen unterdrücken Vanilla-Use.
                                Label vanilla=new Label();
                                super.visitVarInsn(Opcodes.ALOAD,0);
                                super.visitMethodInsn(Opcodes.INVOKESTATIC,"mcov/FrameSignal","macroUse","(Ljava/lang/Object;)Z",false);
                                super.visitJumpInsn(Opcodes.IFEQ,vanilla);
                                super.visitInsn(Opcodes.RETURN);
                                super.visitLabel(vanilla);
                                super.visitFrame(Opcodes.F_NEW,initialLocals.length,initialLocals,0,null);
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
                            if(movementAxis!=0 && opcode==Opcodes.FRETURN) {
                                // Nur im Physik-Scope liefert der Getter die Kamera. Außerhalb
                                // bleiben Modell, Netzwerk und Combat bei der echten Rotation.
                                super.visitVarInsn(Opcodes.ALOAD,0);
                                super.visitInsn(Opcodes.SWAP);
                                super.visitMethodInsn(Opcodes.INVOKESTATIC,"mcov/FrameSignal",movementAxis==1 ? "movementYaw" : "movementPitch","(Ljava/lang/Object;F)F",false);
                            }
                            if(movementPart!=0 && opcode==Opcodes.RETURN)
                                super.visitMethodInsn(Opcodes.INVOKESTATIC,"mcov/FrameSignal","endMovement","()V",false);
                            if (role == 3 && opcode == Opcodes.RETURN) {
                                // Erst nach Vanilla-Movement und Modell-Updates:
                                // neue Zielpositionen, kein anschließendes Überschreiben.
                                super.visitVarInsn(Opcodes.ALOAD, 0);
                                super.visitMethodInsn(Opcodes.INVOKESTATIC, "mcov/FrameSignal", "combatTick", "(Ljava/lang/Object;)V", false);
                            }
                            if(freeCamera!=null && opcode==Opcodes.RETURN && analyzer.locals!=null) {
                                // Keep all live locals, including mod-added ones. An enclosing
                                // exception handler can still read them after this branch.
                                java.util.ArrayList<Object> returnLocals=new java.util.ArrayList<>();
                                for(int i=0;i<analyzer.locals.size();i++) {
                                    Object type=analyzer.locals.get(i); returnLocals.add(type);
                                    // Analyzer locals are JVM slots; stack-map locals count
                                    // a long/double once, without its second TOP slot.
                                    if(Opcodes.LONG.equals(type)||Opcodes.DOUBLE.equals(type)) ++i;
                                }
                                Label vanilla=new Label();
                                super.visitMethodInsn(Opcodes.INVOKESTATIC,"mcov/FrameSignal","freecamFrame","()Z",false);
                                super.visitJumpInsn(Opcodes.IFEQ,vanilla);
                                super.visitVarInsn(Opcodes.ALOAD,0);
                                for(String axis:new String[]{"renderX","renderY","renderZ"}) super.visitFieldInsn(Opcodes.GETSTATIC,"mcov/FrameSignal",axis,"D");
                                super.visitMethodInsn(Opcodes.INVOKEVIRTUAL,name,freeCamera.setPosition(),"(DDD)V",false);
                                super.visitVarInsn(Opcodes.ALOAD,0);
                                super.visitFieldInsn(Opcodes.GETSTATIC,"mcov/FrameSignal","viewYaw","F");
                                super.visitFieldInsn(Opcodes.GETSTATIC,"mcov/FrameSignal","viewPitch","F");
                                super.visitMethodInsn(Opcodes.INVOKEVIRTUAL,name,freeCamera.setRotation(),"(FF)V",false);
                                super.visitVarInsn(Opcodes.ALOAD,0); super.visitInsn(Opcodes.ICONST_1);
                                super.visitFieldInsn(Opcodes.PUTFIELD,name,freeCamera.detached(),"Z");
                                super.visitLabel(vanilla);
                                super.visitFrame(Opcodes.F_NEW,returnLocals.size(),returnLocals.toArray(),0,null);
                            }
                            if (role == 2 && opcode == Opcodes.RETURN)
                                super.visitMethodInsn(Opcodes.INVOKESTATIC, "mcov/FrameSignal", "cameraReady", "()V", false);
                            super.visitInsn(opcode);
                        }
                        @Override public void visitMaxs(int stack,int locals) {
                            if(movementPart!=0) {
                                // Letzter Handler: vorhandene Minecraft-/Mod-Catches behalten
                                // Vorrang. Auch eine Exception aus einem Callee beendet den Scope.
                                super.visitLabel(movementEnd);
                                super.visitTryCatchBlock(movementStart,movementEnd,movementHandler,null);
                                super.visitLabel(movementHandler);
                                // Keine Locals im Handler nötig; originale Stack-Maps bleiben erhalten.
                                super.visitFrame(Opcodes.F_NEW,0,null,1,new Object[]{"java/lang/Throwable"});
                                super.visitMethodInsn(Opcodes.INVOKESTATIC,"mcov/FrameSignal","endMovement","()V",false);
                                super.visitInsn(Opcodes.ATHROW);
                            }
                            super.visitMaxs(stack,locals);
                        }
                    };
                }
            }, ClassReader.EXPAND_FRAMES);
            // Preserve stack-map frames; markers have no arguments/new locals.
            if(cameraAxes[0] && cameraAxes[1]) signals.getMethod("markCamera",long.class).invoke(null,FrameProfiles.cameraMask(name));
            if(turnBits[0]!=0) signals.getMethod("markTurn",long.class).invoke(null,turnBits[0]);
            for(int part=1;part<=4;++part) {
                long mask=FrameProfiles.movementMask(name,movementMethods,part);
                if(mask!=0) signals.getMethod("markMovement",int.class,long.class).invoke(null,part,mask);
            }
            for(int i=0;i<FrameProfiles.PROFILES.length;++i) {
                FrameProfiles.Profile p=FrameProfiles.PROFILES[i]; long mask=1L<<i;
                if(p.camera().equals(name)&&declaredMethods.contains(p.freeSetup()+p.freeSetupDesc())&&!overwrittenReceivers.contains(p.freeSetup()+p.freeSetupDesc())&&declaredMethods.contains(p.setPosition()+"(DDD)V")&&declaredMethods.contains(p.setRotation()+"(FF)V")&&declaredFields.contains(p.detached()+"Z"))
                    signals.getMethod("markFreecam",int.class,long.class).invoke(null,1,mask);
                if(p.renderer().equals(name)&&declaredMethods.contains(p.hand()+p.handDesc())) signals.getMethod("markFreecam",int.class,long.class).invoke(null,4,mask);
                if(p.localPlayer().equals(name)&&declaredMethods.contains(p.localStep()+"()V")) signals.getMethod("markFreecam",int.class,long.class).invoke(null,2,mask);
                if(p.client().equals(name)&&declaredMethods.contains(p.attack()+"()Z")&&declaredMethods.contains(p.continueAttack()+"(Z)V")&&declaredMethods.contains(p.use()+p.useDesc()))
                    signals.getMethod("markFreecam",int.class,long.class).invoke(null,3,mask);
            }
            return changed[0] ? writer.toByteArray() : null;
        } catch (Throwable error) {
            failure = "Frame marker for " + name + ": " + error;
            System.err.println("[mcov] " + failure);
            return null;
        }
    }
}
